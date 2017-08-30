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
    return set_error(pp, "invalid preprocessing directive: '#%s'", directive_name);
  
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

static int stringify_arg(struct sp_preprocessor *pp, struct sp_pp_token_list *arg, struct sp_pp_token *ret)
{
  char str[4096];  // enough according to "5.2.4.1 Translation limits"
  size_t str_len = 0;

#define ENSURE_STR_SPACE(n)   do { if (str_len+(n)+1 >= sizeof(str)) goto err; } while (0)
#define ADD_SPACE_IF_NEEDED() do { if (last_was_space) { ENSURE_STR_SPACE(1); str[str_len++] = ' '; last_was_space = false; } } while (0)
  
  bool last_was_space = false;
  struct sp_pp_token *tok = sp_rewind_pp_token_list(arg);
  while (sp_read_pp_token_from_list(arg, &tok)) {
    switch (tok->type) {
    case TOK_PP_EOF:
      ADD_SPACE_IF_NEEDED();
      break;

    case TOK_PP_NEWLINE:
    case TOK_PP_SPACE:
      last_was_space = true;
      break;

    case TOK_PP_OTHER:
      ADD_SPACE_IF_NEEDED();
      ENSURE_STR_SPACE(1);
      str[str_len++] = (char) tok->data.other;
      last_was_space = false;
      break;

    case TOK_PP_CHAR_CONST:
      ADD_SPACE_IF_NEEDED();
      // TODO: how do we print this?
      break;
    
    case TOK_PP_IDENTIFIER:
    case TOK_PP_HEADER_NAME:
    case TOK_PP_NUMBER:
      ADD_SPACE_IF_NEEDED();
      {
        const char *src = sp_get_pp_token_string(pp, tok);
        size_t src_len = strlen(src);
        ENSURE_STR_SPACE(src_len);
        memcpy(str + str_len, src, src_len);
        str_len += src_len;
      }
      last_was_space = false;
      break;

    case TOK_PP_STRING:
      ADD_SPACE_IF_NEEDED();
      {
        ENSURE_STR_SPACE(2);
        str[str_len++] = '\\';
        str[str_len++] = '\"';
        const char *src = sp_get_pp_token_string(pp, tok);
        for (const char *s = src; *s != '\0'; s++) {
          if (*s == '\\' || *s == '"') {
            ENSURE_STR_SPACE(1);
            str[str_len++] = '\\';
          }
          ENSURE_STR_SPACE(1);
          str[str_len++] = *s;
        }
        ENSURE_STR_SPACE(2);
        str[str_len++] = '\\';
        str[str_len++] = '\"';
      }
      last_was_space = false;
      break;

    case TOK_PP_PUNCT:
      ADD_SPACE_IF_NEEDED();
      {
        const char *src = sp_get_punct_name(tok->data.punct_id);
        size_t src_len = strlen(src);
        ENSURE_STR_SPACE(src_len);
        memcpy(str + str_len, src, src_len);
        str_len += src_len;
      }
      last_was_space = false;
      break;
    }
  }
  str[str_len] = '\0';
  ret->type = TOK_PP_STRING;
  ret->data.str_id = sp_add_string(&pp->token_strings, str);
  if (ret->data.str_id < 0)
    return set_error(pp, "out of memory");
  return 0;

 err:
  return set_error(pp, "string too large");
}

