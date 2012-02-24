CFLAGS= $(shell pkg-config --cflags glib-2.0)
LIBS= $(shell pkg-config --libs glib-2.0)

all: 
	gcc -o ramscanner ramscanner.c $(CFLAGS) $(LIBS)

clean:
	rm -f ramscanner *.o
