/* ast.c */

#include "ast.h"

struct sp_ast *sp_new_ast(struct sp_mem_pool *pool, struct sp_symtab *file_names)
{
  struct sp_ast *ast = sp_malloc(pool, sizeof(struct sp_ast));
  if (! ast)
    return NULL;
  ast->pool = pool;
  ast->file_names = file_names;
  sp_init_symtab(&ast->symtab, pool);
  sp_init_buffer(&ast->string_pool, pool);
  return ast;
}

void sp_free_ast(struct sp_ast *ast)
{
  sp_destroy_buffer(&ast->string_pool);
  sp_destroy_symtab(&ast->symtab);
  sp_free(ast->pool, ast);
}

const char *sp_get_ast_symbol(struct sp_ast *ast, sp_symbol_id id)
{
  return sp_get_symbol_name(&ast->symtab, id);
}

const char *sp_get_ast_string(struct sp_ast *ast, sp_string_id id)
{
  return ast->string_pool.p + id;
}

sp_symbol_id sp_add_ast_file_name(struct sp_ast *ast, const char *filename)
{
  return sp_add_symbol(ast->file_names, filename);
}

const char *sp_get_ast_file_name(struct sp_ast *ast, sp_symbol_id file_id)
{
  return sp_get_symbol_name(ast->file_names, file_id);
}
