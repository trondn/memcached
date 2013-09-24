/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "config.h"
#include <memcached/engine.h>
#include <memcached/types.h>
#include "membase.h"
#include "uprengine.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static struct membase_engine* get_handle(ENGINE_HANDLE* handle)
{
    return (struct membase_engine*)handle;
}

static void do_unlink(struct membase_engine* engine,
                      struct membase_storage_st *storage,
                      membase_item_t *it)
{
    assert(it);
    if (it->prev) {
        it->prev->next = it->next;
    }

    if (it->next) {
        it->next->prev = it->prev;
    }

    if (storage->items == it) {
        if (it->prev) {
            storage->items = it->prev;
        } else {
            storage->items = it->next;
        }
    }
    it->next = it->prev = NULL;
}

static void do_link(struct membase_engine* engine,
                    struct membase_storage_st *storage,
                    membase_item_t *it)
{
    if (storage->items == NULL) {
        storage->items = it;
    } else {
        storage->items->prev = it;
        it->next = storage->items;
        storage->items = it;
    }
}

static membase_item_t *do_get(struct membase_engine* engine,
                              struct membase_storage_st *storage,
                              const void *key,
                              uint16_t nkey)
{
    membase_item_t *it = storage->items;
    uint32_t hash = engine->server.core->hash(key, nkey, 0);

    while (it != NULL) {
        if (hash == it->hash && it->nkey == nkey && memcmp(it + 1, key, nkey) == 0) {
            break;
        }
        it = it->next;
    }

    /* bump it to the head of the LRU */
    if (it) {
        if (it->iflags & MEMBASE_IFLAG_DELETED) {
            return NULL;
        }

        do_unlink(engine, storage, it);
        do_link(engine, storage, it);
    }

    return it;
}

static ENGINE_ERROR_CODE do_delete(struct membase_engine* engine,
                                   struct membase_storage_st *storage,
                                   const void *key,
                                   uint16_t nkey,
                                   uint64_t* cas)
{
    membase_item_t *old;
    membase_item_t *it;

    old = do_get(engine, storage, key, nkey);
    if (old == NULL) {
        return ENGINE_KEY_ENOENT;
    }

    if (*cas && old->cas != *cas) {
        return ENGINE_KEY_EEXISTS;
    }
    it = membase_block_allocate(engine, 0);
    if (it == NULL) {
        return ENGINE_ENOMEM;
    }
    memcpy(it, old, engine->config.slab_size);

    /* clear the fields */
    it->prev = it->next = it->upr = NULL;
    it->nbytes = 0;
    ++it->cas;
    ++it->revno;
    it->refcount = 0;
    it->iflags = MEMBASE_IFLAG_DELETED;
    do_unlink(engine, storage, old);
    do_link(engine, storage, it);

    /* Insterted OK, add it to the MEMBASE tail */
    storage->membase_size++;
    if (storage->upr_head == NULL) {
        storage->upr_head = storage->upr_tail = it;
    } else {
        storage->upr_head->upr = it;
        storage->upr_head = it;
    }

    return ENGINE_SUCCESS;
}

