/* token.h */

#ifndef TOKEN_H_FILE
#define TOKEN_H_FILE

enum sp_token_type {
  TOK_EOF,

  // pp-tokens
  TOK_PP_NEWLINE,
  TOK_PP_HEADER_NAME,
  TOK_PP_NUMBER,
  TOK_PP_CHAR_CONST,
  TOK_PP_OTHER,

  // pp-tokens + tokens
  TOK_IDENTIFIER,
  TOK_STRING,
  TOK_PUNCT,

  // tokens
  TOK_KEYWORD,
  TOK_INT_CONST,
  TOK_FLOAT_CONST,
  TOK_ENUM_CONST,
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

struct sp_token {
  struct sp_token *next;
  enum sp_token_type type;
  struct sp_src_loc loc;
  union {
    double d;
    uint64_t i;
    char c;
    sp_string_id str_id;
    char op_name[4];
    int punct_id;

    enum sp_keyword_type keyword;
  } data;
};

const char *sp_get_token_keyword(struct sp_token *tok);
const char *sp_get_token_op(struct sp_token *tok);
const char *sp_get_token_string(struct sp_ast *ast, struct sp_token *tok);
bool sp_find_keyword(const void *string, int size, enum sp_keyword_type *ret);
const char *sp_dump_token(struct sp_ast *ast, struct sp_token *tok);

int sp_get_punct_id(char *name);
const char *sp_get_punct_name(int punct_id);

#define tok_is_eof(tok)          ((tok)->type == TOK_EOF)
#define tok_is_number(tok)       ((tok)->type == TOK_NUMBER)
#define tok_is_string(tok)       ((tok)->type == TOK_STRING)
#define tok_is_punct(tok, p)     ((tok)->type == TOK_PUNCT && (tok)->data.punct_id == (p))
#define tok_is_keyword(tok, kw)  ((tok)->type == TOK_KEYWORD && (tok)->data.keyword == (kw))
#define tok_is_identifier(tok)   ((tok)->type == TOK_IDENTIFIER)

#endif /* TOKEN_H_FILE */
