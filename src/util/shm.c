#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "internal.h"
#include "util/shm.h"
#include "util/spinlock.h"
#include "util/system.h"

struct shm {
    char *addr;
    size_t size, offset;
    spinlock_t lock; /* lock while allocating shared memory */
};

static struct shm *shm_object;

/* Allocate without reclaiming */
void *shm_alloc(size_t size_bytes)
{
    size_bytes = ROUND_UP(size_bytes, MEM_ALIGN);

    spin_lock(&shm_object->lock);
    if (unlikely(shm_object->offset + size_bytes > shm_object->size)) {
        spin_unlock(&shm_object->lock);
        return NULL;
    }

    char *addr = shm_object->addr + shm_object->offset;
    shm_object->offset += size_bytes;
    spin_unlock(&shm_object->lock);

    return addr;
}

/* unit: page size */
void *shm_pages_alloc(unsigned int pg_count)
{
    void *addr = mmap(NULL, pg_count * get_page_size(), PROT_READ | PROT_WRITE,
                      MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    return (addr == MAP_FAILED) ? NULL : addr;
}

void shm_pages_free(void *addr, unsigned int pg_count)
{
    munmap(addr, pg_count * get_page_size());
}

void shm_init()
{
    char *addr = shm_pages_alloc(1);
    if (!addr) {
        printf("Failed to initialize system shared memory\n");
        exit(0);
    }

    shm_object = (struct shm *) addr;
    shm_object->addr = addr;
    shm_object->size = get_page_size();
    shm_object->offset = ROUND_UP(sizeof(struct shm), MEM_ALIGN);
    spin_lock_init(&shm_object->lock);
}
