CC 		    = gcc
CFLAGS 	  = -Wall -Wextra -O0 -g
OBJS	 		= lpc_test.o pnm.o lpc.o
TARGET    = lpc_test 

all: $(TARGET) 

rebuild:
	make clean
	make all

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET)

$(TARGET) : $(OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(TEST_OBJS) -o $(TARGET)

pnm.o : ../../pnm/pnm.c
	$(CC) $(CFLAGS) -c $<

.c.o:
	$(CC) $(CFLAGS) -c $<
