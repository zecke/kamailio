/**
 * $Id$
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "../../lib/kcore/faked_msg.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../action.h"
#include "../../dprint.h"
#include "../../hashes.h"
#include "../../route.h"
#include "../../ut.h"

#include "../../lib/srutils/sruid.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"

#include "msrp_netio.h"
#include "msrp_env.h"
#include "msrp_tmap.h"

static msrp_tmap_t *_msrp_tmap_head = NULL;

/**
 *
 */
int msrp_titem_free(msrp_titem_t *it)
{
	if(it==NULL)
		return -1;
	shm_free(it);
	return 0;
}

/**
 *
 */
int msrp_tmap_init(int msize)
{
	int i;

	_msrp_tmap_head = (msrp_tmap_t*)shm_malloc(sizeof(msrp_tmap_t));
	if(_msrp_tmap_head==NULL)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(_msrp_tmap_head, 0, sizeof(msrp_tmap_t));
	_msrp_tmap_head->mapsize = msize;

	_msrp_tmap_head->tslots = (msrp_tentry_t*)shm_malloc(
							_msrp_tmap_head->mapsize*sizeof(msrp_tentry_t) );
	if(_msrp_tmap_head->tslots==NULL)
	{
		LM_ERR("no more shm.\n");
		shm_free(_msrp_tmap_head);
		_msrp_tmap_head = NULL;
		return -1;
	}
	memset(_msrp_tmap_head->tslots, 0,
						_msrp_tmap_head->mapsize*sizeof(msrp_tentry_t));

	for(i=0; i<_msrp_tmap_head->mapsize; i++)
	{
		if(lock_init(&_msrp_tmap_head->tslots[i].lock)==0)
		{
			LM_ERR("cannot initalize lock[%d]\n", i);
			i--;
			while(i>=0)
			{
				lock_destroy(&_msrp_tmap_head->tslots[i].lock);
				i--;
			}
			shm_free(_msrp_tmap_head->tslots);
			shm_free(_msrp_tmap_head);
			_msrp_tmap_head = NULL;
			return -1;
		}
	}

	return 0;
}

/**
 *
 */
int msrp_tmap_destroy(void)
{
	int i;
	msrp_titem_t *ita, *itb;

	if(_msrp_tmap_head==NULL)
		return -1;

	for(i=0; i<_msrp_tmap_head->mapsize; i++)
	{
		/* free entries */
		ita = _msrp_tmap_head->tslots[i].first;
		while(ita)
		{
			itb = ita;
			ita = ita->next;
			msrp_titem_free(itb);
		}
		/* free locks */
		lock_destroy(&_msrp_tmap_head->tslots[i].lock);
	}
	shm_free(_msrp_tmap_head->tslots);
	shm_free(_msrp_tmap_head);
	_msrp_tmap_head = NULL;
	return 0;
}

#define msrp_get_hashid(_s)        core_case_hash(_s,0,0)
#define msrp_get_slot(_h, _size)    (_h)&((_size)-1)

/**
 *
 */
