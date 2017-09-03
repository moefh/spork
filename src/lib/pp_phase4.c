/* pp_phase4.c
 *
 * Translation phase 4: execute preprocessing directives and expand macros.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#include "preprocessor.h"
#include "ast.h"
#include "pp_token.h"
#include "punct.h"

#define set_error sp_set_pp_error
static void add_pp_token_list_to_input(struct sp_preprocessor *pp, struct sp_pp_token_list *list);
static int next_processed_token(struct sp_preprocessor *pp, bool expand_macros);

#define NEXT_TOKEN()        do { if (sp_next_pp_ph3_token(pp, false) < 0) return -1; } while (0)
#define NEXT_HEADER_TOKEN() do { if (sp_next_pp_ph3_token(pp, true ) < 0) return -1; } while (0)

#define IS_TOK_TYPE(t)         (pp->tok.type == t)
#define IS_EOF()               IS_TOK_TYPE(TOK_PP_EOF)
#define IS_ENABLE_MACRO()      IS_TOK_TYPE(TOK_PP_ENABLE_MACRO)
#define IS_END_OF_ARG()        IS_TOK_TYPE(TOK_PP_END_OF_ARG)
#define IS_SPACE()             IS_TOK_TYPE(TOK_PP_SPACE)
#define IS_NEWLINE()           IS_TOK_TYPE(TOK_PP_NEWLINE)
#define IS_PP_HEADER_NAME()    IS_TOK_TYPE(TOK_PP_HEADER_NAME)
#define IS_STRING()            IS_TOK_TYPE(TOK_PP_STRING)
#define IS_IDENTIFIER()        IS_TOK_TYPE(TOK_PP_IDENTIFIER)
#define IS_PUNCT(id)           (IS_TOK_TYPE(TOK_PP_PUNCT) && pp->tok.data.punct_id == (id))

static int token_to_string(struct sp_preprocessor *pp, struct sp_pp_token *tok, char *str, size_t str_size, bool escape)
{
  switch (tok->type) {
  case TOK_PP_EOF:
  case TOK_PP_ENABLE_MACRO:
  case TOK_PP_END_OF_ARG:
  case TOK_PP_NEWLINE:
  case TOK_PP_SPACE:
  case TOK_PP_PASTE_MARKER:
    set_error(pp, "invalid token to paste");
    return -1;

  case TOK_PP_OTHER:
    if (str_size < 2) goto err;
    *str++ = (char) tok->data.other;
    break;

  case TOK_PP_IDENTIFIER:
  case TOK_PP_HEADER_NAME:
  case TOK_PP_NUMBER:
    {
      const char *src = sp_get_pp_token_string(pp, tok);
      size_t src_len = strlen(src);
      if (str_size+1 < src_len) goto err;
      memcpy(str, src, src_len);
      str += src_len;
    }
    break;
    
  case TOK_PP_CHAR_CONST:
    if (! escape) {
      const char *src = sp_get_pp_token_string(pp, tok);
      size_t src_len = strlen(src);
      if (str_size+1 < src_len) goto err;
      memcpy(str, src, src_len);
      str += src_len;
    } else {
      if (str_size < 1) goto err;
      const char *src = sp_get_pp_token_string(pp, tok);
      for (const char *s = src; *s != '\0'; s++) {
        if (*s == '\\' || *s == '"') {
          if (str_size < 2) goto err;
          *str++ = '\\';
        }
        if (str_size < 2) goto err;
        *str++ = *s;
      }
      if (str_size < 1) goto err;
    }
    break;
    
  case TOK_PP_STRING:
    if (! escape) {
      const char *src = sp_get_pp_token_string(pp, tok);
      size_t src_len = strlen(src);
      if (str_size+1 < src_len) goto err;
      memcpy(str, src, src_len);
      str += src_len;
    } else {
      if (str_size < 1) goto err;
      const char *src = sp_get_pp_token_string(pp, tok);
      for (const char *s = src; *s != '\0'; s++) {
        if (*s == '\\' || *s == '"') {
          if (str_size < 2) goto err;
          *str++ = '\\';
        }
        if (str_size < 2) goto err;
        *str++ = *s;
      }
      if (str_size < 1) goto err;
    }
    break;
    
  case TOK_PP_PUNCT:
    {
      const char *src = sp_get_punct_name(tok->data.punct_id);
      size_t src_len = strlen(src);
      if (str_size+1 < src_len) goto err;
      memcpy(str, src, src_len);
      str += src_len;
    }
    break;
  }

  *str = '\0';
  return 0;

 err:
  set_error(pp, "string too large");
  return -1;
}

static int paste_tokens(struct sp_preprocessor *pp, struct sp_pp_token *tok1, struct sp_pp_token *tok2, struct sp_pp_token *ret)
{
  //printf("PASTING ('%s'", sp_dump_pp_token(pp, tok1));
  //printf(" AND '%s')", sp_dump_pp_token(pp, tok2));
  
  if (tok1->type == TOK_PP_PASTE_MARKER) {
    *ret = *tok2;
    return 0;
  }
  if (tok2->type == TOK_PP_PASTE_MARKER) {
    *ret = *tok1;
    return 0;
  }
  
  char str[4096];
  size_t str_len = 0;
  if (token_to_string(pp, tok1, str + str_len, sizeof(str) - str_len, false) < 0)
    return -1;
  str_len += strlen(str);
  if (token_to_string(pp, tok2, str + str_len, sizeof(str) - str_len, false) < 0)
    return -1;

  return sp_string_to_pp_token(pp, str, ret);
}

static int stringify_list(struct sp_preprocessor *pp, struct sp_pp_token_list *list, struct sp_pp_token *ret)
{
  char str[4096];  // enough according to "5.2.4.1 Translation limits"
  char *cur = str;
  
  if (str + sizeof(str) <= cur+1)
    goto err;
  *cur++ = '"';

  bool last_was_space = false;
  struct sp_pp_token *tok = sp_rewind_pp_token_list(list);
  while (sp_read_pp_token_from_list(list, &tok)) {
    switch (tok->type) {
    case TOK_PP_EOF:
      if (str + sizeof(str) <= cur+1)
        goto err;
      *cur++ = ' ';
      break;

    case TOK_PP_ENABLE_MACRO:
    case TOK_PP_END_OF_ARG:
      break;

    case TOK_PP_NEWLINE:
    case TOK_PP_SPACE:
      last_was_space = true;
      break;

    default:
      if (str + sizeof(str) <= cur+1)
        goto err;
      if (last_was_space)
        *cur++ = ' ';
      if (str + sizeof(str) <= cur+1)
        goto err;
      if (token_to_string(pp, tok, cur, sizeof(str) - (cur-str), true) < 0)
        return -1;
      cur += strlen(cur);
      last_was_space = false;
      break;
    }
  }
  if (str + sizeof(str) <= cur+1)
    goto err;
  *cur++ = '"';
  if (str + sizeof(str) <= cur+1)
    goto err;
  *cur = '\0';
  ret->type = TOK_PP_STRING;
  ret->data.str_id = sp_add_string(&pp->token_strings, str);
  if (ret->data.str_id < 0)
    return set_error(pp, "out of memory");
  return 0;

 err:
  return set_error(pp, "string too large");
}

static struct sp_macro_args *read_macro_args(struct sp_preprocessor *pp, struct sp_macro_def *macro)
{
  struct sp_macro_args *args = sp_new_macro_args(macro, &pp->macro_exp_pool);
  if (! args)
    goto err_oom;
  
  pp->macro_args_reading_level++;

  //printf("READING MACRO ARGS FOR '%s'\n", sp_get_macro_name(macro, pp));

  do {
    if (next_processed_token(pp, false) < 0)
      return NULL;
  } while (IS_SPACE());
  if (! IS_PUNCT('(')) {
    abort();
    set_error(pp, "expected '(', found '%s'", sp_dump_pp_token(pp, &pp->tok));
    goto err;
  }

  int paren_level = 0;
  bool arg_start = true;
  while (true) {
    // skip initial spaces and newlines
    do {
      if (next_processed_token(pp, false) < 0)
        goto err;
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
        if (! macro->is_variadic) {
          set_error(pp, "too many arguments in macro invocation");
          goto err;
        }
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
      if (! macro->is_variadic) {
        set_error(pp, "too many arguments in macro invocation");
        goto err;
      }
      add_index = args->cap-1;
    }
    if (sp_append_pp_token(&args->args[add_index], &pp->tok) < 0)
      goto err_oom;
  }

  if (macro->is_variadic) {
    if (args->len < macro->n_params) {
      set_error(pp, "macro '%s' requires at least %d arguments, found %d", sp_get_macro_name(macro, pp), macro->n_params, args->len);
      goto err;
    }
  } else {
    if (args->len != macro->n_params) {
      set_error(pp, "macro '%s' requires %d arguments, found %d", sp_get_macro_name(macro, pp), macro->n_params, args->len);
      goto err;
    }
  }

  // append end-of-arg marker to each arg
  for (int i = 0; i < args->len; i++) {
    struct sp_pp_token eoa = ((struct sp_pp_token) { .type = TOK_PP_END_OF_ARG });
    if (sp_append_pp_token(&args->args[i], &eoa) < 0)
      goto err_oom;
  }
  
  pp->macro_args_reading_level--;
  return args;

 err_oom:
  set_error(pp, "expected '(', found '%s'", sp_dump_pp_token(pp, &pp->tok));
 err:
  return NULL;
}

static struct sp_pp_token_list *expand_arg(struct sp_preprocessor *pp, struct sp_pp_token_list *arg)
{
  struct sp_pp_token_list *exp_args = sp_new_pp_token_list(&pp->macro_exp_pool, sp_pp_token_list_size(arg));
  if (! exp_args)
    goto err_oom;

  //printf("ARG NEXT: %p\n", (void*) arg->next);
  add_pp_token_list_to_input(pp, arg);
  while (true) {
    if (next_processed_token(pp, true) < 0)
      return NULL;
    //printf("[arg] %s\n", sp_dump_pp_token(pp, &pp->tok));
    if (IS_END_OF_ARG())
      break;
    if (IS_EOF()) {
      set_error(pp, "end-of-file found while reading macro args");
      goto err;
    }
    if (sp_append_pp_token(exp_args, &pp->tok) < 0)
      goto err_oom;
  }
  return exp_args;

 err_oom:
  set_error(pp, "out of memory");
 err:
  return NULL;
}

static int append_arg_to_list(struct sp_pp_token_list *list, struct sp_pp_token_list *arg)
{
  if (sp_macro_arg_is_empty(arg)) {
    struct sp_pp_token placemarker;
    placemarker.type = TOK_PP_PASTE_MARKER;
    if (sp_append_pp_token(list, &placemarker) < 0)
      return -1;
    return 0;
  }
  
  struct sp_pp_token *t = sp_rewind_pp_token_list(arg);
  while (sp_read_pp_token_from_list(arg, &t)) {
    if (pp_tok_is_end_of_arg(t))
      break;
    if (sp_append_pp_token(list, t) < 0)
      return -1;
  }
  return 0;
}

static int append_list_to_list(struct sp_pp_token_list *dest, struct sp_pp_token_list *src)
{
  struct sp_pp_token *t = sp_rewind_pp_token_list(src);
  while (sp_read_pp_token_from_list(src, &t)) {
    if (pp_tok_is_end_of_arg(t))
      break;
    if (sp_append_pp_token(dest, t) < 0)
      return -1;
  }
  return 0;
}

static struct sp_pp_token_list *expand_macro(struct sp_preprocessor *pp, struct sp_macro_def *macro, struct sp_macro_args *args)
{
#if 0
  if (macro->is_function) {
    printf("-----------------\n");
    printf("expanding macro '%s' with %d args:\n", sp_get_macro_name(macro, pp), args->len);
    for (int i = 0; i < args->len; i++) {
      printf("[arg %d] ", i);
      sp_dump_pp_token_list(&args->args[i], pp);
      printf("\n");
    }
    printf("-----------------\n");
  }
#endif
  //printf("<%s>'s body = ", sp_get_macro_name(macro, pp)); sp_dump_pp_token_list(&macro->body, pp); printf("\n");

  struct sp_pp_token_list *macro_exp = sp_new_pp_token_list(&pp->macro_exp_pool, sp_pp_token_list_size(&macro->body));
  if (! macro_exp)
    goto err_out_of_memory;
  
  macro->enabled = false;

  // replace args, expanding as necessary
  struct sp_pp_token *t = sp_rewind_pp_token_list(&macro->body);
  while (sp_read_pp_token_from_list(&macro->body, &t)) {
    // # parameter
    if (macro->is_function && pp_tok_is_punct(t, '#')) {
      struct sp_pp_token *next = sp_peek_nonblank_pp_token_from_list(&macro->body);
      if (! next) {
        set_error(pp, "'#' must not be the end of the macro body");
        goto err;
      }
      if (pp_tok_is_identifier(next)) {
        struct sp_pp_token_list *arg = sp_get_macro_arg(macro, args, sp_get_pp_token_string_id(next));
        if (arg) {
          // keep everything up to and including next
          do {
            //printf("-> '%s'\n", sp_dump_pp_token(pp, t));
            if (sp_append_pp_token(macro_exp, t) < 0)
              goto err_out_of_memory;
            if (t == next)
              break;
          } while (sp_read_pp_token_from_list(&macro->body, &t));
          continue;
        }
      }
      set_error(pp, "'#' must be followed by argument name");
      goto err;
    }

    // ## parameter
    if (macro->is_function && pp_tok_is_punct(t, PUNCT_HASHES)) {
      struct sp_pp_token *next = sp_peek_nonblank_pp_token_from_list(&macro->body);
      if (! next) {
        set_error(pp, "'##' must not be the end of the macro body");
        goto err;
      }
      if (pp_tok_is_identifier(next)) {
        struct sp_pp_token_list *arg = sp_get_macro_arg(macro, args, sp_get_pp_token_string_id(next));
        if (arg) {
          // keep everything up to next
          do {
            if (t == next)
              break;
            //printf("-> '%s'\n", sp_dump_pp_token(pp, t));
            if (sp_append_pp_token(macro_exp, t) < 0)
              goto err_out_of_memory;
          } while (sp_read_pp_token_from_list(&macro->body, &t));
          
          // [6.10.3.3] parameter preceded by '##' is not expanded
          if (append_arg_to_list(macro_exp, arg) < 0)
            goto err_out_of_memory;
          continue;
        }
      }
    }

    // parameter
    if (macro->is_function && pp_tok_is_identifier(t)) {
      struct sp_pp_token_list *arg = sp_get_macro_arg(macro, args, sp_get_pp_token_string_id(t));
      if (arg) {
        struct sp_pp_token *hashes = sp_peek_nonblank_pp_token_from_list(&macro->body);
        if (hashes && pp_tok_is_punct(hashes, PUNCT_HASHES)) {
          // [6.10.3.3] parameter followed by '##' is not expanded
          if (append_arg_to_list(macro_exp, arg) < 0)
            goto err_out_of_memory;
        } else {
          //printf("<expanding arg for %s>", sp_get_macro_name(macro, pp));
          struct sp_pp_token_list *exp_arg = expand_arg(pp, arg);
          //printf("</expanding arg for %s>", sp_get_macro_name(macro, pp));
          if (! exp_arg)
            goto err_out_of_memory;
          if (append_list_to_list(macro_exp, exp_arg) < 0)
            goto err_out_of_memory;
        }
        continue;
      }
    }

    if (sp_append_pp_token(macro_exp, t) < 0)
      goto err_out_of_memory;
  }

  //printf("<%s>'s macro_exp = ", sp_get_macro_name(macro, pp)); sp_dump_pp_token_list(macro_exp, pp); printf("\n");

  bool has_hash = false;
  t = sp_rewind_pp_token_list(macro_exp);
  while (sp_read_pp_token_from_list(macro_exp, &t)) {
    if (pp_tok_is_punct(t, PUNCT_HASHES) || pp_tok_is_punct(t, '#')) {
      has_hash = true;
      break;
    }
  }
  
  // paste and stringify
  struct sp_pp_token_list *out;
  if (has_hash) {
    out = sp_new_pp_token_list(&pp->macro_exp_pool, sp_pp_token_list_size(macro_exp));
    t = sp_rewind_pp_token_list(macro_exp);
    while (sp_read_pp_token_from_list(macro_exp, &t)) {
      // paste
      if (! pp_tok_is_space(t)) {
        struct sp_pp_token_list_pos save_pos = sp_get_pp_token_list_pos(macro_exp);
        struct sp_pp_token *next;
        if (sp_read_nonblank_pp_token_from_list(macro_exp, &next)) {
          if (pp_tok_is_punct(next, PUNCT_HASHES)) {
            struct sp_pp_token *second;
            struct sp_pp_token pasted;
          paste_again:
            if (sp_read_nonblank_pp_token_from_list(macro_exp, &second)) {
              if (paste_tokens(pp, t, second, &pasted) < 0)
                goto err;
              //printf("pasted: '%s'", sp_dump_pp_token(pp, &pasted));
              next = sp_peek_nonblank_pp_token_from_list(macro_exp);
              if (next && pp_tok_is_punct(next, PUNCT_HASHES)) {
                sp_read_nonblank_pp_token_from_list(macro_exp, &next); // skip '##'
                t = &pasted;
                goto paste_again;
              }
              if (sp_append_pp_token(out, &pasted) < 0)
                goto err_out_of_memory;
              continue;
            }
          }
        }
        sp_set_pp_token_list_pos(macro_exp, save_pos);
      }

      // stringify
      if (macro->is_function && pp_tok_is_punct(t, '#')) {
        struct sp_pp_token *next = sp_peek_nonblank_pp_token_from_list(macro_exp);
        if (pp_tok_is_identifier(next) && sp_get_macro_arg(macro, args, sp_get_pp_token_string_id(next))) {
          if (! sp_read_nonblank_pp_token_from_list(macro_exp, &next)) {
            set_error(pp, "'#' must not be the end of the macro body");
            goto err;
          }
          struct sp_pp_token_list *arg = sp_get_macro_arg(macro, args, sp_get_pp_token_string_id(next));
          if (! arg) {
            set_error(pp, "'#' must be followed by a parameter name");
            goto err;
          }
          struct sp_pp_token stringified;
          //printf("<%s> stringifying ", sp_get_macro_name(macro, pp)); sp_dump_pp_token_list(arg, pp); printf("\n");
          if (stringify_list(pp, arg, &stringified) < 0)
            goto err;
          if (sp_append_pp_token(out, &stringified) < 0)
            goto err_out_of_memory;
          continue;
        }
      }

      // remove placemarkers
      if (pp_tok_is_paste_marker(t)) {
        continue;
      }
      
      if (sp_append_pp_token(out, t) < 0)
        goto err_out_of_memory;
    }
    //printf("<%s>'s out = ", sp_get_macro_name(macro, pp)); sp_dump_pp_token_list(out, pp); printf("\n");
  } else {
    out = macro_exp;
  }

  // remove all remaining paste markers
  bool has_paste_marker = false;
  t = sp_rewind_pp_token_list(macro_exp);
  while (sp_read_pp_token_from_list(macro_exp, &t)) {
    if (pp_tok_is_paste_marker(t)) {
      has_paste_marker = true;
      break;
    }
  }
  if (has_paste_marker) {
    struct sp_pp_token_list *out2 = sp_new_pp_token_list(&pp->macro_exp_pool, sp_pp_token_list_size(out));
    t = sp_rewind_pp_token_list(out);
    while (sp_read_pp_token_from_list(out, &t)) {
      if (pp_tok_is_paste_marker(t))
        continue;
      if (sp_append_pp_token(out2, t) < 0)
        goto err_out_of_memory;
    }
    out = out2;
  }
  
  // [6.10.3.4] 2. prevent any further expansion of an identifier with the same name as the macro being expanded
  t = sp_rewind_pp_token_list(out);
  while (sp_read_pp_token_from_list(out, &t)) {
    if (pp_tok_is_identifier(t) && t->data.str_id == macro->name_id)
      t->macro_dead = true;
  }
    
  // add marker to re-enable macro:
  struct sp_pp_token enable_macro;
  enable_macro.type = TOK_PP_ENABLE_MACRO;
  enable_macro.data.str_id = macro->name_id;
  if (sp_append_pp_token(out, &enable_macro) < 0)
    goto err_out_of_memory;
  return out;

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
  if (list == pp->in_tokens) {
    printf("\n\n");
    printf("ERROR: adding input buffer to input buffer! input buffer contains:\n");
    sp_dump_pp_token_list(list, pp);
    printf("\n\n");
    abort();
  }
#endif
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
    while (pp->in_tokens && ! sp_peek_pp_token_from_list(pp->in_tokens))
      pp->in_tokens = pp->in_tokens->next;
    
    if (pp->in_tokens) {
      struct sp_pp_token *tok;
      if (sp_read_pp_token_from_list(pp->in_tokens, &tok)) {
        pp->tok = *tok;
        //printf("* read from macro_exp: '%s'\n", sp_dump_pp_token(pp, tok));

        // we MUST ensure that any spent list is removed from pp->in_tokens:
        while (pp->in_tokens && ! sp_peek_pp_token_from_list(pp->in_tokens))
          pp->in_tokens = pp->in_tokens->next;
        if (! pp->in_tokens && pp->macro_expansion_level == 0)
          sp_clear_mem_pool(&pp->macro_exp_pool);
        
        return true;
      }
    }

    if (pp->macro_expansion_level == 0)
      sp_clear_mem_pool(&pp->macro_exp_pool);
  }
  
  return false;
}

static int next_processed_token(struct sp_preprocessor *pp, bool expand_macros)
{
  while (true) {
    if (! next_token_from_buffer(pp))
      NEXT_TOKEN();

    //if (pp->macro_args_reading_level) printf("macro arg -> '%s'\n", sp_dump_pp_token(pp, &pp->tok));

    if (IS_ENABLE_MACRO()) {
      struct sp_macro_def *macro = sp_get_idht_value(&pp->macros, sp_get_pp_token_string_id(&pp->tok));
      if (! macro)
        return set_error(pp, "internal error: enable macro for unknown macro id '%d'", sp_get_pp_token_string_id(&pp->tok));
      macro->enabled = true;
      continue;
    }
    
    if (IS_NEWLINE()) {
      if (pp->macro_args_reading_level) {
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
      if (pp->macro_args_reading_level)
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
      //printf("!!!PUNCT!!! %d\n", pp->macro_args_reading_level);
      if (! pp->at_newline) {
        pp->at_newline = false;
        pp->last_was_space = false;
        return 0;
      }
      if (pp->macro_args_reading_level)
        return set_error(pp, "preprocessing directive in macro arguments");
      if (sp_process_pp_directive(pp) < 0)
        return -1;
      pp->at_newline = true;
      pp->last_was_space = false;
      continue;
    }

    if (expand_macros && pp_tok_is_identifier(&pp->tok) && ! pp->tok.macro_dead) {
      struct sp_pp_token ident = pp->tok;
      sp_string_id ident_id = sp_get_pp_token_string_id(&ident);

      //printf("-> ident '%s' (%d)\n", sp_get_string(&pp->token_strings, ident_id), ident_id);
      
      struct sp_pp_token next;
      if (peek_nonblank_token(pp, &next) < 0)
        return -1;

      if (! pp_tok_is_punct(&next, PUNCT_HASHES)) {
        struct sp_macro_def *macro = sp_get_idht_value(&pp->macros, ident_id);
        if (macro) {
          if (! macro->enabled) {
            // kill identifier with the name of a disabled macro
            pp->tok.macro_dead = true;
          } else if (! macro->is_function || pp_tok_is_punct(&next, '(')) {
            pp->macro_expansion_level++;
            struct sp_pp_token_list *macro_exp;
            if (macro->pre_id != PP_MACRO_NOT_PREDEFINED) {
              //printf("expanding predefined macro\n");
              macro_exp = sp_expand_predefined_macro(pp, macro);
            } else {
              struct sp_macro_args *args = NULL;
              if (macro->is_function) {
                //printf("<reading args for %s>", sp_get_macro_name(macro, pp));
                args = read_macro_args(pp, macro);
                if (! args)
                  return -1;
              }
              //printf("<expanding macro %s>", sp_get_macro_name(macro, pp));
              macro_exp = expand_macro(pp, macro, args);
            }
            if (! macro_exp)
              return -1;
            add_pp_token_list_to_input(pp, macro_exp);
            pp->macro_expansion_level--;
            continue;
          }
        }
      }
    }
    
    pp->last_was_space = false;
    pp->at_newline = false;
    return 0;
  }
}

int sp_next_pp_ph4_token(struct sp_preprocessor *pp)
{
  return next_processed_token(pp, true);
}
