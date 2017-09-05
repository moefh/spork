/* pp_macro.c */

#include <string.h>
#include <stdio.h>
#include <time.h>

#include "pp_macro.h"
#include "pp_token.h"
#include "preprocessor.h"
#include "punct.h"
#include "ast.h"

#define ADD_PREDEF_OBJ_MACRO(name) { PP_MACRO_ ## name, "__" # name "__" }

static const struct {
  enum sp_predefined_macro_id id;
  const char *name;
} predefined_obj_macros[] = {
  ADD_PREDEF_OBJ_MACRO(DATE),
  ADD_PREDEF_OBJ_MACRO(TIME),
  ADD_PREDEF_OBJ_MACRO(FILE),
  ADD_PREDEF_OBJ_MACRO(LINE),
  ADD_PREDEF_OBJ_MACRO(STDC),
  ADD_PREDEF_OBJ_MACRO(STDC_VERSION),
  ADD_PREDEF_OBJ_MACRO(STDC_HOSTED),
  ADD_PREDEF_OBJ_MACRO(STDC_MB_MIGHT_NEQ_WC),
};

#define ADD_PREDEF_FUNC_MACRO(name, n_params) { PP_MACRO ## name, # name, n_params }

static const struct {
  enum sp_predefined_macro_id id;
  const char *name;
  int n_params;
} predefined_func_macros[] = {
  ADD_PREDEF_FUNC_MACRO(_Pragma, 1),
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

  if (macro->pre_id != PP_MACRO_NOT_PREDEFINED) {
    printf("<predefined>\n");
  } else {
    struct sp_pp_token_list_walker w;
    struct sp_pp_token *t = sp_rewind_pp_token_list(&w, &macro->body);
    while (sp_read_pp_token_from_list(&w, &t))
      printf("%s", sp_dump_pp_token(pp, t));
    printf("\n");
  }
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
    if (! pp_tok_is_end_of_list(a))
      return false;
  }
  return true;
}

static int add_predefined_obj_macro(struct sp_preprocessor *pp, enum sp_predefined_macro_id pre_macro_id, const char *name)
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

