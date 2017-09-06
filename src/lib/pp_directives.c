/* pp_directives.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#include "preprocessor.h"
#include "ast.h"
#include "pp_token.h"
#include "punct.h"

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

#define set_error    sp_set_pp_error
#define set_error_at sp_set_pp_error_at

#define NEXT_TOKEN()        do { if (sp_next_pp_ph3_token(pp, false) < 0) return -1; } while (0)
#define NEXT_HEADER_TOKEN() do { if (sp_next_pp_ph3_token(pp, true ) < 0) return -1; } while (0)

#define IS_TOK_TYPE(t)         (pp->tok.type == t)
#define IS_EOF()               IS_TOK_TYPE(TOK_PP_EOF)
#define IS_END_OF_LIST()       IS_TOK_TYPE(TOK_PP_END_OF_LIST)
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

static const char *get_pp_directive_name(enum pp_directive_type dir)
{
  for (int i = 0; i < ARRAY_SIZE(pp_directives); i++) {
    if (pp_directives[i].type == dir)
      return pp_directives[i].name;
  }
  return NULL;
}

static int read_unexpanded_directive_args(struct sp_preprocessor *pp, bool allow_header, struct sp_pp_token_list **ret)
{
  sp_clear_mem_pool(&pp->directive_pool);
  struct sp_pp_token_list *expr = sp_new_pp_token_list(&pp->directive_pool, 10);
  if (! expr)
    goto err_oom;
  while (true) {
    if (allow_header)
      NEXT_HEADER_TOKEN();
    else
      NEXT_TOKEN();
    if (IS_NEWLINE() || IS_EOF())
      break;
    if (sp_append_pp_token(expr, &pp->tok) < 0)
      goto err_oom;
  }
  *ret = expr;
  return 0;

 err_oom:
  return set_error(pp, "out of memory");
}

static int read_expanded_directive_args(struct sp_preprocessor *pp, bool allow_header, struct sp_pp_token_list **ret)
{
  // read unexpanded tokens
  struct sp_pp_token_list *expr = NULL;
  if (read_unexpanded_directive_args(pp, allow_header, &expr) < 0)
    return -1;

  // add <end-of-list> token to expand
  struct sp_pp_token end_of_list = pp->tok;
  end_of_list.type = TOK_PP_END_OF_LIST;
  if (sp_append_pp_token(expr, &end_of_list) < 0)
    goto err_oom;

  //printf("expanding args << "); sp_dump_pp_token_list(expr, pp); printf(" >>\n");

  // expand
  if (sp_add_pp_token_list_to_ph4_input(pp, expr) < 0)
    return -1;
  struct sp_pp_token_list *exp_expr = sp_new_pp_token_list(&pp->directive_pool, sp_pp_token_list_size(expr));
  if (! exp_expr)
    goto err_oom;
  while (true) {
    if (sp_next_pp_ph4_processed_token(pp, true) < 0)
      return -1;
    if (IS_EOF())
      return set_error(pp, "unterminated expression");
    if (IS_END_OF_LIST())
      break;
    //printf("exp-> %s\n", sp_dump_pp_token(pp, &pp->tok));
    if (sp_append_pp_token(exp_expr, &pp->tok) < 0)
      goto err_oom;
  }
  
  sp_clear_mem_pool(&pp->directive_pool);
  *ret = exp_expr;
  return 0;

 err_oom:
  return set_error(pp, "out of memory");
}

/* ================================================ */
/* == #error ====================================== */

static int process_error(struct sp_preprocessor *pp, struct sp_src_loc loc)
{
  // read unexpanded tokens
  struct sp_pp_token_list *args = NULL;
  if (read_unexpanded_directive_args(pp, false, &args) < 0)
    return -1;

  char str[1024];
  size_t str_len = 0;

  struct sp_pp_token_list_walker w;
  struct sp_pp_token *tok = sp_rewind_pp_token_list(&w, args);
  while (sp_read_pp_token_from_list(&w, &tok)) {
    const char *s = sp_dump_pp_token(pp, tok);
    size_t s_len = strlen(s);
    if (str_len + s_len + 2 < str_len || str_len + s_len + 2 > sizeof(str))
      return set_error(pp, "error message too long");
    memcpy(str + str_len, s, s_len);
    str_len += s_len;
  }
  str[str_len] = '\0';
  return set_error_at(pp, loc, "#error %s", str);
}

