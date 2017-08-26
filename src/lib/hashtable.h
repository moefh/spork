/* hashtable.h */

#ifndef HASHTABLE_H_FILE
#define HASHTABLE_H_FILE

#include <stdbool.h>
#include "mem_pool.h"

struct sp_ht_entry {
  const void *key;
  size_t key_len;
  void *val;
};

struct sp_hashtable {
  struct sp_mem_pool *pool;
  struct sp_ht_entry *entries;
  int len;
  int cap;
};

void sp_init_ht(struct sp_hashtable *ht, struct sp_mem_pool *pool);
struct sp_hashtable *sp_new_ht(struct sp_mem_pool *pool);
void sp_dump_ht(struct sp_hashtable *ht);
int sp_alloc_ht_len(struct sp_hashtable *ht, int len);

void *sp_get_ht_value  (struct sp_hashtable *ht, const void *key, size_t key_len);
int  sp_add_ht_entry   (struct sp_hashtable *ht, const void *key, size_t key_len, void *val);
int  sp_delete_ht_entry(struct sp_hashtable *ht, const void *key, size_t key_len);
bool sp_next_ht_key    (struct sp_hashtable *ht, const void **key, size_t *key_len);

#endif /* HASHTABLE_H_FILE */