int msrp_tmap_save(msrp_frame_t *mf)
{
	unsigned int idx;
	unsigned int hid;
	str local_uri;
	msrp_hdr_t *fpath, *mid, *brange;
	int brange_len, msize;
	msrp_titem_t *it;
	msrp_titem_t *itb;

	if(_msrp_tmap_head==NULL || mf==NULL)
		return -1;
	if(mf->fline.rtypeid!=MSRP_REQ_SEND)
	{
		LM_DBG("save can be used only for SEND\n");
		return -2;
	}

	if(msrp_frame_get_first_to_path(mf, &local_uri)<0)
	{
		LM_ERR("cannot get local URI from To-Path:\n");
		return -1;
	}
	fpath = msrp_get_hdr_by_id(mf, MSRP_HDR_FROM_PATH);
	if (fpath == NULL)
	{
		LM_ERR("cannot get From-Path:\n");
		return -1;
	}
	mid = msrp_get_hdr_by_id(mf, MSRP_HDR_MESSAGE_ID);
	if (mid == NULL)
	{
		LM_ERR("cannot get Message-ID:\n");
		return -1;
	}
	brange = msrp_get_hdr_by_id(mf, MSRP_HDR_BYTE_RANGE);
	if (brange == NULL)
		brange_len = 0;
	else
		brange_len = brange->body.len;

	hid = msrp_get_hashid(&mf->fline.transaction);	
	idx = msrp_get_slot(hid, _msrp_tmap_head->mapsize);

	msize = sizeof(msrp_titem_t) + (mf->fline.transaction.len
					+ local_uri.len + fpath->body.len
					+ mid->body.len + brange_len
					+ (brange_len > 0) ? 5 : 4)
						* sizeof(char);

	/* build the item */
	it = (msrp_titem_t*)shm_malloc(msize);
	if(it==NULL)
	{
		LM_ERR("no more shm\n");
		return -1;
	}
	memset(it, 0, msize);
	it->titemid = hid;

	it->transactionid.s = (char*)it +  + sizeof(msrp_titem_t);
	it->transactionid.len = mf->fline.transaction.len;
	memcpy(it->transactionid.s, mf->fline.transaction.s, mf->fline.transaction.len);
	it->transactionid.s[it->transactionid.len] = '\0';

	it->req_cache.local_uri.s = it->transactionid.s + it->transactionid.len + 1;
	it->req_cache.local_uri.len = local_uri.len;
	memcpy(it->req_cache.local_uri.s, local_uri.s, local_uri.len);
	it->req_cache.local_uri.s[it->req_cache.local_uri.len] = '\0';

	it->req_cache.from_path.s = it->req_cache.local_uri.s + it->req_cache.local_uri.len + 1;
	it->req_cache.from_path.len = fpath->body.len;
	memcpy(it->req_cache.from_path.s, fpath->body.s, fpath->body.len);
	it->req_cache.from_path.s[it->req_cache.from_path.len] = '\0';

	it->req_cache.message_id.s = it->req_cache.from_path.s + it->req_cache.from_path.len + 1;
	it->req_cache.message_id.len = mid->body.len;
	memcpy(it->req_cache.message_id.s, mid->body.s, mid->body.len);
	it->req_cache.message_id.s[it->req_cache.message_id.len] = '\0';

	if (brange_len == 0)
	{
		it->req_cache.byte_range.len = 0;
		it->req_cache.byte_range.s = NULL;
	}
	else
	{
		it->req_cache.byte_range.s = it->req_cache.message_id.s + it->req_cache.message_id.len + 1;
		it->req_cache.byte_range.len = brange->body.len;
		memcpy(it->req_cache.byte_range.s, brange->body.s, brange->body.len);
		it->req_cache.byte_range.s[it->req_cache.byte_range.len] = '\0';
	}

	it->expires = time(NULL) + 30;

	/* insert item in tmap */
	lock_get(&_msrp_tmap_head->tslots[idx].lock);
	if(_msrp_tmap_head->tslots[idx].first==NULL) {
		_msrp_tmap_head->tslots[idx].first = it;
	} else {
		for(itb=_msrp_tmap_head->tslots[idx].first; itb; itb=itb->next)
		{
			if(itb->titemid>it->titemid || itb->next==NULL) {
				if(itb->next==NULL) {
					itb->next=it;
					it->prev = itb;
				} else {
					it->next = itb;
					if(itb->prev==NULL) {
						_msrp_tmap_head->tslots[idx].first = it;
					} else {
						itb->prev->next = it;
					}
					it->prev = itb->prev;
					itb->prev = it;
				}
				break;
			}
		}
	}
	_msrp_tmap_head->tslots[idx].lsize++;
	lock_release(&_msrp_tmap_head->tslots[idx].lock);

	return 0;
}

/**
 *
 */
