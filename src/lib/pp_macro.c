/* pp_macro.c */

#include <string.h>
#include <stdio.h>

#include "pp_macro.h"
#include "pp_token.h"
#include "preprocessor.h"
#include "punct.h"

#define ADD_PREDEF_MACRO(name) { PP_MACRO_ ## name, "__" # name "__" }

static const struct {
  enum sp_predefined_macro_id id;
  const char *name;
} predefined_macros[] = {
  ADD_PREDEF_MACRO(DATE),
  ADD_PREDEF_MACRO(TIME),
  ADD_PREDEF_MACRO(FILE),
  ADD_PREDEF_MACRO(LINE),
  ADD_PREDEF_MACRO(STDC),
  ADD_PREDEF_MACRO(STDC_VERSION),
  ADD_PREDEF_MACRO(STDC_HOSTED),
  ADD_PREDEF_MACRO(STDC_MB_MIGHT_NEQ_WC),
};


static int validate_macro_params(struct sp_macro_def *macro, struct sp_preprocessor *pp)
{
  int param_n = 0;
  struct sp_pp_token_list_walker w;
  struct sp_pp_token *param = sp_rewind_pp_token_list(&w, &macro->params);
  while (sp_read_pp_token_from_list(&w, &param)) {
    if (pp_tok_is_punct(param, PUNCT_ELLIPSIS)) {
      if (sp_peek_pp_token_from_list(&w) != NULL)
        return sp_set_pp_error(pp, "parameter '...' must be the last one");
      macro->is_variadic = true;
      param->type = TOK_PP_IDENTIFIER;
      param->data.str_id = sp_add_string(&pp->token_strings, "__VA_ARGS__");
      if (param->data.str_id < 0)
        return sp_set_pp_error(pp, "out of memory");
    }

    sp_string_id param_name_id = sp_get_pp_token_string_id(param);
    for (int i = 0; i < param_n; i++)
      if (macro->param_name_ids[i] == param_name_id)
        return sp_set_pp_error(pp, "duplicate parameter name: '%s'", sp_dump_pp_token(pp, param));
    macro->param_name_ids[param_n++] = param_name_id;
  }
  return 0;
}

static int validate_macro_body(struct sp_macro_def *macro, struct sp_preprocessor *pp)
{
  int pos = 0;
  struct sp_pp_token_list_walker w;
  struct sp_pp_token *tok = sp_rewind_pp_token_list(&w, &macro->body);
  while (sp_read_pp_token_from_list(&w, &tok)) {
    if ((! macro->is_variadic || macro->is_named_variadic) && pp_tok_is_identifier(tok)) {
      const char *ident = sp_get_pp_token_string(pp, tok);
      if (strcmp(ident, "__VA_ARGS__") == 0)
        return sp_set_pp_error(pp, "__VA_ARGS__ is only allowed in variadic macros");
    }
    
    struct sp_pp_token *next = sp_peek_nonblank_pp_token_from_list(&w);
    if (pp_tok_is_punct(tok, PUNCT_HASHES) && (pos == 0 || next == NULL))
      return sp_set_pp_error(pp, "## is not allowed at the start or end of macro body");
  
    if (macro->is_function && pp_tok_is_punct(tok, '#')) {
      bool next_is_some_argument = false;
      if (next && pp_tok_is_identifier(next)) {
        for (int i = 0; i < macro->n_params; i++) {
          if (sp_get_pp_token_string_id(next) == macro->param_name_ids[i]) {
            next_is_some_argument = true;
            break;
          }
        }
      }
      if (! next_is_some_argument)
        return sp_set_pp_error(pp, "'#' must be followed by a parameter name, found '%s'", sp_dump_pp_token(pp, next));
    }
    pos++;
  }
  return 0;
}

struct sp_macro_def *sp_new_macro_def(struct sp_preprocessor *pp, sp_string_id name_id,
                                      bool is_function, bool is_variadic, bool is_named_variadic,
                                      struct sp_pp_token_list *params, struct sp_pp_token_list *body)
{
  int n_params = sp_pp_token_list_size(params);
  struct sp_macro_def *macro = sp_malloc(pp->pool, sizeof(struct sp_macro_def) + n_params*sizeof(sp_string_id));
  if (! macro)
    return NULL;
  macro->pre_id = PP_MACRO_NOT_PREDEFINED;
  macro->name_id = name_id;
  macro->n_params = n_params;
  macro->enabled = true;
  macro->is_function = is_function;
  macro->is_variadic = is_variadic;
  macro->is_named_variadic = is_named_variadic;
  macro->params = *params;
  macro->body = *body;

  if (validate_macro_params(macro, pp) < 0)
    return NULL;
  if (validate_macro_body(macro, pp) < 0)
    return NULL;

  return macro;
}

bool sp_macros_are_equal(struct sp_macro_def *m1, struct sp_macro_def *m2)
{
  if (m1->is_function != m2->is_function
      || m1->is_variadic != m2->is_variadic)
    return false;
  
  if (! sp_pp_token_lists_are_equal(&m1->params, &m2->params))
    return false;
  if (! sp_pp_token_lists_are_equal(&m1->body, &m2->body))
    return false;
  return true;
}

