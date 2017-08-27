/* id_hashtable.h */

#ifndef ID_HASHTABLE_H_FILE
#define ID_HASHTABLE_H_FILE

#include <stdbool.h>
#include "mem_pool.h"
#include "string_tab.h"

struct sp_idht_entry {
  sp_string_id key;
  void *val;
};

struct sp_id_hashtable {
  struct sp_mem_pool *pool;
  struct sp_idht_entry *entries;
  int len;
  int cap;
};

void sp_init_idht(struct sp_id_hashtable *ht, struct sp_mem_pool *pool);
struct sp_id_hashtable *sp_new_idht(struct sp_mem_pool *pool);
void sp_dump_idht(struct sp_id_hashtable *ht);
int sp_alloc_idht_len(struct sp_id_hashtable *ht, int len);

void *sp_get_idht_value  (struct sp_id_hashtable *ht, sp_string_id key);
int  sp_add_idht_entry   (struct sp_id_hashtable *ht, sp_string_id key, void *val);
int  sp_delete_idht_entry(struct sp_id_hashtable *ht, sp_string_id key);
bool sp_next_idht_key    (struct sp_id_hashtable *ht, sp_string_id *key);

#endif /* ID_HASHTABLE_H_FILE */