msrp_req_cache_t *pkg_copy_req_cache(msrp_req_cache_t *orig)
{
	msrp_req_cache_t *copy;
	int msize;

	if (orig == NULL)
	{
		LM_ERR("nothing to copy\n");
		return NULL;
	}

	msize = sizeof(msrp_req_cache_t) + (orig->local_uri.len + orig->from_path.len
					+ orig->message_id.len + orig->byte_range.len
					+ (orig->byte_range.len > 0) ? 4 : 3) * sizeof(char);
	copy = pkg_malloc(msize);
	if (copy == NULL)
	{
		LM_ERR("no more pkg mem\n");
		return NULL;
	}

	memset(copy, 0, msize);

	copy->local_uri.s = (char*)copy +  + sizeof(msrp_titem_t);
	copy->local_uri.len = orig->local_uri.len;
	memcpy(copy->local_uri.s, orig->local_uri.s, orig->local_uri.len);
	copy->local_uri.s[copy->local_uri.len] = '\0';

	copy->from_path.s = copy->local_uri.s + copy->local_uri.len + 1;
	copy->from_path.len = orig->from_path.len;
	memcpy(copy->from_path.s, orig->from_path.s, orig->from_path.len);
	copy->from_path.s[copy->from_path.len] = '\0';

	copy->message_id.s = copy->from_path.s + copy->from_path.len + 1;
	copy->message_id.len = orig->message_id.len;
	memcpy(copy->message_id.s, orig->message_id.s, orig->message_id.len);
	copy->message_id.s[copy->message_id.len] = '\0';

	if (orig->byte_range.len == 0)
	{
		copy->byte_range.len = 0;
		copy->byte_range.s = NULL;
	}
	else
	{
		copy->byte_range.s = copy->message_id.s + copy->message_id.len + 1;
		copy->byte_range.len = orig->byte_range.len;
		memcpy(copy->byte_range.s, orig->byte_range.s, orig->byte_range.len);
		copy->byte_range.s[copy->byte_range.len] = '\0';
	}

	return copy;
}

/**
 *
 */
int msrp_tmap_lookup(msrp_frame_t *mf)
{
	unsigned int idx;
	unsigned int hid;
	msrp_titem_t *itb;
	str *tid = &mf->fline.transaction;

	if(_msrp_tmap_head==NULL || mf==NULL)
		return -1;
	if(mf->fline.msgtypeid==MSRP_REQUEST)
	{
		LM_DBG("lookup cannot be used for requests\n");
		return -2;
	}

	LM_DBG("searching for transaction [%.*s]\n", tid->len, tid->s);

	hid = msrp_get_hashid(tid);
	idx = msrp_get_slot(hid, _msrp_tmap_head->mapsize);

	lock_get(&_msrp_tmap_head->tslots[idx].lock);
	for(itb=_msrp_tmap_head->tslots[idx].first; itb; itb=itb->next)
	{
		if(itb->titemid>hid) {
			break;
		} else {
			if(itb->transactionid.len == tid->len
					&& memcmp(itb->transactionid.s, tid->s, tid->len)==0) {
				LM_DBG("found transaction [%.*s]\n", tid->len, tid->s);
				mf->req_cache = pkg_copy_req_cache(&itb->req_cache);
				break;
			}
		}
	}
	lock_release(&_msrp_tmap_head->tslots[idx].lock);
	if(itb==NULL)
		return -4;
	return (mf->req_cache == NULL)?-5:0;
}

/**
 *
 */
int msrp_tmap_del(msrp_frame_t *mf)
{
	unsigned int idx;
	unsigned int hid;
	msrp_titem_t *itb;
	str *tid = &mf->fline.transaction;

	if(_msrp_tmap_head==NULL || mf==NULL)
		return -1;
	if(mf->fline.msgtypeid==MSRP_REQUEST)
	{
		LM_DBG("lookup cannot be used for requests\n");
		return -2;
	}

	LM_DBG("searching for transaction [%.*s]\n", tid->len, tid->s);

	hid = msrp_get_hashid(tid);
	idx = msrp_get_slot(hid, _msrp_tmap_head->mapsize);

	lock_get(&_msrp_tmap_head->tslots[idx].lock);
	for(itb=_msrp_tmap_head->tslots[idx].first; itb; itb=itb->next)
	{
		if(itb->titemid>hid) {
			break;
		} else {
			if(itb->transactionid.len == tid->len
					&& memcmp(itb->transactionid.s, tid->s, tid->len)==0) {
				LM_DBG("found transaction [%.*s]\n", tid->len, tid->s);
				msrp_titem_free(itb);
				break;
			}
		}
	}
	lock_release(&_msrp_tmap_head->tslots[idx].lock);
	if(itb==NULL)
		return -4;
	return 0;
}

/**
 *
 */
