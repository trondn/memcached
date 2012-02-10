
#include "mem_hooks.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(HAVE_LIBTCMALLOC)
#include <google/malloc_extension_c.h>
#include <google/malloc_hook_c.h>
#else
#error You need to write the malloc interposer
#endif

bool mc_add_new_hook(void (*hook)(const void* ptr, size_t size)) {
    return MallocHook_AddNewHook(hook) == 1 ? true : false;
}

bool mc_remove_new_hook(void (*hook)(const void* ptr, size_t size)) {
    return MallocHook_RemoveNewHook(hook);
}

bool mc_add_delete_hook(void (*hook)(const void* ptr)) {
    return MallocHook_AddDeleteHook(hook);
}

bool mc_remove_delete_hook(void (*hook)(const void* ptr)) {
    return MallocHook_RemoveDeleteHook(hook);
}

void mc_get_allocator_stats(allocator_stat stats[]) {
#if defined(HAVE_LIBTCMALLOC)
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

    stats[0].key = strdup("tcmalloc_allocated_bytes");
    stats[0].value = allocated_memory;
    stats[1].key = strdup("tcmalloc_heap_size");
    stats[1].value = heap_size;
    stats[2].key = strdup("tcmalloc_free_bytes");
    stats[2].value = pageheap_free_bytes;
    stats[3].key = strdup("tcmalloc_unmapped_bytes");
    stats[3].value = pageheap_unmapped_bytes;
    stats[4].key = strdup("tcmalloc_max_thread_cache_bytes");
    stats[4].value = max_thread_cache_bytes;
    stats[5].key = strdup("tcmalloc_current_thread_cache_bytes");
    stats[5].value = current_thread_cache_bytes;
#endif
}