/* ================================================ */
/* == #include ==================================== */

static struct sp_input *try_open_include_file(struct sp_preprocessor *pp, struct sp_src_loc loc, const char *filename)
{
  //printf("-> trying '%s'\n", filename);
  
  struct sp_input *in = sp_new_input_from_file(filename);
  if (! in) {
    set_error_at(pp, loc, "can't open file '%s'", filename);
    return NULL;
  }
  sp_string_id file_id = sp_add_ast_file_name(pp->ast, filename);
  if (file_id < 0) {
    sp_free_input(in);
    set_error_at(pp, loc, "out of memory");
    return NULL;
  }
  in->file_id = file_id;
  return in;
}

static struct sp_input *try_open_include_file_at(struct sp_preprocessor *pp, struct sp_src_loc loc, const char *filename, const char *dir, size_t dir_len)
{
  if (! dir)
    return try_open_include_file(pp, loc, filename);

  char path[1024];

  size_t len = 0;
  if (len + dir_len + 1 > sizeof(path))
    return NULL;
  memcpy(path, dir, dir_len);
  len += dir_len;
  if (len > 0 && path[len-1] != '/' && path[len-1] != '\\')
    path[len++] = '/';
  
  size_t file_len = strlen(filename);
  if (len + file_len + 1 > sizeof(path))
    return NULL;
  memcpy(path + len, filename, file_len);
  len += file_len;
  path[len] = '\0';
  return try_open_include_file(pp, loc, path);
}

static struct sp_input *search_include_file(struct sp_preprocessor *pp, struct sp_src_loc loc, const char *filename, const char *base_filename, bool is_system_header)
{
  // if filename is absolute, just try to open it
  if (filename[0] == '/' || filename[0] == '\\')
    return try_open_include_file(pp, loc, filename);

  // if it's an "include", search the directory of the base file
  if (! is_system_header) {
    const char *base_last_slash = strrchr(base_filename, '/');
    const char *base_last_backslash = strrchr(base_filename, '\\');
    if (! base_last_slash || (base_last_backslash && base_last_backslash > base_last_slash))
      base_last_slash = base_last_backslash;

    const char *dir;
    size_t dir_len;
    if (base_last_slash) {
      dir = base_filename;
      dir_len = base_last_slash - base_filename;
    } else {
      dir = NULL;
      dir_len = 0;
    }
    struct sp_input *in = try_open_include_file_at(pp, loc, filename, dir, dir_len);
    if (in)
      return in;
  }

  // if it's an "include", search the user directories
  if (! is_system_header) {
    for (size_t i = 0; i < pp->user_include_search_path_len; i++) {
      const char *dir = pp->user_include_search_path[i];
      struct sp_input *in = try_open_include_file_at(pp, loc, filename, dir, strlen(dir));
      if (in)
        return in;
    }
  }
  
  // search the system directories
  for (size_t i = 0; i < pp->sys_include_search_path_len; i++) {
    const char *dir = pp->sys_include_search_path[i];
    struct sp_input *in = try_open_include_file_at(pp, loc, filename, dir, strlen(dir));
    if (in)
      return in;
  }
  
  set_error_at(pp, loc, "can't find include file '%s'", filename);
  return NULL;
}

