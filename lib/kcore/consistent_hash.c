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

#include "consistent_hash.h"
#include "km_crc.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "string.h"

#include <stdlib.h>

#define NRANGE 65536

static int compare_hash_val(const void* h1, const void* h2){
  unsigned int value1 = ((hash_elem_t*)h1)->hvalue;
  unsigned int value2 = ((hash_elem_t*)h2)->hvalue;
  
  if(value1 > value2) 
    return 1;
  else 
    if(value1 < value2) 
      return -1;
    else 
      return 0;
}

static 
hash_elem_t* ch_binary_search(hash_ring_t* ulist, int val);


int ch_create(hash_ring_t* ulist, int replicas, int locations){
   char aux[256];
   str a;
   int r;
   int k;
   unsigned int val;
   int index = 0;
   if(!ulist) return -1;
   a.s = aux;
   ulist->hashes = (hash_elem_t*)pkg_malloc(replicas*locations*sizeof(hash_elem_t));

   if(!ulist->hashes){
	LM_ERR("no more memory available\n");
	return -1;
   }
   ulist->replicas = replicas;
   ulist->locations = locations;
   
   for(r=0; r<replicas; r++){
     for ( k = 0; k<locations ; k++ ){
	    index = r*ulist->locations+k;
	    /* get value by computing a crc32 */
	    snprintf(a.s, 255, "%d%d", r, k);
	    a.len = strlen(a.s);
	    crc32_uint(&a, &val);
	    ulist->hashes[index].l_id = k;
	    ulist->hashes[index].r_id = r;
	    ulist->hashes[index].hvalue = val%NRANGE;
     }
   }
   /* sort the hashes */
   qsort(ulist->hashes, replicas*locations, sizeof(hash_elem_t), compare_hash_val);
   return 0;
}


int ch_search_location(hash_ring_t* ulist, str* key){
  unsigned int val = 0;
  int value;
  hash_elem_t *pos;

  /* hash the key */
  crc32_uint(key, &val);
  value = val % NRANGE;

  LM_DBG("Searching value %d\n", value);

  if(value > ulist->hashes[ulist->locations*ulist->replicas-1].hvalue)
	pos =  &ulist->hashes[0];
  else
	pos = ch_binary_search(ulist, value);
  
  return pos->l_id;
}


int ch_increment_location(hash_ring_t** ulist){
  hash_ring_t* tmp = pkg_malloc(sizeof(hash_ring_t));
  if(!tmp){
	printf("no more memory");
	return -1;
  }
  ch_create(tmp, (*ulist)->replicas, (*ulist)->locations+1);
  *ulist = tmp;
  return 0;
}

static 
hash_elem_t* ch_binary_search(hash_ring_t* ulist, int val){
  
    int len = ulist->locations * ulist->replicas;
    hash_elem_t* first = &ulist->hashes[0];
    hash_elem_t* middle;
    int half;
    while (len > 0)
	{
	  half = len >> 1;
	  middle = first;
	  middle += half;
	  if ( val < middle->hvalue )
	      len = half;
	  else
	  {  first = middle;
	      ++first;
	      len = len - half - 1;
	  }
	}
      return first;
}

void print(hash_ring_t * ulist){
  int r;
  int k;
  int index=0;
  for(r = 0 ; r < ulist->replicas ; r++){
    for ( k = 0; k < ulist->locations ; k++){
	index = r*ulist->locations+k;
        LM_INFO("Hash %d : value=%d location_id=%d replica=%d\n", index, ulist->hashes[index].hvalue,ulist->hashes[index].l_id, ulist->hashes[index].r_id);
    }
  }
}
