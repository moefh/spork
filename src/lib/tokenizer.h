/* tokenizer.h */

#ifndef TOKENIZER_H_FILE
#define TOKENIZER_H_FILE

#include "internal.h"

#define TOKENIZER_BUF_SIZE 256

enum sp_token_type {
  TOK_EOF,
  TOK_KEYWORD,
  TOK_SYMBOL,
  TOK_STRING,
  TOK_DOUBLE,
  TOK_INT,
  TOK_OP,
  TOK_PUNCT,
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
  enum sp_token_type type;
  struct sp_src_loc loc;
  union {
    double d;
    uint64_t i;
    sp_string_id str;
    enum sp_keyword_type keyword;
    sp_symbol_id symbol_id;
    char op_name[4];
    uint32_t punct;
  } data;
};

struct sp_tokenizer {
  struct sp_program *prog;
  struct sp_input *in;
  struct sp_ast *ast;
  struct sp_buffer *tmp;
  uint16_t file_id;
  bool at_line_start;

  struct sp_src_loc cur_loc;
  struct sp_src_loc last_err_loc;
  struct sp_src_loc saved_loc;
};

void sp_init_tokenizer(struct sp_tokenizer *t, struct sp_program *prog, struct sp_input *in, struct sp_ast *ast, struct sp_buffer *tmp_buf, uint16_t file_id);
int sp_read_token(struct sp_tokenizer *t, struct sp_token *tok);
struct sp_src_loc sp_get_tokenizer_error_loc(struct sp_tokenizer *t);
const char *sp_get_token_keyword(struct sp_token *tok);
const char *sp_get_token_symbol(struct sp_ast *ast, struct sp_token *tok);
const char *sp_get_token_string(struct sp_ast *ast, struct sp_token *tok);
const char *sp_get_token_op(struct sp_token *tok);

const char *sp_dump_token(struct sp_ast *ast, struct sp_token *tok);

#define tok_is_eof(tok)          ((tok)->type == TOK_EOF)
#define tok_is_number(tok)       ((tok)->type == TOK_NUMBER)
#define tok_is_string(tok)       ((tok)->type == TOK_STRING)
#define tok_is_punct(tok, p)     ((tok)->type == TOK_PUNCT && (tok)->data.punct == (p))
#define tok_is_keyword(tok, kw)  ((tok)->type == TOK_KEYWORD && (tok)->data.keyword == (kw))
#define tok_is_symbol(tok)       ((tok)->type == TOK_SYMBOL)

#endif /* TOKENIZER_H_FILE */
