/* tokenizer.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#include "tokenizer.h"
#include "input.h"
#include "ast.h"

static int read_token(struct sp_tokenizer *t, struct sp_token *tok, bool process_keywords);

#define IS_SPACE(c) ((c) == ' ' || (c) == '\r' || (c) == '\n' || (c) == '\t')
#define IS_ALPHA(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') || (c) == '_')
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define IS_ALNUM(c) (IS_ALPHA(c) || IS_DIGIT(c))

#define ADD_KW(kw) { KW_ ## kw, # kw }
static const struct keyword {
  enum sp_keyword_type type;
  const char *name;
} keywords[] = {
  ADD_KW(auto),
  ADD_KW(break),
  ADD_KW(case),
  ADD_KW(char),
  ADD_KW(const),
  ADD_KW(continue),
  ADD_KW(default),
  ADD_KW(do),
  ADD_KW(double),
  ADD_KW(else),
  ADD_KW(enum),
  ADD_KW(extern),
  ADD_KW(float),
  ADD_KW(for),
  ADD_KW(goto),
  ADD_KW(if),
  ADD_KW(inline),
  ADD_KW(int),
  ADD_KW(long),
  ADD_KW(register),
  ADD_KW(restrict),
  ADD_KW(return),
  ADD_KW(short),
  ADD_KW(signed),
  ADD_KW(sizeof),
  ADD_KW(static),
  ADD_KW(struct),
  ADD_KW(switch),
  ADD_KW(typedef),
  ADD_KW(unsigned),
  ADD_KW(void),
  ADD_KW(volatile),
  ADD_KW(while),  
};

enum pp_directive_type {
  PP_NONE,
  PP_if,
  PP_ifdef,
  PP_ifndef,
  PP_elif,
  PP_else,
  PP_endif,
  PP_include,
  PP_define,
  PP_undef,
  PP_line,
  PP_error,
  PP_pragma,
};

#define ADD_PP(pp) { PP_ ## pp, # pp }
static const struct pp_directive {
  enum pp_directive_type type;
  const char *name;
} pp_directives[] = {
  ADD_PP(if),
  ADD_PP(ifdef),
  ADD_PP(ifndef),
  ADD_PP(elif),
  ADD_PP(else),
  ADD_PP(endif),
  ADD_PP(include),
  ADD_PP(define),
  ADD_PP(undef),
  ADD_PP(line),
  ADD_PP(error),
  ADD_PP(pragma),
};

void sp_init_tokenizer(struct sp_tokenizer *t, struct sp_program *prog, struct sp_input *in, struct sp_ast *ast, struct sp_buffer *tmp_buf, uint16_t file_id)
{
  t->prog = prog;
  t->at_line_start = true;
  t->in = in;
  t->ast = ast;
  t->tmp = tmp_buf;
  t->file_id = file_id;
  t->cur_loc = sp_make_src_loc(file_id, 1, 0);
  t->last_err_loc = t->cur_loc;
}

static int set_error(struct sp_tokenizer *t, struct sp_src_loc loc, char *fmt, ...)
{
  char str[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  sp_set_error(t->prog, "%s:%d:%d: %s", sp_get_ast_file_name(t->ast, loc.file_id), loc.line, loc.col, str);
  t->last_err_loc = loc;
  return -1;
}

struct sp_src_loc sp_get_tokenizer_error_loc(struct sp_tokenizer *t)
{
  return t->last_err_loc;
}

const char *sp_get_token_keyword(struct sp_token *tok)
{
  for (int i = 0; i < ARRAY_SIZE(keywords); i++) {
    if (keywords[i].type == tok->data.keyword)
      return keywords[i].name;
  }
  return NULL;
}

const char *sp_get_token_symbol(struct sp_ast *ast, struct sp_token *tok)
{
  return sp_get_symbol_name(&ast->symtab, tok->data.symbol_id);
}

const char *sp_get_token_op(struct sp_token *tok)
{
  return tok->data.op_name;
}

const char *sp_get_token_string(struct sp_ast *ast, struct sp_token *tok)
{
  if (tok->type == TOK_STRING)
    return sp_get_ast_string(ast, tok->data.str);
  return NULL;
}

static int next_byte(struct sp_tokenizer *t)
{
  while (t->in) {
    int c = sp_input_next_byte(t->in);
    if (c >= 0)
      return c;
    t->in = t->in->next;
  }
  return -1;
}

#define unget_byte(t, b) sp_input_unget_byte((t)->in, b)

static bool find_keyword(const void *string, int size, enum sp_keyword_type *ret)
{
  for (int i = 0; i < ARRAY_SIZE(keywords); i++) {
    if (strncmp(string, keywords[i].name, size) == 0 && keywords[i].name[size] == '\0') {
      *ret = keywords[i].type;
      return true;
    }
  }
  return false;
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

static char to_hex_char(int i)
{
  if (i < 10) return '0' + i;
  return 'a' + i - 10;
}

const char *sp_dump_token(struct sp_ast *ast, struct sp_token *tok)
{
  static char str[256];
  
  switch (tok->type) {
  case TOK_EOF:
    snprintf(str, sizeof(str), "<end-of-file>");
    break;
    
  case TOK_KEYWORD:
    snprintf(str, sizeof(str), "%s", sp_get_token_keyword(tok));
    break;
    
  case TOK_SYMBOL:
    snprintf(str, sizeof(str), "%s", sp_get_token_symbol(ast, tok));
    break;
    
  case TOK_OP:
    snprintf(str, sizeof(str), "%s", sp_get_token_op(tok));
    break;
    
  case TOK_PUNCT:
    snprintf(str, sizeof(str), "%c", tok->data.punct);
    break;
    
  case TOK_STRING:
    {
      const uint8_t *in = (const uint8_t *) sp_get_token_string(ast, tok);
      char *out = str;
      *out++ = '\"';
      while (*in != '\0') {
        if (*in == '\n') {
          if (out - str + 4 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = 'n';
        } else if (*in == '\r') {
          if (out - str + 4 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = 'r';
        } else if (*in == '\t') {
          if (out - str + 4 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = 't';
        } else if (*in < 32) {
          if (out - str + 6 >= (int)sizeof(str)) break;
          *out++ = '\\';
          *out++ = 'x';
          *out++ = to_hex_char((*in>>4) & 0xf);
          *out++ = to_hex_char(   (*in) & 0xf);
        } else {
          if (out - str + 3 >= (int)sizeof(str)) break;
          *out++ = *in;
        }
        in++;
      }
      *out++ = '\"';
      *out++ = '\0';
    }
    break;
    
  case TOK_DOUBLE:
    snprintf(str, sizeof(str), "%g", tok->data.d);
    break;
    
  case TOK_INT:
    snprintf(str, sizeof(str), "%" PRIu64, tok->data.i);
    break;
    
  default:
    snprintf(str, sizeof(str), "<unknown token type %d>", tok->type);
    break;
  }

  return str;
}

static int process_pp_directive(struct sp_tokenizer *t)
{
  struct sp_token tok;
  
  if (read_token(t, &tok, false) < 0)
    return -1;

  enum pp_directive_type pp_directive = PP_NONE;
  if (tok.type != TOK_SYMBOL || ! find_pp_directive(sp_get_token_symbol(t->ast, &tok), &pp_directive))
    return set_error(t, tok.loc, "unknown preprocessor directive: '%.*s'", t->tmp->size, t->tmp->p);

  switch (pp_directive) {
  case PP_include:
    if (t->in->type != SP_INPUT_FILE)
      return set_error(t, tok.loc, "invalid #include location");
    const char *base_filename = sp_get_ast_file_name(t->ast, t->in->source.file.file_id);
      
    if (read_token(t, &tok, false) < 0)
      return -1;
    // TODO: support <file> and MACRO
    if (tok.type != TOK_STRING)
      return set_error(t, tok.loc, "expected string", t->tmp->size, t->tmp->p);
    const char *filename = sp_get_token_string(t->ast, &tok);
    sp_symbol_id file_id = sp_add_ast_file_name(t->ast, filename);
    if (file_id < 0)
      return set_error(t, tok.loc, "out of memory");
    struct sp_input *in = sp_open_input_file(t->ast->pool, filename, (uint16_t) file_id, base_filename);
    if (! in)
      return set_error(t, tok.loc, "can't open '%s'", filename);
    in->next = t->in;
    t->in = in;
    return 0;

  case PP_if:
  case PP_ifdef:
  case PP_ifndef:
  case PP_elif:
  case PP_else:
  case PP_endif:
  case PP_define:
  case PP_undef:
  case PP_line:
  case PP_error:
  case PP_pragma:
    return set_error(t, tok.loc, "this preprocessor directive is not implemented");

  case PP_NONE:
    break;
  }
  
  return set_error(t, tok.loc, "invalid preprocessor directive: '%.*s'", t->tmp->size, t->tmp->p);
  return -1;
}

#define NEXT_BYTE(c, t) do { c = next_byte(t); if (c < 0) goto eof; } while (0)

static int read_token(struct sp_tokenizer *t, struct sp_token *tok, bool process_keywords)
{
  int c;

  // This really should be a finite state machine
  while (true) {
    NEXT_BYTE(c, t);
    if (c == '\n')
      t->at_line_start = true;
    if (IS_SPACE(c))
      continue;

    // backslash
    if (c == '\\') {
      while (true) {
        NEXT_BYTE(c, t);
        if (c == '\n') {
          NEXT_BYTE(c, t);
          break;
        }
        if (! IS_SPACE(c))
          return set_error(t, t->cur_loc, "invalid character after '\\'");
      }
      if (! IS_SPACE(c))
        break;
    }

    if (c == '#') {
      if (process_pp_directive(t) < 0)
        return -1;
      continue;
    }

    // comment
    if (c == '/') {
      NEXT_BYTE(c, t);
      if (c == '*') {
        while (true) {
          do {
            NEXT_BYTE(c, t);
          } while (c != '*');
          NEXT_BYTE(c, t);
          if (c == '/')
            break;
        }
        continue;
      } else if (c == '/') {
        while (true) {
          NEXT_BYTE(c, t);
          if (c == '\n')
            break;
          if (c == '\\') {
            do {
              NEXT_BYTE(c, t);
              if (c == '\n')
                break;
            } while (IS_SPACE(c));
            if (c != '\n')
              break;
          }
        }
        continue;
      } else {
        unget_byte(t, c);
      }
    }
    
    break;
  }

  tok->loc = t->cur_loc;

  // string
  if (c == '"') {
    t->tmp->size = 0;
    while (1) {
      c = next_byte(t);
      if (c < 0)
        return set_error(t, tok->loc, "unterminated string");
      if (c == '"')
        break;
      if (c == '\\') {
        int next = next_byte(t);
        if (next < 0)
          return set_error(t, tok->loc, "unterminated string");
        switch (next) {
        case '"': c = '"'; break;
        case '\\': c = '\\'; break;
        case '\'': c = '\''; break;
        case 'e': c = '\x1b'; break;
        case 'n': c = '\n'; break;
        case 't': c = '\t'; break;
        case 'r': c = '\r'; break;
        default:
          return set_error(t, t->cur_loc, "bad escape sequence");
        }
      }
      if (sp_buf_add_byte(t->tmp, c) < 0)
        return set_error(t, tok->loc, "out of memory");
    }
    if (sp_utf8_len(t->tmp->p, t->tmp->size) < 0)
      return set_error(t, tok->loc, "invalid utf-8 string");

    sp_string_id str_pos = sp_buf_add_string(&t->ast->string_pool, t->tmp->p, t->tmp->size);
    if (str_pos < 0)
      return set_error(t, tok->loc, "out of memory");
    tok->type = TOK_STRING;
    tok->data.str = str_pos;
    return 0;
  }

  // number
  if (IS_DIGIT(c)) {
    t->tmp->size = 0;
    int got_point = 0;
    while (IS_DIGIT(c) || c == '.') {
      if (c == '.') {
        if (got_point)
          break;
        got_point = 1;
      }
      if (sp_buf_add_byte(t->tmp, c) < 0)
        return set_error(t, tok->loc, "out of memory");
      c = next_byte(t);
    }
    if (c >= 0)
      unget_byte(t, c);

    if (sp_buf_add_byte(t->tmp, '\0') < 0)
      return set_error(t, tok->loc, "out of memory");
    
    char *end = NULL;
    double num = strtod(t->tmp->p, &end);
    if (t->tmp->p == end)
      return set_error(t, tok->loc, "invalid number");
    tok->type = TOK_DOUBLE;
    tok->data.d = num;
    return 0;
  }

  // keyword or symbol
  if (IS_ALPHA(c)) {
    t->tmp->size = 0;
    while (IS_ALNUM(c)) {
      if (sp_buf_add_byte(t->tmp, c) < 0)
        return set_error(t, tok->loc, "out of memory");
      c = next_byte(t);
    }
    if (c >= 0)
      unget_byte(t, c);

    enum sp_keyword_type keyword;
    if (process_keywords && find_keyword(t->tmp->p, t->tmp->size, &keyword)) {
      // keyword
      tok->type = TOK_KEYWORD;
      tok->data.keyword = keyword;
    } else {
      // other symbol
      if (sp_buf_add_byte(t->tmp, '\0') < 0)
        return set_error(t, tok->loc, "out of memory");
      sp_symbol_id symbol_id = sp_add_symbol(&t->ast->symtab, t->tmp->p);
      if (symbol_id < 0)
        return set_error(t, tok->loc, "out of memory");
      tok->type = TOK_SYMBOL;
      tok->data.symbol_id = symbol_id;
    }
    
    return 0;
  }

  // punctuation
  if (c == ',' || c == '.' || c == ';' || c == ':' ||
      c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}') {
    tok->type = TOK_PUNCT;
    tok->data.punct = c;
    return 0;
  }

  // operator
  struct sp_src_loc op_loc = t->cur_loc;
  char op_name[4] = { c };
  int op_len = 1;
  while (1) {
    op_name[op_len] = '\0';
    if (! sp_get_op(op_name)) {
      op_name[--op_len] = '\0';
      unget_byte(t, c);
      break;
    }

    if (op_len == sizeof(op_name)-1)
      break;
    c = next_byte(t);
    if (c < 0)
      break;
    op_name[op_len++] = c;
  }
  if (op_len > 0) {
    tok->type = TOK_OP;
    strcpy(tok->data.op_name, op_name);
    return 0;
  }

  if (c >= 32 && c < 128)
    set_error(t, op_loc, "invalid character: '%c'", c);
  else
    set_error(t, op_loc, "invalid byte: 0x%02x", c);
  return -1;

 eof:
  tok->type = TOK_EOF;
  tok->loc = t->cur_loc;
  return 0;
}

int sp_read_token(struct sp_tokenizer *t, struct sp_token *tok)
{
  return read_token(t, tok, true);
}
