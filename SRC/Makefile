CC 		    = gcc
CFLAGS 	  = -Wall -Wextra -O3
LDFLAGS		= -lm
OBJS	 		= src_main.o bit_stream.o sgm.o predictive_coding.o coding_utility.o adaptive_huffman.o horc.o src.o
TARGET    = src

all: $(TARGET) 

rebuild:
	make clean
	make all

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET)

src : $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

bit_stream.o : ../BitStream/bit_stream.c
	$(CC) $(CFLAGS) -c $<

sgm.o : ../SGM/sgm.c
	$(CC) $(CFLAGS) -c $<

adaptive_huffman.o : ../AdaptiveHuffman/adaptive_huffman.c
	$(CC) $(CFLAGS) -c $<

horc.o : ../HORC/horc.c
	$(CC) $(CFLAGS) -c $<

.c.o:
	$(CC) $(CFLAGS) -c $<
