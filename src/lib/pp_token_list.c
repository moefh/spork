/* pp_token_list.c */

#include <stdio.h>

#include "internal.h"
#include "pp_token_list.h"
#include "mem_pool.h"
#include "pp_token.h"

struct sp_pp_token_list *sp_new_pp_token_list(struct sp_mem_pool *pool, int page_size)
{
  struct sp_pp_token_list *tl = sp_malloc(pool, sizeof(struct sp_pp_token_list));
  if (! tl)
    return NULL;
  sp_init_pp_token_list(tl, pool, page_size);
  return tl;
}

void sp_init_pp_token_list(struct sp_pp_token_list *tl, struct sp_mem_pool *pool, int page_size)
{
  tl->next = NULL;
  tl->pool = pool;
  tl->page_list = NULL;
  tl->last_page = NULL;
  tl->r_page = NULL;
  tl->r_index = 0;
  tl->page_size = page_size;
}

int sp_append_pp_token(struct sp_pp_token_list *tl, struct sp_pp_token *tok)
{
  if (! tl->last_page || tl->last_page->size == tl->page_size) {
    struct sp_pp_token_list_page *node = sp_malloc(tl->pool, (sizeof(struct sp_pp_token_list_page)
                                                              + tl->page_size*sizeof(struct sp_pp_token)));
    if (! node)
      return -1;
    node->next = NULL;
    node->size = 0;

    if (! tl->last_page)
      tl->page_list = node;
    else
      tl->last_page->next = node;
    tl->last_page = node;
  }

  tl->last_page->tokens[tl->last_page->size++] = *tok;
  return 0;
}

bool sp_pp_token_lists_are_equal(struct sp_pp_token_list *l1, struct sp_pp_token_list *l2)
{
  struct sp_pp_token *t1 = sp_rewind_pp_token_list(l1);
  struct sp_pp_token *t2 = sp_rewind_pp_token_list(l2);
  while (true) {
    bool ok1 = sp_read_pp_token_from_list(l1, &t1);
    bool ok2 = sp_read_pp_token_from_list(l2, &t2);
    if (ok1 != ok2)
      return false;
    if (! ok1)
      break;
    if (! sp_pp_tokens_are_equal(t1, t2))
      return false;
  }
  return true;
}

struct sp_pp_token *sp_rewind_pp_token_list(struct sp_pp_token_list *tl)
{
  //printf("rewind list to node %p\n", (void *) tl->page_list);
  tl->r_page = tl->page_list;
  tl->r_index = 0;
  return sp_peek_pp_token_from_list(tl);
}

int sp_pp_token_list_size(struct sp_pp_token_list *tl)
{
  int size = 0;
  for (struct sp_pp_token_list_page *page = tl->page_list; page != NULL; page = page->next)
    size += page->size;
  return size;
}

bool sp_read_pp_token_from_list(struct sp_pp_token_list *tl, struct sp_pp_token **ret)
{
  if (! tl->r_page)
    return false;

  *ret = &tl->r_page->tokens[tl->r_index++];
  if (tl->r_index >= tl->r_page->size) {
    tl->r_page = tl->r_page->next;
    tl->r_index = 0;
  }
  return true;
}

bool sp_read_nonblank_pp_token_from_list(struct sp_pp_token_list *tl, struct sp_pp_token **ret)
{
  do {
    if (! sp_read_pp_token_from_list(tl, ret))
      return false;
  } while (pp_tok_is_space(*ret) || pp_tok_is_newline(*ret));
  return true;
}

struct sp_pp_token *sp_peek_pp_token_from_list(struct sp_pp_token_list *tl)
{
  if (! tl->r_page)
    return NULL;
  return &tl->r_page->tokens[tl->r_index];
}

struct sp_pp_token *sp_peek_nonblank_pp_token_from_list(struct sp_pp_token_list *tl)
{
  struct sp_pp_token_list_pos pos = sp_get_pp_token_list_pos(tl);

  struct sp_pp_token *tok = NULL;
  do {
    if (! sp_read_pp_token_from_list(tl, &tok))
      break;
  } while (pp_tok_is_space(tok) || pp_tok_is_newline(tok));

  sp_set_pp_token_list_pos(tl, pos);
  return tok;
}

