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

#ifndef _MSRP_TMAP_H_
#define _MSRP_TMAP_H_

#include <time.h>

#include "../../str.h"
#include "../../locking.h"

#include "msrp_parser.h"

typedef struct _msrp_titem
{
    unsigned int titemid;
	msrp_req_cache_t req_cache;
	time_t  expires;
    struct _msrp_titem *prev;
    struct _msrp_titem *next;
} msrp_titem_t;

typedef struct _msrp_tentry
{
	unsigned int lsize;
	msrp_titem_t *first;
	gen_lock_t lock;	
} msrp_tentry_t;

typedef struct _msrp_tmap
{
	unsigned int mapexpire;
	unsigned int mapsize;
	msrp_tentry_t *tslots;
	struct _msrp_tmap *next;
} msrp_tmap_t;

int msrp_tmap_init(int msize);
int msrp_tmap_destroy(void);
int msrp_tmap_clean(void);

int msrp_tmap_save(msrp_frame_t *mf);
int msrp_tmap_lookup(msrp_frame_t *mf);
int msrp_tmap_del(msrp_frame_t *mf);

int msrp_tmap_init_rpc(void);
#endif
