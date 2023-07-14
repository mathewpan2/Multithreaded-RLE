cc==gcc
CFLAGS=-g -pedantic -std=gnu17 -Wall -Werror -Wextra -O
LDFLAGS=-lpthread

.PHONY: all 
all: encode 

nyuenc: nyuenc.c

.PHONY: clean
clean:
	rm -f *.o encode