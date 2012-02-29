CC=gcc
CFLAGS= $(shell pkg-config --cflags glib-2.0)
LIBS= $(shell pkg-config --libs glib-2.0)

all: ramscanner_main.o ramscanner_display.o ramscanner_collect.o ramscanner_common.o 
	$(CC)  ramscanner_display.o ramscanner_collect.o ramscanner_common.o ramscanner_main.o -o ramscanner    $(LIBS) -Wall

debug: 
	$(CC) -o ramscanner ramscanner.c $(CFLAGS) $(LIBS) -Wall -g



ramscanner_main.o: ramscanner_main.c ramscanner_common.h ramscanner_collect.h ramscanner_display.h
			$(CC) ramscanner_main.c -c ramscanner_main.o $(CFLAGS)

ramscanner_display.o:ramscanner_display.c ramscanner_display.h ramscanner_common.h
			$(CC) ramscanner_display.c -c ramscanner_display.o $(CFLAGS)

ramscanner_collect.o: ramscanner_collect.c ramscanner_collect.h ramscanner_common.h
			$(CC) ramscanner_collect.c -c ramscanner_collect.o $(CFLAGS)

ramscanner_common.o: ramscanner_common.h ramscanner_common.c
			$(CC) ramscanner_common.c -c ramscanner_common.o $(CFLAGS)

clean:
	rm -f ramscanner *.o
