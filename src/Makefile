
OBJS = main.o

EXTRA_CFLAGS = -Ilib

.PHONY: $(TARGETS) clean

spork: $(OBJS) lib/libspork.a
	$(CC) $(LDFLAGS) -o $@ $(OBJS) lib/libspork.a $(LIBS)

lib/libspork.a:
	$(MAKE) -C lib

clean:
	rm -f fh *.o *~
	$(MAKE) -C lib clean

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<

.c.o:
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<
