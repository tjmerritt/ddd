
OBJS=		ddd.o

ddd:		$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
