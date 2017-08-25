/* input.c */

#include <stdio.h>
#include <string.h>

#include "input.h"

static struct sp_input *new_input(struct sp_mem_pool *pool, enum sp_input_type type)
{
  struct sp_input *in = sp_malloc(pool, sizeof(struct sp_input));
  if (! in)
    return NULL;
  in->next = NULL;
  in->buf_size = 0;
  in->buf_pos = 0;
  in->unget_size = 0;
  in->type = type;
  return in;
}

struct sp_input *sp_open_input_file(struct sp_mem_pool *pool, const char *filename, uint16_t file_id, const char *base_filename)
{
  char path[256];
  
  /*
   * If 'filename' is not an absolute path, base it on the directory
   * of the parent input
   */
  if (base_filename && filename[0] != '/' && filename[0] != '\\') {
    const char *last_slash = strrchr(base_filename, '/');
    if (! last_slash)
      last_slash = strrchr(base_filename, '\\');
    if (last_slash) {
      size_t base_len = last_slash + 1 - base_filename;
      
      if (base_len + strlen(filename) + 1 > sizeof(path))
        return NULL;  // path is too big
      memcpy(path, base_filename, base_len);
      strcpy(path + base_len, filename);
      filename = path;
    }
  }

  FILE *f = fopen(filename, "r");
  if (! f)
    return NULL;

  struct sp_input *in = new_input(pool, SP_INPUT_FILE);
  if (! in) {
    fclose(f);
    return NULL;
  }
  in->source.file.f = f;
  in->source.file.id = file_id;
  return in;
}

struct sp_input *sp_new_input_string(struct sp_mem_pool *pool, const char *string)
{
  struct sp_input *in = new_input(pool, SP_INPUT_STRING);
  if (! in)
    return NULL;
  in->source.str.data = (uint8_t *) string;
  in->source.str.len = strlen(string);
  in->source.str.pos = 0;
  return in;
}

void sp_close_input(struct sp_input *in)
{
  switch (in->type) {
  case SP_INPUT_FILE: fclose(in->source.file.f); return;
  case SP_INPUT_STRING: return;
  }
}

int sp_input_fill_buffer_and_next_byte(struct sp_input *in)
{
  switch (in->type) {
  case SP_INPUT_STRING:
    return -1;

  case SP_INPUT_FILE:
    {
      size_t ret = fread(in->buf, 1, sizeof(in->buf), in->source.file.f);
      if (ret == 0)
        return -1;
      in->buf_size = (int) ret;
      in->buf_pos = 0;
      return in->buf[in->buf_pos++];
    }
  }
  return -1;
}

void sp_input_unget_byte(struct sp_input *in, uint8_t b)
{
  if (in->unget_size < (int) sizeof(in->unget_buf))
    in->unget_buf[in->unget_size++] = b;
}
