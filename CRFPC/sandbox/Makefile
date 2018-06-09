CC 		    = gcc
CFLAGS 	  = -Wall -Wextra -O3
OBJS	 		= crfpc_main.o crfpc.o
TARGET    = crfpc_ex

all: $(TARGET)

rebuild:
	make clean
	make all

clean:
	rm -f $(OBJS) $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

crfpc.o : ../crfpc.c
	$(CC) $(CFLAGS) -c $<

.c.o:
	$(CC) $(CFLAGS) -c $<
