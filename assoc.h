/* associative array */
void assoc_init(void);
item *assoc_find(struct slabber_engine* engine, const char *key,
                 const size_t nkey);
int assoc_insert(struct slabber_engine* engine, item *item);
void assoc_delete(struct slabber_engine* engine, const char *key,
                  const size_t nkey);
void assoc_move_next_bucket(struct slabber_engine* engine);
uint32_t hash( const void *key, size_t length, const uint32_t initval);
