/*
 * Copyright (c) <2008>, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the  nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SUN MICROSYSTEMS, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SUN MICROSYSTEMS, INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Summary: Specification of the storage engine interface.
 *
 * Copy: See Copyright for the status of this software.
 *
 * Author: Trond Norbye <trond.norbye@sun.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>

#include "memcached.h"
#include "slab_engine.h"


static int start_maintenance_thread(struct slabber_engine *engine);
static void stop_maintenance_thread(struct slabber_engine *engine);

static const char* slabber_get_info(struct engine_handle* handle);
static ENGINE_ERROR_CODE slabber_initialize(struct engine_handle* handle, const char* config_str);
static void slabber_destroy(struct engine_handle* handle);
static item* slabber_item_allocate(struct engine_handle* handle, const void* key,
        const size_t nkey, const int flags, const rel_time_t exptime,
        const int nbytes);
static void slabber_item_release(struct engine_handle* handle, item* item);
static item* slabber_get(struct engine_handle* handle, const void* key, const int nkey);
static char* slabber_get_stats(struct engine_handle* handle, const char* what_to_fetch);
static void slabber_touch(struct engine_handle* handle, item *item, const rel_time_t newtime);
static ENGINE_ERROR_CODE slabber_store(struct engine_handle* handle, item* item, enum operation operation);
static void slabber_flush(struct engine_handle* handle, time_t when);
static ENGINE_ERROR_CODE slabber_item_delete(struct engine_handle* handle, item* item, const rel_time_t exptime);
static ENGINE_ERROR_CODE slabber_arithmetic(struct engine_handle* handle,
                                          const void* key,
                                          const int nkey,
                                          const bool increment,
                                          const bool create,
                                          const uint64_t delta,
                                          const uint64_t initial,
                                          const rel_time_t exptime,
                                          uint64_t *cas,
                                          uint64_t *result);


extern bool item_size_ok(struct engine_handle* handle, const size_t nkey, const int flags, const size_t nbytes);

ENGINE_HANDLE* create_instance(int version, ENGINE_ERROR_CODE* error) {
   struct slabber_engine *handle;
   if (version != 1) {
      if (error != NULL) {
         *error = ENGINE_ENOTSUP;
      }
      return 0;
   }

   if ((handle = calloc(1, sizeof(*handle))) == NULL) {
      if (error != NULL) {
         *error = ENGINE_ENOMEM;
      }
      return 0;
   }

   handle->engine.interface_level = version;
   handle->initialized = false;
   handle->engine.get_info = slabber_get_info;
   handle->engine.initialize = slabber_initialize;
   handle->engine.destroy = slabber_destroy;
   handle->engine.item_size_ok = item_size_ok;
   handle->engine.item_allocate = slabber_item_allocate;
   handle->engine.item_delete = slabber_item_delete;
   handle->engine.item_release = slabber_item_release;
   handle->engine.get = slabber_get;
   handle->engine.get_stats = slabber_get_stats;
   handle->engine.store = slabber_store;
   handle->engine.arithmetic = slabber_arithmetic;
   handle->engine.flush = slabber_flush;
   handle->engine.touch = slabber_touch;

   return &handle->engine;
}

static inline struct slabber_engine* get_handle(struct engine_handle* handle) {
   return (struct slabber_engine*)handle;
}

/**
 * Initialize the slabber module.
 *
 * Initialize the different subsystems (items, assoc, slabber) and
 * start the maintenance thread.
 *
 * @param handle The handle to the slabber instance to initialize
 * @param config_str A string containing configuration variables
 * @return ENGINE_SUCCESS if success, otherwise a uniq error code is returned.
 */
