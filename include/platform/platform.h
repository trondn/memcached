/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2013 Couchbase, Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
#pragma once
#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <stdint.h>
#ifdef __sun
#include <sys/time.h> /* for hrtime_t */
#endif
#endif

#include <platform/visibility.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
typedef DWORD cb_thread_t;
typedef CRITICAL_SECTION cb_mutex_t;
typedef CONDITION_VARIABLE cb_cond_t;
typedef unsigned __int64 hrtime_t;

#else
typedef pthread_t cb_thread_t;
typedef pthread_mutex_t cb_mutex_t;
typedef pthread_cond_t cb_cond_t;

#ifdef __sun

#else
typedef uint64_t hrtime_t;
#endif

#endif

    /***********************************************************************
     *                     Thread related functions                        *
     **********************************************************************/

    /**
     * Thread function signature.
     */
    typedef void (*cb_thread_main_func)(void *argument);

    /**
     * Create a new thread (in a running state).
     *
     * @param id The thread identifier (returned)
     * @param func The entry point for the newly created thread
     * @param arg Arguments passed to the newly created thread
     * @param detached Set to non-null if the thread should be
     *                 created in a detached state (which you
     *                 can't call cb_join_thread on).
     */
    PLATFORM_PUBLIC_API
    int cb_create_thread(cb_thread_t *id,
                         cb_thread_main_func func,
                         void *arg,
                         int detached);

    /**
     * Wait for a thread to complete
     *
     * @param id The thread identifier to wait for
     */
    PLATFORM_PUBLIC_API
    int cb_join_thread(cb_thread_t id);

    /**
     * Get the id for the running thread
     *
     * @return the id for the running thread
     */
    PLATFORM_PUBLIC_API
    cb_thread_t cb_thread_self(void);

    /***********************************************************************
     *                      Mutex related functions                        *
     **********************************************************************/
    /**
     * Initialize a mutex.
     *
     * We don't have <b>any</b> static initializers, so the mutex <b>must</b>
     * be initialized by calling this function before being used.
     *
     * @param mutex the mutex object to initialize
     */
    PLATFORM_PUBLIC_API
    void cb_mutex_initialize(cb_mutex_t* mutex);

    /**
     * Destroy (and release all allocated resources) a mutex.
     *
     * @param mutex the mutex object to destroy
     */
    PLATFORM_PUBLIC_API
    void cb_mutex_destroy(cb_mutex_t* mutex);

    /**
     * Enter a locked section
     *
     * @param mutex the mutex protecting this section
     */
    PLATFORM_PUBLIC_API
    void cb_mutex_enter(cb_mutex_t*mutex);

    /**
     * Exit a locked section
     *
     * @param mutex the mutex protecting this section
     */
    PLATFORM_PUBLIC_API
    void cb_mutex_exit(cb_mutex_t*mutex);

    /***********************************************************************
     *                 Condition variable related functions                *
     **********************************************************************/
    /**
     * Initialize a condition variable
     * @param cond the condition variable to initialize
     */
    PLATFORM_PUBLIC_API
    void cb_cond_initialize(cb_cond_t*cond);

    /**
     * Destroy and release all allocated resources for a condition variable
     * @param cond the condition variable to destroy
     */
    PLATFORM_PUBLIC_API
    void cb_cond_destroy(cb_cond_t*cond);

    /**
     * Wait for a condition variable to be signaled.
     *
     * The mutex must be in a locked state, and this method will release
     * the mutex and wait for the condition variable to be signaled in an
     * atomic operation.
     *
     * The mutex is locked when the method returns.
     *
     * @param cond the condition variable to wait for
     * @param mutex the locked mutex protecting the critical section
     */
    PLATFORM_PUBLIC_API
    void cb_cond_wait(cb_cond_t *cond, cb_mutex_t *mutex);

    /**
     * Wait for a condition variable to be signaled, but give up after a
     * given time.
     *
     * The mutex must be in a locked state, and this method will release
     * the mutex and wait for the condition variable to be signaled in an
     * atomic operation.
     *
     * The mutex is locked when the method returns.
     *
     * @param cond the condition variable to wait for
     * @param mutex the locked mutex protecting the critical section
     * @param ms the number of milliseconds to wait.
     */
    PLATFORM_PUBLIC_API
    void cb_cond_timedwait(cb_cond_t *cond, cb_mutex_t *mutex, unsigned int ms);

    /**
     * Singal a single thread waiting for a condition variable
     *
     * @param cond the condition variable to signal
     */
    PLATFORM_PUBLIC_API
    void cb_cond_signal(cb_cond_t *cond);

    /**
     * Singal all threads waiting for on condition variable
     *
     * @param cond the condition variable to signal
     */
    PLATFORM_PUBLIC_API
    void cb_cond_broadcast(cb_cond_t *cond);


#ifndef __sun
    /**
     * Get a high resolution time
     *
     * @return number of nanoseconds since some arbitrary time in the past
     */
    PLATFORM_PUBLIC_API
    hrtime_t gethrtime(void);
#endif

    typedef void* cb_dlhandle_t;

    PLATFORM_PUBLIC_API
    cb_dlhandle_t cb_dlopen(const char *library, char **errmsg);

    PLATFORM_PUBLIC_API
    void *cb_dlsym(cb_dlhandle_t handle, const char *symbol, char **errmsg);

    PLATFORM_PUBLIC_API
    void cb_dlclose(cb_dlhandle_t handle);


#ifdef __cplusplus
}
#endif
