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
#include <time.h>
#include <assert.h>

/* Forward Declarations */
static void item_link_q(item *it, partition_t *p);
static void item_unlink_q(item *it, partition_t *p);

/*
 * We only reposition items in the LRU queue if they haven't been repositioned
 * in this many seconds. That saves us from churning on frequently-accessed
 * items.
 */
#define ITEM_UPDATE_INTERVAL 60

void item_stats_reset(void) {
    for (int ii = 0; ii < no_partitions; ++ii) {
        partition_t *p = &partitions[ii];

        pthread_mutex_lock(&p->mutex);
        memset(p->itemstats, 0, sizeof(p->itemstats));
        pthread_mutex_unlock(&p->mutex);
    }
}


/* Get the next CAS id for a new item. */
uint64_t get_cas_id(void) {
    static uint64_t cas_id = 0;
    return ++cas_id;
}

/* Enable this for reference-count debugging. */
#if 0
# define DEBUG_REFCNT(it,op) \
                fprintf(stderr, "item %x refcnt(%c) %d %c%c%c\n", \
                        it, op, it->refcount, \
                        (it->it_flags & ITEM_LINKED) ? 'L' : ' ', \
                        (it->it_flags & ITEM_SLABBED) ? 'S' : ' ')
#else
# define DEBUG_REFCNT(it,op) while(0)
#endif

static bool evict_from_partition(partition_t *p, unsigned int id) {
    int tries = 50;
    item *search;
    /* If requested to not push old items out of cache when memory runs
     * out, we're out of luck at this point...
     */
    if (settings.evict_to_free == 0 || p->tails[id] == 0) {
        p->itemstats[id].outofmemory++;
        return false;
    }

    /*
     * try to get one off the right LRU
     * don't necessariuly unlink the tail because it may be locked:
     * refcount>0 search up from tail an item with refcount==0 and
     * unlink it; give up after 50 tries
     */
    for (search = p->tails[id]; tries > 0 && search != NULL; tries--, search=search->prev) {
        if (search->refcount == 0) {
            if (search->exptime == 0 || search->exptime > current_time) {
                p->itemstats[id].evicted++;
                p->itemstats[id].evicted_time = current_time - search->time;
                STATS_LOCK();
                stats.evictions++;
                STATS_UNLOCK();
            }
            do_item_unlink(search, p);
            return true;
        }
    }
    return false;
}

/**
 * Generates the variable-sized part of the header for an object.
 *
 * key     - The key
 * nkey    - The length of the key
 * flags   - key flags
 * nbytes  - Number of bytes to hold value and addition CRLF terminator
 * suffix  - Buffer for the "VALUE" line suffix (flags, size).
 * nsuffix - The length of the suffix is stored here.
 *
 * Returns the total size of the header.
 */
static size_t item_make_header(const uint8_t nkey, const int flags, const int nbytes,
                     char *suffix, uint8_t *nsuffix) {
    /* suffix is defined at 40 chars elsewhere.. */
    *nsuffix = (uint8_t) snprintf(suffix, 40, " %d %d\r\n", flags, nbytes - 2);
    return sizeof(item) + nkey + *nsuffix + nbytes;
}

