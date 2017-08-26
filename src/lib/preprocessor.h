/* preprocessor.h */

#ifndef PREPROCESSOR_H_FILE
#define PREPROCESSOR_H_FILE

#include "internal.h"
#include "input.h"
#include "ast.h"
#include "token.h"
#include "token_list.h"
#include "buffer.h"
#include "hashtable.h"

struct sp_macro_arg {
  struct sp_macro_arg_list *next;
  struct sp_token *arg;
};

struct sp_macro_def {
  sp_string_id name_id;
  bool is_function;
  bool enabled;
  struct sp_token_list params;
  struct sp_token_list body;
};

struct sp_preprocessor {
  struct sp_program *prog;
  struct sp_ast *ast;
  struct sp_input *in;

  struct sp_mem_pool *pool;
  struct sp_mem_pool macro_exp_pool;
  
  struct sp_hashtable macros;
  struct sp_buffer tmp_buf;

  struct sp_token_list macro_expansion;

  bool at_newline;
  struct sp_token tok;
  struct sp_src_loc loc;
};

void sp_init_preprocessor(struct sp_preprocessor *pp, struct sp_program *prog, struct sp_mem_pool *pool);
void sp_destroy_preprocessor(struct sp_preprocessor *pp);
void sp_set_preprocessor_io(struct sp_preprocessor *pp, struct sp_input *in, struct sp_ast *ast);
int sp_read_token(struct sp_preprocessor *pp, struct sp_token *tok);
void sp_dump_macros(struct sp_preprocessor *pp);

#endif /* PREPROCESSOR_H_FILE */
