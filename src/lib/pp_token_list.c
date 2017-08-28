/* pp_token_list.c */

#include <stdio.h>

#include "internal.h"
#include "pp_token_list.h"
#include "mem_pool.h"
#include "pp_token.h"

void sp_init_pp_token_list(struct sp_pp_token_list *tl, struct sp_mem_pool *pool)
{
  tl->pool = pool;
  tl->node_list = NULL;
  tl->w_node = NULL;
  tl->r_node = NULL;
  tl->r_index = 0;
}

int sp_append_pp_token(struct sp_pp_token_list *tl, struct sp_pp_token *tok)
{
  if (! tl->w_node || tl->w_node->size == NUM_TOKENS_PER_LIST_NODE) {
    struct sp_pp_token_list_node *node = sp_malloc(tl->pool, sizeof(struct sp_pp_token_list_node));
    if (! node)
      return -1;
    node->next = NULL;
    node->size = 0;

    if (! tl->w_node)
      tl->node_list = node;
    else
      tl->w_node->next = node;
    tl->w_node = node;
  }

  tl->w_node->tokens[tl->w_node->size++] = *tok;
  return 0;
}

struct sp_pp_token *sp_rewind_pp_token_list(struct sp_pp_token_list *tl)
{
  //printf("rewind list to node %p\n", (void *) tl->node_list);
  tl->r_node = tl->node_list;
  tl->r_index = 0;
  return sp_peek_pp_token_from_list(tl);
}

int sp_pp_token_list_size(struct sp_pp_token_list *tl)
{
  int size = 0;
  for (struct sp_pp_token_list_node *node = tl->node_list; node != NULL; node = node->next)
    size += node->size;
  return size;
}

bool sp_read_pp_token_from_list(struct sp_pp_token_list *tl, struct sp_pp_token **ret)
{
  if (! tl->r_node)
    return false;

  *ret = &tl->r_node->tokens[tl->r_index++];
  if (tl->r_index >= tl->r_node->size) {
    tl->r_node = tl->r_node->next;
    tl->r_index = 0;
  }
  return true;
}

struct sp_pp_token *sp_peek_pp_token_from_list(struct sp_pp_token_list *tl)
{
  if (! tl->r_node)
    return NULL;
  return &tl->r_node->tokens[tl->r_index];
}
