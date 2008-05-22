/* slabs memory allocation */

/**
 * Initialize the slab subsystem.
 *
 * @param limit The number of bytes to allocate.
 * @param factor The growth factor. Each slab will use a chunk size equal
 *               to the previous slab's chunk size times this factor.
 * @param prealloc specifies if the slab allocator should allocate all memory
 *                 up front (if true), or allocate memory in chunks as it is
 *                 needed (if false).
 */
void slabs_init(const size_t limit, const double factor, const bool prealloc);


/**
 * Given object size, return id to use when allocating/freeing memory for
 * object.
 *
 * 0 means error: can't store such a large object
 */
unsigned int slabs_clsid(const size_t size);

/**
 * Allocate a memory area of a given size in the specified class.
 * You should call this function if you don't hold the slab mutex
 *
 * @param engine handle to the slabber engine
 * @param size the size to allocate
 * @param id the slab class to allocate in
 * @return pointer to allocated area or NULL when there is no more space
 */
void *slabs_alloc(struct slabber_engine *engine, size_t size, unsigned int id);

/**
 * Allocate a memory area of a given size in the specified class.
 *
 * @param size the size to allocate
 * @param id the slab class to allocate in
 * @return pointer to allocated area or NULL when there is no more space
 */
void *do_slabs_alloc(const size_t size, unsigned int id);

/**
 * Free previously allocated object.
 * You should call this function if you don't hold the slab mutex
 *
 * @param engine handle to the slabber engine.
 * @param ptr pointer to the object
 * @param size size of the object
 * @param id slab class for the object
 */
void slabs_free(struct slabber_engine *engine, void *ptr, size_t size, unsigned int id);

/**
 * Free previously allocated object.
 *
 * @param ptr pointer to the object
 * @param size size of the object
 * @param id slab class for the object
 */
void do_slabs_free(void *ptr, size_t size, unsigned int id);

/**
 * Get statistics from the slab subsystem
 *
 * @return a null-terminated string containing statistics information. Caller
 *         must free this string to avoid memory leakage.
 */
char* do_slabs_stats(void);


#ifdef ALLOW_SLABS_REASSIGN
/**
 * Request some slab be moved between classes.
 *
 * @param srcid slab class to move from
 * @param dstid slab class to move to
 * @return 1 success, 0 fail, -1 tried, but busy (you should try again)
 */
int do_slabs_reassign(unsigned char srcid, unsigned char dstid);
#endif
