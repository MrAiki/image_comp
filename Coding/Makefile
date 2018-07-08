CC 		    = gcc
CFLAGS 	  = -Wall -Wextra -O0 -g
LDFLAGS		= -lm
OBJS	 		= coding.o pnm.o bit_stream.o coding_main.o
OBJS_TENKAICHI = coding.o pnm.o bit_stream.o coding_main_for_tenkaichi.o
TARGET    = coding coding_main_for_tenkaichi

all: $(TARGET) 

rebuild:
	make clean
	make all

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET)

coding : $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

coding_main_for_tenkaichi : $(OBJS_TENKAICHI)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS_TENKAICHI) -o $@

pnm.o : ../pnm/pnm.c
	$(CC) $(CFLAGS) -c $<

bit_stream.o : ../../BitStream/bit_stream.c
	$(CC) $(CFLAGS) -c $<

.c.o:
	$(CC) $(CFLAGS) -c $<
