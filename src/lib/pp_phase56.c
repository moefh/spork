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
#include "pp_token_list.h"
#include "token.h"

#define set_error    sp_set_pp_error
#define set_error_at sp_set_pp_error_at

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

static int32_t get_oct_escape_char(const unsigned char **p)
{
  const unsigned char *cur = *p;

  uint32_t ret = 0;
  while (*cur >= '0' && *cur <= '7') {
    ret = (ret<<3) | (uint32_t) (*cur-'0');
    cur++;
  }
  if (ret > INT32_MAX)
    return -1;
  *p = cur;
  return (int32_t) ret;
}

static int32_t get_hex_escape_char(const unsigned char **p)
{
  uint32_t ret = 0;
  const unsigned char *cur = *p;
  while (true) {
    uint32_t old = ret;
    if (*cur >= '0' && *cur <= '9') {
      ret = (ret<<4) | (*cur-'0');
    } else if (*cur >= 'A' && *cur <= 'F') {
      ret = (ret<<4) | ((*cur-'A') + 10);
    } else if (*cur >= 'a' && *cur <= 'f') {
      ret = (ret<<4) | ((*cur-'a') + 10);
    } else
      break;
    if (old > 0x00ffffffu)
      return -1;
    cur++;
  }
  if (ret > INT32_MAX)
    return -1;
  *p = cur;
  return (int32_t) ret;
}

static int32_t get_u_escape_char(const unsigned char **p, int len)
{
  uint32_t ret = 0;
  const unsigned char *cur = *p;
  while (len-- > 0) {
    if (*cur >= '0' && *cur <= '9') {
      ret = (ret<<4) | (*cur-'0');
    } else if (*cur >= 'A' && *cur <= 'F') {
      ret = (ret<<4) | ((*cur-'A') + 10);
    } else if (*cur >= 'a' && *cur <= 'f') {
      ret = (ret<<4) | ((*cur-'a') + 10);
    } else
      break;
    cur++;
  }
  if (len != -1 || ret > INT32_MAX || ret > 0x10ffff)
    return -1;
  *p = cur;
  return (int32_t) ret;
}

static int32_t convert_escape_sequence(const unsigned char **p, bool *is_uchar)
{
  if (**p == 'u' || **p == 'U') {
    int len = (**p == 'u') ? 4 : 8;
    (*p)++;
    if (is_uchar)
      *is_uchar = true;
    return get_u_escape_char(p, len);
  }

  if (is_uchar)
    *is_uchar = false;
  if (**p == 'x') {
    (*p)++;
    return get_hex_escape_char(p);
  }
  
  if (**p >= '0' && **p <= '7')
    return get_oct_escape_char(p);
  
  int32_t ch = get_escape_char(**p);
  if (ch >= 0) {
    (*p)++;
    return ch;
  }
  return -1;
}

static int conv_char_const(struct sp_preprocessor *pp, struct sp_pp_token *tok, struct sp_token *ret)
{
  const char *str = sp_get_pp_token_string(pp, tok);
  const unsigned char *cur = (const unsigned char *) str;

  bool is_wide = false;
  if (*cur == 'L') {
    is_wide = true;
    cur++;
  }
  cur++;

  int32_t ch_val = 0;
  int shift = 0;
  while (*cur != '\'') {
    if (*cur == '\\') {
      cur++;
      int val = convert_escape_sequence(&cur, NULL);
      if (val < 0)
        goto err_invalid;
      if (val != 0) {
        if (shift >= 32 || (is_wide && shift != 0))
          return set_error_at(pp, tok->loc, "character value out of range");
        ch_val |= (int32_t) val << shift;
      }
    } else {
      if (*cur != '\0') {
        if (shift >= 32 || (is_wide && shift != 0))
          return set_error_at(pp, tok->loc, "character value out of range");
        ch_val |= (int32_t) *cur << shift;
      }
      cur++;
    }
    shift += 8;
  }

  ret->type = TOK_CHAR_CONST;
  ret->data.char_const.ch = ch_val;
  ret->data.char_const.is_wide = is_wide;
  return 0;

 err_invalid:
  return set_error_at(pp, tok->loc, "invalid escape sequence");
}

static void add_wide_str_value(void *data, size_t *ppos, uint32_t val)
{
  if (data)
    *((uint32_t *)data + *ppos) = val;
  (*ppos)++;
}

static void add_char_str_byte(void *data, size_t *ppos, uint32_t val)
{
  if (data)
    *((uint8_t *)data + *ppos) = val;
  (*ppos)++;
}

