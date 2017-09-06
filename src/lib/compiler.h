/* compiler.h */

#ifndef COMPILER_H_FILE
#define COMPILER_H_FILE

#include "internal.h"

struct sp_include_search_dir {
  struct sp_include_search_dir *next;
  char dir[];
};

struct sp_compiler {
  struct sp_program *prog;
  struct sp_mem_pool pool;
  struct sp_preprocessor *pp;
  struct sp_ast *ast;

  struct sp_include_search_dir *sys_include_search_dirs;
  struct sp_include_search_dir *user_include_search_dirs;
};

int sp_init_compiler(struct sp_compiler *comp, struct sp_program *prog);
void sp_destroy_compiler(struct sp_compiler *comp);
int sp_comp_add_include_search_dir(struct sp_compiler *comp, const char *dir, bool is_system);
int sp_comp_preprocess_file(struct sp_compiler *comp, const char *filename);
int sp_comp_compile_file(struct sp_compiler *comp, const char *filename, struct sp_ast *ast);

#endif /* COMPILER_H_FILE */
