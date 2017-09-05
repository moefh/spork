/* input.h */

#ifndef INPUT_H_FILE
#define INPUT_H_FILE

#include <stdint.h>

struct sp_input {
  struct sp_input *next;
  uint16_t file_id;
  int base_cond_level;
  size_t size;
  size_t pos;
  unsigned char data[];
};

struct sp_input *sp_new_input_from_file(const char *filename, uint16_t file_id, const char *base_filename);
void sp_free_input(struct sp_input *in);

#define sp_get_input_file_id(in)  ((in)->file_id)

#endif /* INPUT_H_FILE */