static int expand_macro(struct sp_preprocessor *pp, struct sp_macro_def *macro, struct sp_macro_args *args)
{
  UNUSED(args);

  //printf("expanding macro '%s' with %d args\n", sp_get_macro_name(macro, pp), args->len);
  
  struct sp_pp_token_list *macro_exp = sp_new_pp_token_list(&pp->macro_exp_pool);
  if (! macro_exp)
    return set_error(pp, "out of memory");
  
  macro->enabled = false;

  struct sp_pp_token *t = sp_rewind_pp_token_list(&macro->body);
  while (sp_read_pp_token_from_list(&macro->body, &t)) {
    //printf("adding token '%s' to macro_exp\n", sp_dump_pp_token(pp, t));

    if (pp_tok_is_punct(t, '#')) {
      struct sp_pp_token_list *arg = NULL;
      while (! arg) {
        if (! sp_read_pp_token_from_list(&macro->body, &t))
          break;
        if (! pp_tok_is_identifier(t))
          continue;
        arg = sp_get_macro_arg(macro, args, sp_get_pp_token_string_id(t));
        if (! arg)
          return set_error(pp, "'#' must be followed by a parameter name");
      }
      struct sp_pp_token str;
      if (stringify_arg(pp, arg, &str) < 0)
        return -1;
      if (sp_append_pp_token(macro_exp, &str) < 0)
        return set_error(pp, "out of memory");
      continue;
    }
    
    if (pp_tok_is_identifier(t)) {
      struct sp_pp_token_list *arg = sp_get_macro_arg(macro, args, sp_get_pp_token_string_id(t));
      if (arg) {
        // TODO: expand tokens of 'arg' before inserting (6.10.3.1)
        struct sp_pp_token *a = sp_rewind_pp_token_list(arg);
        while (sp_read_pp_token_from_list(arg, &a)) {
          if (sp_append_pp_token(macro_exp, a) < 0)
            return set_error(pp, "out of memory");
        }
        continue;
      }
    }
    if (sp_append_pp_token(macro_exp, t) < 0)
      return set_error(pp, "out of memory");
  }

  sp_rewind_pp_token_list(macro_exp);
  macro_exp->next = pp->in_macro_exp;
  pp->in_macro_exp = macro_exp;
  
  macro->enabled = true;
  return 0;
}

static int read_macro_args(struct sp_preprocessor *pp, struct sp_macro_def *macro, struct sp_macro_args *args)
{
  pp->reading_macro_args = true;

  //printf("READING MACRO ARGS FOR '%s'\n", sp_get_macro_name(macro, pp));

  do {
    if (sp_next_pp_ph4_token(pp) < 0)
      return -1;
  } while (IS_SPACE() || IS_NEWLINE());
  if (! IS_PUNCT('('))
    return set_error(pp, "expected '(', found '%s'", sp_dump_pp_token(pp, &pp->tok));

  int paren_level = 0;
  bool arg_start = true;
  while (true) {
    // skip initial spaces and newlines
    do {
      if (sp_next_pp_ph4_token(pp) < 0)
        return -1;
      if (! arg_start)
        break;
    } while (IS_SPACE() || IS_NEWLINE());
    arg_start = false;
    
    // check end or next
    if (paren_level == 0) {
      if (IS_PUNCT(')')) {
        if (args->len < macro->n_params)
          args->len++;
        break;
      }
      if (IS_PUNCT(',')) {
        args->len++;
        if (args->len < args->cap) {
          arg_start = true;
          continue;
        }
        if (! macro->is_variadic)
          return set_error(pp, "too many arguments in macro invocation");
      }
    }
    if (IS_PUNCT('('))
      paren_level++;
    else if (IS_PUNCT(')'))
      paren_level--;

    // add
    int add_index = args->len;
    //printf("ADDING TOKEN '%s' TO ARG %d\n", sp_dump_pp_token(pp, &pp->tok), add_index);
    if (add_index >= args->cap) {
      if (! macro->is_variadic)
        return set_error(pp, "too many arguments in macro invocation");
      add_index = args->cap-1;
    }
    if (sp_append_pp_token(&args->args[add_index], &pp->tok) < 0)
      return set_error(pp, "out of memory");
  }

  // TODO: remove trailing spaces and newlines from each argument
  
  if (macro->is_variadic) {
    if (args->len < macro->n_params)
      return set_error(pp, "macro '%s' requires at least %d arguments, found %d", sp_get_macro_name(macro, pp), macro->n_params, args->len);
  } else {
    if (args->len != macro->n_params)
      return set_error(pp, "macro '%s' requires %d arguments, found %d", sp_get_macro_name(macro, pp), macro->n_params, args->len);
  }
  pp->reading_macro_args = false;
  return 0;
}

