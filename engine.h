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
#ifndef ENGINE_H
#define ENGINE_H

#include <sys/types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

   typedef enum {
      ENGINE_SUCCESS = 0x00,
      ENGINE_KEY_ENOENT = 0x01,
      ENGINE_KEY_EEXISTS = 0x02,
      ENGINE_ENOMEM = 0x03,
      ENGINE_NOT_STORED = 0x04,
      ENGINE_EINVAL = 0x05,
      ENGINE_ENOTSUP = 0x06,
   } ENGINE_ERROR_CODE;

   typedef struct engine_handle {
      /**
       * In order to allow future modifications to the interface, we need to
       * store the interface version in the structure to allow the core server
       * to work with multiple versions of the interface at the same time.
       */
      uint32_t interface_level;

      /**
       * Get information about this storage engine (to use in the version
       * command)
       *
       * @param handle Pointer to the instance.
       * @return A preformatted string with the name and version of the engine
       *         (NOTE: Do NOT try to release this pointer).
       */
      const char* (*get_info)(struct engine_handle* handle);

      /**
       * Initialize the instance.
       * @param handle Pointer to the instance
       * @param config_str pointer to a string containing configuration data
       * @return 0 success, TODO specify different error codes..
       */
      ENGINE_ERROR_CODE (*initialize)(struct engine_handle* handle,
                                      const char* config_str);

      /**
       * Destroy the instance.
       *
       * @param handle Pointer to the instance
       */
      void (*destroy)(struct engine_handle* handle);

      /**
       * Test to see if this item size if allowed by the engine
       *
       * @param handle pointer to the instance
       * @param nkey Length of the key
       * @param flags The flags for the object
       * @param nbytes Size of userdata
       * @return true if ok, false otherwise
       */
      bool (*item_size_ok)(struct engine_handle* handle, const size_t nkey,
                           const int flags, const size_t nbytes);


      /**
       * Allocate an item structure
       *
       * @param handle Pointer to the instance
       * @param key Pointer to the key
       * @param nkey Length of key
       * @param flags The flags for the object
       * @param exptime expiry time
       * @param nbytes Total size of userdata
       * @return pointer to an item or null if out of memory
       */
      item* (*item_allocate)(struct engine_handle* handle, const void* key,
                             const size_t nkey, const int flags,
                             const rel_time_t exptime, const int nbytes);

      /**
       * Delete an item. Note that this function will release your
       * handle to the item)
       *
       * @param handle Pointer to the instance
       * @param item Pointer to the item to be deleted
       * @param exptime When the item should be deleted
       */
      ENGINE_ERROR_CODE (*item_delete)(struct engine_handle* handle,
                                       item* item,
                                       const rel_time_t exptime);

      /**
       * Release the the "refcount" to an object (so that the engine may modify
       * the object)
       *
       * @param handle Pointer to the instance
       * @param item Pointer to the item to be released
       */
      void (*item_release)(struct engine_handle* handle, item* item);

      /**
       * Get an object from the storage.
       * @param handle Pointer to the instance
       * @param key Pointer to the key
       * @param nkey Number of bytes in the key
       * @return Pointer to the object if found, null otherwise
       */
      item* (*get)(struct engine_handle* handle, const void* key,
                   const int nkey);

      /**
       * Get an object from the storage.
       *
       * @param handle Pointer to the instance
       * @param key Pointer to the key
       * @param nkey Number of bytes in the key
       * @param delete_locked Is this set to returned as true if the item is
       *                      locked by a delete
       * @return Pointer to the object if found, null otherwise
       */
      item* (*get_not_deleted)(struct engine_handle* handle, const void* key,
                               const int nkey, bool* delete_locked);

      /**
       * Get statistics from the engine
       *
       * @param handle Pointer to the instance
       * @param what_to_fetch The statistics information to get
       * @return a pointer to a string with the data (or null if not supported).
       *         Caller must free.
       */
      char* (*get_stats)(struct engine_handle* handle,
                         const char* what_to_fetch);

      /**
       * Store an item in the cache.
       *
       * @param handle Pointer to the instance
       * @param item the item to store in the cache
       * @param operation the operation to do with the item (add, set, replace)
       * @return an error code specifying the result of the operation.
       */
      ENGINE_ERROR_CODE (*store)(struct engine_handle* handle, item* item,
                                 enum operation operation);

      /**
       * Arithmetic operation on the value for a key (incr or decr)
       *
       * @param handle Pointer to the instance
       * @param key Pointer to the key
       * @param nkey Length of the key
       * @param increment true if increment, false otherwise
       * @param create true if the item should be created if nonexisting
       * @param delta the amount to increment
       * @param initial Initial value (if key don't exist)
       * @param exptime When the key should expire
       * @param cas The requested CAS on entry, the resulting CAS on return
       * @param result The result after the operation (return)
       * @return an error code specifying the result of the operation.
       */
      ENGINE_ERROR_CODE (*arithmetic)(struct engine_handle* handle,
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
       * Flush the cache!
       *
       * @param handle Pointer to the instance
       * @param when When to flush the cache (see the protocol spec)
       */
      void (*flush)(struct engine_handle* handle, time_t when);

      /**
       * Set the LRU time for an item
       *
       * @param handle Pointer to the instance
       * @param item The item to set the LRU item on
       * @param newtime The new time for the object
       */
      void (*update_lru_time)(struct engine_handle* handle, item *item,
                              const rel_time_t newtime);
   } ENGINE_HANDLE;

   /**
    * The signature for the "create_instance" function exported from the module
    */
   typedef ENGINE_HANDLE* (*CREATE_INSTANCE)(int version,
                                             ENGINE_ERROR_CODE* error);

#ifdef __cplusplus
}
#endif

#endif