static int process_include(struct sp_preprocessor *pp, struct sp_src_loc loc)
{
  struct sp_pp_token_list *args = NULL;
  if (read_expanded_directive_args(pp, true, &args) < 0)
    return -1;

  struct sp_pp_token *header = NULL;
  struct sp_pp_token_list_walker w;
  struct sp_pp_token *tok = sp_rewind_pp_token_list(&w, args);
  while (sp_read_pp_token_from_list(&w, &tok)) {
    if (pp_tok_is_header_name(tok) || pp_tok_is_string(tok)) {
      header = tok;
      continue;
    }
    if (! pp_tok_is_space(tok)) {
      if (header)
        return set_error(pp, "extra token after include file name: '%s'", sp_dump_pp_token(pp, tok));
      break;
    }
  }
  if (! header)
    return set_error(pp, "expected <filename> or \"filename\"");
  
  const char *include_file = sp_get_pp_token_string(pp, tok);

  // remove surrounding <> or ""
  char filename[1024];
  size_t include_filename_len = strlen(include_file);
  if (include_filename_len < 2)
    return set_error_at(pp, loc, "invalid filename");
  if (include_filename_len > sizeof(filename)+1)
    return set_error_at(pp, loc, "filename too long");
  memcpy(filename, include_file+1, include_filename_len-2);
  filename[include_filename_len-2] = '\0';
  bool is_system_header = (include_file[0] == '<');
  
  // open file (TODO: search file according to 'is_system_header')
  const char *base_filename = sp_get_ast_file_name(pp->ast, sp_get_input_file_id(pp->in));
  struct sp_input *in = search_include_file(pp, loc, filename, base_filename, is_system_header);
  if (! in)
    return -1;
  in->base_cond_level = pp->cond_level;
  in->next = pp->in;
  pp->in = in;
  
  return 0;
}

/* ================================================ */
/* == #define / #undef ============================ */

static int process_undef(struct sp_preprocessor *pp, struct sp_src_loc loc)
{
  NEXT_TOKEN();
  if (IS_SPACE())
    NEXT_TOKEN();
  if (IS_NEWLINE())
    return set_error_at(pp, loc, "macro name required");
  
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

static int read_macro_params(struct sp_preprocessor *pp, struct sp_pp_token_list *params, bool *is_variadic, bool *is_named_variadic)
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
    if (IS_PUNCT(PUNCT_ELLIPSIS)) {
      found_ellipsis = true;
      *is_variadic = true;
    }
      
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
      if (found_ellipsis)
        return set_error(pp, "expected ')' after '...'");
      found_ellipsis = true;
      *is_variadic = true;
      *is_named_variadic = true;
      continue;
    }
    if (IS_PUNCT(')'))
      break;
    if (IS_PUNCT(','))
      continue;
    return set_error(pp, "expected ',' or ')'");
  }

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

static int process_define(struct sp_preprocessor *pp, struct sp_src_loc loc)
{
  do {
    NEXT_TOKEN();
  } while (IS_SPACE());
  if (IS_NEWLINE())
    return set_error(pp, "macro name required");
  
  if (! IS_IDENTIFIER())
    return set_error_at(pp, loc, "macro name must be an identifier, found '%s'", sp_dump_pp_token(pp, &pp->tok));
  
  sp_string_id macro_name_id = sp_get_pp_token_string_id(&pp->tok);
  bool is_function = sp_next_pp_ph3_char_is_lparen(pp);

  // params
  struct sp_pp_token_list params;
  sp_init_pp_token_list(&params, pp->pool, 4);
  bool is_variadic = false;
  bool is_named_variadic = false;
  if (is_function) {
    if (read_macro_params(pp, &params, &is_variadic, &is_named_variadic) < 0)
      return -1;
  }

  // body
  struct sp_pp_token_list body;
  sp_init_pp_token_list(&body, pp->pool, 16);
  if (read_macro_body(pp, &body) < 0)
    return -1;

  struct sp_macro_def *macro = sp_new_macro_def(pp, macro_name_id, is_function, is_variadic,
                                                is_named_variadic, &params, &body);
  if (! macro)
    return -1;

  struct sp_macro_def *old_macro = sp_get_idht_value(&pp->macros, macro_name_id);
  if (old_macro) {
    if (! sp_macros_are_equal(macro, old_macro))
      return set_error_at(pp, loc, "redefinition of macro '%s'", sp_get_string(&pp->token_strings, macro_name_id));
    return 0;
  }
  
  if (sp_add_idht_entry(&pp->macros, macro_name_id, macro) < 0)
    return set_error_at(pp, loc, "out of memory");
  return 0;
}

