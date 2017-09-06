/* program.c */

#include <stdlib.h>
#include <stdio.h>

#include "program.h"
#include "input.h"
#include "buffer.h"
#include "ast.h"
#include "preprocessor.h"
#include "token.h"
#include "punct.h"

struct sp_program *sp_new_program(void)
{
  struct sp_program *prog = malloc(sizeof(struct sp_program));
  if (! prog)
    return NULL;
  prog->last_error_msg[0] = '\0';
  sp_init_string_table(&prog->src_file_names, NULL);
  sp_init_compiler(&prog->comp, prog);
  return prog;
}

void sp_free_program(struct sp_program *prog)
{
  sp_destroy_compiler(&prog->comp);
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

int sp_add_include_search_dir(struct sp_program *prog, const char *dir, bool is_system)
{
  return sp_comp_add_include_search_dir(&prog->comp, dir, is_system);
}

int sp_preprocess_file(struct sp_program *prog, const char *filename)
{
  return sp_comp_preprocess_file(&prog->comp, filename);
}

int sp_compile_file(struct sp_program *prog, const char *filename)
{
  struct sp_mem_pool ast_pool;
  sp_init_mem_pool(&ast_pool);
  
  struct sp_ast *ast = sp_new_ast(&ast_pool, &prog->src_file_names);
  if (! ast) {
    sp_set_error(prog, "out of memory");
    goto err;
  }
  
  if (sp_comp_compile_file(&prog->comp, filename, ast) < 0)
    goto err;
  
  sp_destroy_mem_pool(&ast_pool);
  return 0;

 err:
  sp_destroy_mem_pool(&ast_pool);
  return -1;
}
