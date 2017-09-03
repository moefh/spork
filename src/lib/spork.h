/* spork.h */

#ifndef SPORK_H_FILE
#define SPORK_H_FILE

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#if defined(__GNUC__)
#define SP_PRINTF_FORMAT(x,y) __attribute__((format (printf, (x), (y))))
#else
#define SP_PRINTF_FORMAT(x,y)
#endif

struct sp_program;

struct sp_program *sp_new_program(void);
void sp_free_program(struct sp_program *prog);

int sp_set_error(struct sp_program *prog, const char *fmt, ...) SP_PRINTF_FORMAT(2,3);
const char *sp_get_error(struct sp_program *prog);
int sp_compile_file(struct sp_program *prog, const char *filename);
int sp_preprocess_file(struct sp_program *prog, const char *filename);

#endif /* SPORK_H_FILE */
