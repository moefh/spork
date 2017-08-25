/* main.c */

#include <stdio.h>

#include "lib/internal.h"
#include "lib/input.h"
#include "lib/program.h"
#include "lib/tokenizer.h"

void run(const char *filename)
{
  struct sp_program *prog = sp_new_program();
  if (! prog) {
    printf("ERROR: out of memory\n");
    return;
  }
  if (sp_compile_program(prog, filename) < 0)
    printf("ERROR: %s\n", sp_get_error(prog));
  sp_free_program(prog);
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    printf("USAGE: %s filename.spork\n", argv[0]);
    return 1;
  }
  run(argv[1]);
  return 0;
}
