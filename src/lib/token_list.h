/* token_list.h */

#ifndef TOKEN_LIST_H_FILE
#define TOKEN_LIST_H_FILE

#define NUM_TOKENS_PER_LIST_NODE  16

#include "internal.h"
#include "ast.h"
#include "token.h"

struct sp_mem_pool;

struct sp_token_list_node {
  struct sp_token_list_node *next;
  int size;
  struct sp_token tokens[NUM_TOKENS_PER_LIST_NODE];
};

struct sp_token_list {
  struct sp_mem_pool *pool;
  struct sp_token_list_node *node_list;
  struct sp_token_list_node *w_node;
  struct sp_token_list_node *r_node;
  int r_index;
};

void sp_init_token_list(struct sp_token_list *tl, struct sp_mem_pool *pool);
int sp_token_list_size(struct sp_token_list *tl);
int sp_append_token(struct sp_token_list *tl, struct sp_token *tok);
struct sp_token *sp_rewind_token_list(struct sp_token_list *tl);
bool sp_read_token_from_list(struct sp_token_list *tl, struct sp_token **ret);
struct sp_token *sp_peek_token_from_list(struct sp_token_list *tl);

#endif /* TOKEN_LIST_H_FILE */
