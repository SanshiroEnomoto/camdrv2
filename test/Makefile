# Makefile for camdrv libraries
# Created by Enomoto Sanshiro on 23 July 1999.  
# Last updated by Enomoto Sanshiro on 23 July 1999.


TARGETS = initialize_test lam_test camaction_test speed_test

CC = gcc
CFLAGS = -O -Wall -I/usr/include -Wno-unused-result -I..

all: $(TARGETS)


test: test.o
	$(CC) $(CFLAGS) -o $@ $@.o ../camlib.o

initialize_test: initialize_test.o
	$(CC) $(CFLAGS) -o $@ $@.o ../camlib.o

lam_test: lam_test.o
	$(CC) $(CFLAGS) -o $@ $@.o ../camlib.o

camaction_test: camaction_test.o
	$(CC) $(CFLAGS) -o $@ $@.o ../toyocamac.o

speed_test: speed_test.o
	$(CC) $(CFLAGS) -o $@ $@.o ../toyocamac.o


.c.o:
	$(CC) $(CFLAGS) -c $< 


clean:
	rm -f *.o
	rm -f $(TARGETS)

