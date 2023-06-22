#pragma once

void *shm_alloc(size_t size_bytes);
void *shm_pages_alloc(unsigned int pg_count);
void shm_pages_free(void *addr, unsigned int pg_count);
void shm_init();
