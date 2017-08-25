/* ast.c */

#include "ast.h"

struct sp_ast *sp_new_ast(struct sp_mem_pool *pool, struct sp_string_table *file_names)
{
  struct sp_ast *ast = sp_malloc(pool, sizeof(struct sp_ast));
  if (! ast)
    return NULL;
  ast->pool = pool;
  ast->file_names = file_names;
  sp_init_string_table(&ast->strings, pool);
  return ast;
}

const char *sp_get_ast_string(struct sp_ast *ast, sp_string_id id)
{
  return sp_get_string(&ast->strings, id);
}

sp_string_id sp_add_ast_file_name(struct sp_ast *ast, const char *filename)
{
  return sp_add_string(ast->file_names, filename);
}

const char *sp_get_ast_file_name(struct sp_ast *ast, sp_string_id file_id)
{
  return sp_get_string(ast->file_names, file_id);
}
