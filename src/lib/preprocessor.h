/* preprocessor.h */

#ifndef PREPROCESSOR_H_FILE
#define PREPROCESSOR_H_FILE

#include "internal.h"
#include "input.h"
#include "string_tab.h"
#include "pp_token.h"
#include "pp_token_list.h"
#include "buffer.h"
#include "id_hashtable.h"
#include "pp_macro.h"

struct sp_ast;
struct sp_token;

struct sp_preprocessor {
  struct sp_program *prog;
  struct sp_ast *ast;

  struct sp_input *in;
  struct sp_pp_token_list_walker *in_tokens;

  struct sp_mem_pool *pool;
  struct sp_mem_pool macro_exp_pool;
  struct sp_string_table token_strings;
  
  struct sp_buffer tmp_buf;
  struct sp_id_hashtable macros;

  // phase 3:
  struct sp_pp_token tok;
  struct sp_src_loc loc;

  // phase 4:
  int macro_args_reading_level;
  int macro_expansion_level;
  bool last_was_space;
  bool at_newline;

  // phase 6:
  bool init_ph6;
  struct sp_pp_token tok_ph6;
  struct sp_pp_token next_ph6;
  struct sp_buffer tmp_str_buf;
};

void sp_init_preprocessor(struct sp_preprocessor *pp, struct sp_program *prog, struct sp_mem_pool *pool);
void sp_destroy_preprocessor(struct sp_preprocessor *pp);
int sp_set_pp_error(struct sp_preprocessor *pp, char *fmt, ...) SP_PRINTF_FORMAT(2,3);
void sp_set_preprocessor_io(struct sp_preprocessor *pp, struct sp_input *in, struct sp_ast *ast);
void sp_dump_macros(struct sp_preprocessor *pp);

int sp_peek_nonblank_pp_ph3_token(struct sp_preprocessor *pp, struct sp_pp_token *next, bool parse_header);
int sp_next_pp_ph3_token(struct sp_preprocessor *pp, bool parse_header);
bool sp_next_pp_ph3_char_is_lparen(struct sp_preprocessor *pp);
int sp_string_to_pp_token(struct sp_preprocessor *pp, const char *str, struct sp_pp_token *ret);

int sp_process_pp_directive(struct sp_preprocessor *pp);
int sp_next_pp_ph4_token(struct sp_preprocessor *pp);

int sp_next_pp_token(struct sp_preprocessor *pp, struct sp_pp_token *tok);
int sp_next_token(struct sp_preprocessor *pp, struct sp_token *tok);

#endif /* PREPROCESSOR_H_FILE */
