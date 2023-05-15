# Makefile for client and server

CC = gcc
OBJCS = drone8.c
OBJCSS = drone8.c

CFLAGS =  -g -Wall -lm
# setup for system
nLIBS =

all: drone8

client0: $(OBJCS)
	$(CC) $(CFLAGS) -o $@ $(OBJCS) $(LIBS)

server0: $(OBJCSS)
	$(CC) $(CFLAGS) -o $@ $(OBJCSS) $(LIBS)


clean:
	rm drone8