/* ================================================ */
/* == #if / #else / #endif ======================== */

static int eval_cond_expr_list(struct sp_preprocessor *pp, struct sp_pp_token_list *expr, bool *ret)
{
  // TODO: properly eval conditional expression according to [6.10.1]
  // paragraph 4.  We should convert the pp-tokens in 'expr' to
  // tokens, parse the resulting expression and evaluate it.
  
  sp_string_id str_id_zero = sp_add_string(&pp->token_strings, "0");
  if (str_id_zero < 0)
    return set_error(pp, "out of memory");
  
  if (sp_pp_token_list_size(expr) == 1) {
    struct sp_pp_token_list_walker w;
    struct sp_pp_token *tok = sp_rewind_pp_token_list(&w, expr);
    while (sp_read_pp_token_from_list(&w, &tok)) {
      if (pp_tok_is_number(tok)) {
        if (tok->data.str_id == str_id_zero)
          *ret = false;
        else
          *ret = true;
        return true;
      }
    }
  }
  return set_error(pp, "evaluation of complex conditionals is not implemented, only '0', '1' or 'defined IDENT' are supported");
}

static int test_cond_expr(struct sp_preprocessor *pp, bool *ret)
{
  // read unexpanded expression
  sp_clear_mem_pool(&pp->directive_pool);
  struct sp_pp_token_list *expr = sp_new_pp_token_list(&pp->directive_pool, 10);
  if (! expr)
    goto err_oom;
  while (! IS_NEWLINE() && ! IS_EOF()) {
    if (sp_append_pp_token(expr, &pp->tok) < 0)
      goto err_oom;
    NEXT_TOKEN();
  }
  if (sp_pp_token_list_size(expr) == 0)
    return set_error(pp, "conditional expression expected");

  // add <end-of-list> token to expand the expression
  struct sp_pp_token end_of_list = pp->tok;
  end_of_list.type = TOK_PP_END_OF_LIST;
  if (sp_append_pp_token(expr, &end_of_list) < 0)
    goto err_oom;

  // kill every 'define' and the following identifier in 'expr'
  sp_string_id str_id_defined = sp_add_string(&pp->token_strings, "defined");
  sp_string_id str_id_zero = sp_add_string(&pp->token_strings, "0");
  sp_string_id str_id_one = sp_add_string(&pp->token_strings, "1");
  if (str_id_defined < 0 || str_id_zero < 0 || str_id_one < 0)
    goto err_oom;
  struct sp_pp_token_list_walker w;
  struct sp_pp_token *tok = sp_rewind_pp_token_list(&w, expr);
  bool last_was_defined = false;
  while (sp_read_pp_token_from_list(&w, &tok)) {
    if (pp_tok_is_identifier(tok)) {
      if (tok->data.str_id == str_id_defined) {
        tok->macro_dead = true;
        last_was_defined = true;
      } else if (last_was_defined) {
        tok->macro_dead = true;
        last_was_defined = false;
      }
    }
  }

  // expand expression
  //printf("EXPANDING: << "); sp_dump_pp_token_list(expr, pp); printf(" >>\n");
  if (sp_add_pp_token_list_to_ph4_input(pp, expr) < 0)
    return -1;
  struct sp_pp_token_list *exp_expr = sp_new_pp_token_list(&pp->directive_pool, sp_pp_token_list_size(expr));
  if (! exp_expr)
    goto err_oom;
  int defined_reading_state = 0;
  while (true) {
    if (sp_next_pp_ph4_processed_token(pp, true) < 0)
      return -1;
    if (IS_EOF())
      return set_error(pp, "unterminated conditional expression");
    if (IS_END_OF_LIST())
      break;
    if (IS_SPACE())
      continue;

    // "defined"
    if (IS_IDENTIFIER() && sp_get_pp_token_string_id(&pp->tok) == str_id_defined) {
      defined_reading_state = 1;
      continue;
    }

    // tokens following "defined"
    switch (defined_reading_state) {
    case 1:  // defined
      if (IS_PUNCT('(')) {
        defined_reading_state = 2;
        continue;
      }
      // fallthrough
      
    case 2:  // defined (
      if (IS_IDENTIFIER()) {
        defined_reading_state = (defined_reading_state == 1) ? 0 : 3;
        // replace "defined IDENT" with "0" or "1"
        struct sp_pp_token val = pp->tok;
        val.type = TOK_PP_NUMBER;
        if (sp_get_idht_value(&pp->macros, sp_get_pp_token_string_id(&pp->tok)))
          val.data.str_id = str_id_one;
        else
          val.data.str_id = str_id_zero;
        if (sp_append_pp_token(exp_expr, &val) < 0)
          goto err_oom;
        continue;
      }
      return set_error(pp, (defined_reading_state == 1) ? "expected '(' or identifier" : "expected identifier");

    case 3:
      if (! IS_PUNCT(')'))
        return set_error(pp, "expected ')'");
      defined_reading_state = 0;
      continue;
    }

    struct sp_pp_token tok = pp->tok;

    // other identifiers are replaced with "0"
    if (IS_IDENTIFIER()) {
      tok.type = TOK_PP_NUMBER;
      tok.data.str_id = str_id_zero;
    }

    //printf("adding -> '%s'\n", sp_dump_pp_token(pp, &tok));
    if (sp_append_pp_token(exp_expr, &tok) < 0)
      goto err_oom;
  }
  
  if (eval_cond_expr_list(pp, exp_expr, ret) < 0)
    return -1;
  
  sp_clear_mem_pool(&pp->directive_pool);
  return 0;

 err_oom:
  return set_error(pp, "out of memory");
}

