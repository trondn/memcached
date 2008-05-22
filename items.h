/* See items.c */
void item_init(void);
/*@null@*/
item *do_item_alloc(struct slabber_engine* engine, const void *key,
                    const size_t nkey, const int flags,
                    const rel_time_t exptime, const int nbytes);

void item_free(struct slabber_engine* engine, item *it);

int  do_item_link(struct slabber_engine* engine, item *it);     /** may fail if transgresses limits */
void do_item_unlink(struct slabber_engine* engine, item *it);
void do_item_remove(struct slabber_engine* engine, item *it);
void do_item_update(struct slabber_engine* engine, item *it,
                    const rel_time_t newtime); /** update LRU time to current and reposition */
int  do_item_replace(struct slabber_engine* engine, item *it, item *new_it);

/*@null@*/
char *do_item_cachedump(const unsigned int slabs_clsid,
                        const unsigned int limit, unsigned int *bytes);
char *do_item_stats(void);

/*@null@*/
char *do_item_stats_sizes(void);
void do_item_flush_expired(struct slabber_engine* engine);
item *item_get(const char *key, const size_t nkey);

item *do_item_get_notedeleted(struct slabber_engine* engine,
                              const char *key, const size_t nkey,
                              bool *delete_locked);
item *do_item_get_nocheck(struct slabber_engine* engine, const char *key,
                          const size_t nkey);
bool item_delete_lock_over(const item *it);
