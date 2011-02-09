/*
 * $Id$
 *
 * Copyright (C) 2011 Marius Zbihlei marius.zbihlei@1and1.ro
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 
#ifndef CONSISTENT_HASH_H
#define CONSISTENT_HASH_H

#include "../../str.h"

typedef struct hash_elem_{
    int l_id; /* location id */
    int r_id; /*replica id */
    unsigned int hvalue; /* hash value */
} hash_elem_t;

typedef struct hash_ring_{
    int replicas; /* number of replicas */
    int locations; /*number of locations */
    hash_elem_t *hashes;
} hash_ring_t;

/*!
 create a hash ring and populate it 
 \return 0 on success 
*/
int ch_create(hash_ring_t* ulist, int replicas, int locations);


/*!
brief perform a binary search (upper bound) for a given key
\param ulist the hash ring to match
\param key the string on which to perform matching
\return the location id 
*/
int ch_search_location(hash_ring_t* ulist, str* key);

/*!
increment the location count
*/
int ch_increment_location(hash_ring_t** ulist);

void print(hash_ring_t*);

#endif//CONSISTENT_HASH_H