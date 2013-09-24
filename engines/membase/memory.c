/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "config.h"
#include <memcached/engine.h>
#include <memcached/types.h>
#include "membase.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

void *membase_block_allocate(struct membase_engine* engine, bool clear)
{
    void *ret;
    cb_mutex_enter(&engine->memory.mutex);
    ret = engine->memory.freelist;
    if (ret) {
        engine->memory.freelist = engine->memory.freelist->next;
    }
    cb_mutex_exit(&engine->memory.mutex);

    if (clear && ret) {
        memset(ret, 0, engine->config.slab_size);
    }

    return ret;
}

void membase_block_free(struct membase_engine* engine, void *block)
{
    membase_iov_t *blk = block;
    cb_mutex_enter(&engine->memory.mutex);
    blk->next = engine->memory.freelist;
    engine->memory.freelist = blk;
    cb_mutex_exit(&engine->memory.mutex);
}

int membase_memory_init(struct membase_engine* engine)
{
    membase_iov_t *ptr;
    size_t total_slabs = engine->config.max_memory / engine->config.slab_size;
    size_t ii;

    cb_mutex_initialize(&engine->memory.mutex);
    engine->memory.freelist = malloc(total_slabs * engine->config.slab_size);
    if (engine->memory.freelist == NULL) {
        return -1;
    }

    /* Link all of the blocks */
    engine->memory.root = ptr = engine->memory.freelist;
    --total_slabs;
    for (ii = 0; ii < total_slabs; ++ii, ++ptr) {
        ptr->next = ptr + 1;
    }
    ptr->next = NULL;

    return 0;
}
