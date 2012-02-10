#ifndef MEMCACHED_DEFAULT_ENGINE_H
#define MEMCACHED_DEFAULT_ENGINE_H

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

uint64_t total_mem = 0;

typedef struct interpose_s {
    void* (*new_func)();
    void* (*orig_func)();
} interpose_t;

void* mc_malloc(size_t);

#endif
