/* program.c */

#include <stdlib.h>
#include <stdio.h>

#include "program.h"
#include "input.h"
#include "buffer.h"
#include "ast.h"
#include "preprocessor.h"

struct sp_program *sp_new_program(void)
{
  struct sp_program *prog = malloc(sizeof(struct sp_program));
  if (! prog)
    return NULL;
  prog->last_error_msg[0] = '\0';
  sp_init_string_table(&prog->src_file_names, NULL);
  return prog;
}

void sp_free_program(struct sp_program *prog)
{
  sp_destroy_string_table(&prog->src_file_names);
  free(prog);
}

const char *sp_get_error(struct sp_program *prog)
{
  return prog->last_error_msg;
}

int sp_set_error(struct sp_program *prog, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(prog->last_error_msg, sizeof(prog->last_error_msg), fmt, ap);
  va_end(ap);
  return -1;
}

void test_new_input(struct sp_input *in);

int sp_compile_program(struct sp_program *prog, const char *filename)
{
  struct sp_input *file = NULL;

  struct sp_mem_pool pool;
  sp_init_mem_pool(&pool);

  struct sp_preprocessor pp;
  sp_init_preprocessor(&pp, prog, &pool);

  struct sp_ast *ast = sp_new_ast(&pool, &prog->src_file_names);
  if (! ast) {
    sp_set_error(prog, "out of memory");
    goto err;
  }

  sp_string_id file_id = sp_add_ast_file_name(ast, filename);
  if (file_id < 0) {
    sp_set_error(prog, "out of memory");
    goto err;
  }
  
  file = sp_new_input_from_file(filename, (uint16_t) file_id, NULL);
  if (! file) {
    sp_set_error(prog, "can't open '%s'", filename);
    goto err;
  }

  sp_set_preprocessor_io(&pp, file, ast);
  printf("===================================\n");
  struct sp_pp_token tok;
  do {
    if (sp_next_pp_token(&pp, &tok) < 0)
      goto err;
    printf("%s", sp_dump_pp_token(&pp, &tok));
    if (pp_tok_is_newline(&tok))
      printf("\n");
  } while (! pp_tok_is_eof(&tok));
  printf("\n");
  printf("===================================\n");

  if (pp.macros.len > 0) {
    printf("// macros:\n");
    sp_dump_macros(&pp);
    printf("===================================\n");
  }

  sp_free_input(file);
  sp_destroy_preprocessor(&pp);
  sp_destroy_mem_pool(&pool);
  return 0;
      
 err:
  if (file)
    sp_free_input(file);
  sp_destroy_preprocessor(&pp);
  sp_destroy_mem_pool(&pool);
  return -1;
}
