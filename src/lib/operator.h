/* operator.h */

#ifndef OPERATOR_H_FILE
#define OPERATOR_H_FILE

enum sp_op_assoc {
  SP_ASSOC_PREFIX = (1<<0),
  SP_ASSOC_LEFT   = (1<<1),
  SP_ASSOC_RIGHT  = (1<<2),
};

enum sp_punct {
  PUNCT_UNM = 256,
  PUNCT_EQ,
  PUNCT_NEQ,
  PUNCT_GT,
  PUNCT_GE,
  PUNCT_LT,
  PUNCT_LE,
  PUNCT_OR,
  PUNCT_AND,
  PUNCT_ARROW,
};

struct sp_operator {
  uint32_t op;
  char name[4];
  enum sp_op_assoc assoc;
  int32_t prec;
};

const char *sp_get_op_name(uint32_t op);
struct sp_operator *sp_get_op_by_id(uint32_t op);
struct sp_operator *sp_get_op(const char *name);
struct sp_operator *sp_get_prefix_op(const char *name);
struct sp_operator *sp_get_binary_op(const char *name);


#endif /* OPERATOR_H_FILE */