static ENGINE_ERROR_CODE slabber_initialize(struct engine_handle* handle,
                                          const char* config_str) {
   ENGINE_ERROR_CODE ret = ENGINE_SUCCESS;
   struct slabber_engine* se = get_handle(handle);

   if (se->initialized) {
      return ENGINE_EINVAL;
   }

   se->assoc_expanding = false;
   /* initialise deletion array */
   se->deltotal = 200;
   se->delcurr = 0;
   if ((se->todelete = calloc(se->deltotal, sizeof(item *))) == NULL) {
      return ENGINE_ENOMEM;
   }

   if (pthread_mutex_init(&se->cache_lock, NULL) != 0) {
      return ENGINE_EINVAL;
   }

   if (pthread_mutex_init(&se->slabs_lock, NULL) != 0) {
      pthread_mutex_destroy(&se->cache_lock);
      return ENGINE_EINVAL;
   }

   if (pthread_mutex_init(&se->maintenance_mutex, NULL) != 0) {
      pthread_mutex_destroy(&se->cache_lock);
      pthread_mutex_destroy(&se->slabs_lock);
      return ENGINE_EINVAL;
   }

   if (pthread_cond_init(&se->maintenance_cond, NULL) != 0) {
      pthread_mutex_destroy(&se->cache_lock);
      pthread_mutex_destroy(&se->slabs_lock);
      pthread_mutex_destroy(&se->maintenance_mutex);
      return ENGINE_EINVAL;
   }

   if (start_maintenance_thread(se) != 0) {
      pthread_mutex_destroy(&se->cache_lock);
      pthread_mutex_destroy(&se->slabs_lock);
      pthread_mutex_destroy(&se->maintenance_mutex);
      pthread_cond_destroy(&se->maintenance_cond);
      return ENGINE_EINVAL;
   }

   item_init();
   assoc_init();
   slabs_init(settings.maxbytes, settings.factor, false);

   se->initialized = true;
   return ret;
}

/**
 * Destroy this slabber instance by releasing all allocated resources.
 *
 * @param handle The handle to the slabber instance to destory
 */
static void slabber_destroy(struct engine_handle* handle) {
   struct slabber_engine* se = get_handle(handle);

   if (se->initialized) {
      stop_maintenance_thread(se);

      pthread_mutex_destroy(&se->cache_lock);
      pthread_mutex_destroy(&se->slabs_lock);
      pthread_mutex_destroy(&se->maintenance_mutex);
      pthread_cond_destroy(&se->maintenance_cond);
      se->initialized = false;

      /* @TODO clean up the other resources! */
   }
   free(se);
}

/**
 * Get a version string from the slabber engine
 */
static const char* slabber_get_info(struct engine_handle* handle) {
   return "Slabber engine v0.1";
}

/**
 * Get statistics information.
 * @todo implement me!
 */
static char* slabber_get_stats(struct engine_handle* handle, const char* what_to_fetch){
   char* ret = NULL;
   struct slabber_engine* engine = get_handle(handle);

   if (strcmp(what_to_fetch, "slabs") == 0) {
      pthread_mutex_lock(&engine->slabs_lock);
      ret = do_slabs_stats();
      pthread_mutex_unlock(&engine->slabs_lock);
   } else if (strcmp(what_to_fetch, "items") == 0) {
      pthread_mutex_lock(&engine->cache_lock);
      ret = do_item_stats();
      pthread_mutex_unlock(&engine->cache_lock);
   } else if (strcmp(what_to_fetch, "sizes") == 0) {
      pthread_mutex_lock(&engine->cache_lock);
      ret = do_item_stats_sizes();
      pthread_mutex_unlock(&engine->cache_lock);
   }

   return ret;
}


/*
 * Decrements the reference count on an item and adds it to the freelist if
 * needed.
 */
static void slabber_item_release(struct engine_handle* handle, item* item) {
   struct slabber_engine* engine = get_handle(handle);
   pthread_mutex_lock(&engine->cache_lock);
   do_item_remove(engine, item);
   pthread_mutex_unlock(&engine->cache_lock);
}

/*
 * Moves an item to the back of the LRU queue.
 */
static void slabber_touch(struct engine_handle* handle,
                                    item *item, const rel_time_t newtime) {
    struct slabber_engine* engine = get_handle(handle);
    pthread_mutex_lock(&engine->cache_lock);
    do_item_update(engine, item, newtime);
    pthread_mutex_unlock(&engine->cache_lock);
}

/*
 * Adds an item to the deferred-delete list so it can be reaped later.
 */
static ENGINE_ERROR_CODE slabber_item_delete(struct engine_handle* handle,
                                                   item* it,
                                                   const rel_time_t exptime) {

   struct slabber_engine* se = get_handle(handle);
   ENGINE_ERROR_CODE ret = ENGINE_SUCCESS;

   pthread_mutex_lock(&se->cache_lock);

   if (exptime == 0) {
      do_item_unlink(se, it);
      do_item_remove(se, it);
   } else {
      if (se->delcurr >= se->deltotal) {
         item **new_delete = realloc(se->todelete,
                                     sizeof(item *) * se->deltotal * 2);
         if (new_delete) {
            se->todelete = new_delete;
            se->deltotal *= 2;
         } else {
            /*
             * can't delete it immediately, user wants a delay,
             * but we ran out of memory for the delete queue
             */
            /* release reference */
            do_item_remove(se, it);
            ret = ENGINE_ENOMEM;
         }
      }

      if (ret == ENGINE_SUCCESS) {
         /* use its expiration time as its deletion time now */
         it->exptime = exptime;
         it->it_flags |= ITEM_DELETED;
         se->todelete[se->delcurr++] = it;
      }
   }

   pthread_mutex_unlock(&se->cache_lock);

   return ret;
}

