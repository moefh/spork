
#define X(a) a+1
#define paste(x,y)  x ## y

paste(a,b);
paste(a+b,c);
paste(a,b+c);
paste(a+b,c+d);
//paste(X(a),2);
paste(,);
paste(a,);
paste(,b);

paste(L,'\a');
paste(L,"a");

#define MAKE_WIDE_A L ## "a"
L ## "a";      // this is NOT pasted
MAKE_WIDE_A;   // this is pasted into L"a"

//paste("", paste(\, ""))

#define Y(x) x
#define Z(k) Y(k)

Z(a ## b);

/*
#define paste_with_y(x)  x ## y

paste(a, paste(##, b))

paste_with_y(A)
#define y yyy
paste_with_y(A)
*/

#define     hash_hash # ## #
#define     mkstr(a) # a
#define     in_between(a) mkstr(a)
#define     join(c, d) in_between(c hash_hash d)
char p[] = join(x, y);  // "x ## y"

