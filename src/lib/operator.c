/* operator.h */

#include <stdlib.h>
#include <string.h>

#include "ast.h"

static struct sp_operator ops[] = {
  { '=',        "=",  SP_ASSOC_RIGHT,  10 },
  
  { AST_OP_OR,  "||", SP_ASSOC_LEFT,   20 },
  { AST_OP_AND, "&&", SP_ASSOC_LEFT,   30 },
  
  { '|',        "|",  SP_ASSOC_LEFT,   40 },
  { '&',        "&",  SP_ASSOC_LEFT,   50 },

  { AST_OP_EQ,  "==", SP_ASSOC_LEFT,   60 },
  { AST_OP_NEQ, "!=", SP_ASSOC_LEFT,   60 },
  { '<',        "<",  SP_ASSOC_LEFT,   70 },
  { '>',        ">",  SP_ASSOC_LEFT,   70 },
  { AST_OP_LE,  "<=", SP_ASSOC_LEFT,   70 },
  { AST_OP_GE,  ">=", SP_ASSOC_LEFT,   70 },

  { '+',        "+",  SP_ASSOC_LEFT,   80 },
  { '-',        "-",  SP_ASSOC_LEFT,   80 },
  { '*',        "*",  SP_ASSOC_LEFT,   90 },
  { '/',        "/",  SP_ASSOC_LEFT,   90 },
  { '%',        "%",  SP_ASSOC_LEFT,   90 },

  { AST_OP_UNM, "-",  SP_ASSOC_PREFIX, 100 },
  { '!',        "!",  SP_ASSOC_PREFIX, 100 },

  { '^',        "^",  SP_ASSOC_RIGHT,  110 },
};

static struct sp_operator *find_op(const char *name, unsigned int assoc_mask)
{
  for (int i = 0; i < ARRAY_SIZE(ops); i++) {
    if ((ops[i].assoc & assoc_mask) != 0 && strcmp(ops[i].name, name) == 0)
      return &ops[i];
  }
  return NULL;
}

struct sp_operator *sp_get_binary_op(const char *name)
{
  return find_op(name, SP_ASSOC_LEFT|SP_ASSOC_RIGHT);
}

struct sp_operator *sp_get_prefix_op(const char *name)
{
  return find_op(name, SP_ASSOC_PREFIX);
}

struct sp_operator *sp_get_op(const char *name)
{
  struct sp_operator *op;
  if ((op = sp_get_prefix_op(name)) != NULL)
    return op;
  if ((op = sp_get_binary_op(name)) != NULL)
    return op;
  return NULL;
}

struct sp_operator *sp_get_op_by_id(uint32_t op)
{
  for (int i = 0; i < ARRAY_SIZE(ops); i++)
    if (ops[i].op == op)
      return &ops[i];
  return NULL;
}

const char *sp_get_op_name(uint32_t op)
{
  struct sp_operator *opr = sp_get_op_by_id(op);
  if (opr == NULL)
    return NULL;
  return opr->name;
}

