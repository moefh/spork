/* token.c */

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "internal.h"
#include "token.h"
#include "punct.h"
#include "buffer.h"

/* keywords */

#define ADD_KW(kw) { KW_ ## kw, # kw }
static const struct keyword {
  enum sp_keyword_type type;
  const char *name;
} keywords[] = {
  ADD_KW(auto),
  ADD_KW(break),
  ADD_KW(case),
  ADD_KW(char),
  ADD_KW(const),
  ADD_KW(continue),
  ADD_KW(default),
  ADD_KW(do),
  ADD_KW(double),
  ADD_KW(else),
  ADD_KW(enum),
  ADD_KW(extern),
  ADD_KW(float),
  ADD_KW(for),
  ADD_KW(goto),
  ADD_KW(if),
  ADD_KW(inline),
  ADD_KW(int),
  ADD_KW(long),
  ADD_KW(register),
  ADD_KW(restrict),
  ADD_KW(return),
  ADD_KW(short),
  ADD_KW(signed),
  ADD_KW(sizeof),
  ADD_KW(static),
  ADD_KW(struct),
  ADD_KW(switch),
  ADD_KW(typedef),
  ADD_KW(unsigned),
  ADD_KW(void),
  ADD_KW(volatile),
  ADD_KW(while),  
};

bool sp_find_keyword(const char *string, enum sp_keyword_type *ret)
{
  for (int i = 0; i < ARRAY_SIZE(keywords); i++) {
    if (strcmp(string, keywords[i].name) == 0) {
      *ret = keywords[i].type;
      return true;
    }
  }
  return false;
}

const char *sp_get_keyword_name(enum sp_keyword_type keyword_type)
{
  for (int i = 0; i < ARRAY_SIZE(keywords); i++) {
    if (keyword_type == keywords[i].type)
      return keywords[i].name;
  }
  return NULL;
}

const char *sp_get_token_string(struct sp_token *tok, struct sp_string_table *tab)
{
  return sp_get_string(tab, tok->data.str_id);
}

static char to_hex_char(int i)
{
  if (i < 10) return '0' + i;
  return 'a' + i - 10;
}

