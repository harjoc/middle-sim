CC=gcc
CFLAGS=-Wall -ggdb -O
LDFLAGS=-lgdi32

all:middle-sim.exe

middle-sim.exe: fsm.o main.o | Makefile
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c fsm.h | Makefile
	$(CC) $(CFLAGS) -c $<
