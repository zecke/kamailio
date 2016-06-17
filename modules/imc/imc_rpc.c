/*
 * imc module - RPC functions
 *
 * Copyright (C) 2016 Edvina AB, Olle E. Johansson
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../parser/parse_uri.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"

#include "imc.h"
#include "imc_cmd.h"
#include "imc_mng.h"

/* imc hash table */
extern imc_hentry_p _imc_htable;
extern int imc_hash_size;


static const char* imc_rpc_list_doc[2] = {
	"List all IMC conference members. Arg: <conf>",
	0
};

/*
 * RPC command to list conference members:
 * 	imc.list <conf> <domain>
 */
static void imc_rpc_list(rpc_t* rpc, void* ctx)
{
	void* th;	/* Main structure */
	void* sh;	/* User data */
	str confname = STR_NULL;
	str domain;
	str options = STR_NULL;
	imc_room_p room = 0;
	int flag_room = 0;
	int flag_member = 0;
	int no_args;
	imc_member_p imp = NULL;
	int count = 0;

	/* First check if there are two arguments */
	no_args = rpc->scan(ctx, "SS", &confname, &domain);
	LM_DBG("Number of arguments: %d\n", no_args);

	/* Accept 2 arguments */
	if (no_args != 2) {
		rpc->fault(ctx, 500, "Missing parameters (Parameters: room, domain)");
		return;
	}

	LM_DBG("Looking for room %.*s@%.*s!\n", confname.len, confname.s, domain.len, domain.s);
	room = imc_get_room(&confname, &domain);
	if(room == NULL) {
		/* Error */
		LM_ERR("room [%.*s] does not exist!\n", confname.len, confname.s);
		rpc->fault(ctx, 500, "Conference room does not exist");
		return;
	}
	LM_DBG("Found room: [%.*s] \n", confname.len, confname.s);

	/* add entry node */
        if (rpc->add(ctx, "{", &th) < 0)
        {
                rpc->fault(ctx, 500, "Internal error creating root reply");
                return;
        }

	/* We can state number here    room->nr_of_members */
	LM_DBG("Number of members: %d\n", room->nr_of_members);

	imp = room->members;
	if(rpc->struct_add(th, "d{",
		"NO_USERS", room->nr_of_members,
		"USERS", &sh) < 0)
	{
		rpc->fault(ctx, 500, "Internal error creating dest");
		return;
	}

        while(imp) {
		count++;
		LM_DBG("Member %d : [%.*s] \n", count, imp->user.len, imp->user.s);

		if(rpc->struct_add(sh, "SS",
			"USER", &imp->user,
			"DOMAIN", &imp->domain) < 0) {
				rpc->fault(ctx, 500, "Internal error creating dest struct");
				return;
		}

		imp = imp->next;
	}

	imc_release_room(room);

	return;
}

static const char* imc_rpc_listall_doc[2] = {
	"List all IMC conference members. Arg: <conf> <domain>",
	0
};


/*
 * RPC command to list all conferences :
 * 	imc.listall 
 */
static void imc_rpc_listall(rpc_t* rpc, void* ctx)
{
	void* th;
	void* sh;
	void* rh;
	imc_room_p room = 0;
	int i;
	int activeconferences = 0;
	imc_room_p irp = NULL;

	if(_imc_htable==NULL) {
		rpc->fault(ctx, 400, "No active conferences");
		return;
	}
	/* add entry node */
        if (rpc->add(ctx, "{", &th) < 0)
        {
                rpc->fault(ctx, 500, "Internal error creating root reply");
                return;
        }
	if(rpc->struct_add(th, "{", "ROOMS", &sh) < 0)
	{
		rpc->fault(ctx, 500, "Internal error creating room list");
		return;
	}
	/* Loop through all hash entries to find rooms */
	for(i=0; i<imc_hash_size; i++)
	{
		LM_DBG("      ---- Locking hash entry %d \n", i);
		lock_get(&_imc_htable[i].lock);
		if(_imc_htable[i].rooms==NULL) {
			LM_DBG("      ---- No rooms in hash entry %d \n", i);
			lock_release(&_imc_htable[i].lock);
			continue;
		}
		irp = _imc_htable[i].rooms;
		while(irp) {
			if(rpc->struct_add(sh, "{", "ROOM", &rh) < 0)
			{
				rpc->fault(ctx, 500, "Internal error creating room list");
				lock_release(&_imc_htable[i].lock);
				return;
			}
			activeconferences++;
			/* Make sure to add data on number of users
			 * as well as last time for action 
			 */
			if(rpc->struct_add(rh, "SSd",
				"NAME", &irp->name,
				"DOMAIN", &irp->domain,
				"MEMBERS", irp->nr_of_members) < 0) {
					rpc->fault(ctx, 500, "Internal error creating dest struct");
					lock_release(&_imc_htable[i].lock);
					return;
			}
			irp = irp->next;
		}
		LM_DBG("      ---- Unlocking hash entry %d \n", i);
		lock_release(&_imc_htable[i].lock);
	}
	LM_DBG("*** Number of rooms: %d \n", activeconferences);
	return;
}