const char *sp_dump_token(struct sp_token *tok, struct sp_string_table *tab)
{
  static char str[256];

#if 1
#define MARK_COLOR(x)     "\x1b[31m" x "\x1b[0m"
#define KEYWORD_COLOR(x)  "\x1b[1;36m" x "\x1b[0m"
#define IDENT_COLOR(x)    "\x1b[1;37m" x "\x1b[0m"
#define PUNCT_COLOR(x)    "\x1b[1;34m" x "\x1b[0m"
#define NUMBER_COLOR(x)   "\x1b[1;32m" x "\x1b[0m"
#else
#define MARK_COLOR(x)     x
#define KEYWORD_COLOR(x)  x
#define IDENT_COLOR(x)    x
#define PUNCT_COLOR(x)    x
#define NUMBER_COLOR(x)   x
#endif
  
  switch (tok->type) {
  case TOK_EOF:
    snprintf(str, sizeof(str), MARK_COLOR("<eof>"));
    return str;
    
  case TOK_PUNCT:
    snprintf(str, sizeof(str), PUNCT_COLOR("%s"), sp_get_punct_name(tok->data.punct_id));
    return str;
    
  case TOK_IDENTIFIER:
    snprintf(str, sizeof(str), IDENT_COLOR("%s"), sp_get_token_string(tok, tab));
    return str;

  case TOK_KEYWORD:
    snprintf(str, sizeof(str), KEYWORD_COLOR("%s"), sp_get_keyword_name(tok->data.keyword_type));
    return str;

  case TOK_INT_CONST:
    snprintf(str, sizeof(str), NUMBER_COLOR("%" PRIu64), tok->data.int_const.n);
    if (tok->data.int_const.flags & TOK_INT_CONST_FLAG_U)
      strcat(str, NUMBER_COLOR("U"));
    if (tok->data.int_const.flags & TOK_INT_CONST_FLAG_L)
      strcat(str, NUMBER_COLOR("L"));
    if (tok->data.int_const.flags & TOK_INT_CONST_FLAG_LL)
      strcat(str, NUMBER_COLOR("LL"));
    return str;

  case TOK_FLOAT_CONST:
    snprintf(str, sizeof(str), NUMBER_COLOR("%gF"), tok->data.float_const.n);
    if (tok->data.int_const.flags & TOK_INT_CONST_FLAG_L)
      strcat(str, NUMBER_COLOR("L"));
    return str;

  case TOK_CHAR_CONST:
    if (tok->data.char_const.ch == '\\' || tok->data.char_const.ch == '\'') {
      snprintf(str, sizeof(str), NUMBER_COLOR("%s'\\%c'"), (tok->data.char_const.is_wide) ? "L" : "", (int) tok->data.char_const.ch);
    } else if (tok->data.char_const.ch >= 32 && tok->data.char_const.ch < 127)
      snprintf(str, sizeof(str), NUMBER_COLOR("%s'%c'"), (tok->data.char_const.is_wide) ? "L" : "", (int) tok->data.char_const.ch);
    else if (tok->data.char_const.ch == 0) {
      snprintf(str, sizeof(str), NUMBER_COLOR("%s'\\0'"), (tok->data.char_const.is_wide) ? "L" : "");
    } else {
      size_t len = 0;
      snprintf(str + len, sizeof(str) - len, NUMBER_COLOR("%s'\\x"), (tok->data.char_const.is_wide) ? "L" : "");
      len += strlen(str + len);
      int size = 7;
      uint32_t v = tok->data.char_const.ch;
      while (size > 0 && (0xf & (v >> (4*size))) == 0)
        size--;
      if (size < 2)
        size = 1;
      for (int i = size; i >= 0; i--) {
        snprintf(str + len, sizeof(str) - len, NUMBER_COLOR("%x"), 0xf & (v>>(4*i)));
        len += strlen(str + len);
      }
      snprintf(str + len, sizeof(str) - len, NUMBER_COLOR("'"));
    }
    return str;
    
  case TOK_WIDE_STRING:
    {
      const uint32_t *in = tok->data.wide_str_literal.data;
      size_t len = tok->data.wide_str_literal.len;
      size_t pos = 0;
      struct sp_buffer out;
      sp_init_buffer(&out, NULL);
      sp_buf_add_string(&out, "\x1b[1;32mL\"");
      while (pos < len) {
        if (in[pos] == '"') {
          sp_buf_add_string(&out, "\\\"");
        } else if (in[pos] == '\\') {
          sp_buf_add_string(&out, "\\\\");
        } else if (in[pos] == '\a') {
          sp_buf_add_string(&out, "\\a");
        } else if (in[pos] == '\b') {
          sp_buf_add_string(&out, "\\b");
        } else if (in[pos] == '\n') {
          sp_buf_add_string(&out, "\\n");
        } else if (in[pos] == '\r') {
          sp_buf_add_string(&out, "\\r");
        } else if (in[pos] == '\t') {
          sp_buf_add_string(&out, "\\t");
        } else if (in[pos] == '\v') {
          sp_buf_add_string(&out, "\\v");
        } else if (in[pos] == '\0') {
          sp_buf_add_string(&out, "\\0");
        } else if (in[pos] < 32 || (in[pos] >= 127 && in[pos] <= 0xffff)) {
          sp_buf_add_string(&out, "\\u");
          sp_buf_add_byte(&out, to_hex_char((in[pos]>>12) & 0xf));
          sp_buf_add_byte(&out, to_hex_char((in[pos]>> 8) & 0xf));
          sp_buf_add_byte(&out, to_hex_char((in[pos]>> 4) & 0xf));
          sp_buf_add_byte(&out, to_hex_char((in[pos]    ) & 0xf));
        } else if (in[pos] > 0xffff) {
          sp_buf_add_string(&out, "\\U");
          sp_buf_add_byte(&out, to_hex_char((in[pos]>>28) & 0xf));
          sp_buf_add_byte(&out, to_hex_char((in[pos]>>24) & 0xf));
          sp_buf_add_byte(&out, to_hex_char((in[pos]>>20) & 0xf));
          sp_buf_add_byte(&out, to_hex_char((in[pos]>>16) & 0xf));
          sp_buf_add_byte(&out, to_hex_char((in[pos]>>12) & 0xf));
          sp_buf_add_byte(&out, to_hex_char((in[pos]>> 8) & 0xf));
          sp_buf_add_byte(&out, to_hex_char((in[pos]>> 4) & 0xf));
          sp_buf_add_byte(&out, to_hex_char((in[pos]    ) & 0xf));
        } else {
          sp_buf_add_byte(&out, in[pos]);
        }
        pos++;
      }
      sp_buf_add_string(&out, "\"\x1b[0m");
      sp_buf_add_byte(&out, '\0');
      if (out.p) {
        strncpy(str, out.p, sizeof(str));
        str[sizeof(str)-1] = '\0';
      } else {
        str[0] = '\0';
      }
      sp_destroy_buffer(&out);
    }
    return str;

  case TOK_CHAR_STRING:
    {
      const uint8_t *in = (const uint8_t *) tok->data.char_str_literal.data;
      size_t len = tok->data.char_str_literal.len;
      size_t pos = 0;
      struct sp_buffer out;
      sp_init_buffer(&out, NULL);
      sp_buf_add_string(&out, "\x1b[1;32m\"");
      while (pos < len) {
        if (in[pos] == '"') {
          sp_buf_add_string(&out, "\\\"");
        } else if (in[pos] == '\\') {
          sp_buf_add_string(&out, "\\\\");
        } else if (in[pos] == '\a') {
          sp_buf_add_string(&out, "\\a");
        } else if (in[pos] == '\b') {
          sp_buf_add_string(&out, "\\b");
        } else if (in[pos] == '\n') {
          sp_buf_add_string(&out, "\\n");
        } else if (in[pos] == '\r') {
          sp_buf_add_string(&out, "\\r");
        } else if (in[pos] == '\t') {
          sp_buf_add_string(&out, "\\t");
        } else if (in[pos] == '\v') {
          sp_buf_add_string(&out, "\\v");
        } else if (in[pos] == '\0') {
          sp_buf_add_string(&out, "\\0");
        } else if (in[pos] < 32) {
          sp_buf_add_string(&out, "\\u");
          sp_buf_add_byte(&out, to_hex_char((in[pos]>>12) & 0xf));
          sp_buf_add_byte(&out, to_hex_char((in[pos]>> 8) & 0xf));
          sp_buf_add_byte(&out, to_hex_char((in[pos]>> 4) & 0xf));
          sp_buf_add_byte(&out, to_hex_char((in[pos]    ) & 0xf));
        } else {
          sp_buf_add_byte(&out, in[pos]);
        }
        pos++;
      }
      sp_buf_add_string(&out, "\"\x1b[0m");
      sp_buf_add_byte(&out, '\0');
      if (out.p) {
        strncpy(str, out.p, sizeof(str));
        str[sizeof(str)-1] = '\0';
      } else {
        str[0] = '\0';
      }
      sp_destroy_buffer(&out);
    }
    return str;
  }

  snprintf(str, sizeof(str), "<unknown token type %d>", tok->type);
  return str;
}

