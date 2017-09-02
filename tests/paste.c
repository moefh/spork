
#define X(a) a+1
#define paste(x,y)  x ## y

paste(1,a)
//paste(X(a),2)

#define paste_with_y(x)  x ## y

paste_with_y(A)
#define y yyy
paste_with_y(A)

//#define Z # a
//Z

#define     hash_hash # ## #
#define     mkstr(a) # a
#define     in_between(a) mkstr(a)
#define     join(c, d) in_between(c hash_hash d)
char p[] = join(x, y);
