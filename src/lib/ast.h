/* ast.h */

#ifndef AST_H_FILE
#define AST_H_FILE

#include "internal.h"

struct sp_ast {
  struct sp_mem_pool *pool;
  struct sp_string_table strings;
  struct sp_string_table *file_names;
};

struct sp_ast *sp_new_ast(struct sp_mem_pool *pool, struct sp_string_table *file_names);
const char *sp_get_ast_string(struct sp_ast *ast, sp_string_id id);
const char *sp_get_ast_file_name(struct sp_ast *ast, sp_string_id file_id);
sp_string_id sp_add_ast_file_name(struct sp_ast *ast, const char *filename);

#endif /* AST_H_FILE */
