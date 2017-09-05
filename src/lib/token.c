/* token.c */

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "internal.h"
#include "token.h"
#include "punct.h"

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
    
  case TOK_STRING:
    {
      const uint8_t *in = (const uint8_t *) sp_get_token_string(tok, tab);
      strcpy(str, "\x1b[1;32m");
      if (tok->data.str_literal.is_wide)
        strcat(str, "L");
      strcat(str, "\"");
      char *out = str + strlen(str);
      while (*in != '\0') {
        if (*in == '"') {
          if (out - str + 4 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = '"';
        } else if (*in == '\n') {
          if (out - str + 4 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = 'n';
        } else if (*in == '\\') {
          if (out - str + 4 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = '\\';
        } else if (*in == '\r') {
          if (out - str + 4 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = 'r';
        } else if (*in == '\t') {
          if (out - str + 4 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = 't';
        } else if (*in < 32) {
          if (out - str + 8 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = 'u';
          *out++ = '0';
          *out++ = '0';
          *out++ = to_hex_char((*in>>4) & 0xf);
          *out++ = to_hex_char(   (*in) & 0xf);
        } else {
          if (out - str + 3 >= (int)sizeof(str)) break;
          *out++ = *in;
        }
        in++;
      }
      *out++ = '"';
      *out++ = '\0';
      if (out - str + strlen("\x1b[0m") < (int)sizeof(str))
        strcat(str, "\x1b[0m");
    }
    return str;
  }

  snprintf(str, sizeof(str), "<unknown token type %d>", tok->type);
  return str;
}

