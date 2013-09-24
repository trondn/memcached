/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#pragma once

#include <platform/platform.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEMBASE_IFLAG_TMP_OBJ  1
#define MEMBASE_IFLAG_SLAB_OBJ 2
#define MEMBASE_IFLAG_DELETED  4
#define MEMBASE_IFLAG_LINKED   8

#define MEMBASE_IOV_PARTIAL 1
#define MEMBASE_IOV_BLOCK 2
typedef struct membase_iov_st {
    struct membase_iov_st *next;
    uint16_t num_iov;
    uint8_t magic;
} membase_iov_t;

typedef struct membase_item_st {
    struct membase_item_st *prev;
    struct membase_item_st *next;
    struct membase_item_st *upr;
    membase_iov_t *data;
    uint64_t cas;
    uint32_t revno;
    uint32_t flags;
    uint32_t nbytes;
    uint32_t hash;
    rel_time_t exptime;
    uint16_t iflags; /* Internal flags */
    uint16_t nkey;
    uint16_t niov;
    uint8_t refcount;
    uint8_t datatype;
} membase_item_t;



/*
 * The item layout
 *    +-------------------------------+
 *    | stuct membase_item_st         |
 *    +-------------------------------+
 *    | key data                      |
 *    +-------------------------------+
 *    | payload                       |
 *    |                               |
 *    |                               |
 *    +-------------------------------+
 *
 *  If the data body is small enough to fit at the end of the blcok,
 *  the payload will contain the data itself. If the data is too big
 *  to fit in the datay, the rest of the payload is an membase_iov_t
 *  structure.
 *
 *
 *    +-------------------------------+
 *    | membase_iov_t                 |
 *    |                               |
 *    |                               |
 *    +-------------------------------+
 *
 *    +-------------------------------+
 *    | data                          |
 *    |                               |
 *    |                               |
 *    |                               |
 *    |                               |
 *    +-------------------------------+
 */

struct membase_storage_st {
    cb_mutex_t mutex;
    /* @todo I should use a hashtable for faster seach */
    membase_item_t *items;

    membase_item_t *upr_tail;
    membase_item_t *upr_head;
    size_t membase_size;
    vbucket_state_t state;
};

struct membase_config {
    size_t max_item_size;
    size_t slab_size;
    size_t max_memory;
    size_t max_nkey;
    char *uuid;
};

#define MEMBASE_NUM_VBUCKETS 1024
#define VBUCKET_GUARD(vbid) assert(vbid < 1024)

struct membase_memory {
    cb_mutex_t mutex;
    void *root;
    membase_iov_t *freelist;
};

struct membase_engine {
    ENGINE_HANDLE_V1 engine;
    SERVER_HANDLE_V1 server;

    bool initialized;

    struct membase_config config;
    struct membase_memory memory;

    void *upr_engine;

    struct membase_storage_st vbuckets[MEMBASE_NUM_VBUCKETS];
    union {
        engine_info engine_info;
        char buffer[1024/* sizeof(engine_info) + */
                    /* (sizeof(feature_info) * LAST_REGISTERED_ENGINE_FEATURE) */];
    } info;
};

membase_item_t *membase_tmp_obj_to_slab_obj(struct membase_engine* engine,
                                            struct membase_storage_st *storage,
                                            membase_item_t *tmpobj);

void membase_nuke_object(struct membase_engine* engine,
                         struct membase_storage_st *storage,
                         membase_item_t *it);


int membase_memory_init(struct membase_engine* engine);
void *membase_block_allocate(struct membase_engine* engine, bool clear);
void membase_block_free(struct membase_engine* engine, void *block);


MEMCACHED_PUBLIC_API
ENGINE_ERROR_CODE create_instance(uint64_t interface,
                                  GET_SERVER_API get_server_api,
                                  ENGINE_HANDLE **handle);

#ifdef __cplusplus
}
#endif
