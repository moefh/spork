/* ast.h */

#ifndef AST_H_FILE
#define AST_H_FILE

#include "internal.h"

enum {
  AST_OP_UNM = 256,
  AST_OP_EQ,
  AST_OP_NEQ,
  AST_OP_GT,
  AST_OP_GE,
  AST_OP_LT,
  AST_OP_LE,
  AST_OP_OR,
  AST_OP_AND,
};

struct sp_ast {
  struct sp_mem_pool *pool;
  struct sp_buffer string_pool;
  struct sp_symtab symtab;
  struct sp_symtab *file_names;
};

struct sp_ast *sp_new_ast(struct sp_mem_pool *pool, struct sp_symtab *file_names);
void sp_free_ast(struct sp_ast *ast);
const char *sp_get_ast_symbol(struct sp_ast *ast, sp_symbol_id id);
const char *sp_get_ast_string(struct sp_ast *ast, sp_string_id id);
const char *sp_get_ast_file_name(struct sp_ast *ast, sp_symbol_id file_id);
sp_symbol_id sp_add_ast_file_name(struct sp_ast *ast, const char *filename);

#endif /* AST_H_FILE */
