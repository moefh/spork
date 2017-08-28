/* pp_token.h */

#ifndef PP_TOKEN_H_FILE
#define PP_TOKEN_H_FILE

enum sp_pp_token_type {
  TOK_PP_EOF = 256,
  TOK_PP_SPACE,
  TOK_PP_NEWLINE,
  TOK_PP_HEADER_NAME,
  TOK_PP_IDENTIFIER,
  TOK_PP_NUMBER,
  TOK_PP_CHAR_CONST,
  TOK_PP_STRING,
  TOK_PP_PUNCT,
  TOK_PP_OTHER,
};

enum {
  PUNCT_ARROW = 256,
  PUNCT_PLUSPLUS,
  PUNCT_MINUSMINUS,
  PUNCT_LSHIFT,
  PUNCT_RSHIFT,
  PUNCT_LEQ,
  PUNCT_GEQ,
  PUNCT_EQ,
  PUNCT_NEQ,
  PUNCT_AND,
  PUNCT_OR,
  PUNCT_ELLIPSIS,
  PUNCT_MULEQ,
  PUNCT_DIVEQ,
  PUNCT_MODEQ,
  PUNCT_PLUSEQ,
  PUNCT_MINUSEQ,
  PUNCT_LSHIFTEQ,
  PUNCT_RSHIFTEQ,
  PUNCT_ANDEQ,
  PUNCT_XOREQ,
  PUNCT_OREQ,
  PUNCT_HASHES,
};

enum sp_keyword_type {
  KW_auto,
  KW_break,
  KW_case,
  KW_char,
  KW_const,
  KW_continue,
  KW_default,
  KW_do,
  KW_double,
  KW_else,
  KW_enum,
  KW_extern,
  KW_float,
  KW_for,
  KW_goto,
  KW_if,
  KW_inline,
  KW_int,
  KW_long,
  KW_register,
  KW_restrict,
  KW_return,
  KW_short,
  KW_signed,
  KW_sizeof,
  KW_static,
  KW_struct,
  KW_switch,
  KW_typedef,
  KW_unsigned,
  KW_void,
  KW_volatile,
  KW_while,
};

struct sp_pp_token {
  enum sp_pp_token_type type;
  struct sp_src_loc loc;
  union {
    sp_string_id str_id;
    int punct_id;
    int other;
  } data;
};

struct sp_preprocessor;

#define sp_get_pp_token_string_id(tok) ((tok)->data.str_id)
const char *sp_get_pp_token_punct(struct sp_pp_token *tok);
const char *sp_get_pp_token_string(struct sp_preprocessor *pp, struct sp_pp_token *tok);
const char *sp_dump_pp_token(struct sp_preprocessor *pp, struct sp_pp_token *tok);

int sp_get_punct_id(char *name);
const char *sp_get_punct_name(int punct_id);

#define pp_tok_is_eof(tok)          ((tok)->type == TOK_PP_EOF)
#define pp_tok_is_newline(tok)      ((tok)->type == TOK_PP_NEWLINE)
#define pp_tok_is_space(tok)        ((tok)->type == TOK_PP_SPACE)
#define pp_tok_is_number(tok)       ((tok)->type == TOK_PP_NUMBER)
#define pp_tok_is_string(tok)       ((tok)->type == TOK_PP_STRING)
#define pp_tok_is_punct(tok, p)     ((tok)->type == TOK_PP_PUNCT && (tok)->data.punct_id == (p))
#define pp_tok_is_identifier(tok)   ((tok)->type == TOK_PP_IDENTIFIER)

#endif /* PP_TOKEN_H_FILE */
