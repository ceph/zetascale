#ifndef __BTREE_RAW_INTERNAL_H
#define __BTREE_RAW_INTERNAL_H

#include "btree_raw.h"
#include "platform/rwlock.h"

#define BAD_CHILD       0
#define META_LOGICAL_ID 0x8000000000000000L

typedef struct node_vkey {
    uint32_t    keylen;
    uint32_t    keypos;
    uint64_t    ptr;
    uint64_t    seqno;
} node_vkey_t;

#ifdef BIG_NODES
typedef uint32_t keylen_t;
typedef uint32_t keypos_t;
#else
typedef uint16_t keylen_t;
typedef uint16_t keypos_t;
#endif

typedef struct node_vlkey {
    keylen_t    keylen;
    keypos_t    keypos;
    uint64_t    datalen;
    uint64_t    ptr;
    uint64_t    seqno;
} 
__attribute__ ((__packed__))
node_vlkey_t;

typedef struct node_fkey {
    uint64_t    key;
    uint64_t    ptr;
    uint64_t    seqno;
} node_fkey_t;

typedef struct node_flkey {
    uint64_t    key;
    uint64_t    datalen;
    uint64_t    ptr;
    uint64_t    seqno;
} node_flkey_t;

// xxxzzz
// typedef union node_key {
//     node_fkey_t    fkey;
//     node_vkey_t    vkey;
// } node_key_t;

typedef void *node_key_t;

typedef struct key_stuff {
    int       fixed;
    int       leaf;
    uint64_t  ptr;
    uint32_t  nkey;
    uint32_t  offset;
    void     *pkey_struct;
    char     *pkey_val;
    keylen_t  keylen;
    uint64_t  datalen;
    uint32_t  fkeys_per_node;
    uint64_t  seqno;
    uint64_t  syndrome;
} key_stuff_t;

typedef struct btree_raw_node {
    uint32_t      flags;
    uint64_t      logical_id;
    uint64_t      lsn;
    uint32_t      checksum;
    uint32_t      insert_ptr;
    uint32_t      nkeys;
    uint64_t      prev;
    uint64_t      next;
    uint64_t      rightmost;
    node_key_t    keys[0];
} btree_raw_node_t;

typedef struct btree_raw_mem_node btree_raw_mem_node_t;

#define	NODE_DIRTY		0x1
#define NODE_DELETED	0x2

#define	mark_node_dirty(n)		((n)->flag |= (char)NODE_DIRTY)
#define mark_node_clean(n)		((n)->flag &= (char)(~((char)NODE_DIRTY)))
#define is_node_dirty(n)		((n)->flag & NODE_DIRTY)
#define	mark_node_deleted(n)	((n)->flag |= (char)NODE_DELETED)
#define	is_node_deleted(n)		((n)->flag & NODE_DELETED)


//#define DEBUG_STUFF
struct btree_raw_mem_node {
	char     flag;
	uint64_t modified;
#ifdef DEBUG_STUFF
	uint64_t last_dump_modified;
#endif
	plat_rwlock_t lock;
	btree_raw_mem_node_t *next; // dirty list
	btree_raw_node_t *pnode;
};

#define BTREE_RAW_L1CACHE_LIST_MAX 10000

typedef struct btree_raw {
    uint32_t           n_partition;
    uint32_t           n_partitions;
    uint32_t           flags;
    uint32_t           max_key_size;
    uint32_t           min_keys_per_node;
    uint32_t           nodesize;
    uint32_t           nodesize_less_hdr;
    uint32_t           big_object_size;
    uint32_t           fkeys_per_node;
    uint64_t           logical_id_counter;
    uint64_t           rootid;
    uint32_t           n_l1cache_buckets;
    struct PMap       *l1cache;
    read_node_cb_t    *read_node_cb;
    void              *read_node_cb_data;
    write_node_cb_t   *write_node_cb;
    void              *write_node_cb_data;
    flush_node_cb_t   *flush_node_cb;
    void              *flush_node_cb_data;
    freebuf_cb_t      *freebuf_cb;
    void              *freebuf_cb_data;
    create_node_cb_t  *create_node_cb;
    void              *create_node_cb_data;
    delete_node_cb_t  *delete_node_cb;
    void              *delete_node_cb_data;
    log_cb_t          *log_cb;
    void              *log_cb_data;
    msg_cb_t          *msg_cb;
    void              *msg_cb_data;
    cmp_cb_t          *cmp_cb;
    void              *cmp_cb_data;
    trx_cmd_cb_t      *trx_cmd_cb;

    btree_stats_t      stats;

    plat_rwlock_t      lock;

    uint64_t           modified;
	uint64_t           cguid;
uint64_t		next_logical_id;
} btree_raw_t;

