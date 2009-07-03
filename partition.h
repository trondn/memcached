#ifndef PARTITION_H
#define PARTITION_H

#define LARGEST_ID 255
typedef struct {
    unsigned int evicted;
    rel_time_t evicted_time;
    unsigned int outofmemory;
    unsigned int tailrepairs;
} itemstats_t;


typedef  unsigned long  int  ub4;   /* unsigned 4-byte quantities */
typedef  unsigned       char ub1;   /* unsigned 1-byte quantities */

#define hashsize(n) ((ub4)1<<(n))
#define hashmask(n) (hashsize(n)-1)

extern int no_partitions;

typedef struct {
   pthread_mutex_t mutex;
   item *heads[LARGEST_ID];
   item *tails[LARGEST_ID];

   unsigned int sizes[LARGEST_ID];

   itemstats_t itemstats[LARGEST_ID];


   /* how many powers of 2's worth of buckets we use */
   unsigned int hashpower;

   /* Main hash table. This is where we look except during expansion. */
   item** primary_hashtable;

   /*
    * Previous hash table. During expansion, we look here for keys that haven't
    * been moved over to the primary yet.
    */
   item** old_hashtable;

   /* Number of items in the hash table. */
   unsigned int hash_items;

   /* Flag: Are we in the middle of expanding now? */
   bool expanding;

   /*
    * During expansion we migrate values with bucket granularity; this is how
    * far we've gotten so far. Ranges from 0 .. hashsize(hashpower - 1) - 1.
    */
   unsigned int expand_bucket;
} partition_t;

extern partition_t* partitions;

void partition_init(void);
partition_t *get_partition_by_key(const void *key, size_t nkey);
struct _stritem;
partition_t *get_partition(const struct _stritem *item);
partition_t *get_partition_by_hash(uint32_t hash);
#endif
