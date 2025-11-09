# Makefile for camdrv libraries
# Created by Enomoto Sanshiro on 23 July 1999.  
# Last updated by Enomoto Sanshiro on 23 July 1999.


CC = gcc
CFLAGS = -O -Wall -I/usr/include -Wno-unused-result -I..

all: inhibit_test lam_test camaction_test poll_test speed_test


inhibit_test: inhibit_test.o
	$(CC) $(CFLAGS) -o $@ $@.o ../camlib.o

lam_test: lam_test.o
	$(CC) $(CFLAGS) -o $@ $@.o ../camlib.o

camaction_test: camaction_test.o
	$(CC) $(CFLAGS) -o $@ $@.o ../toyocamac.o

poll_test: poll_test.o
	$(CC) $(CFLAGS) -o $@ $@.o

speed_test: speed_test.o
	$(CC) $(CFLAGS) -o $@ $@.o ../toyocamac.o


.c.o:
	$(CC) $(CFLAGS) -c $< 


clean:
	rm -f *.o
	rm -f inhibit_test lam_test camaction_test poll_test speed_test

