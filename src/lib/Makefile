
OBJS = util.o mem_pool.o buffer.o string_tab.o input.o ast.o token.o preprocessor.o program.o

libspork.a: $(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -f *.a *.o *~

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<
