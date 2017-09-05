/* pp_token.h */

#ifndef PP_TOKEN_H_FILE
#define PP_TOKEN_H_FILE

enum sp_pp_token_type {
  TOK_PP_EOF = 256,
  TOK_PP_SPACE,
  TOK_PP_NEWLINE,
  TOK_PP_ENABLE_MACRO,
  TOK_PP_END_OF_LIST,
  TOK_PP_PASTE_MARKER,
  
  TOK_PP_HEADER_NAME,
  TOK_PP_IDENTIFIER,
  TOK_PP_NUMBER,
  TOK_PP_CHAR_CONST,
  TOK_PP_STRING,
  TOK_PP_PUNCT,
  TOK_PP_OTHER,
};

struct sp_pp_token {
  enum sp_pp_token_type type;
  struct sp_src_loc loc;
  bool macro_dead;
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
bool sp_pp_tokens_are_equal(struct sp_pp_token *t1, struct sp_pp_token *t2);

#define pp_tok_is_eof(tok)          ((tok)->type == TOK_PP_EOF)
#define pp_tok_is_end_of_list(tok)  ((tok)->type == TOK_PP_END_OF_LIST)
#define pp_tok_is_paste_marker(tok) ((tok)->type == TOK_PP_PASTE_MARKER)
#define pp_tok_is_enable_macro(tok) ((tok)->type == TOK_PP_ENABLE_MACRO)
#define pp_tok_is_newline(tok)      ((tok)->type == TOK_PP_NEWLINE)
#define pp_tok_is_space(tok)        ((tok)->type == TOK_PP_SPACE)
#define pp_tok_is_number(tok)       ((tok)->type == TOK_PP_NUMBER)
#define pp_tok_is_header_name(tok)  ((tok)->type == TOK_PP_HEADER_NAME)
#define pp_tok_is_string(tok)       ((tok)->type == TOK_PP_STRING)
#define pp_tok_is_identifier(tok)   ((tok)->type == TOK_PP_IDENTIFIER)
#define pp_tok_is_any_punct(tok)    ((tok)->type == TOK_PP_PUNCT)
#define pp_tok_is_punct(tok, p)     ((tok)->type == TOK_PP_PUNCT && (tok)->data.punct_id == (p))

#endif /* PP_TOKEN_H_FILE */
