#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sysexits.h>
#include "memcached.h"

partition_t* partitions;
int no_partitions;

void partition_init(void) {
    int ii = 0;
    char *env = getenv("PARTITION_SIZE");
    if (env != NULL) {
      no_partitions = atoi(env);
      if (no_partitions < 1) {
    no_partitions = 1;
      }
    } else {
      no_partitions = settings.num_threads << 2;
    }
    partitions = calloc(no_partitions, sizeof(partition_t));
    if (partitions == NULL) {
      perror("Failed to allocate memory for hash partitions");
      exit(EXIT_FAILURE);
    }
    for (ii = 0; ii < no_partitions; ++ii) {
        if (pthread_mutex_init(&partitions[ii].mutex, NULL) != 0) {
            perror("Failed to initialize mutex");
            exit(EXIT_FAILURE);
        }
        partitions[ii].hashpower = 16;

        partitions[ii].primary_hashtable = calloc(hashsize(16), sizeof(void *));
        if (! partitions[ii].primary_hashtable) {
            fprintf(stderr, "Failed to init hashtable.\n");
            exit(EXIT_FAILURE);
        }
    }
}

partition_t *get_partition_by_key(const void *key, size_t nkey) {
   return get_partition_by_hash(hash(key, nkey, 0));
}

partition_t *get_partition(const item *it) {
   return get_partition_by_key(ITEM_key(it), it->nkey);
}

partition_t *get_partition_by_hash(uint32_t hv) {
   return  &partitions[hv % no_partitions];
}

void item_stats(ADD_STAT add_stats, void *c) {
   rel_time_t times[LARGEST_ID];
   bool used[LARGEST_ID];
   itemstats_t itemstats[LARGEST_ID];
   size_t sizes[LARGEST_ID];

   memset(used, 0, sizeof(used));
   memset(itemstats, 0, sizeof(itemstats));
   memset(sizes, 0, sizeof(sizes));

   /* aggregate stats */

   for (int ii = 0; ii < no_partitions; ++ii) {
      partition_t *p = &partitions[ii];
      pthread_mutex_lock(&p->mutex);

      for (int i = 0; i < LARGEST_ID; i++) {
         if (p->tails[i] != NULL) {
            sizes[i] += p->sizes[i];
            itemstats[i].evicted += p->itemstats[i].evicted;
            if (itemstats[i].evicted == 1 ||
                itemstats[i].evicted_time > p->itemstats[i].evicted_time) {
               itemstats[i].evicted_time = p->itemstats[i].evicted_time;
            }

            itemstats[i].outofmemory += p->itemstats[i].outofmemory;
            itemstats[i].tailrepairs += p->itemstats[i].tailrepairs;

            if (!used[i]) {
               used[i] = true;
               times[i] = p->tails[i]->time;
            } else {
               if (times[i] > p->tails[i]->time) {
                  times[i] = p->tails[i]->time;
               }
            }
         }
      }
      pthread_mutex_unlock(&p->mutex);
   }

    int i;
    for (i = 0; i < LARGEST_ID; i++) {
        if (used[i]) {
            const char *fmt = "items:%d:%s";
            char key_str[128];
            char val_str[256];
            int klen = 0, vlen = 0;

            APPEND_NUM_FMT_STAT(fmt, i, "number", "%lu", sizes[i]);
            APPEND_NUM_FMT_STAT(fmt, i, "age", "%u", times[i]);
            APPEND_NUM_FMT_STAT(fmt, i, "evicted",
                                "%u", itemstats[i].evicted);
            APPEND_NUM_FMT_STAT(fmt, i, "evicted_time",
                                "%u", itemstats[i].evicted_time);
            APPEND_NUM_FMT_STAT(fmt, i, "outofmemory",
                                "%u", itemstats[i].outofmemory);
            APPEND_NUM_FMT_STAT(fmt, i, "tailrepairs",
                                "%u", itemstats[i].tailrepairs);;
        }
    }

    /* getting here means both ascii and binary terminators fit */
    add_stats(NULL, 0, NULL, 0, c);
}

