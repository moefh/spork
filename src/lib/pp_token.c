/* pp_token.c */

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "internal.h"
#include "preprocessor.h"
#include "pp_token.h"
#include "punct.h"

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
  case TOK_PP_END_OF_LIST:
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

  case TOK_PP_END_OF_LIST:
    snprintf(str, sizeof(str), MARK_COLOR("<end-of-list>"));
    return str;

  case TOK_PP_PASTE_MARKER:
    snprintf(str, sizeof(str), MARK_COLOR("<placemarker>"));
    return str;

  case TOK_PP_ENABLE_MACRO:
    snprintf(str, sizeof(str), MARK_COLOR("<enable-macro '%s' (%d)>"), sp_get_pp_token_string(pp, tok), tok->data.str_id);
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

  case TOK_PP_IDENTIFIER:
    snprintf(str, sizeof(str), IDENT_COLOR("%s"), sp_get_pp_token_string(pp, tok));
    return str;
    
  case TOK_PP_HEADER_NAME:
  case TOK_PP_NUMBER:
    snprintf(str, sizeof(str), NUMBER_COLOR("%s"), sp_get_pp_token_string(pp, tok));
    return str;

  case TOK_PP_CHAR_CONST:
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
