/* pp_phase123.c
 *
 * Translation phases 1, 2 and 3.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#include "preprocessor.h"
#include "input.h"
#include "pp_token.h"

#define ERR_ERROR                -1
#define ERR_OUT_OF_MEMORY        -2
#define ERR_UNTERMINATED_HEADER  -3
#define ERR_UNTERMINATED_STRING  -4
#define ERR_EOF_IN_COMMENT       -5

#define IS_SPACE(c) ((c) == ' ' || (c) == '\r' || (c) == '\n' || (c) == '\t')
#define IS_ALPHA(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') || (c) == '_')
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define IS_ALNUM(c) (IS_ALPHA(c) || IS_DIGIT(c))

#define CUR   ((in->pos   < in->size) ? (int)in->data[in->pos  ] : -1)
#define NEXT  ((in->pos+1 < in->size) ? (int)in->data[in->pos+1] : -1)
#define NEXT2 ((in->pos+2 < in->size) ? (int)in->data[in->pos+2] : -1)

#define ADVANCE()        (in->pos++)
#define CUR_POS          (in->pos)
#define SET_POS(p)       (in->pos = (p))
#define CUR_IN_POS(in)   ((in)->pos)
#define SET_IN_POS(in,p) ((in)->pos = (p))

static bool skip_bs_newline(struct sp_input *in)
{
  //printf("=== skip_bs_newline ===\n");
  bool skipped = false;
  while (CUR == '\\') {
    int rewind_pos = CUR_POS;
    ADVANCE();
    while (IS_SPACE(CUR) && CUR != '\n')
      ADVANCE();
    if (CUR == '\n') {
      skipped = true;
      ADVANCE();
      continue;
    }
    SET_POS(rewind_pos);
    break;
  }
  return skipped;
}

static bool skip_spaces(struct sp_input *in)
{
  bool skipped = false;
  
  //printf("=== skip_spaces ===\n");
  while (IS_SPACE(CUR) && CUR != '\n') {
    skipped = true;
    do
      ADVANCE();
    while (IS_SPACE(CUR) && CUR != '\n');
    skip_bs_newline(in);
  }
  return skipped;
}

static bool skip_comments(struct sp_input *in, int *err)
{
  bool skipped = false;
  
  while (CUR == '/') {
    if (NEXT == '\\') {
      int rewind_pos = CUR_POS;
      ADVANCE();
      if (! skip_bs_newline(in) || (CUR != '/' && CUR != '*')) {
        SET_POS(rewind_pos);
        return false;
      }
    } else if (NEXT == '/' || NEXT == '*') {
      ADVANCE();
    } else
      break;

    skipped = true;

    // multi-line
    if (CUR == '*') {
      ADVANCE();
      while (true) {
        if (CUR < 0) {
          *err = ERR_EOF_IN_COMMENT;
          return false;
        }
        if (CUR == '*') {
          ADVANCE();
          if (CUR == '\\')
            skip_bs_newline(in);
          if (CUR == '/') {
            ADVANCE();
            break;
          }
          if (CUR != '*')
            ADVANCE();
          continue;
        }
        ADVANCE();
      }
      while (skip_bs_newline(in) || skip_spaces(in))
        ;
      continue;
    }

    // single-line
    ADVANCE();
    while (true) {
      while (CUR >= 0 && CUR != '\n' && CUR != '\\')
        ADVANCE();
      if (CUR < 0 || CUR == '\n')
        break;
      if (CUR == '\\') {
        if (! skip_bs_newline(in))
          ADVANCE();
      }
    }

    while (skip_bs_newline(in) || skip_spaces(in))
      ;
  }
  return skipped;
}

static int read_header(struct sp_input *in, struct sp_buffer *buf)
{
  char end_char = (CUR == '<') ? '>' : '"';
  
  buf->size = 0;
  if (sp_buf_add_byte(buf, CUR) < 0)
    return ERR_OUT_OF_MEMORY;
  while (true) {
    ADVANCE();
    if (CUR == '\\')
      skip_bs_newline(in);
    if (CUR < 0 || CUR == '\n')
      return ERR_UNTERMINATED_HEADER;
    if (CUR == end_char)
      break;
    if (sp_buf_add_byte(buf, CUR) < 0)
      return ERR_OUT_OF_MEMORY;
  }
  if (sp_buf_add_byte(buf, CUR) < 0)
    return ERR_OUT_OF_MEMORY;
  ADVANCE();
  if (sp_buf_add_byte(buf, '\0') < 0)
    return ERR_OUT_OF_MEMORY;
  return TOK_PP_HEADER_NAME;
}

static int read_string(struct sp_input *in, struct sp_buffer *buf)
{
  buf->size = 0;
  while (true) {
    ADVANCE();
    if (CUR == '\\') {
      if (! skip_bs_newline(in)) {
        // actual backslash
        if (sp_buf_add_byte(buf, CUR) < 0)
          return ERR_OUT_OF_MEMORY;
        ADVANCE();
        if (CUR < 0)
          return ERR_UNTERMINATED_STRING;
        if (CUR == '\\')
          skip_bs_newline(in);
        if (sp_buf_add_byte(buf, CUR) < 0)
          return ERR_OUT_OF_MEMORY;
        ADVANCE();
        if (CUR == '\\')
          skip_bs_newline(in);
      }
    }
    if (CUR == '\n' || CUR < 0)
      return ERR_UNTERMINATED_STRING;
    if (CUR == '"') {
      ADVANCE();
      break;
    }
    if (sp_buf_add_byte(buf, CUR) < 0)
      return ERR_OUT_OF_MEMORY;
  }
  if (sp_buf_add_byte(buf, '\0') < 0)
    return ERR_OUT_OF_MEMORY;
  return TOK_PP_STRING;
}

static int read_number(struct sp_input *in, struct sp_buffer *buf)
{
  buf->size = 0;
  if (sp_buf_add_byte(buf, CUR) < 0)
    return ERR_OUT_OF_MEMORY;
  ADVANCE();
  while (true) {
    if (CUR == '\\' && ! skip_bs_newline(in))
      break;

    // [eEpP][+-]
    if (CUR == 'e' || CUR == 'E' || CUR == 'p' || CUR == 'P') {
      char exp = CUR;
      int rewind_pos = CUR_POS;
      ADVANCE();
      if (CUR == '\\' && ! skip_bs_newline(in)) {
        SET_POS(rewind_pos);
        break;
      }
      if (CUR == '-' || CUR == '+') {
        if (sp_buf_add_byte(buf, exp) < 0 || sp_buf_add_byte(buf, CUR) < 0)
          return ERR_OUT_OF_MEMORY;
        ADVANCE();
        continue;
      }
    }

    // [0-9A-Za-z_.]
    if (IS_DIGIT(CUR) || IS_ALPHA(CUR) || CUR == '.') {
      if (sp_buf_add_byte(buf, CUR) < 0)
        return ERR_OUT_OF_MEMORY;
      ADVANCE();
      continue;
    }
    
    break;
  }
  if (sp_buf_add_byte(buf, '\0') < 0)
    return ERR_OUT_OF_MEMORY;
  return TOK_PP_NUMBER;
}

static int read_ident(struct sp_input *in, struct sp_buffer *buf)
{
  buf->size = 0;
  if (sp_buf_add_byte(buf, CUR) < 0)
    return ERR_OUT_OF_MEMORY;
  ADVANCE();
  while (true) {
    if (CUR == '\\' && ! skip_bs_newline(in))
      break;
    if (! IS_ALNUM(CUR))
      break;
    if (sp_buf_add_byte(buf, CUR) < 0)
      return ERR_OUT_OF_MEMORY;
    ADVANCE();
  }
  if (sp_buf_add_byte(buf, '\0') < 0)
    return ERR_OUT_OF_MEMORY;
  return TOK_PP_IDENTIFIER;
}

static int read_token(struct sp_input *in, struct sp_buffer *buf, int *pos, bool parse_header)
{
  int err = 0;
  
  if (CUR < 0) {
    *pos = in->size;
    return TOK_PP_EOF;
  }

  skip_bs_newline(in);

  /* comment */
  if (skip_comments(in, &err))
    goto skip_spaces;
  if (err)
    return err;

  /* spaces/newlines */
  if (skip_spaces(in)) {
  skip_spaces:;
    bool got_newline = false;

    *pos = CUR_POS;
    while (skip_spaces(in) || skip_bs_newline(in))
      ;
    do {
      if (CUR == '\n') {
        *pos = CUR_POS;
        ADVANCE();
        got_newline = true;
      }
    } while (skip_spaces(in) || skip_bs_newline(in) || (got_newline && CUR == '\n'));
    if (got_newline) {
      if (skip_comments(in, &err))
        goto skip_newlines;
    } else {
      if (skip_comments(in, &err))
        goto skip_spaces;
    }
    if (err)
      return err;
    return (got_newline) ? '\n' : ' ';
  }

  /* newlines */
  if (CUR == '\n') {
  skip_newlines:
    do {
      if (CUR == '\n') {
        *pos = CUR_POS;
        ADVANCE();
      }
    } while (skip_spaces(in) || skip_bs_newline(in) || CUR == '\n');
    if (skip_comments(in, &err))
      goto skip_newlines;
    if (err)
      return err;
    return '\n';
  }

  /* <header> */
  if (parse_header && (CUR == '<' || CUR == '"')) {
    *pos = CUR_POS;
    return read_header(in, buf);
  }

  /* "string" */
  if (CUR == '"') {
    *pos = CUR_POS;
    return read_string(in, buf);
  }

  /* number */
  if (IS_DIGIT(CUR)) {
    *pos = CUR_POS;
    return read_number(in, buf);
  }
  if (CUR == '.') {
    int rewind_pos = CUR_POS;
    ADVANCE();
    if (IS_DIGIT(CUR) || (CUR == '\\' && skip_bs_newline(in) && IS_DIGIT(CUR))) {
      SET_POS(rewind_pos);
      *pos = CUR_POS;
      return read_number(in, buf);
    }
    SET_POS(rewind_pos);
  }

  /* identifier */
  if (IS_ALPHA(CUR)) {
    *pos = CUR_POS;
    return read_ident(in, buf);
  }

  /* TODO: character constant */
  
  /* punctuation */
  buf->size = 0;
  // ensure we have enough space for any punctuation
  if (sp_buf_add_string(buf, "...", 3) < 0)
    return ERR_OUT_OF_MEMORY;
  int start_pos = CUR_POS;
  for (int try_size = 3; try_size > 0; try_size--) {
    buf->size = 0;
    for (int i = 0; i < try_size; i++) {
      sp_buf_add_byte(buf, CUR);
      ADVANCE();
      if (CUR == '\\')
        skip_bs_newline(in);
    }
    sp_buf_add_byte(buf, '\0');
    if (sp_get_punct_id(buf->p) >= 0) {
      *pos = start_pos;
      return TOK_PP_PUNCT;
    }
    SET_POS(start_pos);
  }
  
  int c = CUR;
  *pos = CUR_POS;
  ADVANCE();
  return c;
}

