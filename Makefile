all: ramscanner doc

CC ?= gcc

CFLAGS := $(shell pkg-config --cflags glib-2.0) -D_LARGEFILE64_SOURCE
LIBS := $(shell pkg-config --libs glib-2.0)

SOURCES=ramscanner_main.c ramscanner_common.c ramscanner_collect.c ramscanner_display.c

OBJECTS := $(SOURCES:.c=.o)

DEPS := $(SOURCES:.c=.d)

ramscanner: $(OBJECTS)
	$(CC) $(OBJECTS) -o ramscanner $(LIBS) -Wall

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -MMD -MF $(@:.o=.d) -c $<

doc:
	doxygen Doxyfile

clean:
	$(RM) ramscanner $(OBJECTS) $(DEPS)
	$(RM) -r doc

-include $(DEPS)

