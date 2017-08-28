/* pp_phase123.c
 *
 * Translation phase 4.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#include "preprocessor.h"
#include "ast.h"
#include "pp_token.h"

enum pp_directive_type {
  PP_DIR_if,
  PP_DIR_ifdef,
  PP_DIR_ifndef,
  PP_DIR_elif,
  PP_DIR_else,
  PP_DIR_endif,
  PP_DIR_include,
  PP_DIR_define,
  PP_DIR_undef,
  PP_DIR_line,
  PP_DIR_error,
  PP_DIR_pragma,
};

#define ADD_PP_DIR(dir) { PP_DIR_ ## dir, # dir }
static const struct pp_directive {
  enum pp_directive_type type;
  const char *name;
} pp_directives[] = {
  ADD_PP_DIR(if),
  ADD_PP_DIR(ifdef),
  ADD_PP_DIR(ifndef),
  ADD_PP_DIR(elif),
  ADD_PP_DIR(else),
  ADD_PP_DIR(endif),
  ADD_PP_DIR(include),
  ADD_PP_DIR(define),
  ADD_PP_DIR(undef),
  ADD_PP_DIR(line),
  ADD_PP_DIR(error),
  ADD_PP_DIR(pragma),
};

#define set_error sp_set_pp_error

#define NEXT_TOKEN()        do { if (sp_next_pp_ph3_token(pp, false) < 0) return -1; } while (0)
#define NEXT_HEADER_TOKEN() do { if (sp_next_pp_ph3_token(pp, true ) < 0) return -1; } while (0)

#define IS_TOK_TYPE(t)         (pp->tok.type == t)
#define IS_EOF()               IS_TOK_TYPE(TOK_PP_EOF)
#define IS_SPACE()             IS_TOK_TYPE(TOK_PP_SPACE)
#define IS_NEWLINE()           IS_TOK_TYPE(TOK_PP_NEWLINE)
#define IS_PP_HEADER_NAME()    IS_TOK_TYPE(TOK_PP_HEADER_NAME)
#define IS_STRING()            IS_TOK_TYPE(TOK_PP_STRING)
#define IS_IDENTIFIER()        IS_TOK_TYPE(TOK_PP_IDENTIFIER)
#define IS_PUNCT(id)           (IS_TOK_TYPE(TOK_PP_PUNCT) && pp->tok.data.punct_id == (id))

static bool find_pp_directive(const char *string, enum pp_directive_type *ret)
{
  for (int i = 0; i < ARRAY_SIZE(pp_directives); i++) {
    if (strcmp(string, pp_directives[i].name) == 0) {
      *ret = pp_directives[i].type;
      return true;
    }
  }
  return false;
}

static int process_include(struct sp_preprocessor *pp)
{
  do {
    NEXT_HEADER_TOKEN();
  } while (IS_SPACE());
  
  if (! IS_PP_HEADER_NAME())   // TODO: allow macro expansion
    return set_error(pp, "bad include file name: '%s'", sp_dump_pp_token(pp, &pp->tok));
  
  const char *include_file = sp_get_pp_token_string(pp, &pp->tok);
  do {
    NEXT_TOKEN();
  } while (IS_SPACE());
  if (! IS_NEWLINE())
    return set_error(pp, "unexpected input after include file name: '%s'", sp_dump_pp_token(pp, &pp->tok));

  // remove surrounding <> or ""
  char filename[256];
  size_t include_filename_len = strlen(include_file);
  if (include_filename_len < 2)
    return set_error(pp, "invalid filename");
  if (include_filename_len > sizeof(filename)+1)
    return set_error(pp, "filename too long");
  memcpy(filename, include_file+1, include_filename_len-2);
  filename[include_filename_len-2] = '\0';
  //bool is_system_header = (include_file[0] == '<');

  // open file (TODO: search file according to 'is_system_header')
  const char *base_filename = sp_get_ast_file_name(pp->ast, sp_get_input_file_id(pp->in));
  sp_string_id file_id = sp_add_ast_file_name(pp->ast, filename);
  if (file_id < 0)
    return set_error(pp, "out of memory");
  struct sp_input *in = sp_new_input_from_file(filename, (uint16_t) file_id, base_filename);
  if (! in)
    return set_error(pp, "can't open '%s'", filename);
  in->next = pp->in;
  pp->in = in;

  return 0;
}

static int process_undef(struct sp_preprocessor *pp)
{
  NEXT_TOKEN();
  if (IS_SPACE())
    NEXT_TOKEN();
  if (IS_NEWLINE())
    return set_error(pp, "macro name required");
  
  if (! IS_IDENTIFIER())
    return set_error(pp, "macro name must be an identifier, found '%s'", sp_dump_pp_token(pp, &pp->tok));
  
  sp_delete_idht_entry(&pp->macros, sp_get_pp_token_string_id(&pp->tok));

  do {
    NEXT_TOKEN();
  } while (IS_SPACE());
  if (! IS_NEWLINE())
    return set_error(pp, "unexpected input after #undef MACRO: '%s'", sp_dump_pp_token(pp, &pp->tok));
  return 0;
}

static int read_macro_params(struct sp_preprocessor *pp, struct sp_pp_token_list *params, bool *last_param_is_variadic)
{
  NEXT_TOKEN();
  if (! IS_PUNCT('('))
    return set_error(pp, "expected '('");

  bool found_ellipsis = false;
  while (true) {
    NEXT_TOKEN();
    if (IS_SPACE())
      NEXT_TOKEN();
    if (IS_EOF() || IS_NEWLINE())
      return set_error(pp, "unterminated macro parameter list");
    if (IS_PUNCT(')') && (found_ellipsis || sp_pp_token_list_size(params) == 0))
      break;
    if (found_ellipsis)
      return set_error(pp, "expected ')' after '...'");
      
    if (! IS_IDENTIFIER() && ! IS_PUNCT(PUNCT_ELLIPSIS))
      return set_error(pp, "invalid macro parameter: '%s'", sp_dump_pp_token(pp, &pp->tok));

    if (sp_append_pp_token(params, &pp->tok) < 0)
      return set_error(pp, "out of memory");

    NEXT_TOKEN();
    if (IS_SPACE())
      NEXT_TOKEN();
    if (IS_EOF() || IS_NEWLINE())
      return set_error(pp, "unterminated macro parameter list");
    if (IS_PUNCT(PUNCT_ELLIPSIS)) {
      found_ellipsis = true;
      continue;
    }
    if (IS_PUNCT(')'))
      break;
    if (IS_PUNCT(','))
      continue;
    return set_error(pp, "expected ',' or ')'");
  }

  *last_param_is_variadic = found_ellipsis;
  return 0;
}

static int read_macro_body(struct sp_preprocessor *pp, struct sp_pp_token_list *body)
{
  NEXT_TOKEN();

  // skip leading space  
  if (IS_SPACE())
    NEXT_TOKEN();
  
  while (true) {
    if (IS_NEWLINE())
      break;
    if (IS_SPACE()) {
      // skip trailing space
      struct sp_pp_token space = pp->tok;
      NEXT_TOKEN();
      if (IS_NEWLINE())
        break;
      // it wasn't the body's end, so we put the space back
      if (sp_append_pp_token(body, &space) < 0)
        return set_error(pp, "out of memory");
    }
    if (sp_append_pp_token(body, &pp->tok) < 0)
      return set_error(pp, "out of memory");
    NEXT_TOKEN();
  }
  return 0;
}

static int process_define(struct sp_preprocessor *pp)
{
  do {
    NEXT_TOKEN();
  } while (IS_SPACE());
  if (IS_NEWLINE())
    return set_error(pp, "macro name required");
  
  if (! IS_IDENTIFIER())
    return set_error(pp, "macro name must be an identifier, found '%s'", sp_dump_pp_token(pp, &pp->tok));
  
  sp_string_id macro_name_id = sp_get_pp_token_string_id(&pp->tok);
  bool is_function = sp_next_pp_ph3_char_is_lparen(pp);

  // params
  struct sp_pp_token_list params;
  sp_init_pp_token_list(&params, pp->pool);
  bool last_param_is_variadic = false;
  if (is_function) {
    if (read_macro_params(pp, &params, &last_param_is_variadic) < 0)
      return -1;
  }

  // body
  struct sp_pp_token_list body;
  sp_init_pp_token_list(&body, pp->pool);
  if (read_macro_body(pp, &body) < 0)
    return -1;

  struct sp_macro_def *macro = sp_new_macro_def(pp, macro_name_id, is_function,
                                                last_param_is_variadic, &params, &body);
  if (! macro)
    return -1;

  struct sp_macro_def *old_macro = sp_get_idht_value(&pp->macros, macro_name_id);
  if (old_macro) {
    if (! sp_macros_are_equal(macro, old_macro))
      return set_error(pp, "redefinition of macro '%s'", sp_get_string(&pp->token_strings, macro_name_id));
    return 0;
  }
  
  if (sp_add_idht_entry(&pp->macros, macro_name_id, macro) < 0)
    return set_error(pp, "out of memory");
  return 0;
}

static int process_pp_directive(struct sp_preprocessor *pp)
{
  NEXT_TOKEN();
  while (IS_SPACE())
    NEXT_TOKEN();
  if (IS_NEWLINE())
    return 0;
  
  if (! IS_IDENTIFIER())
    return set_error(pp, "invalid input after '#': '%s'", sp_dump_pp_token(pp, &pp->tok));
  
  const char *directive_name = sp_get_pp_token_string(pp, &pp->tok);
  enum pp_directive_type directive;
  if (! find_pp_directive(directive_name, &directive))
    return set_error(pp, "invalid preprocessor directive: '#%s'", directive_name);
  
  switch (directive) {
  case PP_DIR_include: return process_include(pp);
  case PP_DIR_define:  return process_define(pp);
  case PP_DIR_undef:   return process_undef(pp);

  case PP_DIR_if:
  case PP_DIR_ifdef:
  case PP_DIR_ifndef:
  case PP_DIR_elif:
  case PP_DIR_else:
  case PP_DIR_endif:
  case PP_DIR_line:
  case PP_DIR_error:
  case PP_DIR_pragma:
    return set_error(pp, "preprocessor directive is not implemented: '#%s'", directive_name);
  }
  return set_error(pp, "invalid preprocessor directive: '#%s'", directive_name);
}

static int expand_macro(struct sp_preprocessor *pp, struct sp_macro_def *macro, struct sp_pp_token_list *args)
{
  UNUSED(args);

  struct sp_pp_token_list *macro_exp = sp_new_pp_token_list(&pp->macro_exp_pool);
  if (! macro_exp)
    return set_error(pp, "out of memory");
  
  macro->enabled = false;

  struct sp_pp_token *t = sp_rewind_pp_token_list(&macro->body);
  while (sp_read_pp_token_from_list(&macro->body, &t)) {
    if (sp_append_pp_token(macro_exp, t) < 0)
      return set_error(pp, "out of memory");
  }
  
  macro->enabled = true;
  return 0;
}

int sp_next_pp_ph4_token(struct sp_preprocessor *pp)
{
  while (true) {
    if (pp->macro_exp) {
      struct sp_pp_token *tok;
      if (sp_read_pp_token_from_list(pp->macro_exp, &tok)) {
        pp->tok = *tok;
        if (! sp_peek_pp_token_from_list(pp->macro_exp))
          pp->macro_exp = pp->macro_exp->next;
        return 0;
      }
    }
    
    NEXT_TOKEN();

    if (IS_NEWLINE()) {
      //printf("!!newline!!");
      pp->at_newline = true;
      return 0;
    }

    //printf("!!%s!!", sp_dump_pp_token(pp, &pp->tok));

    if (IS_EOF()) {
      if (! pp->in->next)
        return 0;
      struct sp_input *next = pp->in->next;
      sp_free_input(pp->in);
      pp->in = next;
      pp->at_newline = true;
      continue;
    }
  
    if (IS_PUNCT('#')) {
      if (! pp->at_newline)
        return 0;
      if (process_pp_directive(pp) < 0)
        return -1;
      pp->at_newline = true;
      continue;
    }

    pp->at_newline = false;

    if (pp_tok_is_identifier(&pp->tok)) {
      struct sp_pp_token ident = pp->tok;
      sp_string_id ident_id = sp_get_pp_token_string_id(&ident);

      struct sp_pp_token next;
      if (sp_peek_nonspace_pp_ph3_token(pp, &next, false) < 0)
        return -1;
      
      if (! pp_tok_is_punct(&next, PUNCT_HASHES)) {
        struct sp_macro_def *macro = sp_get_idht_value(&pp->macros, ident_id);
        if (macro && (! macro->is_function || pp_tok_is_punct(&next, '('))) {
          struct sp_pp_token_list args;
          sp_init_pp_token_list(&args, &pp->macro_exp_pool);
          if (macro->is_function) {
            // TODO: read args
            if (sp_pp_token_list_size(&args) != sp_pp_token_list_size(&macro->params))
              return set_error(pp, "invalid number of arguments in macro invocation");
          }
          if (expand_macro(pp, macro, &args) < 0)
            return -1;
          continue;
        }
      }
    }
    
    return 0;
  }
}