static const char* imc_rpc_create_doc[2] = {
	"Create IMC conference. Arg: <conf> <domain> [<option>] where option can be 'private'",
	0
};

/*
 * RPC command to create conferences :
 * 	imc.create <conf> <domain> [<flag>]
 *		flag: "private"
 */
static void imc_rpc_create(rpc_t* rpc, void* ctx)
{
	void* th;
	void* rh;
	str confname = STR_NULL;
	str domain;
	str options = STR_NULL;
	imc_room_p room = 0;
	int flag_room = 0;
	int flag_member = 0;
	int no_args;

	/* First check if there are two arguments */
	no_args = rpc->scan(ctx, "SS", &confname, &domain, &options);

	LM_DBG("Number of arguments: %d\n", no_args);

	/* Accept 2 or 3 arguments */
	if (no_args != 3 && no_args != 2) {
		rpc->fault(ctx, 500, "Missing parameter roomname, domain (Parameters: room, domain [option])");
		return;
	}
	LM_DBG("Check if room exists: %.*s@%.*s\n", confname.len, confname.s, domain.len, domain.s);

	/* Check if room exists first */
	room = imc_get_room(&confname, &domain);
	if(room != NULL) {
		/* Error */
		rpc->fault(ctx, 500, "Conference room already exists");
                imc_release_room(room);
		return;
	}
	if (options.len > 0 && options.s[0] == 'p') {
		flag_room |= IMC_ROOM_PRIV;
	}
	LM_DBG("Creating room: %.*s@%.*s\n", confname.len, confname.s, domain.len, domain.s);
	room = imc_add_room(&confname, &domain, flag_room);
	if(room == NULL)
	{
		LM_ERR("Failed to add new room (RPC imc.create)\n");
		return;
	}	
	LM_DBG("Added room uri= %.*s\n", room->uri.len, room->uri.s);
	imc_release_room(room);
	rpc->rpl_printf(ctx, "OK. Room successfully created");

	return;
}

static const char* imc_rpc_destroy_doc[2] = {
	"Destroy IMC conference. Kicks all remaining members. Arg: <conf>",
	0
};

/*
 * RPC command to destroy conferences :
 * 	imc.destroy <conf>
 */
static void imc_rpc_destroy(rpc_t* rpc, void* ctx)
{
	void* th;
	void* rh;

	str confname;
	str domain;
	int no_args;
	imc_room_p room = 0;
	int res;

	/* First check if there are two arguments */
	no_args = rpc->scan(ctx, "SS", &confname, &domain);

	LM_DBG("Number of arguments: %d\n", no_args);

	/* Accept only 2 arguments */
	if (no_args != 2) {
		rpc->fault(ctx, 500, "Missing parameters (Parameters: room, domain)");
		return;
	}

        LM_DBG("Before imc_get_room. Searching for: %.*s@%.*s\n", confname.len, confname.s, domain.len, domain.s);
	room = imc_get_room(&confname, &domain);
        LM_DBG("After imc_get_room \n");

	if(room == NULL) {
		/* Error */
		LM_ERR("room [%.*s] does not exist!\n", confname.len, confname.s);
		rpc->fault(ctx, 500, "Conference room does not exist");
		return;
	}

        LM_DBG("Found room to delete, about to set flag\n");
	room->flags |= IMC_ROOM_DELETED;
        LM_DBG("Found room to delete\n");

	/* We could check if there are any members around and kick them */
	imc_release_room(room);

        LM_DBG("deleting room...\n");
        res = imc_del_room(&confname, &domain);
	if (res < 0) {
		rpc->fault(ctx, 500, "Deletion failed");
	} else {
		rpc->rpl_printf(ctx, "OK. Room deleted");
	}
	return;
}

