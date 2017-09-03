/* pp_token.c */

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "internal.h"
#include "preprocessor.h"
#include "pp_token.h"

/* punctuation */

static struct punct {
  int id;
  char name[4];
} puncts[] = {
  { PUNCT_ELLIPSIS,    "..." },
  { PUNCT_LSHIFTEQ,    "<<=" },
  { PUNCT_RSHIFTEQ,    ">>=" },
  { PUNCT_MULEQ,       "*="  },
  { PUNCT_DIVEQ,       "/="  },
  { PUNCT_MODEQ,       "%="  },
  { PUNCT_PLUSEQ,      "+="  },
  { PUNCT_MINUSEQ,     "-="  },
  { PUNCT_ANDEQ,       "&="  },
  { PUNCT_XOREQ,       "^="  },
  { PUNCT_OREQ,        "|="  },
  { PUNCT_HASHES,      "##"  },
  { PUNCT_ARROW,       "->"  },
  { PUNCT_PLUSPLUS,    "++"  },
  { PUNCT_MINUSMINUS,  "--"  },
  { PUNCT_LSHIFT,      "<<"  },
  { PUNCT_RSHIFT,      ">>"  },
  { PUNCT_LEQ,         "<="  },
  { PUNCT_GEQ,         ">="  },
  { PUNCT_EQ,          "=="  },
  { PUNCT_NEQ,         "!="  },
  { PUNCT_AND,         "&&"  },
  { PUNCT_OR,          "||"  },
  { '[',               "["   },
  { ']',               "]"   },
  { '(',               "("   },
  { ')',               ")"   },
  { '{',               "{"   },
  { '}',               "}"   },
  { '.',               "."   },
  { '&',               "&"   },
  { '*',               "*"   },
  { '+',               "+"   },
  { '-',               "-"   },
  { '~',               "~"   },
  { '!',               "!"   },
  { '/',               "/"   },
  { '%',               "%"   },
  { '<',               "<"   },
  { '>',               ">"   },
  { '^',               "^"   },
  { '|',               "|"   },
  { '?',               "?"   },
  { ':',               ":"   },
  { ';',               ";"   },
  { '=',               "="   },
  { ',',               ","   },
  { '~',               "~"   },
  { '#',               "#"   },
};

const char *sp_get_punct_name(int punct_id)
{
  for (int i = 0; i < ARRAY_SIZE(puncts); i++)
    if (puncts[i].id == punct_id)
      return puncts[i].name;
  return NULL;
}

int sp_get_punct_id(const char *name)
{
  for (int i = 0; i < ARRAY_SIZE(puncts); i++)
    if (strcmp(puncts[i].name, name) == 0)
      return puncts[i].id;
  return -1;
}

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

bool sp_find_keyword(const void *string, int size, enum sp_keyword_type *ret)
{
  for (int i = 0; i < ARRAY_SIZE(keywords); i++) {
    if (strncmp(string, keywords[i].name, size) == 0 && keywords[i].name[size] == '\0') {
      *ret = keywords[i].type;
      return true;
    }
  }
  return false;
}

const char *sp_get_pp_token_string(struct sp_preprocessor *pp, struct sp_pp_token *tok)
{
  return sp_get_string(&pp->token_strings, tok->data.str_id);
}

const char *sp_get_pp_token_punct(struct sp_pp_token *tok)
{
  return sp_get_punct_name(tok->data.punct_id);
}

bool sp_pp_tokens_are_equal(struct sp_pp_token *t1, struct sp_pp_token *t2)
{
  if (t1->type != t2->type)
    return false;

  switch (t1->type) {
  case TOK_PP_EOF:
  case TOK_PP_SPACE:
  case TOK_PP_NEWLINE:
  case TOK_PP_END_OF_ARG:
  case TOK_PP_PASTE_MARKER:
    return true;

  case TOK_PP_OTHER:
    return t1->data.other == t2->data.other;

  case TOK_PP_PUNCT:
    return t1->data.punct_id == t2->data.punct_id;

  case TOK_PP_ENABLE_MACRO:
  case TOK_PP_HEADER_NAME:
  case TOK_PP_STRING:
  case TOK_PP_IDENTIFIER:
  case TOK_PP_NUMBER:
  case TOK_PP_CHAR_CONST:
    return t1->data.str_id == t2->data.str_id;
  }

  return false;
}