static ENGINE_ERROR_CODE do_store(struct membase_engine* engine,
                                  struct membase_storage_st *storage,
                                  const void *cookie,
                                  membase_item_t* it,
                                  uint64_t *cas,
                                  ENGINE_STORE_OPERATION operation,
                                  uint16_t vbucket)
{
    ENGINE_ERROR_CODE ret = ENGINE_FAILED;
    membase_item_t *old;

    /* SANITYCHECK */
#ifndef NDEBUG
    assert(it->prev == NULL);
    assert(it->next == NULL);
    assert(it->upr == NULL);
#endif

    old = do_get(engine, storage, it + 1, it->nkey);
    switch (operation) {
    case OPERATION_ADD:
        if (old != NULL) {
            return ENGINE_NOT_STORED;
        }
        break;
    case OPERATION_SET:
        /* Set don't care if it's there or not */
        break;

    case OPERATION_REPLACE:
    case OPERATION_APPEND:
    case OPERATION_PREPEND:
    case OPERATION_CAS:
        /* Require that it is there */
        if (old == NULL) {
            return ENGINE_NOT_STORED;
        }
        break;
    default:
        return ENGINE_EINVAL;
    }

    if (it->cas != 0 && (old == NULL || it->cas != old->cas)) {
        return ENGINE_KEY_EEXISTS;
    }

    if ((it->iflags & MEMBASE_IFLAG_TMP_OBJ) == MEMBASE_IFLAG_TMP_OBJ) {
        if ((it = membase_tmp_obj_to_slab_obj(engine, storage, it)) == NULL) {
            return ENGINE_ENOMEM;
        }
    }

    /* Ok, we're going to start updating shit */
    if (old != NULL) {
        if (operation == OPERATION_APPEND) {
            return ENGINE_ENOTSUP;
        } else if (operation == OPERATION_PREPEND) {
            return ENGINE_ENOTSUP;
        }
        it->revno = old->revno++;
        it->cas = old->cas + 1;
    } else {
        it->cas++;
    }
    *cas = it->cas;

    do_link(engine, storage, it);

    /* unlink the old item */
    if (old) {
        do_unlink(engine, storage, old);
    }

    if (ret == ENGINE_SUCCESS) {
        /* Insterted OK, add it to the MEMBASE tail */
        storage->membase_size++;
        if (storage->upr_head == NULL) {
            storage->upr_head = storage->upr_tail = it;
        } else {
            storage->upr_head->upr = it;
            storage->upr_head = it;
        }
    }

    return ENGINE_SUCCESS;
}

/***************************************************************************
 **                                                                       **
 **                         Engine interface                              **
 **                                                                       **
 ***************************************************************************/
static const engine_info* get_info(ENGINE_HANDLE* handle)
{
    return &get_handle(handle)->info.engine_info;
}

static void destroy(ENGINE_HANDLE* handle, const bool force)
{
    struct membase_engine* engine = get_handle(handle);
    (void)force;

    if (engine->initialized) {
        int ii;
        for (ii = 0; ii < MEMBASE_NUM_VBUCKETS; ++ii) {
            cb_mutex_destroy(&engine->vbuckets[ii].mutex);
        }
        engine->initialized = false;
    }
    free(engine->memory.root);
    free(engine);
}

static void handle_disconnect(const void *cookie,
                              ENGINE_EVENT_TYPE type,
                              const void *event_data,
                              const void *cb_data)
{
    struct membase_engine *engine = (struct membase_engine*)cb_data;
    assert(engine != NULL);
    /* cb_mutex_enter(&engine->tap_connections.lock); */
    /* for (ii = 0; ii < engine->tap_connections.size; ++ii) { */
    /*     if (engine->tap_connections.clients[ii] == cookie) { */
    /*         free(engine->server.cookie->get_engine_specific(cookie)); */
    /*         break; */
    /*     } */
    /* } */
    /* cb_mutex_exit(&engine->tap_connections.lock); */
}


static ENGINE_ERROR_CODE initialize(ENGINE_HANDLE* handle,
                                    const char* config_str)
{
    struct membase_engine* engine = get_handle(handle);

    ENGINE_ERROR_CODE ret = ENGINE_SUCCESS;
    engine->config.slab_size = 512;

    if (config_str != NULL) {
#define CONFIG_SIZE 5
        struct config_item items[CONFIG_SIZE];
        int ii = 0;
        memset(&items, 0, sizeof(items));

        items[ii].key = "config_file";
        items[ii].datatype = DT_CONFIGFILE;
        ++ii;

        items[ii].key = "uuid";
        items[ii].datatype = DT_STRING;
        items[ii].value.dt_string = &engine->config.uuid;
        ++ii;

        items[ii].key = "slab_size";
        items[ii].datatype = DT_SIZE;
        items[ii].value.dt_size = &engine->config.slab_size;
        ++ii;

        items[ii].key = "max_memory";
        items[ii].datatype = DT_SIZE;
        items[ii].value.dt_size = &engine->config.max_memory;
        ++ii;

        items[ii].key = NULL;
        ++ii;
        assert(ii == CONFIG_SIZE);
        ret = engine->server.core->parse_config(config_str, items, stderr);
    }

    if (ret != ENGINE_SUCCESS) {
        return ret;
    }

    engine->config.max_item_size = engine->config.slab_size * (IOV_MAX - 1);
    engine->config.max_nkey = engine->config.slab_size - (sizeof(membase_item_t));

    if (membase_memory_init(engine) == -1) {
        return ENGINE_ENOMEM;
    }

    engine->server.callback->register_callback(handle, ON_DISCONNECT,
                                               handle_disconnect,
                                               handle);
    return ENGINE_SUCCESS;
}

