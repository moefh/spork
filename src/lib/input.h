/* input.h */

#ifndef INPUT_H_FILE
#define INPUT_H_FILE

#include "internal.h"
#include "mem_pool.h"

#define UNGET_BUF_SIZE 16
#define INPUT_BUF_SIZE 256

enum sp_input_type {
  SP_INPUT_FILE,
  SP_INPUT_STRING,
};

struct sp_input_file {
  FILE *f;
  uint16_t id;
};

struct sp_input_string {
  size_t len;
  size_t pos;
  uint8_t *data;
};

struct sp_input {
  struct sp_input *next;
  struct sp_mem_pool *pool;
  int buf_size;
  int buf_pos;
  uint8_t buf[INPUT_BUF_SIZE];
  int unget_size;
  uint8_t unget_buf[UNGET_BUF_SIZE];

  enum sp_input_type type;
  union {
    struct sp_input_file file;
    struct sp_input_string str;
  } source;
};

struct sp_input *sp_open_input_file(struct sp_mem_pool *pool, const char *filename, uint16_t file_id, const char *base_filename);
struct sp_input *sp_new_input_string(struct sp_mem_pool *pool, const char *string);
void sp_close_input(struct sp_input *in);
int sp_input_fill_buffer_and_next_byte(struct sp_input *in);
void sp_input_unget_byte(struct sp_input *in, uint8_t b);

#define sp_get_input_file_id(in)  (((in)->type == SP_INPUT_FILE) ? (in)->source.file.id : (uint16_t)-1)

#define sp_input_next_byte(in)                                          \
  (((in)->unget_size > 0) ? (in)->unget_buf[--(in)->unget_size] :       \
   ((in)->buf_pos < (in)->buf_size) ? (in)->buf[(in)->buf_pos++] :      \
   sp_input_fill_buffer_and_next_byte(in))

#endif /* INPUT_H_FILE */
