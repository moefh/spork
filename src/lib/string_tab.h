/* string_tab.h */

#ifndef STRING_TAB_H_FILE
#define STRING_TAB_H_FILE

struct sp_string_table {
  struct sp_mem_pool *pool;
  int num;
  int cap;
  char **entries;
};

typedef int sp_string_id;

void sp_init_string_table(struct sp_string_table *s, struct sp_mem_pool *pool);
void sp_destroy_string_table(struct sp_string_table *s);
sp_string_id sp_add_string(struct sp_string_table *s, const char *string);
sp_string_id sp_lookup_string(struct sp_string_table *s, const char *string);
const char *sp_get_string(struct sp_string_table *s, sp_string_id id);

#endif /* STRING_TAB_H_FILE */
