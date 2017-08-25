
OBJS = util.o mem_pool.o buffer.o symtab.o input.o ast.o operator.o tokenizer.o program.o

libspork.a: $(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -f *.a *.o *~

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<