static void add_char_str_value(void *data, size_t *ppos, uint32_t val)
{
  if (! data) {
    if (val <= 0x7f) {
      (*ppos)++;
    } else if (val <= 0x7ff) {
      (*ppos) += 2;
    } else if (val <= 0xffff) {
      (*ppos) += 3;
    } else if (val <= 0x10ffff) {
      (*ppos) += 4;
    }
    return;
  }
  
  size_t pos = *ppos;
  if (val <= 0x7f) {
    ((uint8_t *)data)[pos++] = val;
  } else if (val <= 0x7ff) {
    ((uint8_t *)data)[pos++] = 0xC0 | (val >> 6);
    ((uint8_t *)data)[pos++] = 0x80 | (val & 0x3f);
  } else if (val <= 0xffff) {
    ((uint8_t *)data)[pos++] = 0xE0 | ((val >> 12) & 0x3f);
    ((uint8_t *)data)[pos++] = 0x80 | ((val >>  6) & 0x3f);
    ((uint8_t *)data)[pos++] = 0x80 | ((val      ) & 0x3f);
  } else if (val <= 0x10ffff) {
    ((uint8_t *)data)[pos++] = 0xF0 | ((val >> 18) & 0x3f);
    ((uint8_t *)data)[pos++] = 0x80 | ((val >> 12) & 0x3f);
    ((uint8_t *)data)[pos++] = 0x80 | ((val >>  6) & 0x3f);
    ((uint8_t *)data)[pos++] = 0x80 | ((val      ) & 0x3f);
  }
  *ppos = pos;
}

static int conv_string(struct sp_preprocessor *pp, struct sp_src_loc loc, const char *src, void *dest, size_t *dest_len, bool is_wide)
{
  const unsigned char *str = (const unsigned char *)src;
  
  if (*str == 'L')
    str++;
  str++;
  size_t n_chars = 0;
  while (*str != '"') {
    int32_t val;
    bool is_uchar;
    if (*str == '\\') {
      str++;
      is_uchar = false;
      val = convert_escape_sequence(&str, &is_uchar);
      if (val < 0)
        goto err_invalid;
      if (! is_wide && ! is_uchar && val > 0xff)
        return set_error_at(pp, loc, "character value out of range");
    } else {
      // utf-8
      is_uchar = true;
      if ((str[0] & 0x80) == 0) {
        val = str[0];
        str++;
      } else if ((str[0] & 0xE0) == 0xc0) {
        if ((str[1] & 0xc0) != 0x80)
          goto err_bad_utf8;
        val = ((str[0] & 0x1f) << 6) | (str[1] & 0x3f);
        str += 2;
      } else if ((str[0] & 0xf0) == 0xe0) {
        if ((str[1] & 0xc0) != 0x80 || (str[2] & 0xc0) != 0x80)
          goto err_bad_utf8;
        val = ((str[0] & 0x0f) << 12) | ((str[1] & 0x3f) << 6) | (str[2] & 0x3f);
        str += 3;
      } else if ((str[0] & 0xf8) == 0xf0) {
        if ((str[1] & 0xc0) != 0x80 || (str[2] & 0xc0) != 0x80 || (str[3] & 0xc0) != 0x80)
          goto err_bad_utf8;
        val = ((str[0] & 0x07) << 18) | ((str[1] & 0x3f) << 12) | ((str[2] & 0x3f) << 6) | (str[3] & 0x3f);
        str += 4;
      } else
        goto err_bad_utf8;
    }

    if (is_wide) {
      add_wide_str_value(dest, &n_chars, val);
    } else if (is_uchar) {
      add_char_str_value(dest, &n_chars, val);
    } else {
      add_char_str_byte(dest, &n_chars, val);
    }
  }
  *dest_len = n_chars;
  return 0;

 err_bad_utf8:
  return set_error_at(pp, loc, "invalid utf-8 string");
  
 err_invalid:
  return set_error_at(pp, loc, "invalid escape sequence");
}

