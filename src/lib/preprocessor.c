/* preprocessor.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#include "preprocessor.h"
#include "input.h"
#include "ast.h"

#define IS_SPACE(c) ((c) == ' ' || (c) == '\r' || (c) == '\n' || (c) == '\t')
#define IS_ALPHA(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') || (c) == '_')
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define IS_ALNUM(c) (IS_ALPHA(c) || IS_DIGIT(c))

enum pp_directive_type {
  PP_DIR_if,
  PP_DIR_ifdef,
  PP_DIR_ifndef,
  PP_DIR_elif,
  PP_DIR_else,
  PP_DIR_endif,
  PP_DIR_include,
  PP_DIR_define,
  PP_DIR_undef,
  PP_DIR_line,
  PP_DIR_error,
  PP_DIR_pragma,
};

#define ADD_PP_DIR(dir) { PP_DIR_ ## dir, # dir }
static const struct pp_directive {
  enum pp_directive_type type;
  const char *name;
} pp_directives[] = {
  ADD_PP_DIR(if),
  ADD_PP_DIR(ifdef),
  ADD_PP_DIR(ifndef),
  ADD_PP_DIR(elif),
  ADD_PP_DIR(else),
  ADD_PP_DIR(endif),
  ADD_PP_DIR(include),
  ADD_PP_DIR(define),
  ADD_PP_DIR(undef),
  ADD_PP_DIR(line),
  ADD_PP_DIR(error),
  ADD_PP_DIR(pragma),
};

void sp_init_preprocessor(struct sp_preprocessor *pp, struct sp_program *prog, struct sp_ast *ast, struct sp_input *in, struct sp_buffer *tmp_buf)
{
  pp->prog = prog;
  pp->in = in;
  pp->ast = ast;
  pp->tmp = tmp_buf;
  pp->cur_loc = sp_make_src_loc(sp_get_input_file_id(in), 1, 0);
  pp->last_err_loc = pp->cur_loc;
}

static int set_error_at(struct sp_preprocessor *pp, struct sp_src_loc loc, char *fmt, ...)
{
  char str[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  sp_set_error(pp->prog, "%s:%d:%d: %s", sp_get_ast_file_name(pp->ast, loc.file_id), loc.line, loc.col, str);
  pp->last_err_loc = loc;
  return -1;
}

static int set_error(struct sp_preprocessor *pp, char *fmt, ...)
{
  char str[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  sp_set_error(pp->prog, "%s:%d:%d: %s", sp_get_ast_file_name(pp->ast, pp->tok.loc.file_id), pp->tok.loc.line, pp->tok.loc.col, str);
  pp->last_err_loc = pp->tok.loc;
  return -1;
}

static bool find_pp_directive(const char *string, enum pp_directive_type *ret)
{
  for (int i = 0; i < ARRAY_SIZE(pp_directives); i++) {
    if (strcmp(string, pp_directives[i].name) == 0) {
      *ret = pp_directives[i].type;
      return true;
    }
  }
  return false;
}

#define get_byte(pp)      sp_input_next_byte((pp)->in)
#define unget_byte(pp, b) sp_input_unget_byte((pp)->in, b)

#define NEXT_BYTE(c, pp) do { c = sp_input_next_byte(pp->in); if (c < 0) goto eof; } while (0)

/*
 * Translation phases 1-3
 */