/*@null@*/
item *item_alloc(char *key, const size_t nkey, const int flags, const rel_time_t exptime, const int nbytes) {
    uint8_t nsuffix;
    item *it = NULL;
    char suffix[40];
    size_t ntotal = item_make_header(nkey + 1, flags, nbytes, suffix, &nsuffix);
    if (settings.use_cas) {
        ntotal += sizeof(uint64_t);
    }

    unsigned int id = slabs_clsid(ntotal);
    if (id == 0)
        return 0;

    partition_t *p = get_partition_by_key(key, nkey);
    pthread_mutex_lock(&p->mutex);

    /* do a quick check if we have any expired items in the tail.. */
    int tries = 50;
    item *search;

    for (search = p->tails[id];
         tries > 0 && search != NULL;
         tries--, search=search->prev) {
        if (search->refcount == 0 &&
            (search->exptime != 0 && search->exptime < current_time)) {
            it = search;
            /* I don't want to actually free the object, just steal
             * the item to avoid to grab the slab mutex twice ;-)
             */
            it->refcount = 1;
            do_item_unlink(it, p);
            /* Initialize the item block: */
            it->slabs_clsid = 0;
            it->refcount = 0;
            break;
        }
    }
    pthread_mutex_unlock(&p->mutex);

    if (it == NULL && (it = slabs_alloc(ntotal, id)) == NULL) {
        if (settings.evict_to_free == 0) {
#ifdef FUTURE
            itemstats[id].outofmemory++;
#endif
            return NULL;
        }

        // for now, kick out from the partition I'm going to insert into!
        partition_t *p = get_partition_by_key(key, nkey);
        pthread_mutex_lock(&p->mutex);
        bool success = evict_from_partition(p, id);
        pthread_mutex_unlock(&p->mutex);

        if (success) {
            it = slabs_alloc(ntotal, id);
        }

        if (it == 0) {
            // ok, let's search through _all_ of the partitions:
            for (int ii = 0; ii < no_partitions && it == NULL; ++ii) {
                partition_t *p = &partitions[ii];
                pthread_mutex_lock(&p->mutex);
                success = evict_from_partition(p, id);
                pthread_mutex_unlock(&p->mutex);

                if (success) {
                    it = slabs_alloc(ntotal, id);
                }
            }


            if (it == NULL) {
                return NULL;
            }
        }
    }

    assert(it->slabs_clsid == 0);

    it->slabs_clsid = id;

    assert(it != p->heads[it->slabs_clsid]);

    it->next = it->prev = it->h_next = 0;
    it->refcount = 1;     /* the caller will have a reference */
    DEBUG_REFCNT(it, '*');
    it->it_flags = settings.use_cas ? ITEM_CAS : 0;
    it->nkey = nkey;
    it->nbytes = nbytes;
    memcpy(ITEM_key(it), key, nkey);
    it->exptime = exptime;
    memcpy(ITEM_suffix(it), suffix, (size_t)nsuffix);
    it->nsuffix = nsuffix;
    it->hash = hash(key, nkey, 0);
    return it;
}

void item_free(item *it, partition_t *p) {
    size_t ntotal = ITEM_ntotal(it);
    unsigned int clsid;
    assert((it->it_flags & ITEM_LINKED) == 0);
    assert(it != p->heads[it->slabs_clsid]);
    assert(it != p->tails[it->slabs_clsid]);
    assert(it->refcount == 0);

    /* so slab size changer can tell later if item is already free or not */
    clsid = it->slabs_clsid;
    it->slabs_clsid = 0;
    it->it_flags |= ITEM_SLABBED;
    DEBUG_REFCNT(it, 'F');
    slabs_free(it, ntotal, clsid);
}

/**
 * Returns true if an item will fit in the cache (its size does not exceed
 * the maximum for a cache entry.)
 */
bool item_size_ok(const size_t nkey, const int flags, const int nbytes) {
    char prefix[40];
    uint8_t nsuffix;

    return slabs_clsid(item_make_header(nkey + 1, flags, nbytes,
                                        prefix, &nsuffix)) != 0;
}

static void item_link_q(item *it, partition_t *p) { /* item is the new head */
    item **head, **tail;
    /* always true, warns: assert(it->slabs_clsid <= LARGEST_ID); */
    assert((it->it_flags & ITEM_SLABBED) == 0);

    head = &p->heads[it->slabs_clsid];
    tail = &p->tails[it->slabs_clsid];
    assert(it != *head);
    assert((*head && *tail) || (*head == 0 && *tail == 0));
    it->prev = 0;
    it->next = *head;
    if (it->next) it->next->prev = it;
    *head = it;
    if (*tail == 0) *tail = it;
    p->sizes[it->slabs_clsid]++;
    return;
}

static void item_unlink_q(item *it, partition_t *p) {
    item **head, **tail;
    /* always true, warns: assert(it->slabs_clsid <= LARGEST_ID); */
    head = &p->heads[it->slabs_clsid];
    tail = &p->tails[it->slabs_clsid];

    if (*head == it) {
        assert(it->prev == 0);
        *head = it->next;
    }
    if (*tail == it) {
        assert(it->next == 0);
        *tail = it->prev;
    }
    assert(it->next != it);
    assert(it->prev != it);

    if (it->next) it->next->prev = it->prev;
    if (it->prev) it->prev->next = it->next;
    p->sizes[it->slabs_clsid]--;
    return;
}

