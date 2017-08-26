
#include "test.h"

#define SIZE     256
#define ROUND() (((x+SIZE)/SIZE)*SIZE)

#define X Y+Z+1
#define Y X+Z+1
#define Z X+Y+1

#define XX 1 2 3 4 5 6 7 8 9 a b c d e f g i j k l

int main(void)
{
  int x = X;
  int round = ROUND();
  //y = Y;
  //X(f,2,3);
  //printf("Hello, world!\n");
  //a +++++ b;
  //while (i --> 0);
  //return 0;
}