static int next_token(struct sp_preprocessor *pp, bool parse_header)
{
  int c;

  // skip spaces, comments and \\ at the end of the line
  while (true) {
    NEXT_BYTE(c, pp);
    if (c == '\n')
      goto newline;
    if (IS_SPACE(c))
      continue;

    // backslash
    if (c == '\\') {
      while (true) {
        NEXT_BYTE(c, pp);
        if (c == '\n') {
          NEXT_BYTE(c, pp);
          break;
        }
        if (! IS_SPACE(c))
          return set_error(pp, "invalid character after '\\'");
      }
      if (! IS_SPACE(c))
        break;
    }

    // comment
    if (c == '/') {
      NEXT_BYTE(c, pp);
      if (c == '*') {
        while (true) {
          do {
            NEXT_BYTE(c, pp);
          } while (c != '*');
          NEXT_BYTE(c, pp);
          if (c == '/')
            break;
        }
        continue;
      } else if (c == '/') {
        while (true) {
          NEXT_BYTE(c, pp);
          if (c == '\n')
            break;
          if (c == '\\') {
            do {
              NEXT_BYTE(c, pp);
              if (c == '\n')
                break;
            } while (IS_SPACE(c));
            if (c != '\n')
              break;
          }
        }
        continue;
      } else {
        unget_byte(pp, c);
      }
    }
    
    break;
  }

  pp->tok.loc = pp->cur_loc;

  // header
  if (c == '<' && parse_header) {
    pp->tmp->size = 0;
    while (1) {
      c = get_byte(pp);
      if (c < 0)
        return set_error(pp, "unterminated string");
      if (c == '>')
        break;
      if (sp_buf_add_byte(pp->tmp, c) < 0)
        return set_error(pp, "out of memory");
    }
    if (sp_buf_add_byte(pp->tmp, '\0') < 0)
      return set_error(pp, "out of memory");
    sp_string_id str_id = sp_add_string(&pp->ast->strings, pp->tmp->p);
    if (str_id < 0)
      return set_error(pp, "out of memory");
    pp->tok.type = TOK_PP_HEADER_NAME;
    pp->tok.data.str_id = str_id;
    return 0;
  }
  
  // string
  if (c == '"') {
    pp->tmp->size = 0;
    while (1) {
      c = get_byte(pp);
      if (c < 0)
        return set_error(pp, "unterminated string");
      if (c == '"')
        break;
      if (c == '\\') {
        int next = get_byte(pp);
        if (next < 0)
          return set_error(pp, "unterminated string");
        switch (next) {
        case '"': c = '"'; break;
        case '\\': c = '\\'; break;
        case '\'': c = '\''; break;
        case 'e': c = '\x1b'; break;
        case 'n': c = '\n'; break;
        case 't': c = '\t'; break;
        case 'r': c = '\r'; break;
        default:
          return set_error_at(pp, pp->cur_loc, "bad escape sequence");
        }
      }
      if (sp_buf_add_byte(pp->tmp, c) < 0)
        return set_error(pp, "out of memory");
    }

    if (sp_buf_add_byte(pp->tmp, '\0') < 0)
      return set_error(pp, "out of memory");
    sp_string_id str_id = sp_add_string(&pp->ast->strings, pp->tmp->p);
    if (str_id < 0)
      return set_error(pp, "out of memory");
    pp->tok.type = TOK_STRING;
    pp->tok.data.str_id = str_id;
    return 0;
  }

  // TODO: char constant (TOK_PP_CHAR_CONST)
  
  // number
  if (IS_DIGIT(c)) {
    pp->tmp->size = 0;
    int got_point = 0;
    while (IS_DIGIT(c) || c == '.') {
      if (c == '.') {
        if (got_point)
          break;
        got_point = 1;
      }
      if (sp_buf_add_byte(pp->tmp, c) < 0)
        return set_error(pp, "out of memory");
      c = get_byte(pp);
    }
    if (c >= 0)
      unget_byte(pp, c);

    if (sp_buf_add_byte(pp->tmp, '\0') < 0)
      return set_error(pp, "out of memory");
    
    char *end = NULL;
    double num = strtod(pp->tmp->p, &end);
    if (pp->tmp->p == end)
      return set_error(pp, "invalid number");
    pp->tok.type = TOK_PP_NUMBER;
    pp->tok.data.d = num;
    return 0;
  }

  // identifier
  if (IS_ALPHA(c)) {
    pp->tmp->size = 0;
    while (IS_ALNUM(c)) {
      if (sp_buf_add_byte(pp->tmp, c) < 0)
        return set_error(pp, "out of memory");
      c = get_byte(pp);
    }
    if (c >= 0)
      unget_byte(pp, c);

    if (sp_buf_add_byte(pp->tmp, '\0') < 0)
      return set_error(pp, "out of memory");
    sp_string_id ident_id = sp_add_string(&pp->ast->strings, pp->tmp->p);
    if (ident_id < 0)
      return set_error(pp, "out of memory");
    pp->tok.type = TOK_IDENTIFIER;
    pp->tok.data.str_id = ident_id;
    return 0;
  }

  // handle '...'
  if (c == '.') {
    int c2 = get_byte(pp);
    if (c2 == '.') {
      int c3 = get_byte(pp);
      if (c3 == '.') {
        pp->tok.type = TOK_PUNCT;
        pp->tok.data.punct_id = sp_get_punct_id("...");
        return 0;
      }
      if (c3 >= 0)
        unget_byte(pp, c3);
    }
    if (c2 >= 0)
      unget_byte(pp, c2);
  }

  // punctuation
  struct sp_src_loc start_loc = pp->cur_loc;
  char punct_name[4] = { c };
  int punct_len = 1;
  while (1) {
    punct_name[punct_len] = '\0';
    if (sp_get_punct_id(punct_name) < 0) {
      punct_name[--punct_len] = '\0';
      unget_byte(pp, c);
      break;
    }

    if (punct_len == sizeof(punct_name)-1)
      break;
    c = get_byte(pp);
    if (c < 0)
      break;
    punct_name[punct_len++] = c;
  }
  if (punct_len > 0) {
    pp->tok.type = TOK_PUNCT;
    pp->tok.data.punct_id = sp_get_punct_id(punct_name);
    return 0;
  }

  if (c >= 32 && c < 128)
    set_error_at(pp, start_loc, "invalid character: '%c'", c);
  else
    set_error_at(pp, start_loc, "invalid byte: 0x%02x", c);
  return -1;

 newline:
  pp->tok.type = TOK_PP_NEWLINE;
  pp->tok.loc = pp->cur_loc;
  return 0;
  
 eof:
  pp->tok.type = TOK_EOF;
  pp->tok.loc = pp->cur_loc;
  return 0;
}