static ENGINE_ERROR_CODE item_allocate(ENGINE_HANDLE* handle,
                                       const void* cookie,
                                       item **item,
                                       const void* key,
                                       const size_t nkey,
                                       const size_t nbytes,
                                       const int flags,
                                       const rel_time_t exptime)
{
    struct membase_engine* engine = get_handle(handle);
    membase_item_t *it;
    size_t ntotal = sizeof(membase_item_t) + nkey + nbytes;
    size_t slabs = ntotal / engine->config.slab_size;
    if (ntotal % engine->config.slab_size) {
        ++slabs;
    }

    if (nkey > engine->config.max_nkey) {
        return ENGINE_E2BIG;
    }

    if (ntotal > engine->config.max_item_size) {
        return ENGINE_E2BIG;
    }

    it = calloc(slabs, engine->config.slab_size);
    if (it == NULL) {
        return ENGINE_ENOMEM;
    }

    it->iflags = MEMBASE_IFLAG_TMP_OBJ;
    it->refcount = 1;
    it->nbytes = nbytes;
    it->nkey = nkey;
    it->exptime = exptime;
    it->flags = flags;
    it->niov = 0;
    memcpy(it + 1, key, nkey);

    *item = it;
    return ENGINE_SUCCESS;
}

static void item_release(ENGINE_HANDLE* handle,
                         const void *cookie,
                         item* item)
{
    /* struct membase_engine* engine = get_handle(handle); */
    membase_item_t *it = item;
    it->refcount--;
    if (it->refcount == 0) {
        /* @todo check if it is linked and if not free */
        if ((it->iflags & MEMBASE_IFLAG_TMP_OBJ) == MEMBASE_IFLAG_TMP_OBJ) {
            free(it);
        }
    }
}

static void reset_stats(ENGINE_HANDLE* handle, const void *cookie) {
    (void)handle;
    (void)cookie;
    /* EMPTY */
}

static ENGINE_ERROR_CODE get_stats(ENGINE_HANDLE* handle,
                                   const void* cookie,
                                   const char* stat_key,
                                   int nkey,
                                   ADD_STAT add_stat)
{
    /* struct default_engine* engine = get_handle(handle); */
    ENGINE_ERROR_CODE ret = ENGINE_SUCCESS;

    return ret;
}



static void item_set_cas(ENGINE_HANDLE *handle, const void *cookie,
                         item* item, uint64_t val)
{
    membase_item_t* it = item;
    it->cas = val;
}

static ENGINE_ERROR_CODE flush(ENGINE_HANDLE* handle,
                               const void* cookie, time_t when) {
    /* @todo implement me */
    if (when != 0) {
        return ENGINE_ENOTSUP;
    }

    /* Run through all of the vbuckets and clear the root pointer */



    return ENGINE_SUCCESS;
}

static ENGINE_ERROR_CODE item_delete(ENGINE_HANDLE* handle,
                                     const void* cookie,
                                     const void* key,
                                     const size_t nkey,
                                     uint64_t* cas,
                                     uint16_t vbucket)
{
    struct membase_engine* engine = get_handle(handle);
    struct membase_storage_st *storage;
    ENGINE_ERROR_CODE ret = ENGINE_NOT_MY_VBUCKET;

    VBUCKET_GUARD(vbucket);
    storage = engine->vbuckets + vbucket;

    cb_mutex_enter(&storage->mutex);
    if (storage->state == vbucket_state_active) {
        ret = do_delete(engine, storage, key, nkey, cas);
    }
    cb_mutex_exit(&storage->mutex);

    return ret;
}

