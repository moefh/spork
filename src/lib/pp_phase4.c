/* pp_phase123.c
 *
 * Translation phase 4a: execute preprocessing directives.
 */

#define _POSIX_C_SOURCE 199309L
#include <time.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#include "preprocessor.h"
#include "ast.h"
#include "pp_token.h"

#define set_error sp_set_pp_error

#define NEXT_TOKEN()        do { if (sp_next_pp_ph3_token(pp, false) < 0) return -1; } while (0)
#define NEXT_HEADER_TOKEN() do { if (sp_next_pp_ph3_token(pp, true ) < 0) return -1; } while (0)

#define IS_TOK_TYPE(t)         (pp->tok.type == t)
#define IS_EOF()               IS_TOK_TYPE(TOK_PP_EOF)
#define IS_ENABLE_MACRO()      IS_TOK_TYPE(TOK_PP_ENABLE_MACRO)
#define IS_SPACE()             IS_TOK_TYPE(TOK_PP_SPACE)
#define IS_NEWLINE()           IS_TOK_TYPE(TOK_PP_NEWLINE)
#define IS_PP_HEADER_NAME()    IS_TOK_TYPE(TOK_PP_HEADER_NAME)
#define IS_STRING()            IS_TOK_TYPE(TOK_PP_STRING)
#define IS_IDENTIFIER()        IS_TOK_TYPE(TOK_PP_IDENTIFIER)
#define IS_PUNCT(id)           (IS_TOK_TYPE(TOK_PP_PUNCT) && pp->tok.data.punct_id == (id))

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

    case TOK_PP_ENABLE_MACRO:
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

static int read_macro_args(struct sp_preprocessor *pp, struct sp_macro_def *macro, struct sp_macro_args *args)
{
  pp->reading_macro_args = true;

  //printf("READING MACRO ARGS FOR '%s'\n", sp_get_macro_name(macro, pp));

  do {
    if (sp_next_pp_ph4_token(pp) < 0)
      return -1;
  } while (IS_SPACE());
  if (! IS_PUNCT('('))
    return set_error(pp, "expected '(', found '%s'", sp_dump_pp_token(pp, &pp->tok));

  int paren_level = 0;
  bool arg_start = true;
  while (true) {
    //static const struct timespec t = ((struct timespec) { .tv_sec = 0, .tv_nsec = 1 });
    //nanosleep(&t, NULL);
    
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

static struct sp_pp_token_list *expand_arg(struct sp_preprocessor *pp, struct sp_pp_token_list *arg)
{
  // TODO: expand argument (6.10.3.1)
  UNUSED(pp);
  return arg;
}

static struct sp_pp_token_list *expand_macro(struct sp_preprocessor *pp, struct sp_macro_def *macro, struct sp_macro_args *args)
{
  //printf("expanding macro '%s' with %d args\n", sp_get_macro_name(macro, pp), args->len);

  struct sp_pp_token_list *macro_exp = sp_new_pp_token_list(&pp->macro_exp_pool, sp_pp_token_list_size(&macro->body));
  if (! macro_exp)
    goto err_out_of_memory;
  
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
        if (! arg) {
          set_error(pp, "'#' must be followed by a parameter name");
          return NULL;
        }
      }
      struct sp_pp_token str;
      if (stringify_arg(pp, arg, &str) < 0)
        goto err;
      if (sp_append_pp_token(macro_exp, &str) < 0)
        goto err_out_of_memory;
      continue;
    }
    
    if (pp_tok_is_identifier(t)) {
      struct sp_pp_token_list *arg = sp_get_macro_arg(macro, args, sp_get_pp_token_string_id(t));
      if (arg) {
        struct sp_pp_token_list *exp_arg = expand_arg(pp, arg);
        struct sp_pp_token *a = sp_rewind_pp_token_list(exp_arg);
        while (sp_read_pp_token_from_list(exp_arg, &a)) {
          if (sp_append_pp_token(macro_exp, a) < 0)
            goto err_out_of_memory;
        }
        continue;
      }
    }
    if (sp_append_pp_token(macro_exp, t) < 0)
      goto err_out_of_memory;
  }

  // add marker to re-enable macro:
  struct sp_pp_token enable_macro;
  enable_macro.type = TOK_PP_ENABLE_MACRO;
  enable_macro.data.str_id = macro->name_id;
  if (sp_append_pp_token(macro_exp, &enable_macro) < 0)
    goto err_out_of_memory;
  return macro_exp;

 err_out_of_memory:
  set_error(pp, "out of memory");
 err:
  return NULL;
}

void dump_input_buffer(struct sp_preprocessor *pp)
{
  if (! pp->in_tokens)
    return;

  struct sp_pp_token_list *tokens = pp->in_tokens;
  while (tokens) {
    struct sp_pp_token_list_pos save_pos = sp_get_pp_token_list_pos(tokens);
    struct sp_pp_token *next;
    while (sp_read_pp_token_from_list(tokens, &next))
      printf("%s", sp_dump_pp_token(pp, next));
    sp_set_pp_token_list_pos(tokens, save_pos);
    tokens = tokens->next;
  }
}

static void add_pp_token_list_to_input(struct sp_preprocessor *pp, struct sp_pp_token_list *list)
{
  sp_rewind_pp_token_list(list);
#if 0
  printf("\n");
  printf("=============================================================\n");
  printf("adding list to input buffer, input buffer currently contains:\n");
  printf("===[");
  dump_input_buffer(pp);
  printf("]===\n");
  printf("=============================================================\n");
#endif
  list->next = pp->in_tokens;
  pp->in_tokens = list;
}