static int skip_cond_expr(struct sp_preprocessor *pp)
{
  while (! IS_NEWLINE() && ! IS_EOF())
    NEXT_TOKEN();
  return 0;
}

static int process_conditional(struct sp_preprocessor *pp, enum pp_directive_type directive)
{
  do {
    NEXT_TOKEN();
  } while (IS_SPACE());

  switch (directive) {
  case PP_DIR_if:
    {
      if (pp->cond_level >= PP_MAX_COND_NESTING)
        return set_error(pp, "too many conditional nestings");

      if (pp->cond_level >= 0 && pp->cond_state[pp->cond_level] != PP_COND_ACTIVE) {
        pp->cond_state[++pp->cond_level] = PP_COND_DONE;
      } else {
        bool result;
        if (test_cond_expr(pp, &result) < 0)
          return -1;
        pp->cond_state[++pp->cond_level] = (result) ? PP_COND_ACTIVE : PP_COND_INACTIVE;
      }
    }
    break;
    
  case PP_DIR_ifdef:
  case PP_DIR_ifndef:
    {
      if (pp->cond_level >= PP_MAX_COND_NESTING)
        return set_error(pp, "too many conditional nestings");

      if (pp->cond_level >= 0 && pp->cond_state[pp->cond_level] != PP_COND_ACTIVE) {
        pp->cond_state[++pp->cond_level] = PP_COND_DONE;
      } else {
        if (! IS_IDENTIFIER())
          return set_error(pp, "expected identifier for '#%s'", get_pp_directive_name(directive));

        bool is_defined = sp_get_idht_value(&pp->macros, sp_get_pp_token_string_id(&pp->tok)) != NULL;
        pp->cond_state[++pp->cond_level] = ((directive == PP_DIR_ifdef) == is_defined) ? PP_COND_ACTIVE : PP_COND_INACTIVE;
        
        do {
          NEXT_TOKEN();
        } while (IS_SPACE());
        if (! IS_NEWLINE() && ! IS_EOF())
          return set_error(pp, "extra token in '#%s': '%s'", get_pp_directive_name(directive), sp_dump_pp_token(pp, &pp->tok));
      }
    }
    break;

  case PP_DIR_elif:
    if (pp->cond_level < 0 || pp->cond_level < pp->in->base_cond_level)
      return set_error(pp, "'#elif' without '#if'");
    if (pp->cond_state[pp->cond_level] == PP_COND_INACTIVE) {
      bool result;
      if (test_cond_expr(pp, &result) < 0)
        return -1;
      if (result)
        pp->cond_state[pp->cond_level] = PP_COND_ACTIVE;
    } else {
      if (skip_cond_expr(pp) < 0)
        return -1;
      if (pp->cond_state[pp->cond_level] == PP_COND_ACTIVE)
        pp->cond_state[pp->cond_level] = PP_COND_DONE;
    }      
    break;

  case PP_DIR_else:
    if (pp->cond_level < 0 || pp->cond_level <= pp->in->base_cond_level)
      return set_error(pp, "'#else' without '#if'");
    if (pp->cond_state[pp->cond_level] == PP_COND_ACTIVE)
      pp->cond_state[pp->cond_level] = PP_COND_DONE;
    else if (pp->cond_state[pp->cond_level] == PP_COND_INACTIVE)
      pp->cond_state[pp->cond_level] = PP_COND_ACTIVE;
    if (! IS_NEWLINE() && ! IS_EOF())
      return set_error(pp, "extra token in '#else': '%s'", sp_dump_pp_token(pp, &pp->tok));
    break;

  case PP_DIR_endif:
    if (pp->cond_level < 0 || pp->cond_level <= pp->in->base_cond_level)
      return set_error(pp, "'#endif' without '#if'");
    pp->cond_level--;
    if (! IS_NEWLINE() && ! IS_EOF())
      return set_error(pp, "extra token in '#endif': '%s'", sp_dump_pp_token(pp, &pp->tok));
    break;

  default:
    return set_error(pp, "unimplemented directive: '#%s'", get_pp_directive_name(directive));
  }
  
  return 0;
}