static ENGINE_ERROR_CODE get(ENGINE_HANDLE* handle,
                             const void* cookie,
                             item** item,
                             const void* key,
                             const int nkey,
                             uint16_t vbucket)
{
    struct membase_engine* engine = get_handle(handle);
    struct membase_storage_st *storage;
    ENGINE_ERROR_CODE ret = ENGINE_NOT_MY_VBUCKET;

    VBUCKET_GUARD(vbucket);
    storage = engine->vbuckets + vbucket;

    cb_mutex_enter(&storage->mutex);
    if (storage->state == vbucket_state_active) {
        membase_item_t *it = do_get(engine, storage, key, nkey);
        if (it) {
            it->refcount++;
            *item = it;
            ret = ENGINE_SUCCESS;
        } else {
            ret = ENGINE_KEY_ENOENT;
        }
    }
    cb_mutex_exit(&storage->mutex);

    return ret;
}

static ENGINE_ERROR_CODE store(ENGINE_HANDLE* handle,
                               const void *cookie,
                               item* it,
                               uint64_t *cas,
                               ENGINE_STORE_OPERATION operation,
                               uint16_t vbucket)
{
    struct membase_engine* engine = get_handle(handle);
    struct membase_storage_st *storage;
    ENGINE_ERROR_CODE ret = ENGINE_NOT_MY_VBUCKET;

    VBUCKET_GUARD(vbucket);
    storage = engine->vbuckets + vbucket;

    cb_mutex_enter(&storage->mutex);
    if (storage->state == vbucket_state_active) {
        ret = do_store(engine, storage, cookie, it, cas, operation, vbucket);
    }
    cb_mutex_exit(&storage->mutex);

    return ret;
}


static ENGINE_ERROR_CODE unknown_command(ENGINE_HANDLE* handle,
                                         const void* cookie,
                                         protocol_binary_request_header *request,
                                         ADD_RESPONSE response)
{
    return ENGINE_FAILED;
}


static bool get_item_info(ENGINE_HANDLE *handle, const void *cookie,
                          const item* item, item_info *item_info)
{
    const membase_item_t* it = item;
    if (item_info->nvalue < 1) {
        return false;
    }
    item_info->cas = it->cas;
    item_info->exptime = it->exptime;
    item_info->nbytes = it->nbytes;
    item_info->flags = it->flags;
    item_info->clsid = 0;
    item_info->nkey = it->nkey;
    item_info->key = it + 1;

    if (it->data == NULL) {
        item_info->value[0].iov_base = ((char*)(it+1)) + it->nkey;
        item_info->value[0].iov_len = it->nbytes;
        item_info->nvalue = 1;
    } else {
        int ii = 0, jj = 0;
        membase_iov_t *iov = it->data;
        struct iovec *vec = (void*)(iov + 1);
        uint16_t niov = it->niov;

        while (niov > 0) {
            --niov;
            item_info->value[ii].iov_len = vec[jj].iov_len;
            item_info->value[ii++].iov_base = vec[jj++].iov_base;

            if (jj == iov->num_iov) {
                jj = 0;
                iov = iov->next;
                vec = (void*)(iov + 1);
            }
            if (ii == item_info->nvalue) {
                return false;
            }
        }
        item_info->nvalue = ii;
    }
    return true;
}


static ENGINE_ERROR_CODE arithmetic(ENGINE_HANDLE* handle,
                                    const void* cookie,
                                    const void* key,
                                    const int nkey,
                                    const bool increment,
                                    const bool create,
                                    const uint64_t delta,
                                    const uint64_t initial,
                                    const rel_time_t exptime,
                                    uint64_t *cas,
                                    uint64_t *result,
                                    uint16_t vbucket)
{
    return ENGINE_FAILED;
}


static ENGINE_ERROR_CODE upr_open(ENGINE_HANDLE* handle,
                                  const void* cookie,
                                  uint32_t opaque,
                                  uint32_t seqno,
                                  uint32_t flags,
                                  void *name,
                                  uint16_t nname)
{
    struct membase_engine* engine = get_handle(handle);
    return upr_handle_open(engine, cookie, opaque, seqno, flags, name, nname);
}

