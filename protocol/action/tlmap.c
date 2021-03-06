//----------------------------------------------------------------------------
// ZetaScale
// Copyright (c) 2016, SanDisk Corp. and/or all its affiliates.
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published by the Free
// Software Foundation;
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License v2.1 for more details.
//
// A copy of the GNU Lesser General Public License v2.1 is provided with this package and
// can also be found at: http://opensource.org/licenses/LGPL-2.1
// You should have received a copy of the GNU Lesser General Public License along with
// this program; if not, write to the Free Software Foundation, Inc., 59 Temple
// Place, Suite 330, Boston, MA 02111-1307 USA.
//----------------------------------------------------------------------------

/*
 * File:   tlmap.c
 * Author: Brian O'Krafka
 *
 * Created on September 11, 2008
 *
 * (c) Copyright 2008, Schooner Information Technology, Inc.
 * http://www.schoonerinfotech.com/
 *
 * $Id: tlmap.c 308 2008-02-20 22:34:58Z tomr $
 */

#define _TLMAP_C

#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include "platform/logging.h"
#include "platform/stdlib.h"
#include "tlmap.h"
#include "utils/hash.h"
#include "protocol/protocol_alloc.h"

void SDFTLMapInit(SDFTLMap_t *pm, uint64_t nbuckets, 
     int (*print_fn)(SDFTLMapEntry_t *pce, char *sout, int max_len))
{
    uint64_t          i;

    pm->nbuckets      = nbuckets;
    pm->print_fn      = print_fn;
    pm->buckets       = proto_plat_alloc_arena(nbuckets*(sizeof(SDFTLMapBucket_t)), PLAT_SHMEM_ARENA_CACHE_THREAD);
    #ifdef MALLOC_TRACE
        UTMallocTrace("SDFTLMapInit", TRUE, FALSE, FALSE, (void *) pm->buckets, nbuckets*sizeof(SDFTLMapBucket_t));
    #endif // MALLOC_TRACE
    if (pm->buckets == NULL) {
	plat_log_msg(21288, PLAT_LOG_CAT_SDF_CC, PLAT_LOG_LEVEL_FATAL,
	     "Could not allocate thread-local map buckets.");
	plat_abort();
    }

    for (i=0; i<nbuckets; i++) {
	pm->buckets[i].entry = NULL;
    }
}

SDFTLMapEntry_t *SDFTLMapGetCreate(SDFTLMap_t *pm, char *pkey)
{
    int                keylen;
    uint64_t           h;
    SDFTLMapEntry_t   *pme;
    SDFTLMapBucket_t  *pb;

    keylen = strlen(pkey);
    h = hashb((const unsigned char *) pkey, strlen(pkey), 0) %
        pm->nbuckets;
    pb = &(pm->buckets[h]);

    for (pme = pb->entry; pme != NULL; pme = pme->next) {
	if ((pme->keylen == keylen) && 
	    (strcmp((const char *) pme->key, (const char *) pkey) == 0))
	{
	    break;
	}
    }

    if (pme == NULL) {

        /* Create a new entry. */

	pme = (SDFTLMapEntry_t *) proto_plat_alloc_arena(sizeof(SDFTLMapEntry_t), PLAT_SHMEM_ARENA_CACHE_THREAD);
	#ifdef MALLOC_TRACE
	    UTMallocTrace("SDFTLMapGetCreate: entry", FALSE, FALSE, FALSE, (void *) pme, sizeof(SDFTLMapEntry_t));
	#endif // MALLOC_TRACE
	if (pme == NULL) {
	    plat_log_msg(21289, PLAT_LOG_CAT_SDF_CC, PLAT_LOG_LEVEL_ERROR,
		 "Could not allocate a thread-local map entry.");
	    return(NULL);
	}

	pme->contents = NULL;
	pme->key      = (char *) proto_plat_alloc_arena(keylen+1, PLAT_SHMEM_ARENA_CACHE_THREAD);
	#ifdef MALLOC_TRACE
	    UTMallocTrace("SDFTLMapGetCreate: key", FALSE, FALSE, FALSE, (void *) pme->key, keylen+1);
	#endif // MALLOC_TRACE

	if (pme->key == NULL) {
	    plat_log_msg(21290, PLAT_LOG_CAT_SDF_CC, PLAT_LOG_LEVEL_ERROR,
		 "Could not allocate a thread-local key.");
	    plat_free(pme);
	    return(NULL);
	}
	strcpy(pme->key, pkey);
	pme->keylen   = keylen;

	/* put myself on the bucket list */
        pme->next = pb->entry;
        pb->entry = pme;
    }

    return(pme);
}

