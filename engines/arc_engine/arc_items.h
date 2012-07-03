#ifndef ITEMS_H
#define ITEMS_H

enum arc_list_type {
       ARC_LIST_T1 = 0,
       ARC_LIST_T2 = 1,
       ARC_LIST_B1 = 2,
       ARC_LIST_B2 = 3,
       ARC_TOTAL = 4
};

/*
 * You should not try to aquire any of the item locks before calling these
 * functions.
 */
typedef struct _hash_item {
    struct _hash_item *next;
    struct _hash_item *prev;
    struct _hash_item *h_next; /* hash chain next */
    rel_time_t time;  /* least recent access */
    rel_time_t exptime; /**< When the item will expire (relative to process
                         * startup) */
    uint32_t nbytes; /**< The total size of the data (in bytes) */
    uint32_t flags; /**< Flags associated with the item (in network byte order)*/
    uint16_t nkey; /**< The total length of the key (in bytes) */
    uint16_t iflag; /**< Intermal flags. lower 8 bit is reserved for the core
                     * server, the upper 8 bits is reserved for engine
                     * implementation. */
    unsigned short refcount;
    uint8_t slabs_clsid;/* which slab class we're in */
} hash_item;

typedef struct {
    unsigned int evicted;
    unsigned int evicted_nonzero;
    rel_time_t evicted_time;
    unsigned int outofmemory;
    unsigned int tailrepairs;
    unsigned int reclaimed;
} itemstats_t;

struct items {
   hash_item *heads[ARC_TOTAL][POWER_LARGEST];
   hash_item *tails[ARC_TOTAL][POWER_LARGEST];
   itemstats_t itemstats[POWER_LARGEST];
   unsigned int sizes[ARC_TOTAL][POWER_LARGEST];
   unsigned int virt_p[POWER_LARGEST];
};


/**
 * Allocate and initialize a new item structure
 * @param engine handle to the storage engine
 * @param key the key for the new item
 * @param nkey the number of bytes in the key
 * @param flags the flags in the new item
 * @param exptime when the object should expire
 * @param nbytes the number of bytes in the body for the item
 * @return a pointer to an item on success NULL otherwise
 */
hash_item *item_alloc(struct arc_engine *engine,
                      const void *key, size_t nkey, int flags,
                      rel_time_t exptime, int nbytes, const void *cookie);

/**
 * Get an item from the cache
 *
 * @param engine handle to the storage engine
 * @param key the key for the item to get
 * @param nkey the number of bytes in the key
 * @return pointer to the item if it exists or NULL otherwise
 */
hash_item *item_get(struct arc_engine *engine,
                    const void *key, const size_t nkey);

/**
 * Reset the item statistics
 * @param engine handle to the storage engine
 */
void item_stats_reset(struct arc_engine *engine);

/**
 * Get item statitistics
 * @param engine handle to the storage engine
 * @param add_stat callback provided by the core used to
 *                 push statistics into the response
 * @param cookie cookie provided by the core to identify the client
 */
void item_stats(struct arc_engine *engine,
                ADD_STAT add_stat,
                const void *cookie);

/**
 * Get detaild item statitistics
 * @param engine handle to the storage engine
 * @param add_stat callback provided by the core used to
 *                 push statistics into the response
 * @param cookie cookie provided by the core to identify the client
 */
void item_stats_sizes(struct arc_engine *engine,
                      ADD_STAT add_stat, const void *cookie);

/**
 * Dump items from the cache
 * @param engine handle to the storage engine
 * @param slabs_clsid the slab class to get items from
 * @param limit the maximum number of items to receive
 * @param bytes the number of bytes in the return message (OUT)
 * @return pointer to a string containint the data
 *
 * @todo we need to rewrite this to use callbacks!!!! currently disabled
 */
char *item_cachedump(struct arc_engine *engine,
                     const unsigned int slabs_clsid,
                     const unsigned int limit,
                     unsigned int *bytes);

/**
 * Flush expired items from the cache
 * @param engine handle to the storage engine
 * @param when when the items should be flushed
 */
void  item_flush_expired(struct arc_engine *engine, time_t when);

/**
 * Release our reference to the current item
 * @param engine handle to the storage engine
 * @param it the item to release
 */
void item_release(struct arc_engine *engine, hash_item *it);

/**
 * Unlink the item from the hash table (make it inaccessible)
 * @param engine handle to the storage engine
 * @param it the item to unlink
 */
void item_unlink(struct arc_engine *engine, hash_item *it);

/**
 * Set the expiration time for an object
 * @param engine handle to the storage engine
 * @param key the key to set
 * @param nkey the number of characters in key..
 * @param exptime the expiration time
 * @return The (updated) item if it exists
 */
hash_item *touch_item(struct arc_engine *engine,
                      const void *key,
                      uint16_t nkey,
                      uint32_t exptime);

/**
 * Store an item in the cache
 * @param engine handle to the storage engine
 * @param item the item to store
 * @param cas the cas value (OUT)
 * @param operation what kind of store operation is this (ADD/SET etc)
 * @return ENGINE_SUCCESS on success
 *
 * @todo should we refactor this into hash_item ** and remove the cas
 *       there so that we can get it from the item instead?
 */
ENGINE_ERROR_CODE store_item(struct arc_engine *engine,
                             hash_item *item,
                             uint64_t *cas,
                             ENGINE_STORE_OPERATION operation,
                             const void *cookie);

ENGINE_ERROR_CODE arithmetic(struct arc_engine *engine,
                             const void* cookie,
                             const void* key,
                             const int nkey,
                             const bool increment,
                             const bool create,
                             const uint64_t delta,
                             const uint64_t initial,
                             const rel_time_t exptime,
                             uint64_t *cas,
                             uint64_t *result);


/**
 * Start the item scrubber
 * @param engine handle to the storage engine
 */
bool item_start_scrub(struct arc_engine *engine);

/**
 * The tap walker to walk the hashtables
 */
tap_event_t item_tap_walker(ENGINE_HANDLE* handle,
                            const void *cookie, item **itm,
                            void **es, uint16_t *nes, uint8_t *ttl,
                            uint16_t *flags, uint32_t *seqno,
                            uint16_t *vbucket);

bool initialize_item_tap_walker(struct arc_engine *engine,
                                const void* cookie);


#endif
