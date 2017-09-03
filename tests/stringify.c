
#define STR(x) # x

#define STRVAR(...) # __VA_ARGS__

// TODO: should remove trailing spaces from args
STR(  hello \n /* x */
//x
 world!  );

STR(#);
STR("hello, world!\n");
STR(1.2.3);
STR(@
hashtag!);

STR('\n');
STR('\"');
STR('"');


STRVAR();
STRVAR(a);
STRVAR(a,b);
STRVAR(a, b);
