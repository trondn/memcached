#ifndef ASSOC_H
#define ASSOC_H

/* associative array */
item *assoc_find(const void *key, size_t nkey, uint32_t hv, partition_t *p);
int assoc_insert(item *item, partition_t *p);
void assoc_delete(item *item, partition_t *p);

#endif