/*
 * Stores an item in the cache according to the semantics of one of the set
 * commands.
 *
 * Returns True if the item was stored
 */
static int do_store_item(struct slabber_engine* engine, item *it, enum operation comm) {
   char *key = ITEM_key(it);
   bool delete_locked = false;
   item *old_it = do_item_get_notedeleted(engine, key, it->nkey, &delete_locked);

   item *new_it = NULL;
   int flags;

   /* initial value is "NOT_STORED" because we want to keep the same semantic 
      as the original do_store_item implementation. */
   ENGINE_ERROR_CODE stored = ENGINE_NOT_STORED;

   if (old_it != NULL && comm == NREAD_ADD) {
      /* add only adds a nonexistent item, but promote to head of LRU */
      do_item_update(engine, old_it, current_time);
   } else if (!old_it && (comm == NREAD_REPLACE
                          || comm == NREAD_APPEND || comm == NREAD_PREPEND)) {
      /* replace only replaces an existing value; don't store */
   } else if (delete_locked && (comm == NREAD_REPLACE || comm == NREAD_ADD
                                || comm == NREAD_APPEND || comm == NREAD_PREPEND)) {
      /* replace and add can't override delete locks; don't store */
   } else if (comm == NREAD_CAS) {
      /* validate cas operation */
      if (delete_locked) {
         old_it = do_item_get_nocheck(engine, key, it->nkey);
      }

      if(old_it == NULL) {
         // LRU expired
         stored = ENGINE_KEY_ENOENT;
      } else if(it->cas_id == old_it->cas_id) {
         // cas validates
         do_item_replace(engine, old_it, it);
         stored = ENGINE_SUCCESS;
      } else {
         if(settings.verbose > 1) {
            fprintf(stderr, "CAS:  failure: expected %llu, got %llu\n",
                    old_it->cas_id, it->cas_id);
         }
         stored = ENGINE_KEY_EEXISTS;
      }
   } else {
      /*
       * Append - combine new and old record into single one. Here it's
       * atomic and thread-safe.
       */
      if (comm == NREAD_APPEND || comm == NREAD_PREPEND) {
         /*
          * Validate CAS
          */
         if (it->cas_id != 0) {
            // CAS much be equal
            if (it->cas_id != old_it->cas_id) {
               stored = ENGINE_KEY_EEXISTS;
            }
         }

         if (stored == ENGINE_NOT_STORED) {
            /* we have it and old_it here - alloc memory to hold both */
            /* flags was already lost - so recover them from ITEM_suffix(it) */

            flags = (int) strtol(ITEM_suffix(old_it), (char **) NULL, 10);

            new_it = do_item_alloc(engine, key, it->nkey, flags, old_it->exptime,
                                   it->nbytes + old_it->nbytes - 2 /* CRLF */);

            if (new_it == NULL) {
               /* SERVER_ERROR out of memory */
               return 0;
            }

            /* copy data from it and old_it to new_it */

            if (comm == NREAD_APPEND) {
               memcpy(ITEM_data(new_it), ITEM_data(old_it), old_it->nbytes);
               memcpy(ITEM_data(new_it) + old_it->nbytes - 2 /* CRLF */, ITEM_data(it), it->nbytes);
            } else {
               /* NREAD_PREPEND */
               memcpy(ITEM_data(new_it), ITEM_data(it), it->nbytes);
               memcpy(ITEM_data(new_it) + it->nbytes - 2 /* CRLF */, ITEM_data(old_it), old_it->nbytes);
            }

            it = new_it;
         }
      }

      if (stored == ENGINE_NOT_STORED) {
         /* "set" commands can override the delete lock
            window... in which case we have to find the old hidden item
            that's in the namespace/LRU but wasn't returned by
            item_get.... because we need to replace it */
         if (delete_locked)
            old_it = do_item_get_nocheck(engine, key, it->nkey);

         if (old_it != NULL)
            do_item_replace(engine, old_it, it);
         else
            do_item_link(engine, it);

         stored = ENGINE_SUCCESS;
      }
   }

   if (old_it != NULL) {
      do_item_remove(engine, old_it);         /* release our reference */
   }
   if (new_it != NULL) {
      do_item_remove(engine, new_it);
   }

   return stored;
}

