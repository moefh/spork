/* string_tab.c */

#include <string.h>

#include "internal.h"

#define GROW_SIZE 256

void sp_init_string_table(struct sp_string_table *s, struct sp_mem_pool *pool)
{
  s->pool = pool;
  s->num = 0;
  s->cap = 0;
  s->entries = NULL;
}

void sp_destroy_string_table(struct sp_string_table *s)
{
  for (int i = 0; i < s->num; i++)
    sp_free(s->pool, s->entries[i]);
  sp_free(s->pool, s->entries);
}

sp_string_id sp_add_string(struct sp_string_table *s, const char *str)
{
  sp_string_id cur = sp_lookup_string(s, str);
  if (cur >= 0)
    return cur;

  if (s->num == s->cap) {
    int new_cap = (s->cap + GROW_SIZE) / GROW_SIZE * GROW_SIZE;
    void *new_entries = sp_realloc(s->pool, s->entries, new_cap * sizeof(s->entries[0]));
    if (new_entries == NULL)
      return -1;
    s->entries = new_entries;
    s->cap = new_cap;
  }

  size_t len = strlen(str) + 1;
  s->entries[s->num] = sp_malloc(s->pool, len);
  if (! s->entries[s->num])
    return -1;
  memcpy(s->entries[s->num], str, len);
  return s->num++;
}

sp_string_id sp_lookup_string(struct sp_string_table *s, const char *str)
{
  for (sp_string_id i = 0; i < s->num; i++) {
    if (strcmp(str, s->entries[i]) == 0)
      return i;
  }
  return -1;
}

const char *sp_get_string(struct sp_string_table *s, sp_string_id id)
{
  if (id >= 0 && id < s->num)
    return s->entries[id];
  return NULL;
}
