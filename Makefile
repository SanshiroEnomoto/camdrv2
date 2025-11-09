# Makefile for camdrv libraries
# Created by Enomoto Sanshiro on 23 July 1999.  
# Last updated by Enomoto Sanshiro on 23 July 1999.


CC = gcc
CFLAGS = -O -Wall -I/usr/include -Wno-unused-result

all: camlib.o toyocamac.o


camlib.o: camlib.c camlib.h camdrv.h

toyocamac.o: toyocamac.c toyocamac.h camdrv.h


.c.o:
	$(CC) $(CFLAGS) -c $< 


clean:
	rm -f *.o