static ENGINE_ERROR_CODE slabber_store(struct engine_handle* handle,
                                       item* item, enum operation operation) {
   int ret;
   struct slabber_engine* engine = get_handle(handle);

   pthread_mutex_lock(&engine->cache_lock);
   ret = do_store_item(engine, item, operation);
   pthread_mutex_unlock(&engine->cache_lock);
   return ret;
}

/*
 * Flushes expired items after a flush_all call
 */
static void slabber_flush(struct engine_handle* handle, time_t when) {
   struct slabber_engine* engine = get_handle(handle);

   pthread_mutex_lock(&engine->cache_lock);
   do_item_flush_expired(engine);
   pthread_mutex_unlock(&engine->cache_lock);
}

static item* slabber_get(struct engine_handle* handle, const void* key,
                         const int nkey) {
   struct slabber_engine* engine = get_handle(handle);
   item *it;
   pthread_mutex_lock(&engine->cache_lock);
   it = do_item_get_notedeleted(engine, key, nkey, 0);
   pthread_mutex_unlock(&engine->cache_lock);
   return it;
}

/*
 * Allocates a new item.
 */
static item* slabber_item_allocate(struct engine_handle* handle, const void* key,
                                   const size_t nkey, const int flags,
                                   const rel_time_t exptime,
                                   const int nbytes) {
   struct slabber_engine* engine = get_handle(handle);
   item *it;
   pthread_mutex_lock(&engine->cache_lock);
   it = do_item_alloc(engine, key, nkey, flags, exptime, nbytes);
   pthread_mutex_unlock(&engine->cache_lock);
   return it;
}

static ENGINE_ERROR_CODE do_arithmetic(struct slabber_engine* engine, item *it,
                                       const bool incr, const uint64_t delta,
                                       uint64_t *cas, uint64_t *result) {
   *result = strtoull(ITEM_data(it), NULL, 10);
   if (errno == ERANGE) {
      return ENGINE_EINVAL;
   }

   if (incr) {
      *result += delta;
   } else {
      *result -= delta;
      if ((int64_t)*result < 0) {
         *result = 0;
      }
   }

   char buf[48];
   int len = snprintf(buf, sizeof(buf), "%llu", *result);

   if (len + 2 > it->nbytes) { /* need to realloc */
      item *new_it;
      new_it = do_item_alloc(engine, ITEM_key(it), it->nkey,
                             atoi(ITEM_suffix(it) + 1),
                             it->exptime, len + 2);

      if (new_it == 0) {
         return ENGINE_ENOMEM;
      }

      memcpy(ITEM_data(new_it), buf, len);
      memcpy(ITEM_data(new_it) + len, "\r\n", 3);
      do_item_replace(engine, it, new_it);
      do_item_remove(engine, new_it);       /* release our reference */
      *cas = new_it->cas_id;
   } else { /* replace in-place */
      memcpy(ITEM_data(it), buf, len);
      memset(ITEM_data(it) + len, ' ', it->nbytes - len - 2);

      /** TROND: HMM... The cas should be updated??? */
      *cas = it->cas_id;
   }

   return ENGINE_SUCCESS;
}

