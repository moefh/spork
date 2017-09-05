#define A(x) a1 B(x) a2
#define B(x) b1 C(x) b2
#define C(x) c1 D(x) c2

A(1);

#define REC() REC()
REC();

#define REC_A() REC_B()
#define REC_B() REC_A()
REC_A();
REC_B();

#define x(a) "THIS IS WRONG! x expanded when it wasn't supposed to!"
#define R(x) x (
#define S(x) "S expanded with arg:" x

R(S) 1);

#define VAR(x,...) __VA_ARGS__
//#define VAR2(x...) __VA_ARGS__  // TODO: rename __VA_ARGS__ to x (the named variadic parameter)?

VAR(1,2, )x;

line: __LINE__,
file: __FILE__;

date: __DATE__,
time: __TIME__;
