CFLAGS = -Wall -pedantic -O2
INCLUDES = -Iinclude
SOURCES =			\
	src/main.c		\
	src/arp.c		\
	src/network.c
BIN = send-arp

default:
	gcc $(CFLAGS) $(INCLUDES) $(SOURCES) -o $(BIN)

clean:
	rm -f $(BIN)

.PHONY: default clean
