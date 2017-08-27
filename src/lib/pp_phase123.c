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
#include "ast.h"
#include "token.h"

#define IS_SPACE(c) ((c) == ' ' || (c) == '\r' || (c) == '\n' || (c) == '\t')
#define IS_ALPHA(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') || (c) == '_')
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define IS_ALNUM(c) (IS_ALPHA(c) || IS_DIGIT(c))

#define CUR   ((in->pos   < in->size) ? (int)in->data[in->pos  ] : -1)
#define NEXT  ((in->pos+1 < in->size) ? (int)in->data[in->pos+1] : -1)
#define NEXT2 ((in->pos+2 < in->size) ? (int)in->data[in->pos+2] : -1)
#define CUR_POS    (in->pos)
#define ADVANCE()  (in->pos++)
#define SET_POS(p) (in->pos = (p))

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
          *err = 1;
          return false;
        }
        if (CUR == '*' && NEXT == '/') {
          ADVANCE();
          ADVANCE();
          break;
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
  while (true) {
    ADVANCE();
    if (CUR == '\\')
      skip_bs_newline(in);
    if (CUR < 0)
      return -1; // set_error(pp, "unterminated header");
    if (CUR == '\n')
      return -1; // set_error(pp, "unterminated header name");
    if (CUR == end_char) {
      ADVANCE();
      break;
    }
    if (sp_buf_add_byte(buf, CUR) < 0)
      return -1; // set_error(pp, "out of memory");
  }
  if (sp_buf_add_byte(buf, '\0') < 0)
    return -1;   // set_error(pp, "out of memory");
  //sp_string_id str_id = sp_add_string(&pp->ast->strings, buf->p);
  //if (str_id < 0)
  //  return set_error(pp, "out of memory");
  //pp->tok.data.str_id = str_id;
  return TOK_PP_HEADER_NAME;
}

static int read_string(struct sp_input *in, struct sp_buffer *buf)
{
  buf->size = 0;
  while (true) {
    ADVANCE();
    if (CUR == '\\') {
      if (! skip_bs_newline(in)) {
        if (sp_buf_add_byte(buf, CUR) < 0)
          return -1; // set_error(pp, "out of memory");
        ADVANCE();
        if (CUR < 0)
          return -1; // set_error(pp, "unterminated string");
        if (sp_buf_add_byte(buf, CUR) < 0)
          return -1; // set_error(pp, "out of memory");
        ADVANCE();
      }
    }
    if (CUR == '\n')
      return -1; // set_error(pp, "unterminated string");
    if (CUR < 0) {
      printf("*** UNTERMINATED STRING");
      return -1; // set_error(pp, "unterminated string");
    }
    if (CUR == '"') {
      ADVANCE();
      break;
    }
    if (sp_buf_add_byte(buf, CUR) < 0)
      return -1; // set_error(pp, "out of memory");
  }
  if (sp_buf_add_byte(buf, '\0') < 0)
    return -1;   // set_error(pp, "out of memory");
  //sp_string_id str_id = sp_add_string(&pp->ast->strings, buf->p);
  //if (str_id < 0)
  //  return set_error(pp, "out of memory");
  //pp->tok.data.str_id = str_id;
  return TOK_STRING;
}

static int read_number(struct sp_input *in, struct sp_buffer *buf)
{
  buf->size = 0;
  if (sp_buf_add_byte(buf, CUR) < 0)
    return -1; // set_error(pp, "out of memory");
  ADVANCE();
  while (true) {
    if ((CUR == 'e' || CUR == 'E' || CUR == 'p' || CUR == 'P')
        && (NEXT == '-' || NEXT == '+')) {
      if (sp_buf_add_byte(buf, CUR) < 0)
        return -1; // set_error(pp, "out of memory");
      ADVANCE();
      if (sp_buf_add_byte(buf, CUR) < 0)
        return -1; // set_error(pp, "out of memory");
      ADVANCE();
      continue;
    }
    if (IS_DIGIT(CUR) || IS_ALPHA(CUR) || CUR == '.') {
      if (sp_buf_add_byte(buf, CUR) < 0)
        return -1; // set_error(pp, "out of memory");
      ADVANCE();
      continue;
    }
    break;
  }
  if (sp_buf_add_byte(buf, '\0') < 0)
    return -1;   // set_error(pp, "out of memory");
  return TOK_PP_NUMBER;
}

