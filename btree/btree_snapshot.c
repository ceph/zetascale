/************************************************************************
 * 
 *  btree_snapshot.c  Nov 7, 2013   Harihara Kadayam
 * 
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "btree_hash.h"
#include "btree_raw.h"
#include "btree_map.h"
#include "btree_raw_internal.h"
#include "fdf.h"

static int btree_snap_find_meta_index_low(btree_raw_t *bt, uint64_t seqno);

#define offsetof(st, m) ((size_t)(&((st *)0)->m))

uint32_t
btree_snap_get_max_snapshot(btree_snap_meta_t *snap_meta, size_t size)
{
	switch (snap_meta->snap_version) {
		case SNAP_VERSION1 :
					return (size / sizeof(btree_snap_info_v1_t));
		default :	assert(0);
	}
        return 0;
}

void
btree_snap_init_meta(btree_raw_t *bt, size_t size)
{
	btree_snap_meta_t	*snap_meta = bt->snap_meta;

	bzero(snap_meta, size);
	snap_meta->snap_version = SNAP_VERSION;
	snap_meta->total_snapshots = 0;
	snap_meta->max_snapshots = btree_snap_get_max_snapshot(snap_meta, size - offsetof(btree_snap_meta_t, meta));

}

static btree_status_t
btree_snap_create_meta_v1(btree_raw_t *bt, uint64_t seqno)
{
	btree_snap_info_v1_t *info;
	struct timeval     now;

	if (bt->snap_meta->total_snapshots >= bt->snap_meta->max_snapshots) {
		return (BTREE_TOO_MANY_SNAPSHOTS);
	}

	info = &bt->snap_meta->meta.v1_meta.snapshots[bt->snap_meta->total_snapshots];
	info->flag = 0;
	info->seqno = seqno;

	// get current time in GMT
	gettimeofday(&now, NULL);
	info->timestamp = now.tv_sec;

	return (BTREE_SUCCESS);
}

btree_status_t
btree_snap_create_meta(btree_raw_t *bt, uint64_t seqno)
{
	btree_snap_meta_t  *snap_meta = bt->snap_meta;
	btree_status_t     ret;

	pthread_rwlock_wrlock(&bt->snap_lock);
	switch (snap_meta->snap_version) {
	case SNAP_VERSION1: 
		ret = btree_snap_create_meta_v1(bt, seqno);
		if (ret != BTREE_SUCCESS) {
			pthread_rwlock_unlock(&bt->snap_lock);
			return (ret);
		}
		break;

	default:
		assert(0);
	}
	bt->snap_meta->total_snapshots++;
	__sync_add_and_fetch(&(bt->stats.stat[BTSTAT_NUM_SNAPS]), 1);
	pthread_rwlock_unlock(&bt->snap_lock);

	return (flushpersistent(bt));
}

static btree_status_t
btree_snap_delete_meta_v1(btree_raw_t *bt, uint64_t seqno)
{
	btree_snap_info_v1_t *info;
	int index;

	index = btree_snap_find_meta_index_low(bt, seqno);
	if (index == -1) {
		return BTREE_FAILURE;
	}

	info = &bt->snap_meta->meta.v1_meta.snapshots[index];
	if (info->seqno != seqno) {
		/* while seqno is within range of snapshots,
		 * for deletes, exact seqnos have to be given */
		return BTREE_FAILURE;
	}

#if 0
	info->flag = SNAP_DELETED;
#endif
	//Need to wake up scavenger which should clean up this entry too...
	memmove(&bt->snap_meta->meta.v1_meta.snapshots[index],
	        &bt->snap_meta->meta.v1_meta.snapshots[index+1], 
	        ((bt->snap_meta->total_snapshots - 1 - index) * 
	                    sizeof(btree_snap_info_v1_t)));

	return (BTREE_SUCCESS);
}