static int peek_nonspace_token(struct sp_preprocessor *pp, struct sp_pp_token *tok)
{
  // peek in macro expansion tokens, if any
  if (pp->in_macro_exp) {
    struct sp_pp_token_list_pos save_pos = sp_get_pp_token_list_pos(pp->in_macro_exp);
    struct sp_pp_token_list *macro_exp = pp->in_macro_exp;

    bool found = false;
    while (macro_exp && ! found) {
      struct sp_pp_token *next;
      while (sp_read_pp_token_from_list(pp->in_macro_exp, &next)) {
        if (! pp_tok_is_space(next) && ! pp_tok_is_newline(next)) {
          //printf("NEXT NONSPACE: '%s' (type %d)\n", sp_dump_pp_token(pp, next), next->type);
          *tok = *next;
          found = true;
          break;
        }
      }
      if (! found)
        macro_exp = macro_exp->next;
    }
    
    sp_set_pp_token_list_pos(pp->in_macro_exp, save_pos);
    if (found)
      return 0;
  }

  // peek in input
  return sp_peek_nonspace_pp_ph3_token(pp, tok, false);
}

static bool next_token_from_macro_exp(struct sp_preprocessor *pp)
{
  if (pp->in_macro_exp) {
    struct sp_pp_token *tok;
    if (sp_read_pp_token_from_list(pp->in_macro_exp, &tok)) {
      pp->tok = *tok;
      while (pp->in_macro_exp && ! sp_peek_pp_token_from_list(pp->in_macro_exp))
        pp->in_macro_exp = pp->in_macro_exp->next;
      //printf("* read from macro_exp: '%s'\n", sp_dump_pp_token(pp, tok));
      return true;
    }
    while (pp->in_macro_exp && ! sp_peek_pp_token_from_list(pp->in_macro_exp))
      pp->in_macro_exp = pp->in_macro_exp->next;
    //printf("* macro_exp terminated\n");
  }
  return false;
}

int sp_next_pp_ph4_token(struct sp_preprocessor *pp)
{
  while (true) {
    bool from_macro_exp = next_token_from_macro_exp(pp);
    if (! from_macro_exp)
      NEXT_TOKEN();

    if (IS_NEWLINE()) {
      //printf("!!newline!!");
      pp->at_newline = true;
      return 0;
    }

    //printf("!!%s!!", sp_dump_pp_token(pp, &pp->tok));

    if (! from_macro_exp) {
      if (IS_EOF()) {
        if (pp->reading_macro_args)
          return set_error(pp, "unterminated macro argument list");
        if (! pp->in->next)
          return 0;
        struct sp_input *next = pp->in->next;
        sp_free_input(pp->in);
        pp->in = next;
        pp->at_newline = true;
        continue;
      }
  
      if (IS_PUNCT('#')) {
        //printf("!!!PUNCT!!! %d\n", pp->reading_macro_args);
        if (! pp->at_newline)
          return 0;
        if (pp->reading_macro_args)
          return set_error(pp, "preprocessing directive in macro arguments");
        if (process_pp_directive(pp) < 0)
          return -1;
        pp->at_newline = true;
        continue;
      }
    }
      
    pp->at_newline = false;

    if (pp_tok_is_identifier(&pp->tok)) {
      struct sp_pp_token ident = pp->tok;
      sp_string_id ident_id = sp_get_pp_token_string_id(&ident);

      //printf("-> ident '%s' (%d)\n", sp_get_string(&pp->token_strings, ident_id), ident_id);
      
      struct sp_pp_token next;
      if (peek_nonspace_token(pp, &next) < 0)
        return -1;

      //if (ident_id == 2) printf("deciding on expansion: next nonspace is '%s'\n", sp_dump_pp_token(pp, &next));
        
      if (! pp_tok_is_punct(&next, PUNCT_HASHES)) {
        struct sp_macro_def *macro = sp_get_idht_value(&pp->macros, ident_id);
        if (macro && macro->enabled && (! macro->is_function || pp_tok_is_punct(&next, '('))) {
          if (macro->is_function) {
            //printf("expanding function macro\n");
            struct sp_macro_args *args = sp_new_macro_args(macro, &pp->macro_exp_pool);
            if (read_macro_args(pp, macro, args) < 0)
              return -1;
            if (expand_macro(pp, macro, args) < 0)
              return -1;
          } else {
            //printf("expanding object macro\n");
            sp_rewind_pp_token_list(&macro->body);
            macro->body.next = pp->in_macro_exp;
            pp->in_macro_exp = &macro->body;
          }
          continue;
        }
      }
    }
    
    return 0;
  }
}
