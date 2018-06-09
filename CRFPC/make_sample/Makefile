CC 		    			= gcc
CFLAGS 	  			= -Wall -Wextra -O3
DUMPSMPLS_OBJS	= dump_samples.o pnm.o
TRNCSMPLS_OBJS 	= trunc_samples.o
TARGET    			= dump_samples trunc_samples

all: $(TARGET)

rebuild:
	make clean
	make all

clean:
	rm -f $(DUMPSMPLS_OBJS) $(TRNCSMPLS_OBJS) $(TARGET)

dump_samples : $(DUMPSMPLS_OBJS)
	$(CC) $(CFLAGS) $(DUMPSMPLS_OBJS) -o $@

trunc_samples : $(TRNCSMPLS_OBJS)
	$(CC) $(CFLAGS) $(TRNCSMPLS_OBJS) -o $@

pnm.o : ../../pnm/pnm.c
	$(CC) $(CFLAGS) -c $<

.c.o:
	$(CC) $(CFLAGS) -c $<
