CC=gcc
CFLAGS=-Wall -Werror -O2 -DDEBUG=0
LDFLAGS=

all: d4node

d4node: main.o timeout.o message.o node.o
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c Makefile
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	rm -f d4node *.o
