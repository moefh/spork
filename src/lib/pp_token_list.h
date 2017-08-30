/* pp_token_list.h */

#ifndef PP_TOKEN_LIST_H_FILE
#define PP_TOKEN_LIST_H_FILE

#include "internal.h"
#include "pp_token.h"

struct sp_mem_pool;

struct sp_pp_token_list_page {
  struct sp_pp_token_list_page *next;
  int size;
  struct sp_pp_token tokens[];
};

struct sp_pp_token_list_pos {
  struct sp_pp_token_list_page *page;
  int index;
};

struct sp_pp_token_list {
  struct sp_pp_token_list *next;
  struct sp_mem_pool *pool;
  struct sp_pp_token_list_page *page_list;
  struct sp_pp_token_list_page *last_page;
  struct sp_pp_token_list_page *r_page;
  int r_index;
  int page_size;
};

struct sp_pp_token_list *sp_new_pp_token_list(struct sp_mem_pool *pool, int tokens_per_page);
void sp_init_pp_token_list(struct sp_pp_token_list *tl, struct sp_mem_pool *pool, int tokens_per_page);
int sp_pp_token_list_size(struct sp_pp_token_list *tl);
int sp_append_pp_token(struct sp_pp_token_list *tl, struct sp_pp_token *tok);
struct sp_pp_token *sp_rewind_pp_token_list(struct sp_pp_token_list *tl);
bool sp_read_pp_token_from_list(struct sp_pp_token_list *tl, struct sp_pp_token **ret);
struct sp_pp_token *sp_peek_pp_token_from_list(struct sp_pp_token_list *tl);
struct sp_pp_token *sp_peek_nonblank_pp_token_from_list(struct sp_pp_token_list *tl);
bool sp_pp_token_lists_are_equal(struct sp_pp_token_list *l1, struct sp_pp_token_list *l2);

#define sp_get_pp_token_list_pos(tl)      ((struct sp_pp_token_list_pos) { .page = (tl)->r_page, .index = (tl)->r_index })
#define sp_set_pp_token_list_pos(tl,pos)  do { (tl)->r_page = (pos).page; (tl)->r_index = (pos).index; } while (0)

#endif /* PP_TOKEN_LIST_H_FILE */
