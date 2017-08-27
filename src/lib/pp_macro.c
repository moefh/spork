/* pp_macro.c */

#include <string.h>

#include "pp_macro.h"
#include "token.h"
#include "preprocessor.h"

struct sp_macro_def *sp_new_macro_def(struct sp_preprocessor *pp, sp_string_id name_id,
                                      bool is_function, bool last_param_is_variadic,
                                      struct sp_token_list *params, struct sp_token_list *body)
{
  int n_params = sp_token_list_size(params);
  struct sp_macro_def *macro = sp_malloc(pp->pool, sizeof(struct sp_macro_def) + n_params*sizeof(sp_string_id));
  if (! macro)
    return NULL;
  macro->name_id = name_id;
  macro->n_params = n_params;
  macro->enabled = true;
  macro->is_function = is_function;
  macro->is_variadic = last_param_is_variadic;
  macro->params = *params;
  macro->body = *body;
  
  // validate params
  int param_n = 0;
  struct sp_token *param = sp_rewind_token_list(&macro->params);
  while (sp_read_token_from_list(&macro->params, &param)) {
    if (tok_is_punct(param, PUNCT_ELLIPSIS)) {
      if (sp_peek_token_from_list(&macro->params) != NULL) {
        sp_set_pp_error(pp, "parameter '...' must be the last one");
        return NULL;
      }
      macro->is_variadic = true;
      param->type = TOK_IDENTIFIER;
      param->data.str_id = sp_add_string(&pp->ast->strings, "__VA_ARGS__");
      if (param->data.str_id < 0) {
        sp_set_pp_error(pp, "out of memory");
        return NULL;
      }
    }

    sp_string_id param_name_id = sp_get_token_string_id(param);
    for (int i = 0; i < param_n; i++)
      if (macro->param_name_ids[i] == param_name_id) {
        sp_set_pp_error(pp, "duplicate parameter name: '%s'", sp_dump_token(pp->ast, param));
        return NULL;
      }
    macro->param_name_ids[param_n++] = param_name_id;
  }

  // validate body
  int pos = 0;
  struct sp_token *tok = sp_rewind_token_list(&macro->body);
  while (sp_read_token_from_list(&macro->body, &tok)) {
    if ((! macro->is_variadic) && tok_is_identifier(tok)) {
      const char *ident = sp_get_token_string(pp->ast, tok);
      if (strcmp(ident, "__VA_ARGS__") == 0) {
        sp_set_pp_error(pp, "__VA_ARGS__ is only allowed in variadic macros");
        return NULL;
      }
    }
    
    struct sp_token *next = sp_peek_token_from_list(body);
    if (tok_is_punct(tok, PUNCT_HASHES) && (pos == 0 || next == NULL)) {
      sp_set_pp_error(pp, "## is not allowed at the start or end of macro body");
      return NULL;
    }
  
    if (tok_is_punct(tok, '#')) {
      bool next_is_some_argument = false;
      if (next && tok_is_identifier(next)) {
        for (int i = 0; i < macro->n_params; i++) {
          if (next->data.str_id == macro->param_name_ids[i]) {
            next_is_some_argument = true;
            break;
          }
        }
      }
      if (! next_is_some_argument) {
        sp_set_pp_error(pp, "# must be followed by a parameter name");
        return NULL;
      }
    }
    pos++;
  }

  sp_dump_macro(macro, pp->ast);
  return macro;
}

void sp_dump_macro(struct sp_macro_def *macro, struct sp_ast *ast)
{
  printf("#define %s", sp_get_macro_name(macro, ast));
  if (macro->is_function) {
    printf("(");
    struct sp_token *t = sp_rewind_token_list(&macro->params);
    while (sp_read_token_from_list(&macro->params, &t)) {
      printf("%s", sp_dump_token(ast, t));
      if (sp_peek_token_from_list(&macro->params))
        printf(", ");
    }
    printf(") ");
  }
  struct sp_token *t = sp_rewind_token_list(&macro->body);
  while (sp_read_token_from_list(&macro->body, &t))
    printf(" %s", sp_dump_token(ast, t));
  printf("\n");
}

const char *sp_get_macro_name(struct sp_macro_def *macro, struct sp_ast *ast)
{
  return sp_get_ast_string(ast, macro->name_id);
}

struct sp_macro_args *sp_new_macro_args(struct sp_macro_def *macro, struct sp_mem_pool *pool)
{
  int n_args = sp_token_list_size(&macro->params);
  struct sp_macro_args *args = sp_malloc(pool, sizeof(struct sp_macro_args) + n_args*sizeof(struct sp_token_list));
  if (! args)
    return NULL;
  args->pool = pool;
  args->cap = n_args;
  args->len = 0;
  for (int i = 0; i < n_args; i++)
    sp_init_token_list(&args->args[i], pool);
  return args;
}

int sp_read_macro_args(struct sp_preprocessor *pp, struct sp_macro_args *args,
                       struct sp_macro_def *macro, sp_token_reader *reader, void *reader_data)
{
  struct sp_token tok;
  if (reader(pp, &tok, reader_data) < 0)
    return -1;
  if (! tok_is_punct(&tok, '('))
    return sp_set_pp_error(pp, "expected '(', found '%s'", sp_dump_token(pp->ast, &tok));

  int paren_level = 0;
  while (true) {
    if (reader(pp, &tok, reader_data) < 0)
      return -1;
    printf("<<ARG: '%s'>>", sp_dump_token(pp->ast, &tok));
    if (paren_level == 0) {
      if (tok_is_punct(&tok, ')')) {
        if (args->len < macro->n_params)
          args->len++;
        break;
      }
      if (tok_is_punct(&tok, ',')) {
        args->len++;
        if (args->len < args->cap)
          continue;
        if (! macro->is_variadic)
          return sp_set_pp_error(pp, "too many arguments in macro invocation");
      }
    }
    if (tok_is_punct(&tok, '('))
      paren_level++;
    else if (tok_is_punct(&tok, ')'))
      paren_level--;

    int add_index = args->len;
    if (add_index >= args->cap)
      add_index = args->cap-1;
    if (sp_append_token(&args->args[add_index], &tok) < 0)
      return sp_set_pp_error(pp, "out of memory");
  }
  
  return 0;
}
