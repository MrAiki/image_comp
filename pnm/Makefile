CC 		    = gcc
CFLAGS 	  = -Wall -Wextra -O0 -g
OBJS	 		= main.o pnm.o
OBJS			+= $(COMP_OBJS)
TARGET    = pnm

all: $(TARGET)

rebuild:
	make clean
	make all

clean:
	rm -f $(OBJS) $(TARGET)

pnm : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $<
