/* symtab.c */

#include <string.h>

#include "internal.h"

#define GROW_SIZE 128

void sp_init_symtab(struct sp_symtab *s, struct sp_mem_pool *pool)
{
  s->pool = pool;
  s->num = 0;
  s->cap = 0;
  s->entries = NULL;
  sp_init_buffer(&s->symbols, pool);
}

void sp_destroy_symtab(struct sp_symtab *s)
{
  if (s->entries)
    sp_free(s->pool, s->entries);
  sp_destroy_buffer(&s->symbols);
}

sp_symbol_id sp_add_symbol(struct sp_symtab *s, const void *symbol)
{
  sp_symbol_id cur = sp_get_symbol_id(s, symbol);
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

  int entry_pos = sp_buf_add_string(&s->symbols, symbol, strlen(symbol));
  if (entry_pos < 0)
    return -1;
  s->entries[s->num] = entry_pos;
  return s->num++;
}

sp_symbol_id sp_get_symbol_id(struct sp_symtab *s, const void *symbol)
{
  for (sp_symbol_id i = 0; i < s->num; i++) {
    if (strcmp(symbol, (char*) s->symbols.p + s->entries[i]) == 0)
      return i;
  }
  return -1;
}

const char *sp_get_symbol_name(struct sp_symtab *s, sp_symbol_id id)
{
  if (id >= 0 && id < s->num)
    return s->symbols.p + s->entries[id];
  return NULL;
}