#if 0
static char to_hex_char(int i)
{
  if (i < 10) return '0' + i;
  return 'a' + i - 10;
}
const char *sp_dump_token(struct sp_ast *ast, struct sp_token *tok)
{
  case TOK_STRING:
    {
      const uint8_t *in = (const uint8_t *) sp_get_token_string(ast, tok);
      char *out = str;
      while (*in != '\0') {
        if (*in == '\n') {
          if (out - str + 4 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = 'n';
        } else if (*in == '\r') {
          if (out - str + 4 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = 'r';
        } else if (*in == '\t') {
          if (out - str + 4 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = 't';
        } else if (*in < 32) {
          if (out - str + 6 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = 'x';
          *out++ = to_hex_char((*in>>4) & 0xf);
          *out++ = to_hex_char(   (*in) & 0xf);
        } else {
          if (out - str + 3 >= (int)sizeof(str)) break;
          *out++ = *in;
        }
        in++;
      }
      *out++ = '\0';
    }
    return str;
}
#endif

const char *sp_dump_pp_token(struct sp_preprocessor *pp, struct sp_pp_token *tok)
{
  static char str[256];

#if 1
#define MARK_COLOR(x)    "\x1b[31m" x "\x1b[0m"
#define OTHER_COLOR(x)   "\x1b[1;31m" x "\x1b[0m"
#define IDENT_COLOR(x)   "\x1b[1;37m" x "\x1b[0m"
#define PUNCT_COLOR(x)   "\x1b[1;36m" x "\x1b[0m"
#define NUMBER_COLOR(x)  "\x1b[1;32m" x "\x1b[0m"
#else
#define MARK_COLOR(x)   x
#define OTHER_COLOR(x)  x
#define IDENT_COLOR(x)  x
#define PUNCT_COLOR(x)  x
#define NUMBER_COLOR(x) x
#endif

  switch (tok->type) {
  case TOK_PP_EOF:
    snprintf(str, sizeof(str), MARK_COLOR("<end-of-file>"));
    return str;

  case TOK_PP_END_OF_ARG:
    snprintf(str, sizeof(str), MARK_COLOR("<end-of-arg>"));
    return str;

  case TOK_PP_PASTE_MARKER:
    snprintf(str, sizeof(str), MARK_COLOR("<placemarker>"));
    return str;

  case TOK_PP_ENABLE_MACRO:
    snprintf(str, sizeof(str), MARK_COLOR("<enable-macro '%s'>"), sp_get_pp_token_string(pp, tok));
    return str;

  case TOK_PP_NEWLINE:
    snprintf(str, sizeof(str), MARK_COLOR("<newline>"));
    return str;

  case TOK_PP_SPACE:
    snprintf(str, sizeof(str), " ");
    return str;

  case TOK_PP_OTHER:
    snprintf(str, sizeof(str), OTHER_COLOR("%c"), tok->data.other);
    return str;

  case TOK_PP_CHAR_CONST:  // TODO: how do we print this?
    snprintf(str, sizeof(str), "<char const>");
    return str;
    
  case TOK_PP_IDENTIFIER:
    snprintf(str, sizeof(str), IDENT_COLOR("%s"), sp_get_pp_token_string(pp, tok));
    return str;
    
  case TOK_PP_HEADER_NAME:
  case TOK_PP_NUMBER:
    snprintf(str, sizeof(str), NUMBER_COLOR("%s"), sp_get_pp_token_string(pp, tok));
    return str;

  case TOK_PP_STRING:
    snprintf(str, sizeof(str), NUMBER_COLOR("%s"), sp_get_pp_token_string(pp, tok));
    return str;

  case TOK_PP_PUNCT:
    snprintf(str, sizeof(str), PUNCT_COLOR("%s"), sp_get_punct_name(tok->data.punct_id));
    return str;
  }
  
  snprintf(str, sizeof(str), "<unknown token type %d>", tok->type);
  return str;
}
