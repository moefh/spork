/* punct.h */

#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "punct.h"

static struct punct {
  int id;
  char name[4];
} puncts[] = {
  { '[',               "["   },
  { ']',               "]"   },
  { '(',               "("   },
  { ')',               ")"   },
  { '{',               "{"   },
  { '}',               "}"   },
  { '.',               "."   },
  { PUNCT_ARROW,       "->"  },
  { PUNCT_PLUSPLUS,    "++"  },
  { PUNCT_MINUSMINUS,  "--"  },
  { '&',               "&"   },
  { '*',               "*"   },
  { '+',               "+"   },
  { '-',               "-"   },
  { '~',               "~"   },
  { '!',               "!"   },
  { '/',               "/"   },
  { '%',               "%"   },
  { PUNCT_LSHIFT,      "<<"  },
  { PUNCT_RSHIFT,      ">>"  },
  { '<',               "<"   },
  { '>',               ">"   },
  { PUNCT_LEQ,         "<="  },
  { PUNCT_GEQ,         ">="  },
  { PUNCT_EQ,          "=="  },
  { PUNCT_NEQ,         "!="  },
  { '^',               "^"   },
  { '|',               "|"   },
  { PUNCT_AND,         "&&"  },
  { PUNCT_OR,          "||"  },
  { '?',               "?"   },
  { ':',               ":"   },
  { ';',               ";"   },
  { PUNCT_ELLIPSIS,    "..." },
  { '=',               "="   },
  { PUNCT_MULEQ,       "*="  },
  { PUNCT_DIVEQ,       "/="  },
  { PUNCT_MODEQ,       "%="  },
  { PUNCT_PLUSEQ,      "+="  },
  { PUNCT_MINUSEQ,     "-="  },
  { PUNCT_LSHIFTEQ,    "<<=" },
  { PUNCT_RSHIFTEQ,    ">>=" },
  { PUNCT_ANDEQ,       "&="  },
  { PUNCT_XOREQ,       "^="  },
  { PUNCT_OREQ,        "|="  },
  { ',',               ","   },
  { '~',               "~"   },
  { '#',               "#"   },
  { PUNCT_HASHES,      "##"  },
};

const char *sp_get_punct_name(int punct_id)
{
  for (int i = 0; i < ARRAY_SIZE(puncts); i++)
    if (puncts[i].id == punct_id)
      return puncts[i].name;
  return NULL;
}

int sp_get_punct_id(const char *name)
{
  for (int i = 0; i < ARRAY_SIZE(puncts); i++)
    if (strcmp(puncts[i].name, name) == 0)
      return puncts[i].id;
  return -1;
}