int do_item_link(item *it, partition_t *p) {
    MEMCACHED_ITEM_LINK(ITEM_key(it), it->nkey, it->nbytes);
    assert((it->it_flags & (ITEM_LINKED|ITEM_SLABBED)) == 0);
    assert(it->nbytes < (1024 * 1024));  /* 1MB max size */
    it->it_flags |= ITEM_LINKED;
    it->time = current_time;
    assoc_insert(it, p);

    STATS_LOCK();
    stats.curr_bytes += ITEM_ntotal(it);
    stats.curr_items += 1;
    stats.total_items += 1;
    STATS_UNLOCK();

    /* Allocate a new CAS ID on link. */
    ITEM_set_cas(it, (settings.use_cas) ? get_cas_id() : 0);

    item_link_q(it, p);

    return 1;
}

void do_item_unlink(item *it, partition_t *p) {
    MEMCACHED_ITEM_UNLINK(ITEM_key(it), it->nkey, it->nbytes);
    if ((it->it_flags & ITEM_LINKED) != 0) {
        it->it_flags &= ~ITEM_LINKED;
        STATS_LOCK();
        stats.curr_bytes -= ITEM_ntotal(it);
        stats.curr_items -= 1;
        STATS_UNLOCK();
        assoc_delete(it, p);
        item_unlink_q(it, p);
        if (it->refcount == 0) item_free(it, p);
    }
}

void do_item_remove(item *it, partition_t *p) {
    MEMCACHED_ITEM_REMOVE(ITEM_key(it), it->nkey, it->nbytes);
    assert((it->it_flags & ITEM_SLABBED) == 0);
    if (it->refcount != 0) {
        it->refcount--;
        DEBUG_REFCNT(it, '-');
    }
    if (it->refcount == 0 && (it->it_flags & ITEM_LINKED) == 0) {
        item_free(it, p);
    }
}

void do_item_update(item *it, partition_t *p) {
    MEMCACHED_ITEM_UPDATE(ITEM_key(it), it->nkey, it->nbytes);
    if (it->time < current_time - ITEM_UPDATE_INTERVAL) {
        assert((it->it_flags & ITEM_SLABBED) == 0);

        if ((it->it_flags & ITEM_LINKED) != 0) {
            item_unlink_q(it, p);
            it->time = current_time;
            item_link_q(it, p);
        }
    }
}

int do_item_replace(item *it, item *new_it, partition_t *p) {
    MEMCACHED_ITEM_REPLACE(ITEM_key(it), it->nkey, it->nbytes,
                           ITEM_key(new_it), new_it->nkey, new_it->nbytes);
    assert((it->it_flags & ITEM_SLABBED) == 0);

    do_item_unlink(it, p);
    return do_item_link(new_it, p);
}

/*@null@*/
/**
 * Dump a number of keys from a specific slab class
 * @param clsid slab class to dump from
 * @param limit the number of items to dump
 * @param bytes the number of bytes in the response
 */
char *item_cachedump(const unsigned int clsid,
                     const unsigned int limit,
                     unsigned int *bytes) {
    const size_t memlimit = 2 * 1024 * 1024;   /* 2MB max response size */
    size_t avail = memlimit - 5; /* 5 = "END\r\n" */
    char *buffer;

    if (clsid > LARGEST_ID || (buffer = malloc(memlimit)) == NULL) {
        return NULL;
    }

    char *ptr = buffer;
    int shown = 0;
    bool space = true;

    for (int ii = 0; ii < no_partitions && space; ++ii) {
        partition_t *p = &partitions[ii];
        pthread_mutex_lock(&p->mutex);

        for (item* it = p->heads[clsid]; it != NULL && space; it = it->next) {
            char temp[64];
            int len = snprintf(temp, sizeof(temp), " [%d b; %lu s]\r\n",
                               it->nbytes - 2,
                               (unsigned long)it->exptime + process_started);
            if (len > 0) {
                temp[sizeof(temp) - 1] = '\0';
                if ((it->nkey + len + 5) < avail) { /* 5 = "ITEM " */
                    memcpy(ptr, "ITEM ", 5);
                    ptr += 5;
                    memcpy(ptr, ITEM_key(it), it->nkey);
                    ptr += it->nkey;
                    memcpy(ptr, temp, len);
                    ptr += len;
                    ++shown;
                    if (shown == limit) {
                        space = false;
                    }
                    avail -= it->nkey + len + 5;
                } else {
                    space = false;
                }
            } else {
                space = false;
            }
        }
        pthread_mutex_unlock(&p->mutex);
    }

    memcpy(ptr, "END\r\n", 5);
    *bytes = memlimit - avail;
    return buffer;
}