void sp_dump_macro(struct sp_macro_def *macro, struct sp_preprocessor *pp)
{
  printf("#define %s", sp_get_macro_name(macro, pp));
  if (macro->is_function) {
    printf("(");
    struct sp_pp_token_list_walker w;
    struct sp_pp_token *t = sp_rewind_pp_token_list(&w, &macro->params);
    while (sp_read_pp_token_from_list(&w, &t)) {
      if (pp_tok_is_identifier(t) && strcmp(sp_get_pp_token_string(pp, t), "__VA_ARGS__") == 0)
        printf("...");
      else {
        printf("%s", sp_dump_pp_token(pp, t));
        if (sp_peek_pp_token_from_list(&w))
          printf(", ");
        else if (macro->is_named_variadic)
          printf("...");
      }
    }
    printf(") ");
  } else {
    printf(" ");
  }
  struct sp_pp_token_list_walker w;
  struct sp_pp_token *t = sp_rewind_pp_token_list(&w, &macro->body);
  while (sp_read_pp_token_from_list(&w, &t))
    printf("%s", sp_dump_pp_token(pp, t));
  printf("\n");
}

const char *sp_get_macro_name(struct sp_macro_def *macro, struct sp_preprocessor *pp)
{
  return sp_get_string(&pp->token_strings, macro->name_id);
}

struct sp_macro_args *sp_new_macro_args(struct sp_macro_def *macro, struct sp_mem_pool *pool)
{
  int n_args = sp_pp_token_list_size(&macro->params);
  struct sp_macro_args *args = sp_malloc(pool, sizeof(struct sp_macro_args) + n_args*sizeof(struct sp_pp_token_list));
  if (! args)
    return NULL;
  args->pool = pool;
  args->cap = n_args;
  args->len = 0;
  for (int i = 0; i < n_args; i++)
    sp_init_pp_token_list(&args->args[i], pool, 16);
  return args;
}

struct sp_pp_token_list *sp_get_macro_arg(struct sp_macro_def *macro, struct sp_macro_args *args, sp_string_id param_name_id)
{
  for (int i = 0; i < macro->n_params; i++) {
    if (param_name_id == macro->param_name_ids[i])
      return &args->args[i];
  }
  return NULL;
}

bool sp_macro_arg_is_empty(struct sp_pp_token_list *arg)
{
  struct sp_pp_token_list_walker w;
  struct sp_pp_token *a = sp_rewind_pp_token_list(&w, arg);
  while (sp_read_pp_token_from_list(&w, &a)) {
    if (! pp_tok_is_end_of_arg(a))
      return false;
  }
  return true;
}

static int add_predefined_macro(struct sp_preprocessor *pp, enum sp_predefined_macro_id pre_macro_id, const char *name)
{
  sp_string_id name_id = sp_add_string(&pp->token_strings, name);
  if (name_id < 0)
    return -1;

  struct sp_pp_token_list list;
  sp_init_pp_token_list(&list, pp->pool, 1);
  struct sp_macro_def *macro = sp_new_macro_def(pp, name_id, false, false,
                                                false, &list, &list);
  if (! macro)
    return -1;
  macro->pre_id = pre_macro_id;
  
  if (sp_add_idht_entry(&pp->macros, name_id, macro) < 0)
    return -1;
  return 0;
}

int sp_add_predefined_macros(struct sp_preprocessor *pp)
{
  for (int i = 0; i < ARRAY_SIZE(predefined_macros); i++)
    if (add_predefined_macro(pp, predefined_macros[i].id, predefined_macros[i].name) < 0)
      return -1;
  return 0;
}

struct sp_pp_token_list *sp_expand_predefined_macro(struct sp_preprocessor *pp, struct sp_macro_def *macro)
{
  struct sp_pp_token tok;
  switch (macro->pre_id) {
  case PP_MACRO_NOT_PREDEFINED:
    tok.type = TOK_PP_SPACE;
    break;
    
  case PP_MACRO_DATE:
  case PP_MACRO_TIME:
  case PP_MACRO_FILE:
  case PP_MACRO_LINE:
  case PP_MACRO_STDC:
  case PP_MACRO_STDC_VERSION:
  case PP_MACRO_STDC_HOSTED:
  case PP_MACRO_STDC_MB_MIGHT_NEQ_WC:
    tok.type = TOK_PP_NUMBER;
    tok.data.str_id = sp_add_string(&pp->token_strings, "42");
    if (tok.data.str_id < 0)
      goto err_oom;
    break;
  }

  struct sp_pp_token_list *list = sp_new_pp_token_list(&pp->macro_exp_pool, 1);
  if (! list)
    goto err_oom;
  if (sp_append_pp_token(list, &tok) < 0)
    goto err_oom;
  return list;
  
 err_oom:
  sp_set_pp_error(pp, "out of memory");
  return NULL;
}
