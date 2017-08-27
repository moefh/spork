/* input.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "input.h"

struct sp_input *sp_new_input_from_file(const char *filename, uint16_t file_id, const char *base_filename)
{
  char path[1024];
  
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
  if (fseek(f, 0, SEEK_END) < 0)
    goto err;
  long size = ftell(f);
  if (size < 0 || size > INT_MAX)
    goto err;
  if (fseek(f, 0, SEEK_SET) < 0)
    goto err;
  
  struct sp_input *in = malloc(sizeof(struct sp_input) + size);
  if (! in)
    goto err;
  
  if (fread(in->data, 1, size, f) != (size_t) size) {
    free(in);
    goto err;
  }
  
  in->next = NULL;
  in->size = size;
  in->pos = 0;
  in->file_id = file_id;
  fclose(f);
  return in;

 err:
  fclose(f);
  return NULL;
}

void sp_free_input(struct sp_input *in)
{
  free(in);
}
