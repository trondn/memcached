/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "memcached.h"
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

item *assoc_find(const void* key, size_t nkey, uint32_t hv, partition_t *p) {
    item *it;
    unsigned int oldbucket;

    if (p->expanding &&
        (oldbucket = (hv & hashmask(p->hashpower - 1))) >= p->expand_bucket)
    {
        it = p->old_hashtable[oldbucket];
    } else {
        it = p->primary_hashtable[hv & hashmask(p->hashpower)];
    }

    item *ret = NULL;
    int depth = 0;
    while (it) {
        if ((hv == it->hash) && (nkey == it->nkey) && (memcmp(key, ITEM_key(it), nkey) == 0)) {
            ret = it;
            break;
        }
        it = it->h_next;
        ++depth;
    }
    MEMCACHED_ASSOC_FIND(key, nkey, depth);
    return ret;
}

/* returns the address of the item pointer before the key.  if *item == 0,
   the item wasn't found */

static item** _hashitem_before (item *it, partition_t *p) {
    size_t nkey = it->nkey;
    const char* key = ITEM_key(it);
    item **pos;
    unsigned int oldbucket;

    if (p->expanding &&
        (oldbucket = (it->hash & hashmask(p->hashpower - 1))) >= p->expand_bucket)
    {
        pos = &p->old_hashtable[oldbucket];
    } else {
        pos = &p->primary_hashtable[it->hash & hashmask(p->hashpower)];
    }

    while (*pos && ((nkey != (*pos)->nkey) || memcmp(key, ITEM_key(*pos), nkey))) {
        pos = &(*pos)->h_next;
    }
    return pos;
}

static void *assoc_maintenance_thread(void *arg);

/* grows the hashtable to the next power of 2. */
static void assoc_expand(partition_t *p) {
    p->old_hashtable = p->primary_hashtable;
    p->primary_hashtable = calloc(hashsize(p->hashpower + 1), sizeof(void *));
    if (p->primary_hashtable) {
        if (settings.verbose > 1)
            fprintf(stderr, "Hash table expansion starting\n");
        p->hashpower++;
        p->expanding = true;
        p->expand_bucket = 0;

        /* start a thread to do the expansion */
        int ret;
        pthread_t tid;
        if ((ret = pthread_create(&tid, NULL,
                                  assoc_maintenance_thread, p)) != 0) {
            fprintf(stderr, "Can't create thread: %s\n", strerror(ret));
            p->hashpower--;
            p->expanding = false;
            free(p->primary_hashtable);
            p->primary_hashtable = p->old_hashtable;
        }
    } else {
        p->primary_hashtable = p->old_hashtable;
        /* Bad news, but we can keep running. */
    }
}

/* Note: this isn't an assoc_update.  The key must not already exist to call this */
int assoc_insert(item *it, partition_t *p) {
    uint32_t hv = it->hash;
    unsigned int oldbucket;

    assert(assoc_find(ITEM_key(it), it->nkey, it->hash, p) == 0);  /* shouldn't have duplicately named things defined */

    if (p->expanding &&
        (oldbucket = (hv & hashmask(p->hashpower - 1))) >= p->expand_bucket)
    {
        it->h_next = p->old_hashtable[oldbucket];
        p->old_hashtable[oldbucket] = it;
    } else {
        it->h_next = p->primary_hashtable[hv & hashmask(p->hashpower)];
        p->primary_hashtable[hv & hashmask(p->hashpower)] = it;
    }

    p->hash_items++;
    if (! p->expanding && p->hash_items > (hashsize(p->hashpower) * 3) / 2) {
        assoc_expand(p);
    }

    MEMCACHED_ASSOC_INSERT(ITEM_key(it), it->nkey, hash_items);
    return 1;
}

void assoc_delete(item *it, partition_t *p) {
    item **before = _hashitem_before(it, p);

    if (*before) {
        item *nxt;
        p->hash_items--;
        /* The DTrace probe cannot be triggered as the last instruction
         * due to possible tail-optimization by the compiler
         */
        MEMCACHED_ASSOC_DELETE(key, nkey, hash_items);
        nxt = (*before)->h_next;
        (*before)->h_next = 0;   /* probably pointless, but whatever. */
        *before = nxt;
        return;
    }
    /* Note:  we never actually get here.  the callers don't delete things
       they can't find. */
    assert(*before != 0);
}


#define DEFAULT_HASH_BULK_MOVE 1
int hash_bulk_move = DEFAULT_HASH_BULK_MOVE;

static void *assoc_maintenance_thread(void *arg) {
    partition_t *p = arg;
    bool done = false;
    do {
        int ii;
        pthread_mutex_lock(&p->mutex);

        for (ii = 0; ii < hash_bulk_move && p->expanding; ++ii) {
            item *it, *next;
            int bucket;

            for (it = p->old_hashtable[p->expand_bucket];
                 NULL != it; it = next) {
                next = it->h_next;

                bucket = hash(ITEM_key(it), it->nkey, 0) & hashmask(p->hashpower);
                it->h_next = p->primary_hashtable[bucket];
                p->primary_hashtable[bucket] = it;
            }

            p->old_hashtable[p->expand_bucket] = NULL;
            p->expand_bucket++;
            if (p->expand_bucket == hashsize(p->hashpower - 1)) {
                p->expanding = false;
                free(p->old_hashtable);
                if (settings.verbose > 1)
                    fprintf(stderr, "Hash table expansion done\n");
            }
        }
        if (!p->expanding) {
            done = true;
        }
        pthread_mutex_unlock(&p->mutex);
    } while (!done);

    pthread_detach(pthread_self());
    return NULL;
}
