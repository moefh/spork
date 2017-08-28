/* pp_token_list.h */

#ifndef PP_TOKEN_LIST_H_FILE
#define PP_TOKEN_LIST_H_FILE

#define NUM_TOKENS_PER_LIST_NODE  16

#include "internal.h"
#include "pp_token.h"

struct sp_mem_pool;

struct sp_pp_token_list_node {
  struct sp_pp_token_list_node *next;
  int size;
  struct sp_pp_token tokens[NUM_TOKENS_PER_LIST_NODE];
};

struct sp_pp_token_list {
  struct sp_mem_pool *pool;
  struct sp_pp_token_list_node *node_list;
  struct sp_pp_token_list_node *w_node;
  struct sp_pp_token_list_node *r_node;
  int r_index;
};

void sp_init_pp_token_list(struct sp_pp_token_list *tl, struct sp_mem_pool *pool);
int sp_pp_token_list_size(struct sp_pp_token_list *tl);
int sp_append_pp_token(struct sp_pp_token_list *tl, struct sp_pp_token *tok);
struct sp_pp_token *sp_rewind_pp_token_list(struct sp_pp_token_list *tl);
bool sp_read_pp_token_from_list(struct sp_pp_token_list *tl, struct sp_pp_token **ret);
struct sp_pp_token *sp_peek_pp_token_from_list(struct sp_pp_token_list *tl);

#endif /* PP_TOKEN_LIST_H_FILE */