static bool next_char_is_lparen(struct sp_input *in)
{
  int rewind_pos = CUR_POS;
  if (CUR == '\\')
    skip_bs_newline(in);
  if (CUR == '(') {
    SET_POS(rewind_pos);
    return true;
  }
  SET_POS(rewind_pos);
  return false;
}

bool sp_next_pp_ph3_char_is_lparen(struct sp_preprocessor *pp)
{
  return next_char_is_lparen(pp->in);
}

static int next_token(struct sp_preprocessor *pp, struct sp_pp_token *tok, bool parse_header)
{
  int pos;
  int type = read_token(pp->in, &pp->tmp_buf, &pos, parse_header);

  // error
  if (type < 0) {
    switch (type) {
    case ERR_ERROR:               return sp_set_pp_error(pp, "internal error");
    case ERR_OUT_OF_MEMORY:       return sp_set_pp_error(pp, "out of memory");
    case ERR_UNTERMINATED_STRING: return sp_set_pp_error(pp, "unterminated string");
    case ERR_UNTERMINATED_HEADER: return sp_set_pp_error(pp, "unterminated header name");
    case ERR_EOF_IN_COMMENT:      return sp_set_pp_error(pp, "unterminated comment");
    }
    return sp_set_pp_error(pp, "internal error");
  }

  // TODO: set location based on 'pos'
  tok->loc = sp_make_src_loc(sp_get_input_file_id(pp->in),0,0);

  // EOF
  if (type == TOK_PP_EOF) {
    tok->type = TOK_PP_EOF;
    return 0;
  }
  
  // space
  if (type == ' ') {
    tok->type = TOK_PP_SPACE;
    return 0;
  }

  // newline
  if (type == '\n') {
    tok->type = TOK_PP_NEWLINE;
    return 0;
  }

  // other
  if (type < 256) {
    tok->type = TOK_PP_OTHER;
    tok->data.other = type;
    return 0;
  }

  // punct
  if (type == TOK_PP_PUNCT) {
    tok->type = TOK_PP_PUNCT;
    tok->data.punct_id = sp_get_punct_id(pp->tmp_buf.p);
    return 0;
  }

  // other tokens
  sp_string_id str_id = sp_add_string(&pp->token_strings, pp->tmp_buf.p);
  if (str_id < 0)
    return sp_set_pp_error(pp, "out of memory");
  tok->type = type;
  tok->data.str_id = str_id;
  return 0;
}

int sp_next_pp_ph3_token(struct sp_preprocessor *pp, bool parse_header)
{
  return next_token(pp, &pp->tok, parse_header);
}

int sp_peek_nonblank_pp_ph3_token(struct sp_preprocessor *pp, struct sp_pp_token *next, bool parse_header)
{
  int rewind_pos = CUR_IN_POS(pp->in);
  do {
    if (next_token(pp, next, parse_header) < 0)
      goto err;
  } while (next->type == TOK_PP_SPACE || next->type == TOK_PP_NEWLINE);
  SET_IN_POS(pp->in, rewind_pos);
  return 0;
  
 err:
  SET_IN_POS(pp->in, rewind_pos);
  return -1;
}
