
#ifndef MEM_HOOKS_H
#define MEM_HOOKS_H

#include "config.h"
#include <memcached/allocator_hooks.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

bool mc_add_new_hook(void (*hook)(const void* ptr, size_t size));
bool mc_remove_new_hook(void (*hook)(const void* ptr, size_t size));
bool mc_add_delete_hook(void (*hook)(const void* ptr));
bool mc_remove_delete_hook(void (*hook)(const void* ptr));
void mc_get_allocator_stats(allocator_stat stats[]);

#endif /* MEM_HOOKS_H */