/* ================================================ */

int sp_process_pp_directive(struct sp_preprocessor *pp)
{
  NEXT_TOKEN();
  while (IS_SPACE())
    NEXT_TOKEN();
  if (IS_NEWLINE())
    return 0;
  
  if (! IS_IDENTIFIER()) {
    set_error(pp, "invalid input after '#': '%s'", sp_dump_pp_token(pp, &pp->tok));
    goto err;
  }

  struct sp_src_loc loc = pp->tok.loc;
  const char *directive_name = sp_get_pp_token_string(pp, &pp->tok);
  enum pp_directive_type directive;
  if (! find_pp_directive(directive_name, &directive)) {
    set_error(pp, "invalid preprocessing directive: '#%s'", directive_name);
    goto err;
  }

  // always process conditionals
  switch (directive) {
  case PP_DIR_if:
  case PP_DIR_ifdef:
  case PP_DIR_ifndef:
  case PP_DIR_elif:
  case PP_DIR_else:
  case PP_DIR_endif:
    return process_conditional(pp, directive);

  default:;
  }

  // dont process anything else unless we should
  if (pp->cond_level >= 0 && pp->cond_state[pp->cond_level] != PP_COND_ACTIVE)
    return 0;
  
  switch (directive) {
  case PP_DIR_include: return process_include(pp, loc);
  case PP_DIR_define:  return process_define(pp, loc);
  case PP_DIR_undef:   return process_undef(pp, loc);
  case PP_DIR_error:   return process_error(pp, loc);

  case PP_DIR_line:
  case PP_DIR_pragma:
    return set_error(pp, "preprocessor directive is not implemented: '%s'", directive_name);

  case PP_DIR_if:
  case PP_DIR_ifdef:
  case PP_DIR_ifndef:
  case PP_DIR_elif:
  case PP_DIR_else:
  case PP_DIR_endif:;
  }
  return set_error(pp, "invalid preprocessor directive: '#%s'", directive_name);

 err:
  // only error if we should be processing the directive
  if (pp->cond_level < 0 || pp->cond_state[pp->cond_level] == PP_COND_ACTIVE)
    return -1;
  return 0;
}
