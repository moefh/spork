/* pp_macro.h */

#ifndef PP_MACRO_H_FILE
#define PP_MACRO_H_FILE

#include "internal.h"
#include "mem_pool.h"
#include "string_tab.h"
#include "pp_token_list.h"

struct sp_preprocessor;

enum sp_predefined_macro_id {
  PP_MACRO_NOT_PREDEFINED,
  PP_MACRO_DATE,
  PP_MACRO_TIME,
  PP_MACRO_FILE,
  PP_MACRO_LINE,
  PP_MACRO_STDC,
  PP_MACRO_STDC_VERSION,
  PP_MACRO_STDC_HOSTED,
  PP_MACRO_STDC_MB_MIGHT_NEQ_WC,
};

struct sp_macro_def {
  enum sp_predefined_macro_id pre_id;
  sp_string_id name_id;
  bool is_function;
  bool is_variadic;
  bool is_named_variadic;
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

int sp_add_predefined_macros(struct sp_preprocessor *pp);
struct sp_pp_token_list *sp_expand_predefined_macro(struct sp_preprocessor *pp, struct sp_macro_def *macro);

struct sp_macro_def *sp_new_macro_def(struct sp_preprocessor *pp, sp_string_id name_id,
                                      bool is_function, bool is_variadic, bool is_named_variadic,
                                      struct sp_pp_token_list *params, struct sp_pp_token_list *body);
const char *sp_get_macro_name(struct sp_macro_def *macro, struct sp_preprocessor *pp);

struct sp_macro_args *sp_new_macro_args(struct sp_macro_def *macro, struct sp_mem_pool *pool);
struct sp_pp_token_list *sp_get_macro_arg(struct sp_macro_def *macro, struct sp_macro_args *args, int param_name_id);

void sp_dump_macro(struct sp_macro_def *macro, struct sp_preprocessor *pp);
bool sp_macros_are_equal(struct sp_macro_def *m1, struct sp_macro_def *m2);

#endif /* PP_MACRO_H_FILE */
