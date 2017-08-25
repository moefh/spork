/* program.h */

#ifndef PROGRAM_H_FILE
#define PROGRAM_H_FILE

#include "internal.h"

struct sp_program {
  char last_error_msg[256];
  struct sp_symtab src_file_names;
};

#endif /* PROGRAM_H_FILE */
