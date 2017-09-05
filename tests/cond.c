#define NOPE(x) 0
#define DEF

#if 1

#if defined(DEF)
ok1
#elif 1
"ERROR[1a]: shouldn't happen!"
#else
"ERROR[1a]: shouldn't happen!"
#endif

#if NOPE(hello world)
"ERROR[2a]: shouldn't happen!"
#elif 1
ok2
#else
"ERROR[2b]: shouldn't happen!"
#endif

#ifdef DEF
ok3
#elif 0
"ERROR[3b]: shouldn't happen!"
#else
"ERROR[3a]: shouldn't happen!"
#endif

#ifdef UNDEF
"ERROR[4a]: shouldn't happen!"
#elif defined(WHAT)
"ERROR[4a]: shouldn't happen!"
#else
ok4
#endif

#else

"ERROR[5]: shouldn't happen!"

#endif