static ENGINE_ERROR_CODE upr_add_stream(ENGINE_HANDLE* handle,
                                        const void* cookie,
                                        uint32_t opaque,
                                        uint16_t vbucket,
                                        uint32_t flags,
                                        ENGINE_ERROR_CODE (*stream_req)(const void *cookie,
                                                                        uint32_t opaque,
                                                                        uint16_t vbucket,
                                                                        uint32_t flags,
                                                                        uint64_t start_seqno,
                                                                        uint64_t end_seqno,
                                                                        uint64_t vbucket_uuid,
                                                                        uint64_t high_seqno))
{
    struct membase_engine* engine = get_handle(handle);
    return upr_handle_add_stream(engine, cookie, opaque, vbucket, flags, stream_req);
}

MEMCACHED_PUBLIC_API
ENGINE_ERROR_CODE create_instance(uint64_t interface,
                                  GET_SERVER_API get_server_api,
                                  ENGINE_HANDLE **handle) {
    SERVER_HANDLE_V1 *api = get_server_api();
    struct membase_engine *engine;
    int ii;

    if (interface != 1 || api == NULL) {
        return ENGINE_ENOTSUP;
    }

    if ((engine = calloc(1, sizeof(*engine))) == NULL) {
        return ENGINE_ENOMEM;
    }

    if ((engine->upr_engine = create_upr_engine(engine)) == NULL){
        free(engine);
        return ENGINE_ENOMEM;
    }

    for (ii = 0; ii < MEMBASE_NUM_VBUCKETS; ++ii) {
        cb_mutex_initialize(&engine->vbuckets[ii].mutex);
        engine->vbuckets[ii].state = vbucket_state_active;
    }

    engine->info.engine_info.description = "MEMbase Engine v0.1";
    ii = 0;
    engine->info.engine_info.features[ii++].feature = ENGINE_FEATURE_CAS;
    /* engine->info.engine_info.features[ii++].feature = ENGINE_FEATURE_LRU; */
    engine->info.engine_info.features[ii++].feature = ENGINE_FEATURE_VBUCKET;
    engine->info.engine_info.num_features = ii;

    engine->engine.interface.interface = 1;
    engine->server = *api;

    engine->initialized = true;

    engine->engine.allocate = item_allocate;
    engine->engine.arithmetic = arithmetic;
    engine->engine.destroy = destroy;
    engine->engine.flush = flush;
    engine->engine.get = get;
    engine->engine.get_info = get_info;
    engine->engine.get_item_info = get_item_info;
    engine->engine.get_stats = get_stats;
    engine->engine.initialize = initialize;
    engine->engine.item_set_cas = item_set_cas;
    engine->engine.release = item_release;
    engine->engine.remove = item_delete;
    engine->engine.reset_stats = reset_stats;
    engine->engine.store = store;
    engine->engine.unknown_command = unknown_command;

    engine->engine.upr.open = upr_open;
    engine->engine.upr.add_stream = upr_add_stream;


#if 0
    engine->engine.tap_notify = default_tap_notify;
    engine->engine.get_tap_iterator = default_get_tap_iterator;
    engine->engine.upr.step = upr_step;
    engine->engine.upr.get_failover_log = upr_get_failover_log;
    engine->engine.upr.stream_req = upr_stream_req;
    engine->engine.upr.stream_start = upr_stream_start;
    engine->engine.upr.stream_end = upr_stream_end;
    engine->engine.upr.snapshot_start = upr_snapshot_start;
    engine->engine.upr.snapshot_end = upr_snapshot_end;
    engine->engine.upr.mutation = upr_mutation;
    engine->engine.upr.deletion = upr_deletion;
    engine->engine.upr.expiration = upr_expiration;
    engine->engine.upr.flush = upr_flush;
    engine->engine.upr.set_vbucket_state = upr_set_vbucket_state;
#endif

    *handle = (ENGINE_HANDLE*)&engine->engine;
    return ENGINE_SUCCESS;
}
