

#include <stdlib.h>
#include "malloc_interposer.h"

static const interpose_t interposers[] \
    __attribute__ ((section("__DATA, __interpose"))) = {
        { &mc_malloc, &malloc },
};

void* mc_malloc(size_t size) {
    void *ret = malloc(size);
    total_mem += size;
    return ret;
}
