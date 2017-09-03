/* preprocessor.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#include "preprocessor.h"
#include "input.h"
#include "hashtable.h"
#include "ast.h"
#include "pp_token_list.h"
#include "pp_macro.h"

void sp_init_preprocessor(struct sp_preprocessor *pp, struct sp_program *prog, struct sp_mem_pool *pool)
{
  pp->prog = prog;
  pp->pool = pool;
  pp->in = NULL;
  pp->ast = NULL;
  pp->loc = sp_make_src_loc(0,0,0);
  pp->at_newline = false;
  pp->last_was_space = false;
  pp->macro_args_reading_level = 0;
  pp->macro_expansion_level = 0;
  pp->in_tokens = NULL;
  pp->init_ph6 = false;
  sp_init_idht(&pp->macros, pool);
  sp_init_string_table(&pp->token_strings, pool);
  sp_init_buffer(&pp->tmp_buf, pool);
  sp_init_buffer(&pp->tmp_str_buf, pool);
  sp_init_mem_pool(&pp->macro_exp_pool);

  sp_add_predefined_macros(pp);
}

void sp_destroy_preprocessor(struct sp_preprocessor *pp)
{
  while (pp->in) {
    struct sp_input *next = pp->in->next;
    sp_free_input(pp->in);
    pp->in = next;
  }
  sp_destroy_mem_pool(&pp->macro_exp_pool);
}

void sp_set_preprocessor_io(struct sp_preprocessor *pp, struct sp_input *in, struct sp_ast *ast)
{
  pp->in = in;
  pp->ast = ast;
  pp->loc = sp_make_src_loc(sp_get_input_file_id(in), 1, 0);
  pp->at_newline = true;
  pp->macro_args_reading_level = 0;
  pp->macro_expansion_level = 0;
  pp->last_was_space = false;
}

#if 0
static int set_error_at(struct sp_preprocessor *pp, struct sp_src_loc loc, char *fmt, ...)
{
  char str[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  sp_set_error(pp->prog, "%s:%d:%d: %s", sp_get_ast_file_name(pp->ast, loc.file_id), loc.line, loc.col, str);
  return -1;
}
#endif

int sp_set_pp_error(struct sp_preprocessor *pp, char *fmt, ...)
{
  char str[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  sp_set_error(pp->prog, "%s:%d:%d: %s", sp_get_ast_file_name(pp->ast, pp->tok.loc.file_id), pp->tok.loc.line, pp->tok.loc.col, str);
  return -1;
}

void sp_dump_macros(struct sp_preprocessor *pp)
{
  sp_string_id name_id = -1;
  while (sp_next_idht_key(&pp->macros, &name_id)) {
    struct sp_macro_def *macro = sp_get_idht_value(&pp->macros, name_id);
    sp_dump_macro(macro, pp);
  }
}

int sp_next_pp_token(struct sp_preprocessor *pp, struct sp_pp_token *tok)
{
  if (sp_next_pp_ph4_token(pp) < 0)
    return -1;
  *tok = pp->tok;
  return 0;
}
