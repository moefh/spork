
OBJS = util.o mem_pool.o buffer.o hashtable.o id_hashtable.o \
       string_tab.o input.o ast.o token.o token_list.o pp_macro.o \
       pp_phase123.o preprocessor.o program.o old_input.o

libspork.a: $(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -f *.a *.o *~

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<
