CFLAGS = -Wall -O2 -s
INCLUDES = -Iinclude
SOURCES = src/main.c src/arp.c
BIN = send-arp

default:
	gcc $(CFLAGS) $(INCLUDES) $(SOURCES) -o $(BIN)

clean:
	rm -f $(BIN)

.PHONY: default clean
