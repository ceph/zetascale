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
 * ZS Async command handling
 *
 * Author: Manavalan Krishnan
 *
 * Copyright (c) 2013 SanDisk Corporation.  All rights reserved.
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 
#include "zs.h"
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "sdf.h"
#include "sdf_internal.h"
#include "zs.h"
#include "utils/properties.h"
#include "protocol/protocol_utils.h"
#include "protocol/protocol_common.h"
#include "protocol/action/action_thread.h"
#include "protocol/action/async_puts.h"
#include "protocol/home/home_flash.h"
#include "protocol/action/async_puts.h"
#include "protocol/replication/copy_replicator.h"
#include "protocol/replication/replicator.h"
#include "protocol/replication/replicator_adapter.h"
#include "ssd/fifo/mcd_ipf.h"
#include "ssd/fifo/mcd_osd.h"
#include "ssd/fifo/mcd_bak.h"
#include "shared/init_sdf.h"
#include "shared/private.h"
#include "shared/open_container_mgr.h"
#include "shared/container_meta.h"
#include "shared/name_service.h"
#include "shared/shard_compute.h"
#include "shared/internal_blk_obj_api.h"
#include "agent/agent_common.h"
#include "agent/agent_helper.h"
#include "fdf_internal.h"

#define LOG_ID PLAT_LOG_ID_INITIAL
#define LOG_CAT PLAT_LOG_CAT_SDF_NAMING
#define LOG_DBG PLAT_LOG_LEVEL_DEBUG
#define LOG_TRACE PLAT_LOG_LEVEL_TRACE
#define LOG_INFO PLAT_LOG_LEVEL_INFO
#define LOG_ERR PLAT_LOG_LEVEL_ERROR
#define LOG_WARN PLAT_LOG_LEVEL_WARN
#define LOG_FATAL PLAT_LOG_LEVEL_FATAL


fthMbox_t async_cmds_hdr_mbox;
static uint32_t num_deletes_pend;
static uint32_t num_deletes_prog;
static uint32_t num_threads;

typedef enum {
    ZS_ASYNC_CMD_DELETE_CTNR,    
    ZS_ASYNC_CMD_EXIT
}ZS_ASYNC_CMD;

typedef struct {
    ZS_ASYNC_CMD cmd;
    ZS_cguid_t cguid;
    uint32_t    size_free;
} async_cmd_req_t;

/*
 * This CV serves the purpose of blocking wait for a 
 * thread awaiting completion of pending delete operations.
 */
pthread_mutex_t mutex =  PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv     =  PTHREAD_COND_INITIALIZER;

/**
 * @brief If all deletes have completed, signal the waiter
 *
 * return None
 */
void
check_if_zero_num_deletes_pend()
{
	pthread_mutex_lock(&mutex);

	if (0 == num_deletes_pend) {
		pthread_cond_signal(&cv);

        plat_log_msg(160121,
		             LOG_CAT, LOG_TRACE,
					 "Signal completion of pending deletes\n");
	}	
	pthread_mutex_unlock(&mutex);
}

 
/**
 * @brief If deletes are pending, wait for completion
 *
 * return None
 *
 */
void
wait_for_container_del()
{
	pthread_mutex_lock(&mutex);

	if (0 != num_deletes_pend) {
        plat_log_msg(160122,
		             LOG_CAT, LOG_TRACE,
					 "Waiting for completion of pending deletes\n");
		pthread_cond_wait(&cv, &mutex);
	}
	pthread_mutex_unlock(&mutex);
}


static void async_handler_thread(uint64_t arg){
    int rc;
    struct ZS_state *zs_state;
    async_cmd_req_t *req;
    struct ZS_thread_state *thd_state;

    zs_state = (struct ZS_state *)arg;
    /*Create thread state */
    rc = ZSInitPerThreadState(zs_state,&thd_state);
    if( rc != ZS_SUCCESS ) {
        plat_log_msg(160073,LOG_CAT, LOG_ERR,
                                       "Unable to initialize thread state\n");
        return;
    }
    plat_log_msg(70122, LOG_CAT, LOG_DBG,
                 "Async thread started...");
    while (1) {
        req = (async_cmd_req_t *)fthMboxWait(&async_cmds_hdr_mbox);
        if( req->cmd == ZS_ASYNC_CMD_DELETE_CTNR) {
            /* Delete the containers */
            atomic_add(num_deletes_prog,1); 
            zs_delete_container_async_end(thd_state,req->cguid);
            /*Delete completed */
            atomic_sub(num_deletes_prog,1); 
            atomic_sub(num_deletes_pend,1);

			/*
			 * Signal any waiter that is awaiting on pending delete
			 * operations.
			 */			
			check_if_zero_num_deletes_pend();
        }
        else if( req->cmd == ZS_ASYNC_CMD_EXIT ) {
            plat_free(req);
            break; 
        } 
        else {
            plat_log_msg(160070,LOG_CAT, LOG_WARN,
                                      "Invalid command(%d) received",req->cmd);
        }
        plat_free(req);
    }

}

void get_async_delete_stats( uint32_t *num_deletes,uint32_t *num_prog) {
    *num_deletes = num_deletes_pend;
    *num_prog    = num_deletes_prog;
}

ZS_status_t async_command_delete_container(ZS_cguid_t cguid) {
    async_cmd_req_t *req;    

    req = (async_cmd_req_t *)plat_alloc(sizeof( async_cmd_req_t));
    if ( req == NULL ) {
        plat_log_msg(160071,LOG_CAT, LOG_ERR,
                                         "Memory allocation failed");
        return ZS_FAILURE;
    } 
    req->cmd = ZS_ASYNC_CMD_DELETE_CTNR;
    req->cguid = cguid;
    atomic_add(num_deletes_pend,1);
    fthMboxPost(&async_cmds_hdr_mbox,(uint64_t)req);
    return ZS_SUCCESS; 
}

struct ZS_state *my_zs_state = NULL;

ZS_status_t async_command_exit() {
    async_cmd_req_t *req;

    req = (async_cmd_req_t *)plat_alloc(sizeof( async_cmd_req_t));
    if ( req == NULL ) {
        plat_log_msg(160071,LOG_CAT, LOG_ERR,
                                         "Memory allocation failed");
        return ZS_FAILURE;
    } 
    req->cmd = ZS_ASYNC_CMD_EXIT;
    fthMboxPost(&async_cmds_hdr_mbox,(uint64_t)req);
    return ZS_SUCCESS;
}

void init_async_cmd_handler(int num_thds, struct ZS_state *zs_state) {
    int i;

	my_zs_state = zs_state;

    if( zs_state == NULL ) {
        plat_log_msg(160072,LOG_CAT, LOG_ERR,"Invalid ZS state");
        return;
    }
    plat_log_msg(80029,LOG_CAT, LOG_DBG,
                        "Staring asynchronous command handler");
    /* Initialize the Mail box */   
    fthMboxInit(&async_cmds_hdr_mbox); 
    num_threads = num_thds;

    /* Start the threads */
    for ( i = 0; i < num_threads; i++) {
        plat_log_msg(70123, LOG_CAT, LOG_DBG,
                     "Initializing the async threads...");
        fthResume( fthSpawn( &async_handler_thread, MCD_FTH_STACKSIZE ),
                                                   (uint64_t)zs_state);
    }
}

void shutdown_async_cmd_handler() {
   int i;
   /*Stop the threads by sending exit command*/
   for ( i = 0; i < num_threads; i++) {
       async_command_exit();
   }
   /* Wait for all threads to finish */
   /*TODO*/
}

















