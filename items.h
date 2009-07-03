#ifndef ITEMS_H
#define ITEMS_H

/* See items.c */
uint64_t get_cas_id(void);

/*@null@*/
void item_free(item *it, partition_t *p);
bool item_size_ok(const size_t nkey, const int flags, const int nbytes);

int  do_item_link(item *it, partition_t *p);     /** may fail if transgresses limits */
void do_item_unlink(item *it, partition_t *p);
void do_item_remove(item *it, partition_t *p);
void do_item_update(item *it, partition_t *p);   /** update LRU time to current and reposition */
int  do_item_replace(item *it, item *new_it, partition_t *p);

/*@null@*/
char *item_cachedump(const unsigned int slabs_clsid, const unsigned int limit, unsigned int *bytes);
void item_stats(ADD_STAT add_stats, void *c);
/*@null@*/
void item_stats_sizes(ADD_STAT add_stats, void *c);
void item_flush_expired(void);

item *do_item_get(const char *key, const size_t nkey, uint32_t hv, partition_t *p);
item *do_item_get_nocheck(const char *key, const size_t nkey, uint32_t hv, partition_t *p);
void item_stats_reset(void);
#endif
