/* token.h */

#ifndef TOKEN_H_FILE
#define TOKEN_H_FILE

enum sp_token_type {
  TOK_EOF = 256,

  TOK_KEYWORD,
  TOK_IDENTIFIER,
  TOK_STRING,
  TOK_PUNCT,
  TOK_INT_CONST,
  TOK_FLOAT_CONST,
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

#define TOK_INT_CONST_FLAG_U   (1<<0)
#define TOK_INT_CONST_FLAG_L   (1<<1)
#define TOK_INT_CONST_FLAG_LL  (1<<2)

#define TOK_FLOAT_CONST_FLAG_L (1<<0)

struct sp_token {
  enum sp_token_type type;
  struct sp_src_loc loc;
  union {
    sp_string_id str_id;
    enum sp_keyword_type keyword_type;
    struct {
      sp_string_id id;
      bool is_wide;
    } str_literal;
    struct {
      uint64_t n;
      uint8_t flags;  // TOK_INT_FLAG_xxx
    } int_const;
    struct {
      double n;
      uint8_t flags;  // TOK_FLOAT_FLAG_xxx
    } float_const;
    int punct_id;
  } data;
};

#define sp_get_token_string_id(tok) ((tok)->data.str_id)
const char *sp_get_token_punct(struct sp_token *tok);
const char *sp_get_token_string(struct sp_token *tok, struct sp_string_table *table);
bool sp_find_keyword(const char *string, enum sp_keyword_type *ret);
const char *sp_get_keyword_name(enum sp_keyword_type keyword_type);

const char *sp_dump_token(struct sp_token *tok, struct sp_string_table *table);

#define tok_is_eof(tok)          ((tok)->type == TOK_EOF)
#define tok_is_string(tok)       ((tok)->type == TOK_STRING)
#define tok_is_identifier(tok)   ((tok)->type == TOK_IDENTIFIER)
#define tok_is_punct(tok, p)     ((tok)->type == TOK_PUNCT && (tok)->data.punct_id == (p))
#define tok_is_keyword(tok, kw)  ((tok)->type == TOK_KEYWORD && (tok)->data.keyword_type == (kw))

#endif /* TOKEN_H_FILE */
