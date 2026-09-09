PROJECT   = pkgconfu
VERSION   = 0.3.0

PREFIX   ?= /usr/local
BINDIR   ?= $(PREFIX)/bin
DATADIR  ?= $(PREFIX)/share
MANDIR   ?= $(DATADIR)/man

PKGCONFU_DEFAULT_PATH ?= /usr/local/lib/pkgconfig:/usr/local/share/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig
PKGCONFU_SYSTEM_INCLUDE_PATH ?= /usr/include
PKGCONFU_SYSTEM_LIBRARY_PATH ?= /usr/lib

CC       ?= cc
CSTD     ?= c23
CFLAGS   ?= -O2 -g
WARNINGS  = -Wall -Wextra
CPPFLAGS += -D_GNU_SOURCE \
            -DPKGCONFU_VERSION=\"$(VERSION)\" \
            -DPKGCONFU_DEFAULT_PATH=\"$(PKGCONFU_DEFAULT_PATH)\" \
            -DPKGCONFU_SYSTEM_INCLUDE_PATH=\"$(PKGCONFU_SYSTEM_INCLUDE_PATH)\" \
            -DPKGCONFU_SYSTEM_LIBRARY_PATH=\"$(PKGCONFU_SYSTEM_LIBRARY_PATH)\"
ALL_CFLAGS = -std=$(CSTD) $(WARNINGS) $(CFLAGS)

SRC = src/util.c src/parse.c src/pkg.c src/main.c
OBJ = $(SRC:.c=.o)
BIN = $(PROJECT)

INSTALL ?= install

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(ALL_CFLAGS) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(ALL_CFLAGS) $(CPPFLAGS) -c -o $@ $<

src/util.o:  src/util.h
src/parse.o: src/parse.h src/util.h
src/pkg.o:   src/pkg.h src/parse.h src/util.h
src/main.o:  src/pkg.h src/parse.h src/util.h

install: $(BIN)
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)
	$(INSTALL) -d $(DESTDIR)$(MANDIR)/man1
	$(INSTALL) -m 0644 man/$(PROJECT).1 $(DESTDIR)$(MANDIR)/man1/$(PROJECT).1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)
	rm -f $(DESTDIR)$(MANDIR)/man1/$(PROJECT).1

check: $(BIN)
	sh tests/run.sh ./$(BIN)

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all install uninstall check clean
