/* hashtable.c */

#include <string.h>
#include <stdio.h>

#include "hashtable.h"
#include "internal.h"

#define OCCUPIED(e) ((e)->key != NULL)

#define HASH(ht, key, key_len) (sp_hash((key), (key_len)) & ((ht)->cap-1))

static int find_slot(struct sp_hashtable *ht, const void *key, size_t key_len)
{
  //printf("calculating hash of '%s' with len=%zu\n", (char*)key, key_len);
  int i = HASH(ht, key, key_len);
  while (OCCUPIED(&ht->entries[i]) && ((key_len != ht->entries[i].key_len)
                                       || memcmp(key, ht->entries[i].key, key_len) != 0))
    i = (i+1) & (ht->cap-1);
  return i;
}

static int insert(struct sp_hashtable *ht, const void *key, size_t key_len, void *val)
{
  int i = find_slot(ht, key, key_len);
  //printf("inserting key %s in slot %d\n", (char*)key, i);
  if (! OCCUPIED(&ht->entries[i])) {
    ht->entries[i].key = key;
    ht->entries[i].key_len = key_len;
  }
  ht->entries[i].val = val;
  return i;
}

static int rebuild(struct sp_hashtable *ht, int cap)
{
  int old_cap = ht->cap;
  struct sp_ht_entry *old_entries = ht->entries;
  
  ht->entries = sp_malloc(ht->pool, cap * sizeof(struct sp_ht_entry));
  if (! ht->entries) {
    ht->entries = old_entries;
    return -1;
  }
  ht->cap = cap;
  for (int i = 0; i < cap; i++)
    ht->entries[i].key = NULL;

  //printf("rebuilding with cap %u\n", cap);
  for (int i = 0; i < old_cap; i++) {
    if (OCCUPIED(&old_entries[i]))
      insert(ht, old_entries[i].key, old_entries[i].key_len, old_entries[i].val);
  }
  //printf("done rebuilding\n");
  
  sp_free(ht->pool, old_entries);
  return 0;
}

/*
 * Exported functions
 */

void sp_init_ht(struct sp_hashtable *ht, struct sp_mem_pool *pool)
{
  ht->pool = pool;
  ht->cap = 0;
  ht->len = 0;
  ht->entries = NULL;
}

struct sp_hashtable *sp_new_ht(struct sp_mem_pool *pool)
{
  struct sp_hashtable *ht = sp_malloc(pool, sizeof(struct sp_hashtable));
  if (! ht)
    return NULL;
  ht->pool = pool;
  ht->cap = 0;
  ht->len = 0;
  ht->entries = NULL;
  return ht;
}

void sp_dump_ht(struct sp_hashtable *ht)
{
  for (int i = 0; i < ht->cap; i++) {
    struct sp_ht_entry *e = &ht->entries[i];
    printf("[%3u] ", i);
    if (e->key == NULL) {
      printf("--\n");
    } else {
      printf("%p(%zu) -> %p\n", e->key, e->key_len, e->val);
    }
  }
}

void *sp_get_ht_value(struct sp_hashtable *ht, const void *key, size_t key_len)
{
  if (ht->cap == 0)
    return NULL;
  
  int i = find_slot(ht, key, key_len);
  if (! OCCUPIED(&ht->entries[i]))
    return NULL;
  return ht->entries[i].val;
}

int sp_add_ht_entry(struct sp_hashtable *ht, const void *key, size_t key_len, void *val)
{
  if (key == NULL)
    return -1;

  int i = 0;
  if (ht->cap > 0) {
    i = find_slot(ht, key, key_len);
    if (OCCUPIED(&ht->entries[i])) {
      ht->entries[i].val = val;
      return 0;
    }
  }

  if (ht->cap == 0 || ht->len+1 > ht->cap/2) {
    if (rebuild(ht, (ht->cap == 0) ? 8 : 2*ht->cap) < 0)
      return -1;
    i = find_slot(ht, key, key_len);
  }
  ht->len++;
  ht->entries[i].key = key;
  ht->entries[i].key_len = key_len;
  ht->entries[i].val = val;
  return 0;
}

bool sp_next_ht_key(struct sp_hashtable *ht, const void **key, size_t *key_len)
{
  int search_next_i;
  if (*key == NULL || ht->cap == 0) {
    search_next_i = 0;
  } else {
    search_next_i = find_slot(ht, *key, *key_len);
    if (OCCUPIED(&ht->entries[search_next_i]))
      search_next_i++;
  }
  
  for (int i = search_next_i; i < ht->cap; i++) {
    if (OCCUPIED(&ht->entries[i])) {
      *key_len = ht->entries[i].key_len;
      *key = ht->entries[i].key;
      return true;
    }
  }
  return false;
}

int sp_delete_ht_entry(struct sp_hashtable *ht, const void *key, size_t key_len)
{
  if (ht->cap == 0)
    return -1;

  int i = find_slot(ht, key, key_len);
  if (! OCCUPIED(&ht->entries[i]))
    return -1;
  int j = i;
  while (true) {
    ht->entries[i].key = NULL;
  start:
    j = (j+1) & (ht->cap-1);
    if (! OCCUPIED(&ht->entries[j]))
      break;
    int k = HASH(ht, ht->entries[j].key, ht->entries[j].key_len);
    if ((i < j) ? (i<k)&&(k<=j) : (i<k)||(k<=j))
      goto start;
    ht->entries[i] = ht->entries[j];
    i = j;
  }
  ht->len--;
  return 0;
}

int sp_alloc_ht_len(struct sp_hashtable *ht, int len)
{
  // round len up to the nearest power of 2
  len--;
  len |= len >> 1;
  len |= len >> 2;
  len |= len >> 4;
  len |= len >> 8;
  len |= len >> 16;
  len++;

  if (len < ht->len)
    return -1;
  return rebuild(ht, 2*len);
}
