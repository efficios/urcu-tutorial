# Copyright (C) 2013  Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
#
# THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
# OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
#
# Permission is hereby granted to use or copy this program for any
# purpose,  provided the above notices are retained on all copies.
# Permission to modify the code and to distribute modified code is
# granted, provided the above notices are retained, and a notice that
# the code was modified is included with the above copyright notice.
#
# This makefile is purposefully kept simple to support GNU and BSD make.

CC = gcc
CFLAGS = -g -O2 -Wall
LIBS = -lurcu -lurcu-cds -lurcu-common -lpthread

HEADERS = urcu-game.h urcu-game-config.h worker-thread.h ht-hash.h

all: urcu-game

urcu-game: urcu-game.o urcu-game-config.o worker-thread.o user-input.o \
		print-output.o dispatch-thread.o urcu-game-logic.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(AM_CFLAGS) $(AM_LDFLAGS) \
		-o $@ $+ $(LIBS)

urcu-game.o: urcu-game.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(AM_CPPFLAGS) $(AM_CFLAGS) \
		-c -o $@ $<

urcu-game-logic.o: urcu-game-logic.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(AM_CPPFLAGS) $(AM_CFLAGS) \
		-c -o $@ $<

urcu-game-config.o: urcu-game-config.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(AM_CPPFLAGS) $(AM_CFLAGS) \
		-c -o $@ $<

worker-thread.o: worker-thread.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(AM_CPPFLAGS) $(AM_CFLAGS) \
		-c -o $@ $<

user-input.o: user-input.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(AM_CPPFLAGS) $(AM_CFLAGS) \
		-c -o $@ $<

print-output.o: print-output.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(AM_CPPFLAGS) $(AM_CFLAGS) \
		-c -o $@ $<

dispatch-thread.o: dispatch-thread.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(AM_CPPFLAGS) $(AM_CFLAGS) \
		-c -o $@ $<

.PHONY: clean
clean:
	rm -f *.o urcu-game
