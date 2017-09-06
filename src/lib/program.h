/* program.h */

#ifndef PROGRAM_H_FILE
#define PROGRAM_H_FILE

#include "internal.h"
#include "compiler.h"

struct sp_program {
  struct sp_string_table src_file_names;
  struct sp_compiler comp;
  char last_error_msg[256];
};

void sp_init_program(struct sp_program *prog);

#endif /* PROGRAM_H_FILE */
