
OBJS = util.o mem_pool.o buffer.o hashtable.o id_hashtable.o \
       string_tab.o input.o ast.o punct.o pp_token.o pp_token_list.o \
       pp_macro.o pp_directives.o pp_phase123.o pp_phase4.o \
       pp_phase56.o preprocessor.o token.o compiler.o program.o

libspork.a: $(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -f *.a *.o *~

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<