typedef struct btree_raw_persist {
    btree_raw_node_t n; // this must be first member
    uint64_t    rootid,
                logical_id_counter,next_logical_id;
} btree_raw_persist_t;

int get_key_stuff(btree_raw_t *bt, btree_raw_node_t *n, uint32_t nkey, key_stuff_t *pks);
btree_status_t get_leaf_data(btree_raw_t *bt, btree_raw_node_t *n, void *pkey, char **data, uint64_t *datalen, uint32_t meta_flags, int ref);
btree_status_t get_leaf_key(btree_raw_t *bt, btree_raw_node_t *n, void *pkey, char **key, uint32_t *keylen, uint32_t meta_flags);
btree_raw_mem_node_t *get_existing_node_low(btree_status_t *ret, btree_raw_t *btree, uint64_t logical_id, int ref);
btree_raw_mem_node_t *get_existing_node(btree_status_t *ret, btree_raw_t *btree, uint64_t logical_id);
int is_leaf(btree_raw_t *btree, btree_raw_node_t *node);

void deref_l1cache_node(btree_raw_t* btree, btree_raw_mem_node_t *node);
btree_raw_mem_node_t* root_get_and_lock(btree_raw_t* btree, int write_lock);

btree_status_t deref_l1cache(btree_raw_t *btree);

node_key_t* btree_raw_find(struct btree_raw *btree, char *key, uint32_t keylen, uint64_t syndrome, btree_metadata_t *meta, btree_raw_mem_node_t** node, int write_lock, int* pathcnt, int match);

void ref_l1cache(btree_raw_t *btree, btree_raw_mem_node_t *n);
void unlock_and_unreference();
#define unlock_and_unreference_all_but_last(b) unlock_and_unreference(b, 1)
#define unlock_and_unreference_all(b) unlock_and_unreference(b, 0)

static inline int key_idx(struct btree_raw *btree, btree_raw_node_t* node, node_key_t* key)
{
	int size = sizeof(node_fkey_t);

	if(node->flags & LEAF_NODE)
		size = sizeof(node_vlkey_t);
	else if(btree->flags & SECONDARY_INDEX)
		size = sizeof(node_vkey_t);

	return ((void*)key - (void*)node->keys) / size;
}

static inline node_key_t* key_offset(struct btree_raw *btree, btree_raw_node_t* node, int nkey)
{
	if(node->flags & LEAF_NODE)
		return (node_key_t*)(((node_vlkey_t *) node->keys) + nkey);
	else if(btree->flags & SECONDARY_INDEX)
		return (node_key_t*)(((node_vkey_t *) node->keys) + nkey);

	return ((node_key_t*)((node_vkey_t *) node->keys) + nkey);
}

inline static
void get_key_val(btree_raw_t *bt, btree_raw_node_t *n, uint32_t nkey, char** key, int* keylen)
{
	if (n->flags & LEAF_NODE) {
		node_vlkey_t* pvlk = ((node_vlkey_t *) n->keys) + nkey;
		*key               = (char *) n + pvlk->keypos;
		*keylen            = pvlk->keylen;
	}
	else if (bt->flags & SECONDARY_INDEX) {
		node_vkey_t* pvk   = ((node_vkey_t *) n->keys) + nkey;
		*key      = (char *) n + pvk->keypos;
		*keylen        = pvk->keylen;
	} else if (bt->flags & SYNDROME_INDEX) {
		node_fkey_t *pfk   = ((node_fkey_t *) n->keys) + nkey;
		*key      = (char *) (pfk->key);
		*keylen        = sizeof(uint64_t);
	} else {
		assert(0);
	}
}

#ifdef DBG_PRINT
#define dbg_print(msg, ...) do { fprintf(stderr, "%x %s:%d " msg, (int)pthread_self(), __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#define dbg_print_key(key, keylen, msg, ...) do { print_key_func(stderr, __FUNCTION__, __LINE__, key, keylen, msg, ##__VA_ARGS__); } while(0)
#else
#define dbg_print(msg, ...)
#define dbg_print_key(key, keylen, msg, ...)
#endif
extern void print_key_func(FILE *f, const char* func, int line, char* key, int keylen, char *msg, ...);
extern __thread uint64_t dbg_referenced;

#ifdef _OPTIMIZE
#undef assert
#define assert(a)
#endif

#endif // __BTREE_RAW_INTERNAL_H