static int peek_nonblank_token(struct sp_preprocessor *pp, struct sp_pp_token *tok)
{
  // peek in buffer
  if (pp->in_tokens) {
    bool found = false;
    struct sp_pp_token_list *tokens = pp->in_tokens;
    while (tokens) {
      struct sp_pp_token_list_pos save_pos = sp_get_pp_token_list_pos(tokens);
      struct sp_pp_token *next;
      while (sp_read_pp_token_from_list(tokens, &next)) {
        if (! pp_tok_is_space(next) && ! pp_tok_is_newline(next) && ! pp_tok_is_enable_macro(next)) {
          //printf("NEXT NONBLANK: '%s' (type %d)\n", sp_dump_pp_token(pp, next), next->type);
          *tok = *next;
          found = true;
          break;
        }
      }
      sp_set_pp_token_list_pos(tokens, save_pos);
      if (found)
        break;
      tokens = tokens->next;
    }
    
    if (found)
      return 0;
  }

  // peek in input
  return sp_peek_nonblank_pp_ph3_token(pp, tok, false);
}

static bool next_token_from_buffer(struct sp_preprocessor *pp)
{
  if (pp->in_tokens) {
    struct sp_pp_token *tok;
    if (sp_read_pp_token_from_list(pp->in_tokens, &tok)) {
      pp->tok = *tok;
      while (pp->in_tokens && ! sp_peek_pp_token_from_list(pp->in_tokens))
        pp->in_tokens = pp->in_tokens->next;
      //printf("* read from macro_exp: '%s'\n", sp_dump_pp_token(pp, tok));
      return true;
    }
    while (pp->in_tokens && ! sp_peek_pp_token_from_list(pp->in_tokens))
      pp->in_tokens = pp->in_tokens->next;
    //printf("* macro_exp terminated\n");
  }
  return false;
}

int sp_next_pp_ph4_token(struct sp_preprocessor *pp)
{
  while (true) {
    if (! next_token_from_buffer(pp))
      NEXT_TOKEN();

    //if (pp->reading_macro_args) printf("macro arg -> '%s'\n", sp_dump_pp_token(pp, &pp->tok));

    if (IS_ENABLE_MACRO()) {
      struct sp_macro_def *macro = sp_get_idht_value(&pp->macros, sp_get_pp_token_string_id(&pp->tok));
      if (! macro)
        return set_error(pp, "internal error: enable macro for unknown macro id '%d'", sp_get_pp_token_string_id(&pp->tok));
      macro->enabled = true;
      continue;
    }
    
    if (IS_NEWLINE()) {
      if (pp->reading_macro_args) {
        if (pp->last_was_space)
          continue;
        pp->tok.type = TOK_PP_SPACE;
        pp->last_was_space = true;
      } else
        pp->at_newline = true;
      return 0;
    }

    if (IS_SPACE()) {
      if (pp->last_was_space)
        continue;
      pp->last_was_space = true;
      return 0;
    }

    if (IS_EOF()) {
      if (pp->reading_macro_args)
        return set_error(pp, "unterminated macro argument list");
      if (! pp->in->next)
        return 0;
      struct sp_input *next = pp->in->next;
      sp_free_input(pp->in);
      pp->in = next;
      pp->at_newline = true;
      pp->last_was_space = false;
      continue;
    }
    
    if (IS_PUNCT('#')) {
      //printf("!!!PUNCT!!! %d\n", pp->reading_macro_args);
      if (! pp->at_newline)
        return 0;
      if (pp->reading_macro_args)
        return set_error(pp, "preprocessing directive in macro arguments");
      if (sp_process_pp_directive(pp) < 0)
        return -1;
      pp->at_newline = true;
      pp->last_was_space = false;
      continue;
    }

    if (! pp->reading_macro_args && pp_tok_is_identifier(&pp->tok)) {
      struct sp_pp_token ident = pp->tok;
      sp_string_id ident_id = sp_get_pp_token_string_id(&ident);

      //printf("-> ident '%s' (%d)\n", sp_get_string(&pp->token_strings, ident_id), ident_id);
      
      struct sp_pp_token next;
      if (peek_nonblank_token(pp, &next) < 0)
        return -1;

      //if (ident_id == 2) printf("deciding on expansion: next nonblank is '%s'\n", sp_dump_pp_token(pp, &next));
        
      if (! pp_tok_is_punct(&next, PUNCT_HASHES)) {
        struct sp_macro_def *macro = sp_get_idht_value(&pp->macros, ident_id);
        if (macro && macro->enabled && (! macro->is_function || pp_tok_is_punct(&next, '('))) {
          struct sp_pp_token_list *macro_exp;
          if (macro->is_function) {
            struct sp_macro_args *args = sp_new_macro_args(macro, &pp->macro_exp_pool);
            if (read_macro_args(pp, macro, args) < 0)
              return -1;
            //printf("expanding function macro\n");
            macro_exp = expand_macro(pp, macro, args);
          } else {
            //printf("expanding object macro\n");
            macro_exp = &macro->body;
          }
          if (! macro_exp)
            return -1;
          add_pp_token_list_to_input(pp, macro_exp);
          continue;
        }
      }
    }
    
    pp->last_was_space = false;
    pp->at_newline = false;
    return 0;
  }
}
