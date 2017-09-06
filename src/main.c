/* main.c */

#include <stdio.h>
#include <string.h>

#include <spork.h>

static const char *include_dirs[] = {
  "tests/sys_include"
  //"/usr/include"
};

struct sp_program *create_prog(void)
{
  struct sp_program *prog = sp_new_program();
  if (! prog) {
    printf("ERROR: out of memory\n");
    return NULL;
  }

  for (int i = 0; i < (int) (sizeof(include_dirs)/sizeof(include_dirs[0])); i++) {
    if (sp_add_include_search_dir(prog, include_dirs[i], true) < 0) {
      printf("ERROR: %s\n", sp_get_error(prog));
      sp_free_program(prog);
      return NULL;
    }
  }

  return prog;
}

void preprocess(const char *filename)
{
  struct sp_program *prog = create_prog();
  if (sp_preprocess_file(prog, filename) < 0)
    printf("\nERROR: %s\n", sp_get_error(prog));
  sp_free_program(prog);
}

void compile(const char *filename)
{
  struct sp_program *prog = create_prog();
  if (sp_compile_file(prog, filename) < 0)
    printf("\nERROR: %s\n", sp_get_error(prog));
  sp_free_program(prog);
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    printf("USAGE: %s [-E] filename.spork\n", argv[0]);
    return 1;
  }
  if (strcmp(argv[1], "-E") == 0)
    preprocess(argv[2]);
  else
    compile(argv[1]);
  return 0;
}
