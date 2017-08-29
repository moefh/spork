
#define X(a,...) __VA_ARGS__
#define Y(a...) a

X(x,1,2,3)
Y(1,2,3)

#define A(a)  a

A((1,2,3))
