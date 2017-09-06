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
  sp_init_ht(&s->string_to_id, pool);
}

void sp_destroy_string_table(struct sp_string_table *s)
{
  for (sp_string_id i = 0; i < s->num; i++)
    sp_free(s->pool, s->entries[i].str);
  sp_free(s->pool, s->entries);
  sp_destroy_ht(&s->string_to_id);
}

static void update_hashtable(struct sp_string_table *s)
{
  for (sp_string_id i = 0; i < s->num; i++)
    sp_add_ht_entry(&s->string_to_id, s->entries[i].str, s->entries[i].len, &s->entries[i].id);
}

sp_string_id sp_add_string(struct sp_string_table *s, const char *str)
{
  sp_string_id cur = sp_lookup_string(s, str);
  if (cur >= 0)
    return cur;

  if (s->num == s->cap) {
    sp_string_id new_cap = (s->cap + GROW_SIZE) / GROW_SIZE * GROW_SIZE;
    void *new_entries = sp_realloc(s->pool, s->entries, new_cap * sizeof(s->entries[0]));
    if (new_entries == NULL)
      return -1;
    s->entries = new_entries;
    s->cap = new_cap;
    update_hashtable(s);
  }

  s->entries[s->num].id = s->num;
  s->entries[s->num].len = strlen(str) + 1;
  s->entries[s->num].str = sp_malloc(s->pool, s->entries[s->num].len);
  if (! s->entries[s->num].str)
    return -1;
  memcpy(s->entries[s->num].str, str, s->entries[s->num].len);
  if (sp_add_ht_entry(&s->string_to_id, s->entries[s->num].str, s->entries[s->num].len, &s->entries[s->num].id) < 0)
    return -1;
  return s->num++;
}

sp_string_id sp_lookup_string(struct sp_string_table *s, const char *str)
{
#if 0
  for (sp_string_id i = 0; i < s->num; i++) {
    if (strcmp(str, s->entries[i].str) == 0)
      return i;
  }
#else
  sp_string_id *p_id = sp_get_ht_value(&s->string_to_id, str, strlen(str)+1);
  if (! p_id)
    return -1;
  return *p_id;
#endif
  return -1;
}

const char *sp_get_string(struct sp_string_table *s, sp_string_id id)
{
  if (id >= 0 && id < s->num)
    return s->entries[id].str;
  return NULL;
}