#define NEXT_TOKEN()   do { if (next_token(pp, false) < 0) return -1; } while (0)

#define IS_TOK_TYPE(t)         (pp->tok.type == t)
#define IS_EOF()               IS_TOK_TYPE(TOK_EOF)
#define IS_NEWLINE()           IS_TOK_TYPE(TOK_PP_NEWLINE)
#define IS_PP_HEADER_NAME()    IS_TOK_TYPE(TOK_PP_HEADER_NAME)
#define IS_STRING()            IS_TOK_TYPE(TOK_STRING)
#define IS_IDENTIFIER()        IS_TOK_TYPE(TOK_IDENTIFIER)
#define IS_PUNCT(id)           (IS_TOK_TYPE(TOK_PUNCT) && pp->tok.data.punct_id == (id))

static int process_include(struct sp_preprocessor *pp)
{
  if (next_token(pp, true) < 0)
    return -1;
  
  if (! IS_STRING() && ! IS_PP_HEADER_NAME())   // TODO: allow macro expansion
    return set_error(pp, "bad include file name: '%s'", sp_dump_token(pp->ast, &pp->tok));
  
  const char *filename = sp_get_token_string(pp->ast, &pp->tok);
  NEXT_TOKEN();
  if (! IS_NEWLINE())
    return set_error(pp, "unexpected input after include file name: '%s'", sp_dump_token(pp->ast, &pp->tok));
  
  const char *base_filename = sp_get_ast_file_name(pp->ast, sp_get_input_file_id(pp->in));
  sp_string_id file_id = sp_add_ast_file_name(pp->ast, filename);
  if (file_id < 0)
    return set_error(pp, "out of memory");

  // TODO: search file in pre-defined directories
  struct sp_input *in = sp_open_input_file(pp->ast->pool, filename, (uint16_t) file_id, base_filename);
  if (! in)
    return set_error(pp, "can't open '%s'", filename);
  in->next = pp->in;
  pp->in = in;

  return 0;
}

/*
 * Translation phase 4
 */
static int next_expanded_token(struct sp_preprocessor *pp)
{
  bool at_newline = false;

 start:
  NEXT_TOKEN();
  if (IS_NEWLINE()) {
    at_newline = true;
    while (IS_NEWLINE())
      if (next_token(pp, false) < 0)
        return -1;
  }

  if (IS_EOF()) {
    if (! pp->in->next)
      return 0;
    pp->in = pp->in->next;
    at_newline = true;
    goto start;
  }
  
  if (IS_PUNCT('#')) {
    if (! at_newline)
      return set_error(pp, "invalid '#'");

    NEXT_TOKEN();
    if (IS_NEWLINE())
      goto start;
    
    if (! IS_IDENTIFIER())
      return set_error(pp, "invalid input after '#': '%s'", sp_dump_token(pp->ast, &pp->tok));
  
    const char *directive_name = sp_get_token_string(pp->ast, &pp->tok);
    enum pp_directive_type directive;
    if (! find_pp_directive(directive_name, &directive))
      return set_error(pp, "invalid preprocessor directive: '#%s'", directive_name);

    switch (directive) {
    case PP_DIR_include:
      if (process_include(pp) < 0)
        return -1;
      goto start;

    case PP_DIR_if:
    case PP_DIR_ifdef:
    case PP_DIR_ifndef:
    case PP_DIR_elif:
    case PP_DIR_else:
    case PP_DIR_endif:
    case PP_DIR_define:
    case PP_DIR_undef:
    case PP_DIR_line:
    case PP_DIR_error:
    case PP_DIR_pragma:
      return set_error(pp, "preprocessor directive is not implemented: '#%s'", directive_name);
    }
    return set_error(pp, "invalid preprocessor directive: '#%s'", directive_name);
  }

  // TODO: handle macro expansion
  
  return 0;
}


int sp_read_token(struct sp_preprocessor *pp, struct sp_token *tok)
{
  if (next_expanded_token(pp) < 0)
    return -1;
  *tok = pp->tok;
  return 0;
}