btree_status_t
btree_snap_delete_meta(btree_raw_t *bt, uint64_t seqno)
{
	btree_snap_meta_t *smeta = bt->snap_meta;
	btree_status_t ret;

	pthread_rwlock_wrlock(&bt->snap_lock);
	switch (smeta->snap_version) {
	case SNAP_VERSION1:
		ret = btree_snap_delete_meta_v1(bt, seqno);
		if (ret != BTREE_SUCCESS) {
			pthread_rwlock_unlock(&bt->snap_lock);
			return (ret);
		}
		break;

	default:
		assert(0);
	}

	bt->snap_meta->total_snapshots--;
	__sync_sub_and_fetch(&(bt->stats.stat[BTSTAT_NUM_SNAPS]), 1);
	pthread_rwlock_unlock(&bt->snap_lock);
	return (flushpersistent(bt));
}

/* 
 * ASSUMPTION: Caller will take the lock */
static int
btree_snap_find_meta_index_low(btree_raw_t *bt, uint64_t seqno)
{
	btree_snap_meta_t *smeta = bt->snap_meta;
	int i_start, i_end, i_check;

	switch (smeta->snap_version) {
	case SNAP_VERSION1 :
		/* If seqno in active container, return right away */
		if ((smeta->total_snapshots == 0) || 
			(seqno > smeta->meta.v1_meta.snapshots[smeta->total_snapshots-1].seqno)) {
			return -1;
		}

		/* Do binary search for snapshot meta */
		i_start = 0;
		i_end = smeta->total_snapshots - 1;

		while (i_end >= i_start) {
			i_check = (i_start + i_end)/2;

			if (seqno <= smeta->meta.v1_meta.snapshots[i_check].seqno) {
				/* Seqno in range between 2 snapshots */
				if ((i_check == 0) ||
					(seqno > smeta->meta.v1_meta.snapshots[i_check-1].seqno)) {
					/* seqno greater than previous and lesser 
					 * than present, we found it */
					return (i_check);
				}
				
				i_end = i_check - 1;
			} else {
				i_start = i_check + 1;
			}
		}
		break;

	default:	
		assert(0);
	}

	return -1;
}

int
btree_snap_find_meta_index(btree_raw_t *bt, uint64_t seqno)
{
	btree_snap_meta_t *smeta = bt->snap_meta;
	int index;

	pthread_rwlock_rdlock(&bt->snap_lock);
	index = btree_snap_find_meta_index_low(bt, seqno);
	pthread_rwlock_unlock(&bt->snap_lock);

	return (index);
}

/* If seqno in active container, return false */
bool
btree_snap_seqno_in_snap(btree_raw_t *bt, uint64_t seqno)
{
	btree_snap_meta_t *smeta = bt->snap_meta;
	int i_start, i_end, i_check;

	switch (smeta->snap_version) {
	case SNAP_VERSION1 :
		if ((smeta->total_snapshots == 0) || 
			(seqno > smeta->meta.v1_meta.snapshots[smeta->total_snapshots-1].seqno)) {
			return false;
		}
		break;

	default:	
		assert(0);
	}

	return true;
}

btree_status_t
btree_snap_get_meta_list(btree_raw_t *bt, uint32_t *n_snapshots,
							 FDF_container_snapshots_t **snap_seqs)
{
	int					i;
	btree_snap_meta_t	*smeta = bt->snap_meta;

	*n_snapshots = smeta->total_snapshots;
	*snap_seqs = (FDF_container_snapshots_t *)malloc(*n_snapshots * sizeof(FDF_container_snapshots_t));
	pthread_rwlock_rdlock(&bt->snap_lock);
	switch (smeta->snap_version) {
		case SNAP_VERSION1 :
			for (i = 0; i < *n_snapshots; i++) {
				if (i == smeta->total_snapshots) {
					break;
				}
				(*snap_seqs)[i].timestamp = smeta->meta.v1_meta.snapshots[i].timestamp;
				(*snap_seqs)[i].seqno = smeta->meta.v1_meta.snapshots[i].seqno;
			}
			break;
		default : assert(0);
	}
	pthread_rwlock_unlock(&bt->snap_lock);

	return (BTREE_SUCCESS);

}