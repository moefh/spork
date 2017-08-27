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

#define CUR  ((in->pos   < in->size) ? (int)in->data[in->pos  ] : -1)
#define NEXT ((in->pos+1 < in->size) ? (int)in->data[in->pos+1] : -1)
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
    if (CUR != '\\') {
      SET_POS(rewind_pos);
      break;
    }
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

static bool skip_comments(struct sp_input *in)
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

static int next_token(struct sp_input *in)
{
  //printf("=== next_token ===\n");
  if (CUR < 0)
    return TOK_EOF;

  skip_bs_newline(in);
  if (skip_spaces(in) || skip_comments(in)) {
    bool got_newline = false;

    while (skip_spaces(in) || skip_comments(in) || skip_bs_newline(in))
      ;
    do {
      if (CUR == '\n') {
        ADVANCE();
        got_newline = true;
      }
    } while (skip_spaces(in) || skip_comments(in) || skip_bs_newline(in) || (got_newline && CUR == '\n'));
    return (got_newline) ? '\n' : ' ';
  }

  if (CUR == '\n') {
    do {
      if (CUR == '\n')
        ADVANCE();
    } while (skip_spaces(in) || skip_comments(in) || skip_bs_newline(in) || CUR == '\n');
    return '\n';
  }
  
  int c = CUR;
  ADVANCE();
  return c;
}

void test_new_input(struct sp_input *in)
{
  while (true) {
    int c = next_token(in);
    if (c < 0) {
      printf("\n*** ERROR ***\n");
      return;
    }
    switch (c) {
    case TOK_EOF: printf("<eof>\n"); return;
    case TOK_PP_SPACE: printf(" "); break;
    case TOK_PP_NEWLINE:
    case '\n':
      printf("<nl>\n"); break;
    default:
      if (c > 256)
        printf("<unknown token %d>", c);
      else
        printf("%c", c);
    }
  }
}
