/* compiler.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "compiler.h"
#include "ast.h"
#include "program.h"
#include "preprocessor.h"
#include "token.h"

int sp_init_compiler(struct sp_compiler *comp, struct sp_program *prog)
{
  comp->prog = prog;
  comp->sys_include_search_dirs = NULL;
  comp->user_include_search_dirs = NULL;
  sp_init_mem_pool(&comp->pool);
  return 0;
}

static void free_include_search_dirs(struct sp_include_search_dir *list)
{
  while (list) {
    struct sp_include_search_dir *next = list->next;
    free(list);
    list = next;
  }
}

void sp_destroy_compiler(struct sp_compiler *comp)
{
  free_include_search_dirs(comp->sys_include_search_dirs);
  free_include_search_dirs(comp->user_include_search_dirs);
  sp_destroy_mem_pool(&comp->pool);
}

int sp_comp_add_include_search_dir(struct sp_compiler *comp, const char *dir, bool is_system)
{
  size_t dir_len = strlen(dir);
  struct sp_include_search_dir *d = malloc(sizeof(struct sp_include_search_dir) + dir_len + 1);
  if (! d) {
    sp_set_error(comp->prog, "out of memory");
    return -1;
  }
  memcpy(d->dir, dir, dir_len + 1);
  
  if (is_system) {
    d->next = comp->sys_include_search_dirs;
    comp->sys_include_search_dirs = d;
  } else {
    d->next = comp->user_include_search_dirs;
    comp->user_include_search_dirs = d;
  }
  return 0;
}

int sp_comp_preprocess_file(struct sp_compiler *comp, const char *filename)
{
  sp_clear_mem_pool(&comp->pool);

  comp->ast = sp_new_ast(&comp->pool, &comp->prog->src_file_names);
  if (! comp->ast) {
    sp_set_error(comp->prog, "out of memory");
    goto err;
  }

  struct sp_preprocessor pp;
  comp->pp = &pp;
  sp_init_preprocessor(comp->pp, comp, &comp->pool);

  if (sp_set_preprocessor_io(comp->pp, filename, comp->ast) < 0)
    goto err;

  printf("===================================\n");
  struct sp_pp_token tok;
  do {
    if (sp_next_pp_token(comp->pp, &tok) < 0)
      goto err;
    printf("%s", sp_dump_pp_token(comp->pp, &tok));
    if (pp_tok_is_newline(&tok))
      printf("\n");
  } while (! pp_tok_is_eof(&tok));
  printf("\n");
  printf("===================================\n");

  if (pp.macros.len > 0) {
    printf("// macros:\n");
    sp_dump_macros(comp->pp);
    printf("===================================\n");
  }

  comp->ast = NULL;
  sp_destroy_preprocessor(comp->pp);
  sp_clear_mem_pool(&comp->pool);
  return 0;
      
 err:
  comp->ast = NULL;
  sp_destroy_preprocessor(comp->pp);
  sp_clear_mem_pool(&comp->pool);
  return -1;
}

int sp_comp_compile_file(struct sp_compiler *comp, const char *filename, struct sp_ast *ast)
{
  sp_clear_mem_pool(&comp->pool);

  comp->ast = ast;
  
  struct sp_preprocessor pp;
  comp->pp = &pp;
  sp_init_preprocessor(comp->pp, comp, &comp->pool);

  if (sp_set_preprocessor_io(comp->pp, filename, ast) < 0)
    goto err;

  struct sp_token tok;
  do {
    if (sp_next_token(comp->pp, &tok) < 0)
      goto err;
    printf("%s ", sp_dump_token(&tok, &comp->pp->token_strings));
    if (tok_is_punct(&tok, ';') || tok_is_punct(&tok, '{') || tok_is_punct(&tok, '}'))
      printf("\n");
  } while (! tok_is_eof(&tok));
  printf("\n");

  comp->ast = NULL;
  sp_destroy_preprocessor(comp->pp);
  sp_clear_mem_pool(&comp->pool);
  return 0;
      
 err:
  comp->ast = NULL;
  sp_destroy_preprocessor(comp->pp);
  sp_clear_mem_pool(&comp->pool);
  return -1;
}
