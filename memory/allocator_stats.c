
#include "allocator_stats.h"

void get_allocator_stats(alloc_stat *stats, int *length) {
#if defined(HAVE_LIBTCMALLOC)
    *length = 6;
    size_t allocated_memory = 0;
    size_t heap_size = 0;
    size_t pageheap_free_bytes = 0;
    size_t pageheap_unmapped_bytes = 0;
    size_t max_thread_cache_bytes = 0;
    size_t current_thread_cache_bytes = 0;

    MallocExtension_GetNumericProperty("generic.current_allocated_bytes",
                                       &allocated_memory);
    MallocExtension_GetNumericProperty("generic.heap_size",
                                       &heap_size);
    MallocExtension_GetNumericProperty("tcmalloc.pageheap_free_bytes",
                                       &pageheap_free_bytes);
    MallocExtension_GetNumericProperty("tcmalloc.pageheap_unmapped_bytes",
                                       &pageheap_unmapped_bytes);
    MallocExtension_GetNumericProperty("tcmalloc.max_total_thread_cache_bytes",
                                       &max_thread_cache_bytes);
    MallocExtension_GetNumericProperty("tcmalloc.current_total_thread_cache_bytes",
                                       &current_thread_cache_bytes);
/*
    stats = (alloc_stat*)malloc(sizeof(alloc_stat) * 6);
    (*stats).key = strdup("tcmalloc_allocated_bytes");
    (*stats).value = allocated_memory;
    (*(stats + 1)).key = strdup("tcmalloc_heap_size");
    (*(stats + 1)).value = heap_size;
    (*(stats + 2)).key = strdup("tcmalloc_free_bytes");
    (*(stats + 2)).value = pageheap_free_bytes;
    (*(stats + 3)).key = strdup("tcmalloc_unmapped_bytes");
    (*(stats + 3)).value = pageheap_unmapped_bytes;
    (*(stats + 4)).key = strdup("tcmalloc_max_thread_cache_bytes");
    (*(stats + 4)).value = max_thread_cache_bytes;
    (*(stats + 5)).key = strdup("tcmalloc_current_thread_cache_bytes");
    (*(stats + 5)).value = current_thread_cache_bytes;
    */
#else
    *length = 0;
#endif
}