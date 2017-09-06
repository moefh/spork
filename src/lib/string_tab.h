/* string_tab.h */

#ifndef STRING_TAB_H_FILE
#define STRING_TAB_H_FILE

#include "hashtable.h"

typedef int sp_string_id;

struct sp_string_table_entry {
  char *str;
  size_t len;
  sp_string_id id;
};

struct sp_string_table {
  struct sp_mem_pool *pool;
  sp_string_id num;
  sp_string_id cap;
  struct sp_string_table_entry *entries;
  struct sp_hashtable string_to_id;
};

void sp_init_string_table(struct sp_string_table *s, struct sp_mem_pool *pool);
void sp_destroy_string_table(struct sp_string_table *s);
sp_string_id sp_add_string(struct sp_string_table *s, const char *string);
sp_string_id sp_lookup_string(struct sp_string_table *s, const char *string);
const char *sp_get_string(struct sp_string_table *s, sp_string_id id);

#endif /* STRING_TAB_H_FILE */