static void generate_timeout_event_route(msrp_titem_t *transaction)
{
	int rt, backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;
	static msrp_frame_t mf;

	rt = route_get(&event_rt, "msrp:transaction-timeout");
	if (rt < 0 && event_rt.rlist[rt] == NULL) {
		LM_DBG("route does not exist\n");
		return;
	}

	if (faked_msg_init() < 0) {
		LM_ERR("faked_msg_init() failed\n");
		return;
	}
	fmsg = faked_msg_next();

	memset(&mf, 0, sizeof(msrp_frame_t));
	mf.fline.msgtypeid = MSRP_REPLY;
	mf.req_cache = &transaction->req_cache;
	msrp_reset_env();
	msrp_set_current_frame(&mf);

	backup_rt = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	run_top_route(event_rt.rlist[rt], fmsg, 0);
	set_route_type(backup_rt);

	msrp_reset_env();
	mf.req_cache = NULL;
}

/**
 *
 */
int msrp_tmap_clean(void)
{
	time_t tnow;
	msrp_titem_t *ita;
	msrp_titem_t *itb;
	int i;

	if(_msrp_tmap_head==NULL)
		return -1;
	tnow = time(NULL);
	for(i=0; i<_msrp_tmap_head->mapsize; i++)
	{
		lock_get(&_msrp_tmap_head->tslots[i].lock);
		ita = _msrp_tmap_head->tslots[i].first;
		while(ita)
		{
			itb = ita;
			ita = ita->next;
			if(itb->expires<tnow) {
				if(itb->prev==NULL) {
					_msrp_tmap_head->tslots[i].first = itb->next;
				} else {
					itb->prev->next = ita;
				}
				if(ita!=NULL)
					ita->prev = itb->prev;
				generate_timeout_event_route(itb);
				msrp_titem_free(itb);
				_msrp_tmap_head->tslots[i].lsize--;
			}
		}
		lock_release(&_msrp_tmap_head->tslots[i].lock);
	}

	return 0;
}

static const char* msrp_tmap_rpc_list_doc[2] = {
	"Return the content of MSRP transaction map",
	0
};


/*
 * RPC command to print transaction map table
 */
static void msrp_tmap_rpc_list(rpc_t* rpc, void* ctx)
{
	void* th;
	void* ih;
	void* vh;
	msrp_titem_t *it;
	int i;
	int n;
	str edate;

	if(_msrp_tmap_head==NULL)
	{
		LM_ERR("no transaction map table\n");
		rpc->fault(ctx, 500, "No Transaction Map Table");
		return;
	}

	/* add entry node */
	if (rpc->add(ctx, "{", &th) < 0)
	{
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}

	if(rpc->struct_add(th, "d{",
				"MAP_SIZE", _msrp_tmap_head->mapsize,
				"TRANLIST",  &ih)<0)
	{
		rpc->fault(ctx, 500, "Internal error set structure");
		return;
	}
	n = 0;
	for(i=0; i<_msrp_tmap_head->mapsize; i++)
	{
		lock_get(&_msrp_tmap_head->tslots[i].lock);
		for(it=_msrp_tmap_head->tslots[i].first; it; it=it->next)
		{
			if(rpc->struct_add(ih, "{",
						"TRANDATA", &vh)<0)
			{
				rpc->fault(ctx, 500, "Internal error creating connection");
				lock_release(&_msrp_tmap_head->tslots[i].lock);
				return;
			}
			edate.s = ctime(&it->expires);
			edate.len = 24;
			if(rpc->struct_add(vh, "dSSSSSS",
						"TITEMID", it->titemid,
						"TRANSACTIONID", &it->transactionid,
						"LOCALURI", &it->req_cache.local_uri,
						"FROMPATH", &it->req_cache.from_path,
						"MESSAGEID", &it->req_cache.message_id,
						"BYTERANGE", &it->req_cache.byte_range,
						"EXPIRES", &edate)<0)
			{
				rpc->fault(ctx, 500, "Internal error creating dest struct");
				lock_release(&_msrp_tmap_head->tslots[i].lock);
				return;
			}
			n++;
		}
		lock_release(&_msrp_tmap_head->tslots[i].lock);
	}
	if(rpc->struct_add(th, "d", "TRANCOUNT", n)<0)
	{
		rpc->fault(ctx, 500, "Internal error connection counter");
		return;
	}
	return;
}

rpc_export_t msrp_tmap_rpc_cmds[] = {
	{"msrp.tmaplist",   msrp_tmap_rpc_list,
		msrp_tmap_rpc_list_doc,   0},
	{0, 0, 0, 0}
};

/**
 *
 */
int msrp_tmap_init_rpc(void)
{
	if (rpc_register_array(msrp_tmap_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	return 0;
}
