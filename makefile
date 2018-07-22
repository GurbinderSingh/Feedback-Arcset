# Author: Gurbinder Singh
# File: Makefile for Feedback Arc Set
CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -std=c99 -pedantic -Wall $(DEFS)
XFLAGS = -lrt -pthread

SUP_OBJ = supervisor.o
GEN_OBJ = generator.o

.PHONY: all clean

all: supervisor generator

supervisor: $(SUP_OBJ)
	$(CC) -o $@ $^ $(XFLAGS)
	
generator: $(GEN_OBJ)
	$(CC) -o $@ $^ $(XFLAGS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -g -c $^
	

clean:
	rm -f *.o *.gch
