CC 		    = gcc
CFLAGS 	  = -Wall -Wextra -O0 -g
LDFLAGS		= -lm
OBJS	 		= test_main.o test.o test_horc.o bit_stream.o
TARGET    = test 

all: $(TARGET) 

rebuild:
	make clean
	make all

run: $(TARGET)
	./test

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET)

$(TARGET) : $(OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(TEST_OBJS) -o $(TARGET)

bit_stream.o : ../../../BitStream/bit_stream.c
	$(CC) $(CFLAGS) -c $<

.c.o:
	$(CC) $(CFLAGS) -c $<
