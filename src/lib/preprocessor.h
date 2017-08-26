/* preprocessor.h */

#ifndef PREPROCESSOR_H_FILE
#define PREPROCESSOR_H_FILE

#include "internal.h"
#include "input.h"
#include "ast.h"
#include "token.h"
#include "buffer.h"
#include "hashtable.h"

struct sp_preprocessor {
  struct sp_program *prog;
  struct sp_ast *ast;
  struct sp_buffer *tmp;
  struct sp_input *in;

  struct sp_hashtable macros;
  bool at_newline;
  struct sp_token tok;
  struct sp_src_loc cur_loc;
  struct sp_src_loc last_err_loc;
  struct sp_src_loc saved_loc;
};

void sp_init_preprocessor(struct sp_preprocessor *pp, struct sp_program *prog, struct sp_ast *ast, struct sp_input *in, struct sp_buffer *tmp_buf);
int sp_read_token(struct sp_preprocessor *pp, struct sp_token *tok);
void sp_dump_macros(struct sp_preprocessor *pp);

#endif /* PREPROCESSOR_H_FILE */
