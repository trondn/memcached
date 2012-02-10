

#ifndef ALLOCATOR_STATS_H
#define ALLOCATOR_STATS_H

#include "config.h"
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_LIBTCMALLOC)
#include <google/malloc_extension_c.h>
#else
#error Make some default malloc stats
#endif

typedef struct alloc_stat {
    char* key;
    size_t value;
} alloc_stat;

void get_allocator_stats(alloc_stat*, int*);


#endif

