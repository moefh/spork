/* lib.h */

#ifndef LIB_H_FILE
#define LIB_H_FILE

#include "spork.h"
#include "mem_pool.h"

#define ARRAY_SIZE(a)  ((int)(sizeof(a)/sizeof((a)[0])))

enum sp_op_assoc {
  SP_ASSOC_PREFIX = (1<<0),
  SP_ASSOC_LEFT   = (1<<1),
  SP_ASSOC_RIGHT  = (1<<2),
};

struct sp_src_loc {
  uint16_t line;
  uint16_t col;
  uint16_t file_id;
};

#define sp_make_src_loc(f, l, c) ((struct sp_src_loc) { .file_id = (f), .line = (l), .col = (c) })

struct sp_operator {
  uint32_t op;
  char name[4];
  enum sp_op_assoc assoc;
  int32_t prec;
};

typedef int32_t sp_symbol_id;
typedef int sp_string_id;

struct sp_buffer {
  struct sp_mem_pool *pool;
  char *p;
  int size;
  int cap;
};

struct sp_symtab {
  struct sp_mem_pool *pool;
  int num;
  int cap;
  int *entries;
  struct sp_buffer symbols;
};

uint32_t sp_hash(const void *data, size_t len);
int sp_utf8_len(char *str, size_t size);
void sp_dump_string(const char *str);
const void *sp_decode_src_loc(const void *encoded, int encoded_len, struct sp_src_loc *src_loc, int n_instr);
int sp_encode_src_loc_change(struct sp_buffer *buf, struct sp_src_loc *old_loc, struct sp_src_loc *new_loc);

void sp_init_buffer(struct sp_buffer *buf, struct sp_mem_pool *pool);
void sp_destroy_buffer(struct sp_buffer *buf);
int sp_buf_grow(struct sp_buffer *buf, size_t add_size);
int sp_buf_shrink_to_fit(struct sp_buffer *buf);
int sp_buf_add_string(struct sp_buffer *buf, const void *str, size_t str_size);
int sp_buf_add_byte(struct sp_buffer *buf, uint8_t c);
int sp_buf_add_u16(struct sp_buffer *buf, uint16_t c);

void sp_init_symtab(struct sp_symtab *s, struct sp_mem_pool *pool);
void sp_destroy_symtab(struct sp_symtab *s);
sp_symbol_id sp_add_symbol(struct sp_symtab *s, const void *symbol);
sp_symbol_id sp_get_symbol_id(struct sp_symtab *s, const void *symbol);
const char *sp_get_symbol_name(struct sp_symtab *s, sp_symbol_id id);

const char *sp_get_op_name(uint32_t op);
struct sp_operator *sp_get_op_by_id(uint32_t op);
struct sp_operator *sp_get_op(const char *name);
struct sp_operator *sp_get_prefix_op(const char *name);
struct sp_operator *sp_get_binary_op(const char *name);

void sp_init_program(struct sp_program *prog);

#endif /* LIB_H_FILE */
