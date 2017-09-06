/* input.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "input.h"

struct sp_input *sp_new_input_from_file(const char *filename)
{
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
  in->file_id = -1;
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
