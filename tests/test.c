
#include "test.h"

#define SIZE     256
#define ROUND() (((x+SIZE)/SIZE)*SIZE)

//#define X Y+Z+1
//#define Y X+Z+1
//#define Z X+Y+1
//int x = X;

//#define X(x) Y(x)
//#define Y(y) y
//int a = X(42) + bla;

#define bla(a,b,c,d) a
bla(,,,)

#define i(j) i(j+1)+y
#define yps(a) a
#define y(a) yps(a)
#define x(z) z (z)
int k = x(i(j));

int main(void)
{
  int round = ROUND();
  //y = Y;
  //X(f,2,3);
  //printf("Hello, world!\n");
  //a +++++ b;
  //while (i --> 0);
  //return 0;
}
