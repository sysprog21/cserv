#ifndef UTIL_MEMCACHE_H
#define UTIL_MEMCACHE_H

struct memcache {
    void **elements;
    size_t obj_size; /* object size*/
    int cache_size;  /* pool size */
    int curr;        /* current available element count */
};

struct memcache *memcache_create(size_t obj_size, int max_cache_size);
void memcache_destroy(struct memcache *cache);
void *memcache_alloc(struct memcache *cache);
void memcache_free(struct memcache *cache, void *element);

#endif
