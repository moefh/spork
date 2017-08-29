
#define STR(x) # x

#define STRVAR(...) # __VA_ARGS__

// TODO: should remove trailing spaces from args
STR(  hello \n /* x */
//x
 world!  )

//#define X(x) #

STR(#)
STR("hello, world!\n")
STR(1.2.3)
STR(@
hashtag!)


STRVAR()
STRVAR(a)
STRVAR(a,b)
STRVAR(a, b)
