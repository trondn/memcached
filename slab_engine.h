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

#include <pthread.h>
#include <stdbool.h>

/**
 * Definition of the private instance data used by the slabber engine.
 *
 * This is currently "work in progress" so it is not as clean as it should be.
 */
struct slabber_engine {
   ENGINE_HANDLE engine;

   /**
    * Is the engine initalized or not
    */
   bool initialized;

   /**
    * The cache layer (item_* and assoc_*) is currently protected by
    * this single mutex
    */
   pthread_mutex_t cache_lock;

   /**
    * Are we in the middle of expanding the assoc table now?
    */
   volatile bool assoc_expanding;


   /**
    * Access to the slab allocator is protected by this lock
    */
   pthread_mutex_t slabs_lock;

   /*
    * The slabber-engine use it's own maintenance thread to perform
    * expansion of the assoc table and deferred deletions.
    */

   /**
    * Access to the maintenance thread is protected by this mutex
    */
   pthread_mutex_t maintenance_mutex;

   /**
    * To activate the maintenance thread, signal this conditional variable
    */
   pthread_cond_t maintenance_cond;

   /**
    * The thread id of the maintenance thread
    */
   pthread_t maintenance_tid;

   /**
    * Set this variable to 0 to terminate the maintenance thread
    */
   volatile int do_run_maintenance;

   /**
    * Array containing the elements in the "deferred delete queue"
    */
   item **todelete;

   /**
    * The next free slot in the deferred delete queue
    */
   int delcurr;

   /**
    * The total number of slots in the deferred delete queue
    */
   int deltotal;

};

#include "slabs.h"
#include "assoc.h"
#include "items.h"