static int add_predefined_func_macro(struct sp_preprocessor *pp, enum sp_predefined_macro_id pre_macro_id, const char *name, int n_params)
{
  sp_string_id name_id = sp_add_string(&pp->token_strings, name);
  if (name_id < 0)
    return -1;

  struct sp_pp_token_list list;
  sp_init_pp_token_list(&list, pp->pool, n_params);
  for (int i = 0; i < n_params; i++) {
    char param_name[2] = { 'a' + i, '\0' };
    struct sp_pp_token param;
    param.type = TOK_PP_IDENTIFIER;
    param.data.str_id = sp_add_string(&pp->token_strings, param_name);
    if (param.data.str_id < 0)
      return -1;
    if (sp_append_pp_token(&list, &param) < 0)
      return -1;
  }
  struct sp_macro_def *macro = sp_new_macro_def(pp, name_id, true, false,
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
  for (int i = 0; i < ARRAY_SIZE(predefined_obj_macros); i++)
    if (add_predefined_obj_macro(pp, predefined_obj_macros[i].id, predefined_obj_macros[i].name) < 0)
      goto err;
  for (int i = 0; i < ARRAY_SIZE(predefined_func_macros); i++)
    if (add_predefined_func_macro(pp, predefined_func_macros[i].id, predefined_func_macros[i].name, predefined_func_macros[i].n_params) < 0)
      goto err;
  return 0;

 err:
  return sp_set_pp_error(pp, "out of memory");
}

static void escape_file_name(char *dest, size_t dest_len, const char *src)
{
  size_t p = 0;

  dest[p++] = '"';
  while (*src && p+2 < dest_len) {
    if (*src == '\\' || *src == '"') {
      if (p+3 >= dest_len)
        break;
      dest[p++] = '\\';
      dest[p++] = *src++;
    } else
      dest[p++] = *src++;
  }
  if (p+2 > dest_len)
    p = dest_len - 2;
  dest[p++] = '"';
  dest[p++] = '\0';
}

static int get_current_datetime(struct sp_preprocessor *pp, struct sp_src_loc loc)
{
  if (pp->date_str_id >= 0 && pp->time_str_id >= 0)
    return 0;
  
  static const char *month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  time_t cur_ts = time(NULL);
  if (cur_ts == (time_t) -1)
    return sp_set_pp_error_at(pp, loc, "can't read current time");
  struct tm *cur_tm = localtime(&cur_ts);
  if (! cur_tm)
    return sp_set_pp_error_at(pp, loc, "can't read current time");
  char str[256];
  
  if (pp->date_str_id < 0) {
    snprintf(str, sizeof(str), "\"%s %2d %4d\"", month_names[cur_tm->tm_mon], cur_tm->tm_mday, cur_tm->tm_year + 1900);
    pp->date_str_id = sp_add_string(&pp->token_strings, str);
    if (pp->date_str_id < 0)
      goto err_oom;
  }

  if (pp->time_str_id < 0) {
    snprintf(str, sizeof(str), "\"%02d:%02d:%02d\"", cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
    pp->time_str_id = sp_add_string(&pp->token_strings, str);
    if (pp->time_str_id < 0)
      goto err_oom;
  }

  return 0;

 err_oom:
  return sp_set_pp_error_at(pp, loc, "out of memory");
}

struct sp_pp_token_list *sp_expand_predefined_macro(struct sp_preprocessor *pp, struct sp_macro_def *macro, struct sp_macro_args *args, struct sp_src_loc loc)
{
  static char str[256];
  
  UNUSED(args);
  
  struct sp_pp_token tok;
  switch (macro->pre_id) {
  case PP_MACRO_NOT_PREDEFINED:
    tok.type = TOK_PP_EOF;
    break;

  case PP_MACRO_Pragma:
    tok.type = TOK_PP_EOF;
    break;

  case PP_MACRO_LINE:
    tok.type = TOK_PP_NUMBER;
    snprintf(str, sizeof(str), "%d", loc.line);
    tok.data.str_id = sp_add_string(&pp->token_strings, str);
    if (tok.data.str_id < 0)
      goto err_oom;
    break;

  case PP_MACRO_FILE:
    tok.type = TOK_PP_STRING;
    escape_file_name(str, sizeof(str), sp_get_ast_file_name(pp->ast, loc.file_id));
    tok.data.str_id = sp_add_string(&pp->token_strings, str);
    if (tok.data.str_id < 0)
      goto err_oom;
    break;

  case PP_MACRO_DATE:
    tok.type = TOK_PP_STRING;
    if (get_current_datetime(pp, loc) < 0)
      return NULL;
    tok.data.str_id = pp->date_str_id;
    break;

  case PP_MACRO_TIME:
    tok.type = TOK_PP_STRING;
    if (get_current_datetime(pp, loc) < 0)
      return NULL;
    tok.data.str_id = pp->time_str_id;
    break;
    
  case PP_MACRO_STDC:
  case PP_MACRO_STDC_VERSION:
  case PP_MACRO_STDC_HOSTED:
  case PP_MACRO_STDC_MB_MIGHT_NEQ_WC:
    // TODO: update these when we're conforming
    tok.type = TOK_PP_NUMBER;
    tok.data.str_id = sp_add_string(&pp->token_strings, "0");
    if (tok.data.str_id < 0)
      goto err_oom;
    break;
  }

  struct sp_pp_token_list *list = sp_new_pp_token_list(&pp->macro_exp_pool, 1);
  if (! list)
    goto err_oom;
  if (tok.type != TOK_PP_EOF) {
    if (sp_append_pp_token(list, &tok) < 0)
      goto err_oom;
  }
  return list;
  
 err_oom:
  sp_set_pp_error_at(pp, loc, "out of memory");
  return NULL;
}
