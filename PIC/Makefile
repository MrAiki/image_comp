CC 		    = gcc
CFLAGS 	  = -Wall -Wextra -O0 -g
LDFLAGS		= -lm
OBJS	 		= pic.o pnm.o bit_stream.o adaptive_huffman.o
TARGET    = pic 

all: $(TARGET) 

rebuild:
	make clean
	make all

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET)

$(TARGET) : $(OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(TEST_OBJS) -o $(TARGET)

pnm.o : ../pnm/pnm.c
	$(CC) $(CFLAGS) -c $<

bit_stream.o : ../../BitStream/bit_stream.c
	$(CC) $(CFLAGS) -c $<

adaptive_huffman.o : ../AdaptiveHuffman/adaptive_huffman.c
	$(CC) $(CFLAGS) -c $<

.c.o:
	$(CC) $(CFLAGS) -c $<