ENGINE_ERROR_CODE slabber_arithmetic(struct engine_handle* handle,
                                   const void* key,
                                   const int nkey,
                                   const bool increment,
                                   const bool create,
                                   const uint64_t delta,
                                   const uint64_t initial,
                                   const rel_time_t exptime,
                                   uint64_t *cas,
                                   uint64_t *result) {

   struct slabber_engine* engine = get_handle(handle);
   ENGINE_ERROR_CODE ret = ENGINE_SUCCESS;
   item *it;

   pthread_mutex_lock(&engine->cache_lock);
   it = do_item_get_notedeleted(engine, key, nkey, NULL);

   if (it != NULL) {
      if (*cas == 0 || *cas == it->cas_id) {
         /* Weird magic in add_delta forces me to pad here */
         ret = do_arithmetic(engine, it, increment, delta, cas, result);
      } else {
         /* Incorrect CAS */
         ret = ENGINE_KEY_EEXISTS;
      }
   } else if (create) {
      /*
      ** Save some room for the response 24 should be enough for the uint64_t as
      ** a string
      */
      it = do_item_alloc(engine, key, nkey, 0, exptime, 24);

      if (it != NULL) {
         snprintf(ITEM_data(it), it->nbytes, "%llu", initial);
         *result = initial;

         if (do_store_item(engine, it, NREAD_SET)) {
            *cas = it->cas_id;
         } else {
            ret = ENGINE_NOT_STORED;
         }
      } else {
         ret = ENGINE_ENOMEM;
      }
   } else {
      /* NOT FOUND */
      ret = ENGINE_KEY_ENOENT;
   }

   if (it != NULL) {
      /* release our reference */
      do_item_remove(engine, it);
   }

   pthread_mutex_unlock(&engine->cache_lock);
   return ret;
}

/* ----------------- Maintenance thread -------------------- */

/*
 * Walks through the list of deletes that have been deferred because the items
 * were locked down at the tmie.
 */
static void run_deferred_deletes(struct slabber_engine *engine)
{
    int i, j = 0;
    pthread_mutex_lock(&engine->cache_lock);
    int len = engine->delcurr;

    for (i = 0; i < len; i++) {
        item *it = engine->todelete[i];
        if (item_delete_lock_over(it)) {
            assert(it->refcount > 0);
            it->it_flags &= ~ITEM_DELETED;
            do_item_unlink(engine, it);
            do_item_remove(engine, it);
        } else {
            engine->todelete[j++] = it;
        }
    }
    engine->delcurr = j;
    pthread_mutex_unlock(&engine->cache_lock);
}

static void *clock_thread_main(void *arg) {
    struct slabber_engine *engine = arg;
    assert(pthread_mutex_lock(&engine->maintenance_mutex) != -1);

    while (engine->do_run_maintenance) {
        if (engine->assoc_expanding) {
           int ii;

           /*
           ** bulk-move 1000 items at a time
           */
           pthread_mutex_lock(&engine->cache_lock);
           for (ii = 0; ii < 1000; ++ii) {
              assoc_move_next_bucket(engine);
           }
           pthread_mutex_unlock(&engine->cache_lock);
        } else {
           struct timeval timer;
           struct timespec tmo;

           gettimeofday(&timer, NULL);

           run_deferred_deletes(engine);

           tmo.tv_sec = timer.tv_sec + 5;
           tmo.tv_nsec = 0;

           pthread_cond_timedwait(&engine->maintenance_cond,
                                  &engine->maintenance_mutex,
                                  &tmo);
           /* I don't care about spurious wakeups */
        }
    }

    assert(pthread_mutex_unlock(&engine->maintenance_mutex) != -1);
}

static int start_maintenance_thread(struct slabber_engine *engine) {
   pthread_t thread;
   pthread_attr_t attr;
   int ret;

   pthread_attr_init(&attr);
   pthread_attr_setstacksize(&attr, 262144);
   engine->do_run_maintenance = 1;
    if ((ret = pthread_create(&engine->maintenance_tid, &attr,
                              clock_thread_main, engine)) != 0) {
       fprintf(stderr, "Can't create thread: %s\n",
               strerror(ret));
       return -1;
    }

    return 0;
}

static void stop_maintenance_thread(struct slabber_engine *engine) {
   void *ret;
   assert(pthread_mutex_lock(&engine->maintenance_mutex) != -1);
   engine->do_run_maintenance = 0;
   pthread_cond_signal(&engine->maintenance_cond);
   assert(pthread_mutex_unlock(&engine->maintenance_mutex) != -1);

   /* Join the other thread! */
   pthread_join(engine->maintenance_tid, &ret);
}

/******************************* SLAB ALLOCATOR ******************************/

void *slabs_alloc(struct slabber_engine *engine, size_t size, unsigned int id) {
    void *ret;

    pthread_mutex_lock(&engine->slabs_lock);
    ret = do_slabs_alloc(size, id);
    pthread_mutex_unlock(&engine->slabs_lock);
    return ret;
}

void slabs_free(struct slabber_engine *engine, void *ptr, size_t size, unsigned int id){
    pthread_mutex_lock(&engine->slabs_lock);
    do_slabs_free(ptr, size, id);
    pthread_mutex_unlock(&engine->slabs_lock);
}
