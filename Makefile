CC = gcc
CFLAGS = -Wall -Wextra -O2 -g `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0` -lm

OBJS = main.o lexer.o parser.o interp.o builtin.o

vbs: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c vbs.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f vbs $(OBJS)

.PHONY: clean