#define TEST1
#define TEST2 /*...*/
#define TEST3 //
#define TEST4 x
#define TEST5 x y
#define TEST6 x \
              y
#define TEST7 x /*...*/ \
              y \
              z

#define X(a)  { a, # a, test_ ## a ## f }

test1: TEST1;
test2: TEST2;
test3: TEST3;
test4: TEST4;
test5: TEST5;
test6: TEST6;
test7: TEST7;
X(1);
X(a+1 /*x*/);
