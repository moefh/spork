#define STR(x)           # x
#define INCLUDE_FILE(x)  STR(x ## lude.h)

// 1
#undef NUM
#define NUM  "1"
#include "include.h"

// 2
#undef NUM
#define NUM  "2"
#include INCLUDE_FILE(inc)
