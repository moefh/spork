/* pp_macro.h */

#ifndef PP_MACRO_H_FILE
#define PP_MACRO_H_FILE

#include "internal.h"
#include "mem_pool.h"
#include "string_tab.h"
#include "token_list.h"

struct sp_preprocessor;

struct sp_macro_def {
  sp_string_id name_id;
  bool is_function;
  bool is_variadic;
  bool enabled;
  struct sp_pp_token_list params;
  struct sp_pp_token_list body;
  int n_params;
  sp_string_id param_name_ids[];
};

struct sp_macro_args {
  struct sp_mem_pool *pool;
  int cap;
  int len;
  struct sp_pp_token_list args[];
};

typedef int sp_pp_token_reader(struct sp_preprocessor *pp, struct sp_pp_token *tok, void *reader_data);

struct sp_macro_def *sp_new_macro_def(struct sp_preprocessor *pp, sp_string_id name_id,
                                      bool is_function, bool last_param_is_variadic,
                                      struct sp_pp_token_list *params, struct sp_pp_token_list *body);
const char *sp_get_macro_name(struct sp_macro_def *macro, struct sp_preprocessor *pp);

struct sp_macro_args *sp_new_macro_args(struct sp_macro_def *macro, struct sp_mem_pool *pool);
int sp_read_macro_args(struct sp_preprocessor *pp, struct sp_macro_args *args,
                       struct sp_macro_def *macro, sp_pp_token_reader *reader, void *reader_data);
struct sp_pp_token_list *sp_get_macro_arg(struct sp_macro_def *macro, struct sp_macro_args *args, int param_name_id);

void sp_dump_macro(struct sp_macro_def *macro, struct sp_preprocessor *pp);

#endif /* PP_MACRO_H_FILE */
