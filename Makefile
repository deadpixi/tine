STANDARDS := -D_POSIX_C_SOURCE=200908L -D_XOPEN_SOURCE=600 -D_XOPEN_SOURCE_EXTENDED
CFLAGS    := $(STANDARDS) -Wall -Wextra -Wno-unused-parameter -Os
LDFLAGS   := -lncursesw
DESTDIR   ?= /usr/local

all: tine
	strip -s tine

clean:
	rm -rf *.o tine

tine: buffer.o command.o editor.o mode.o parser.o util.o

install: all
	mkdir -p "$(DESTDIR)/bin" "$(DESTDIR)/share/man/man1"
	cp tine "$(DESTDIR)/bin/tine"
	cp tine.1 "$(DESTDIR)/share/man/man1/tine.1"