static int join_string_literals(struct sp_preprocessor *pp, struct sp_pp_token_list *strings, struct sp_pp_token *tok, struct sp_token *ret)
{
  void *data;
  size_t data_len = 0;
  bool is_wide = false;
  
  if (! strings) {
    // convert a single token
    const char *str = sp_get_pp_token_string(pp, tok);
    is_wide = str[0] == 'L';
    if (conv_string(pp, tok->loc, str, NULL, &data_len, is_wide) < 0)
      return -1;
    data = sp_malloc(pp->ast->pool, data_len * (is_wide) ? sizeof(uint32_t) : 1);
    if (! data)
      goto err_oom;
    if (conv_string(pp, tok->loc, str, data, &data_len, is_wide) < 0)
      return -1;
  } else {
    // check if result is wide and count length of all strings
    struct sp_pp_token_list_walker w;
    struct sp_pp_token *s = sp_rewind_pp_token_list(&w, strings);
    while (sp_read_pp_token_from_list(&w, &s)) {
      const char *str = sp_get_pp_token_string(pp, s);
      if (str[0] == 'L')
        is_wide = true;
    }
    s = sp_rewind_pp_token_list(&w, strings);
    while (sp_read_pp_token_from_list(&w, &s)) {
      const char *str = sp_get_pp_token_string(pp, s);
      size_t str_len = 0;
      if (conv_string(pp, s->loc, str, NULL, &str_len, is_wide) < 0)
        return -1;
      data_len += str_len;
    }

    // convert all strings
    size_t ch_size = (is_wide) ? sizeof(uint32_t) : 1;
    char *data_str = sp_malloc(pp->ast->pool, data_len * ch_size);
    if (! data_str)
      goto err_oom;
    size_t pos = 0;
    s = sp_rewind_pp_token_list(&w, strings);
    while (sp_read_pp_token_from_list(&w, &s)) {
      const char *str = sp_get_pp_token_string(pp, s);
      size_t str_len = 0;
      if (conv_string(pp, s->loc, str, data_str + pos * ch_size, &str_len, is_wide) < 0)
        return -1;
      pos += str_len;
    }
    data = data_str;
  }

  if (is_wide) {
    ret->type = TOK_WIDE_STRING;
    ret->data.wide_str_literal.data = data;
    ret->data.wide_str_literal.len = data_len;
  } else {
    ret->type = TOK_CHAR_STRING;
    ret->data.char_str_literal.data = data;
    ret->data.char_str_literal.len = data_len;
  }
  return 0;

 err_oom:
  return set_error_at(pp, tok->loc, "out of memory");
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
    if (n % base != (uint64_t) digit || n / base != ret) {
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
    if (tok->data.punct_id == '#')
      return set_error_at(pp, tok->loc, "invalid character: '#'");
    ret->type = TOK_PUNCT;
    ret->data.punct_id = tok->data.punct_id;
    return 0;

  case TOK_PP_NUMBER:
    return conv_number(pp, tok, ret);

  case TOK_PP_CHAR_CONST:
    return conv_char_const(pp, tok, ret);

  case TOK_PP_OTHER:
    return set_error_at(pp, tok->loc, "invalid character: '%c'", tok->data.other);

  case TOK_PP_SPACE:
  case TOK_PP_NEWLINE:
  case TOK_PP_ENABLE_MACRO:
  case TOK_PP_END_OF_LIST:
  case TOK_PP_PASTE_MARKER:
  case TOK_PP_HEADER_NAME:
  case TOK_PP_STRING:
    return set_error_at(pp, tok->loc, "can't convert preprocessing token of type %d: '%s'", tok->type, sp_dump_pp_token(pp, tok));
  }

  return set_error_at(pp, tok->loc, "invalid pp token type: %d", tok->type);
}

int sp_next_token(struct sp_preprocessor *pp, struct sp_token *ret)
{
  if (! pp->init_ph6 && init_ph6(pp) < 0)
    return -1;

  do {
    NEXT_TOKEN();
  } while (pp_tok_is_space(CUR) || pp_tok_is_newline(CUR));

  ret->loc = CUR->loc;
  
  // string
  if (pp_tok_is_string(CUR)) {
    struct sp_pp_token_list *strings = NULL;
    struct sp_pp_token first = *CUR;

    // read all following string tokens
    while (pp_tok_is_string(NEXT) || pp_tok_is_space(NEXT) || pp_tok_is_newline(NEXT)) {
      NEXT_TOKEN();
      if (pp_tok_is_string(CUR)) {
        if (! strings) {
          sp_clear_mem_pool(&pp->str_join_pool);
          strings = sp_new_pp_token_list(&pp->str_join_pool, 0);
          if (! strings || sp_append_pp_token(strings, &first) < 0)
            goto err_oom;
        }
        if (sp_append_pp_token(strings, CUR) < 0)
          goto err_oom;
      }
    }

    // join strings
    if (join_string_literals(pp, strings, &first, ret) < 0)
      return -1;
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
