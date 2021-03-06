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
 * Fast recovery and enumeration.
 * Author: Johann George
 *
 * Copyright (c) 2010-2013, SanDisk Corporation.  All rights reserved.
 */
#ifndef _RECOVERY_H
#define _RECOVERY_H
#include "zs.h"
#include "fdf_internal.h"
#include "shared/private.h"
#include "ssd/fifo/mcd_osd.h"
#include "protocol/action/action_internal_ctxt.h"


/*
 * Fast recovery.
 */


/*
 * Home flash NOP command extra data.
 */
typedef struct {
    uint16_t rec_ver;                   /* Recovery version */
} sdf_hfnop_t;



/*
 * Linkage to fast recovery code.
 */
struct sdf_rec_funcs {
    /*
     * Copies a container having the cguid from the surviving node with the
     * given rank.  Returns -1 if the other node cannot handle fast recovery, 0
     * if the recovery failed and 1 if we succeeded.  Called from
     * simple_replicator_start_new_replica in simple_replication.c
     */
    int (*ctr_copy)(vnode_t rank,
                    struct shard *shard,
                    SDF_action_init_t *pai);

    /*
     * Called when a message is received by home flash that is intended for
     * recovery.  Called from pts_main in home_flash.c.
     */
    void (*msg_recv)(sdf_msg_t *msg,
                     SDF_action_init_t *pai,
                     SDF_action_state_t *pas,
                     struct flashDev *flash);

    /*
     * Called to fill a HFNOP structure.
     */
    void (*nop_fill)(sdf_hfnop_t *nop);

    /*
     * Called before we send a delete to the other side.
     */
    void (*prep_del)(vnode_t rank, struct shard *shard);
} *sdf_rec_funcs;




/*
 * Enumeration.
 */
void enumerate_stats(enum_stats_t *s, ZS_cguid_t cguid);

ZS_status_t
enumerate_init(SDF_action_init_t *pai, struct shard *shard,
               ZS_cguid_t cguid, struct ZS_iterator **iter);

ZS_status_t
enumerate_done(SDF_action_init_t *pai, struct ZS_iterator *iter);

ZS_status_t
enumerate_next(SDF_action_init_t *pai, struct ZS_iterator *iter,
               char **key, uint64_t *keylen, char **data, uint64_t *datalen);

ZS_cguid_t get_e_cguid(struct ZS_iterator *iter);



/*
 * Cache Hash functions.
 */
int chash_bits(shard_t *sshard);

uint64_t
chash_key(shard_t *sshard, SDF_cguid_t cguid, char *key,
          uint64_t keylen, uint64_t num_bkts, hashsyn_t *hashsynp);

uint64_t
chash_index(struct shard *sshard, uint64_t bkt_i,
            hashsyn_t hashsyn, uint64_t num_bkts);




/*
 * Other functions.
 */
uint64_t blk_to_lba(uint64_t blk);
uint64_t lba_to_blk(uint64_t lba);
uint64_t blk_to_use(mcd_osd_shard_t *shard, uint64_t blk);
uint64_t lba_to_use(mcd_osd_shard_t *shard, uint64_t lba);

void set_cntr_sizes(SDF_action_init_t *pai, shard_t *sshard);
int  evict_object(mcd_osd_shard_t *shard, ZS_cguid_t cguid, uint64_t nblks);

void
delete_all_objects(SDF_action_init_t *pai, shard_t *sshard, ZS_cguid_t cguid);

uint64_t
hashck(const unsigned char *key, uint64_t key_len,
       uint64_t level, cntr_id_t cntr_id);

 
#endif /* _RECOVERY_H */
