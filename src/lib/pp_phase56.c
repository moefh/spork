/* pp_phase56.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <errno.h>

#include "preprocessor.h"
#include "ast.h"
#include "pp_token.h"
#include "token.h"

#define set_error sp_set_pp_error

#define NEXT_TOKEN() do { if (next_token(pp) < 0) return -1; } while (0)
#define CUR  (&pp->tok_ph6)
#define NEXT (&pp->next_ph6)

static int next_token(struct sp_preprocessor *pp)
{
  if (sp_next_pp_ph4_token(pp) < 0)
    return -1;
  pp->tok_ph6 = pp->next_ph6;
  pp->next_ph6 = pp->tok;
  return 0;
}

static int init_ph6(struct sp_preprocessor *pp)
{
  if (sp_next_pp_ph4_token(pp) < 0)
    return -1;
  pp->next_ph6 = pp->tok;
  pp->init_ph6 = true;
  return 0;
}

static int get_escape_char(char ch)
{
  switch (ch) {
  case '\'': return '\'';
  case '"':  return '"';
  case '?':  return '\?';
  case '\\': return '\\';
  case 'a':  return '\a';
  case 'b':  return '\b';
  case 'f':  return '\f';
  case 'n':  return '\n';
  case 'r':  return '\r';
  case 't':  return '\t';
  case 'v':  return '\v';
  }
  return -1;
}

static int conv_string(struct sp_preprocessor *pp, struct sp_pp_token *tok, struct sp_buffer *buf, bool *is_wide)
{
  const char *str = sp_get_pp_token_string(pp, tok);
  const char *cur = str;

  if (*cur == 'L') {
    *is_wide = true;
    cur++;
  } else
    *is_wide = false;
  cur++;
  
  while (*cur != '"') {
    if (*cur == '\\') {
      int escape_char = get_escape_char(*++cur);
      if (escape_char < 0)
        return set_error(pp, "invalid escape sequence: '\\%c'", *cur);
      if (sp_buf_add_byte(buf, escape_char) < 0)
        goto err_oom;
      cur++;
    } else {
      if (sp_buf_add_byte(buf, *cur++) < 0)
        goto err_oom;
    }
  }
  return 0;

 err_oom:
  return set_error(pp, "out of memory");
}

static uint64_t read_int_const_num(struct sp_preprocessor *pp, const char *str, const char **end, int base)
{
  uint64_t ret = 0;
  const char *pos = str;

  while (*pos) {
    int digit = ((  *pos >= '0' && *pos <= '9') ? (*pos - '0')
                 : (*pos >= 'A' && *pos <= 'F') ? (*pos - 'A')
                 : (*pos >= 'a' && *pos <= 'f') ? (*pos - 'a')
                 : -1);
    if (digit < 0 || digit >= base) {
      if (pos == str) {
        set_error(pp, "invalid digit: '%c'", *pos);
        *end = NULL;
        return 0;
      }
      break;
    }
    uint64_t n = (ret * base) + digit;
    //printf("\n-> %" PRIu64 " * %d + %d = %" PRIu64 "\n", ret, base, digit, n);
    if (n % base != (uint64_t) digit || n / base != ret) {
      //printf("\n");
      //printf("** %" PRIu64 " %% %d != %d\n", n, base, digit);
      //printf("** %" PRIu64 " / %d != %" PRIu64 "d\n", n, base, ret);
      set_error(pp, "integer constant too large");
      *end = NULL;
      return 0;
    }
    ret = n;
    pos++;
  }
  *end = pos;
  return ret;
}

static int read_int_const_flags(struct sp_preprocessor *pp, const char *str, uint8_t *ret)
{
  uint8_t flags = 0;
  while (*str) {
    if (*str == 'u' || *str == 'U') {
      flags |= TOK_INT_CONST_FLAG_U;
      str++;
      continue;
    }
    if ((*str == 'l' && str[1] == 'l') || (*str == 'L' && str[1] == 'L')) {
      if (flags & TOK_INT_CONST_FLAG_L)
        return set_error(pp, "invalid integer constant suffix: can't mix 'l' and 'll'");
      flags |= TOK_INT_CONST_FLAG_LL;
      str += 2;
      continue;
    }
    if (*str == 'l' || *str == 'L') {
      if (flags & TOK_INT_CONST_FLAG_LL)
        return set_error(pp, "invalid integer constant suffix: can't mix 'l' and 'll'");
      flags |= TOK_INT_CONST_FLAG_L;
      str++;
      continue;
    }
    return set_error(pp, "invalid integer constant suffix: '%c'", *str);
  }
  //printf("<returning flags %x>", flags);
  *ret = flags;
  return 0;
}

static int read_float_const_flags(struct sp_preprocessor *pp, const char *str, uint8_t *ret)
{
  uint8_t flags = 0;
  while (*str) {
    if (*str == 'f' || *str == 'F') {
      str++;
      continue;
    }
    if (*str == 'l' || *str == 'L') {
      flags |= TOK_FLOAT_CONST_FLAG_L;
      str++;
      continue;
    }
    return set_error(pp, "invalid float constant suffix: '%c'", *str);
  }
  //printf("<returning flags %x>", flags);
  *ret = flags;
  return 0;
}

static int conv_number(struct sp_preprocessor *pp, struct sp_pp_token *tok, struct sp_token *ret)
{
  const char *str = sp_get_pp_token_string(pp, tok);

  if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
    ret->type = TOK_INT_CONST;
    const char *end;
    ret->data.int_const.n = read_int_const_num(pp, str + 2, &end, 16);
    if (end == NULL)
      return -1;
    if (read_int_const_flags(pp, end, &ret->data.int_const.flags) < 0)
      return -1;
    return 0;
  }

  if (str[0] == '0') {
    ret->type = TOK_INT_CONST;
    const char *end;
    ret->data.int_const.n = read_int_const_num(pp, str, &end, 8);
    if (end == NULL)
      return -1;
    if (read_int_const_flags(pp, end, &ret->data.int_const.flags) < 0)
      return -1;
    return 0;
  }
  
  bool only_digits = true;
  for (const char *s = str; *s != '\0'; s++)
    if (! (*s >= '0' && *s <= '9')) {
      only_digits = false;
      break;
    }

  if (only_digits) {
    ret->type = TOK_INT_CONST;
    const char *end;
    ret->data.int_const.n = read_int_const_num(pp, str, &end, 10);
    if (end == NULL)
      return -1;
    if (read_int_const_flags(pp, end, &ret->data.int_const.flags) < 0)
      return -1;
    return 0;
  }

  ret->type = TOK_FLOAT_CONST;
  char *end = NULL;
  errno = 0;
  ret->data.float_const.n = strtod(str, &end);
  if (errno != 0)
    return set_error(pp, "invalid float constant: '%s'", str);
  if (read_float_const_flags(pp, end, &ret->data.float_const.flags) < 0)
    return -1;
  return 0;
}

static int conv_pp_token(struct sp_preprocessor *pp, struct sp_pp_token *tok, struct sp_token *ret)
{
  switch (tok->type) {
  case TOK_PP_IDENTIFIER:
    ret->type = TOK_IDENTIFIER;
    ret->data.str_id = tok->data.str_id;
    return 0;

  case TOK_PP_EOF:
    ret->type = TOK_EOF;
    return 0;

  case TOK_PP_PUNCT:
    ret->type = TOK_PUNCT;
    ret->data.punct_id = tok->data.punct_id;
    return 0;

  case TOK_PP_NUMBER:
    return conv_number(pp, tok, ret);

  case TOK_PP_SPACE:
  case TOK_PP_NEWLINE:
  case TOK_PP_ENABLE_MACRO:
  case TOK_PP_END_OF_LIST:
  case TOK_PP_PASTE_MARKER:
  case TOK_PP_HEADER_NAME:
  case TOK_PP_CHAR_CONST:
  case TOK_PP_STRING:
  case TOK_PP_OTHER:
    return set_error(pp, "can't convert preprocessing token of type %d: '%s'", tok->type, sp_dump_pp_token(pp, tok));
  }

  return set_error(pp, "invalid pp token type: %d", tok->type);
}

int sp_next_token(struct sp_preprocessor *pp, struct sp_token *ret)
{
  int err;
  
  if (! pp->init_ph6 && init_ph6(pp) < 0)
    return -1;

  do {
    NEXT_TOKEN();
  } while (pp_tok_is_space(CUR) || pp_tok_is_newline(CUR));

  ret->loc = CUR->loc;
  
  // string
  if (pp_tok_is_string(CUR)) {
    pp->tmp_str_buf.size = 0;
    bool is_wide = false;

    err = conv_string(pp, CUR, &pp->tmp_str_buf, &is_wide);
    if (err < 0) return err;
    while (pp_tok_is_string(NEXT) || pp_tok_is_space(NEXT) || pp_tok_is_newline(NEXT)) {
      NEXT_TOKEN();
      if (pp_tok_is_string(CUR)) {
        bool check_wide = false;
        err = conv_string(pp, CUR, &pp->tmp_str_buf, &check_wide);
        if (err < 0) return err;
        if (check_wide != is_wide)
          return set_error(pp, "joining wide and non-wide string literals is not implemented!");  // TODO
      }
    }
    if (sp_buf_add_byte(&pp->tmp_str_buf, '\0') < 0)
      goto err_oom;
    ret->type = TOK_STRING;
    ret->data.str_literal.is_wide = is_wide;
    ret->data.str_literal.id = sp_add_string(&pp->token_strings, pp->tmp_str_buf.p);
    if (ret->data.str_literal.id < 0)
      goto err_oom;
    return 0;
  }

  // identifier or keyword
  if (pp_tok_is_identifier(CUR)) {
    enum sp_keyword_type keyword_type;
    if (sp_find_keyword(sp_get_pp_token_string(pp, CUR), &keyword_type)) {
      ret->type = TOK_KEYWORD;
      ret->data.keyword_type = keyword_type;
      return 0;
    }
    return conv_pp_token(pp, CUR, ret);
  }

  // other tokens
  return conv_pp_token(pp, CUR, ret);

 err_oom:
  return set_error(pp, "out of memory");
}
