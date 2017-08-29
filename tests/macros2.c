
#define A(x) a1 B(x) a2
#define B(x) b1 C(x) b2
#define C(x) c1 D(x) c2

A(1)

#define x(a) "x expanded"
#define R(x) x (
#define S(x) "S expanded with arg:" x

R(S) 1)

#define VAR(x,...) __VA_ARGS__

VAR(1,2, )x

#define STR()
