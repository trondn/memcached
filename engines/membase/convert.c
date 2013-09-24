/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "config.h"
#include <memcached/engine.h>
#include <memcached/types.h>
#include "membase.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void *align_ptr(void *base, int offset)
{
    char *ptr = (char*)base;
    ptr += offset;

    ptr += ((intptr_t)ptr) % 8;
    return ptr;
}

void membase_nuke_object(struct membase_engine* engine,
                         struct membase_storage_st *storage,
                         membase_item_t *it)
{
    if (it->iflags & MEMBASE_IFLAG_TMP_OBJ) {
        free(it);
    } else {
        membase_iov_t *ptr = it->data;
        while (ptr != NULL) {
            struct iovec *vec = (void*)(ptr + 1);
            uint16_t ii;
            for (ii = 0; ii < ptr->num_iov; ++ii) {
                membase_block_free(engine, vec[ii].iov_base);
            }
            ptr = ptr->next;
            if (it->data->magic == MEMBASE_IOV_BLOCK) {
                membase_block_free(engine, it->data);
            }
            it->data = ptr;
        }
        membase_block_free(engine, it);
    }
}

#define MINIMUM(a, b) (a < b) ? a : b

membase_item_t *membase_tmp_obj_to_slab_obj(struct membase_engine* engine,
                                            struct membase_storage_st *storage,
                                            membase_item_t *tmpobj)
{
    membase_item_t *ret;
    membase_iov_t *iov;
    size_t size;
    uint16_t ii;
    struct iovec *vec;
    char *ptr;

    assert(tmpobj->refcount == 1);
    assert((tmpobj->iflags & MEMBASE_IFLAG_TMP_OBJ) == MEMBASE_IFLAG_TMP_OBJ);

    ret = membase_block_allocate(engine, 1);
    if (ret == NULL) {
        return NULL;
    }

    memcpy(ret, tmpobj, engine->config.slab_size);
    ret->iflags = MEMBASE_IFLAG_SLAB_OBJ;

    /* is there room for an iov in the root block? */
    size = sizeof(*ret) + ret->nkey + sizeof(membase_iov_t) + sizeof(struct iovec);
    if (size < engine->config.slab_size) {
        size = engine->config.slab_size - size;
        iov = align_ptr(ret + 1, ret->nkey);
        iov->next = NULL;
        iov->num_iov = (uint16_t)(1 + (size / sizeof(struct iovec)));
        /* Calculate the size of it! */
    } else {
        iov = membase_block_allocate(engine, 1);
        if (iov == NULL) {
            membase_nuke_object(engine, storage, ret);
            return NULL;
        }
        /* update its size */
        size = engine->config.slab_size - sizeof(membase_iov_t);
        iov->num_iov = (uint16_t)(size / sizeof(struct iovec));
        iov->magic = MEMBASE_IOV_BLOCK;
    }
    ret->data = iov;
    size = ret->nbytes;
    ii = 0;
    vec = (void*)(iov + 1);
    ptr = (char*)(tmpobj + 1) + ret->nkey;

    while (size > 0) {
        size_t chunk = MINIMUM(size, engine->config.slab_size);
        vec[ii].iov_len = chunk;
        vec[ii].iov_base = membase_block_allocate(engine, 0);

        ++ret->niov;
        if (vec[ii].iov_base == NULL) {
            membase_nuke_object(engine, storage, ret);
            return NULL;
        }
        memcpy(vec[ii].iov_base, ptr, chunk);
        ptr += chunk;
        size -= chunk;
        ++ii;
        if (ii == iov->num_iov) {
            size_t blksz;
            iov->next = membase_block_allocate(engine, 1);
            if (iov->next == NULL) {
                membase_nuke_object(engine, storage, ret);
                return NULL;
            }
            iov = iov->next;
            ii = 0;
            vec = (void*)(iov + 1);
            /* update its size */
            blksz = engine->config.slab_size - sizeof(membase_iov_t);
            iov->num_iov = (uint16_t)(blksz / sizeof(struct iovec));
            iov->magic = MEMBASE_IOV_BLOCK;
        }
    }
    return ret;
}