static int read_ident(struct sp_input *in, struct sp_buffer *buf)
{
  buf->size = 0;
  if (sp_buf_add_byte(buf, CUR) < 0)
    return -1; // set_error(pp, "out of memory");
  ADVANCE();
  while (true) {
    if (CUR == '\\' && ! skip_bs_newline(in))
      break;
    if (! IS_ALNUM(CUR))
      break;
    if (sp_buf_add_byte(buf, CUR) < 0)
      return -1; // set_error(pp, "out of memory");
    ADVANCE();
  }
  if (sp_buf_add_byte(buf, '\0') < 0)
    return -1;   // set_error(pp, "out of memory");
  return TOK_IDENTIFIER;
}

static int read_token(struct sp_input *in, struct sp_buffer *buf, bool parse_header)
{
  int err = 0;
  
  if (CUR < 0)
    return TOK_EOF;

  /* comment */
  if (skip_comments(in, &err))
    goto skip_spaces;
  if (err)
    return -1;  // TODO: error message

  /* spaces/newlines */
  skip_bs_newline(in);
  if (skip_spaces(in)) {
  skip_spaces:;
    bool got_newline = false;

    while (skip_spaces(in) || skip_bs_newline(in))
      ;
    do {
      if (CUR == '\n') {
        ADVANCE();
        got_newline = true;
      }
    } while (skip_spaces(in) || skip_bs_newline(in) || (got_newline && CUR == '\n'));
    if (got_newline) {
      if (skip_comments(in, &err))
        goto skip_newlines;
      if (err)
        return -1;
    } else {
      if (skip_comments(in, &err))
        goto skip_spaces;
      if (err)
        return -1;
    }
    return (got_newline) ? '\n' : ' ';
  }

  /* newlines */
  if (CUR == '\n') {
  skip_newlines:
    do {
      if (CUR == '\n')
        ADVANCE();
    } while (skip_spaces(in) || skip_bs_newline(in) || CUR == '\n');
    if (skip_comments(in, &err))
      goto skip_newlines;
    if (err)
      return -1;
    return '\n';
  }

  /* <header> */
  if (parse_header && (CUR == '<' || CUR == '"'))
    return read_header(in, buf);

  /* "string" */
  if (CUR == '"')
    return read_string(in, buf);

  /* number */
  if (IS_DIGIT(CUR))
    return read_number(in, buf);
  if (CUR == '.' && IS_DIGIT(NEXT))
    return read_number(in, buf);

  /* identifier */
  if (IS_ALPHA(CUR))
    return read_ident(in, buf);

  // TODO: punctuation
  
  int c = CUR;
  ADVANCE();
  return c;
}

void test_new_input(struct sp_input *in)
{
  struct sp_mem_pool pool;
  sp_init_mem_pool(&pool);
  struct sp_buffer tmp_buf;
  sp_init_buffer(&tmp_buf, &pool);
  
  while (true) {
    int c = read_token(in, &tmp_buf, false);
    if (c < 0) {
      printf("\n*** ERROR ***\n");
      goto end;
    }
    switch (c) {
    case TOK_EOF: printf("<eof>\n"); goto end;
    case TOK_PP_SPACE: printf(" "); break;
    case TOK_PP_NEWLINE:
    case '\n':
      printf("<nl>\n"); break;
      
    default:
      if (c > 256)
        printf("<type %d: '%s'>", c, tmp_buf.p);
      else
        printf("%c", c);
    }
  }

 end:
  sp_destroy_mem_pool(&pool);
}
