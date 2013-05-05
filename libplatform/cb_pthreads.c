#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "platform_internal.h"

struct thread_execute {
    cb_thread_main_func func;
    void *argument;
};

static void* platform_thread_wrap(void *arg) {
    assert(arg);
    struct thread_execute *ctx = arg;
    ctx->func(ctx->argument);
    free(ctx);
    return NULL;
}

int cb_create_thread(cb_thread_t *id,
                     cb_thread_main_func func,
                     void *arg,
                     int detached)
{
    int ret;
    struct thread_execute *ctx = malloc(sizeof(struct thread_execute));
    if (ctx == NULL) {
        return -1;
    }

    ctx->func = func;
    ctx->argument = arg;

    if (detached) {
        pthread_attr_t attr;

        if (pthread_attr_init(&attr) != 0 ||
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
            return -1;
        }

        ret = pthread_create(id, &attr, platform_thread_wrap, ctx);
    } else {
        ret = pthread_create(id, NULL, platform_thread_wrap, ctx);
    }

	if (ret != 0) {
		free(ctx);
	}

    return ret;
}

int cb_join_thread(cb_thread_t id)
{
    return pthread_join(id, NULL);
}

cb_thread_t cb_thread_self(void)
{
    return pthread_self();
}

void cb_mutex_initialize(cb_mutex_t*mutex)
{
    pthread_mutex_init(mutex, NULL);
}

void cb_mutex_destroy(cb_mutex_t*mutex)
{
    pthread_mutex_destroy(mutex);
}

void cb_mutex_enter(cb_mutex_t*mutex)
{
    int rv = pthread_mutex_lock(mutex);
    if (rv != 0) {
        fprintf(stderr, "FATAL: Failed to lock mutex: %d %s",
                rv, strerror(rv));
        abort();
    }
}

void cb_mutex_exit(cb_mutex_t*mutex)
{
    int rv = pthread_mutex_unlock(mutex);
    if (rv != 0) {
        fprintf(stderr, "FATAL: Failed to release mutex: %d %s",
                rv, strerror(rv));
        abort();
    }
}

void cb_cond_initialize(cb_cond_t*cond) {
    pthread_cond_init(cond, NULL);
}

void cb_cond_destroy(cb_cond_t*cond) {
    pthread_cond_destroy(cond);
}

void cb_cond_wait(cb_cond_t *cond, cb_mutex_t *mutex) {
    pthread_cond_wait(cond, mutex);
}

void cb_cond_signal(cb_cond_t *cond) {
    pthread_cond_signal(cond);
}

void cb_cond_broadcast(cb_cond_t *cond) {
    pthread_cond_broadcast(cond);
}


void cb_cond_timedwait(cb_cond_t *cond, cb_mutex_t *mutex, unsigned int ms)
{
    struct timespec ts;
    struct timeval tp;
    /* @todo TROND FIXME */

    gettimeofday(&tp, NULL);
    memset(&ts, 0, sizeof(ts));
    ts.tv_sec = tp.tv_sec + (ms / 1000);
    ts.tv_nsec = (tp.tv_usec * 1000) + ((ms % 1000) * 1000000);
    pthread_cond_timedwait(cond, mutex, &ts);
}
