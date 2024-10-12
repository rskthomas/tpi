# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -lssl -lcrypto
DEBUG_FLAGS = -g -O0
#-g Produce debugging information in the operating system's native format (stabs, COFF, XCOFF, or DWARF 2). GDB (and valgrind) can work with this debugging information.

PROD_FLAGS = -O2

# Source files
LIBRARY_SRC = src/library.c
LIBRARY_H = include/library.h

CLIENT_SRC = src/client.c
SERVER_SRC = src/server.c


# where to save binaries
CLIENT_DEBUG_BIN = bin/udpclient_debug
CLIENT_PROD_BIN = bin/udpclient
SERVER_DEBUG_BIN = bin/udpserver_debug
SERVER_PROD_BIN = bin/udpserver

# Default
all: debug prod

# Debug build targets
debug: $(CLIENT_DEBUG_BIN) $(SERVER_DEBUG_BIN)

$(CLIENT_DEBUG_BIN): $(CLIENT_SRC) $(LIBRARY_SRC) $(LIBRARY_H)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $@ $(CLIENT_SRC) $(LIBRARY_SRC)

$(SERVER_DEBUG_BIN): $(SERVER_SRC) $(LIBRARY_SRC) $(LIBRARY_H)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $@ $(SERVER_SRC) $(LIBRARY_SRC)

# Production build targets
prod: $(CLIENT_PROD_BIN) $(SERVER_PROD_BIN)

$(CLIENT_PROD_BIN): $(CLIENT_SRC) $(LIBRARY_SRC) $(LIBRARY_H)
	$(CC) $(CFLAGS) $(PROD_FLAGS) -o $@ $(CLIENT_SRC) $(LIBRARY_SRC)

$(SERVER_PROD_BIN): $(SERVER_SRC) $(LIBRARY_SRC) $(LIBRARY_H)
	$(CC) $(CFLAGS) $(PROD_FLAGS) -o $@ $(SERVER_SRC) $(LIBRARY_SRC)

# Clean up
clean:
	rm -f $(CLIENT_DEBUG_BIN) $(CLIENT_PROD_BIN) $(SERVER_DEBUG_BIN) $(SERVER_PROD_BIN)

.PHONY: all debug prod clean
