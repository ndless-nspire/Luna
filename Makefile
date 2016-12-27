CC ?= gcc
CFLAGS ?= -Wall -Wextra -DUSE_FILE32API -Wno-unused-parameter
LDFLAGS ?= -lz -lcrypto
VPATH ?= minizip-1.1
ifeq ($(USER),root)
	PREFIX ?= /usr/bin
else
	PREFIX ?= ~/bin
endif

OS ?= `uname -s`
ifeq ($(OS),Windows_NT)
  EXEEXT = .exe
endif

BIN := luna$(EXEEXT)

all: $(BIN)

$(BIN): luna.o zip.o ioapi.o
	$(CC) -o $@ $^ $(LDFLAGS)

install: $(BIN)
	mkdir -p $(PREFIX)
	install $(BIN) $(PREFIX)

dist: clean all
	mkdir -p dist/src
	$(RM) *.o
	find . -maxdepth 1 ! -name '$(BIN)' -a ! -name dist -a ! -name . -exec cp -r {} dist/src \;
	cp $(BIN) *.md *.txt *.html dist

clean:
	$(RM) -r *.o $(BIN) dist

.PHONY: all install dist clean
