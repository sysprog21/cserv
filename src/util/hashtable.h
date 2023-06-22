#pragma once

#include "util/list.h"

/* djb2 hash */
#define HASH(key, c) ((unsigned) key * 31 + c)

struct hash_element {
    struct list_head list;
    unsigned key;
    void *object;
};

struct hash_table {
    unsigned bucket_size;
    struct list_head *buckets;
};

void *hash_table_find(struct hash_table *ht, unsigned key);
void *hash_table_remove(struct hash_table *ht, unsigned key);
int hash_table_add(struct hash_table *ht, unsigned key, void *obj);
struct hash_table *hash_table_create(unsigned bucket_size);
