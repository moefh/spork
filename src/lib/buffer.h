/* buffer.h */

#ifndef BUFFER_H_FILE
#define BUFFER_H_FILE

#include <stdint.h>
#include "mem_pool.h"

struct sp_buffer {
  struct sp_mem_pool *pool;
  char *p;
  int size;
  int cap;
};

void sp_init_buffer(struct sp_buffer *buf, struct sp_mem_pool *pool);
void sp_destroy_buffer(struct sp_buffer *buf);
int sp_buf_grow(struct sp_buffer *buf, size_t add_size);
int sp_buf_shrink_to_fit(struct sp_buffer *buf);
int sp_buf_add_string(struct sp_buffer *buf, const void *str, size_t str_size);
int sp_buf_add_byte(struct sp_buffer *buf, uint8_t c);
int sp_buf_add_u16(struct sp_buffer *buf, uint16_t c);

#endif /* BUFFER_H_FILE */
