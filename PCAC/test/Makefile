CC 		    = gcc
CFLAGS 	  = -Wall -Wextra -O0 -g
LDFLAGS		= -lm
OBJS	 		= test_main.o test.o test_pcac.o pnm.o
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

pnm.o : ../../pnm/pnm.c
	$(CC) $(CFLAGS) -c $<

.c.o:
	$(CC) $(CFLAGS) -c $<