static const char* imc_rpc_addmember_doc[2] = {
	"Add member to IMC conference. Arg: <conf> <domain> <SIP uri> [<option>]",
	0
};

/*
 * RPC command to add member to conference :
 * 	imc.addmember <conf> <uri> <user-uri>
 */
static void imc_rpc_addmember(rpc_t* rpc, void* ctx)
{
	str confname = STR_NULL;
	str domain;
	str useruri;
	int no_args;
	imc_room_p room = 0;
	int res;
	imc_member_p imp = NULL;

	/* First check if there are two arguments */
	no_args = rpc->scan(ctx, "SSS", &confname, &domain, &useruri);

	LM_DBG("Number of arguments: %d\n", no_args);

	/* Accept only 3 arguments */
	if (no_args != 3) {
		rpc->fault(ctx, 500, "Missing parameters (Parameters: room, domain, user-uri)");
		return;
	}

	room = imc_get_room(&confname, &domain);
	if(room == NULL) {
		/* Error */
		LM_ERR("room [%.*s] does not exist!\n", confname.len, confname.s);
		rpc->fault(ctx, 500, "Conference room does not exist");
		return;
	}
	/* OEJ: useruri needs to be divided into two. Or we need to create
	   imc_add_uri(room, uri, option)
	 */
	imp = imc_add_member(room, &useruri, &domain, 0);
	if (imp == NULL) {
		rpc->fault(ctx, 500, "Failure adding member");
		imc_release_room(room);
		return;
	}

	rpc->rpl_printf(ctx, "OK. Member added");
	imc_release_room(room);
	return;
}

static const char* imc_rpc_kickmember_doc[2] = {
	"Kick member from IMC conference. Arg: <conf> <member>",
	0
};

/*
 * RPC command to destroy conferences :
 * 	imc.kickmember <conf>
 */
static void imc_rpc_kickmember(rpc_t* rpc, void* ctx)
{
	str confname = STR_NULL;
	str domain;
	str useruri;
	int no_args;
	imc_room_p room = 0;
	int res;
	imc_member_p imp = NULL;

	/* First check if there are three arguments */
	no_args = rpc->scan(ctx, "SSS", &confname, &domain, &useruri);

	LM_DBG("Number of arguments: %d\n", no_args);

	/* Accept only 3 arguments */
	if (no_args != 3) {
		rpc->fault(ctx, 500, "Missing parameters (Parameters: room, domain, user-uri)");
		return;
	}

	room = imc_get_room(&confname, &domain);
	if(room == NULL) {
		/* Error */
		LM_ERR("room [%.*s] does not exist!\n", confname.len, confname.s);
		rpc->fault(ctx, 500, "Conference room does not exist");
		return;
	}
	/* OEJ: useruri needs to be divided into two. Or we need to create
	   imc_add_uri(room, uri, option)
	 */
	imp = imc_del_member(room, &useruri, &domain, 0);
	if (imp == NULL) {
		rpc->fault(ctx, 500, "Failure kicking member");
		imc_release_room(room);
		return;
	}

	rpc->rpl_printf(ctx, "OK. Member kicked");
	imc_release_room(room);
	return;
}

rpc_export_t imc_rpc_cmds[] = {
	{"imc.list",		imc_rpc_list,		imc_rpc_list_doc,	0}, 	/* List conference participants */
	{"imc.listall",		imc_rpc_listall,	imc_rpc_listall_doc,	0}, 	/* List all conferences */
	{"imc.create",		imc_rpc_create,		imc_rpc_create_doc,	0}, 	/* Create conference */
	{"imc.destroy",		imc_rpc_destroy,	imc_rpc_destroy_doc,	0}, 	/* Destroy conference */
	{"imc.addmember",	imc_rpc_addmember,	imc_rpc_addmember_doc,	0}, 	/* Add member to conference */
	{"imc.kickmember",	imc_rpc_kickmember,	imc_rpc_kickmember_doc,	0}, 	/* Kick member from conference */
	{0, 0, 0, 0}
};

/**
 * register RPC commands
 */
int imc_init_rpc(void)
{
	if (rpc_register_array(imc_rpc_cmds)!=0)
	{
		LM_ERR("IMC: Failed to register RPC commands\n");
		return -1;
	}
	return 0;
}