/** dumps out a list of objects of each size, with granularity of 32 bytes */
/*@null@*/
void item_stats_sizes(ADD_STAT add_stats, void *c) {
    /* max 1MB object, divided into 32 bytes size buckets */
    const int num_buckets = 32768;
    unsigned int *histogram = calloc(num_buckets, sizeof(int));

    if (histogram != NULL) {
        int i;

        for (int ii = 0; ii < no_partitions; ++ii) {
            partition_t *p = &partitions[ii];

            pthread_mutex_lock(&p->mutex);

            /* build the histogram */
            for (i = 0; i < LARGEST_ID; i++) {
                for (item *it = p->heads[i]; it != NULL; it = it->next) {
                    int ntotal = ITEM_ntotal(it);
                    int bucket = ntotal / 32;
                    if ((ntotal % 32) != 0) {
                        bucket++;
                    }
                    if (bucket < num_buckets) {
                        histogram[bucket]++;
                    }
                }
            }

            pthread_mutex_unlock(&p->mutex);
        }

        /* write the buffer */
        for (i = 0; i < num_buckets; i++) {
            if (histogram[i] != 0) {
                char key[8];
                int klen = 0;
                klen = sprintf(key, "%d", i * 32);
                assert(klen < sizeof(key));
                APPEND_STAT(key, "%u", histogram[i]);
            }
        }
        free(histogram);
    }
    add_stats(NULL, 0, NULL, 0, c);
}

/** wrapper around assoc_find which does the lazy expiration logic */
item *do_item_get(const char *key, const size_t nkey, uint32_t hv, partition_t *p) {
    item *it = assoc_find(key, nkey, hv, p);
    int was_found = 0;

    if (settings.verbose > 2) {
        if (it == NULL) {
            fprintf(stderr, "> NOT FOUND %s", key);
        } else {
            fprintf(stderr, "> FOUND KEY %s", ITEM_key(it));
            was_found++;
        }
    }

    if (it != NULL && settings.oldest_live != 0 && settings.oldest_live <= current_time &&
        it->time <= settings.oldest_live) {
        do_item_unlink(it, p);           /* MTSAFE - cache_lock held */
        it = NULL;
    }

    if (it == NULL && was_found) {
        fprintf(stderr, " -nuked by flush");
        was_found--;
    }

    if (it != NULL && it->exptime != 0 && it->exptime <= current_time) {
        do_item_unlink(it, p);           /* MTSAFE - cache_lock held */
        it = NULL;
    }

    if (it == NULL && was_found) {
        fprintf(stderr, " -nuked by expire");
        was_found--;
    }

    if (it != NULL) {
        it->refcount++;
        DEBUG_REFCNT(it, '+');
    }

    if (settings.verbose > 2)
        fprintf(stderr, "\n");

    return it;
}

/** returns an item whether or not it's expired. */
item *do_item_get_nocheck(const char *key, const size_t nkey, uint32_t hv, partition_t *p) {
    item *it = assoc_find(key, nkey, hv, p);
    if (it) {
        it->refcount++;
        DEBUG_REFCNT(it, '+');
    }
    return it;
}

/* expires items that are more recent than the oldest_live setting. */
void item_flush_expired(void) {
    if (settings.oldest_live == 0)
        return;

    for (int ii = 0; ii < no_partitions; ++ii) {
        int i;
        item *iter, *next;
        partition_t *p = &partitions[ii];
        pthread_mutex_lock(&p->mutex);

        for (i = 0; i < LARGEST_ID; i++) {
            /* The LRU is sorted in decreasing time order, and an item's timestamp
             * is never newer than its last access time, so we only need to walk
             * back until we hit an item older than the oldest_live time.
             * The oldest_live checking will auto-expire the remaining items.
             */
            for (iter = p->heads[i]; iter != NULL; iter = next) {
                if (iter->time >= settings.oldest_live) {
                    next = iter->next;
                    if ((iter->it_flags & ITEM_SLABBED) == 0) {
                        do_item_unlink(iter, p);
                    }
                } else {
                    /* We've hit the first old item. Continue to the next queue. */
                    break;
                }
            }
        }
        pthread_mutex_unlock(&p->mutex);
    }
}
