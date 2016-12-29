CC     ?= gcc
CFLAGS  = -O2 -flto -Wall -Wextra -DNOCRYPT -DUSE_FILE32API -Wno-unused-parameter
LDFLAGS = -flto -lz
VPATH   = minizip-1.1

PREFIX = /usr/local/bin

OS := $(shell uname -s)
ifeq ($(OS),Windows_NT)
  EXEEXT = .exe
endif

BIN := luna$(EXEEXT)

all: $(BIN)

$(BIN): luna.o zip.o ioapi.o DES.o
	$(CC) -o $@ $^ $(LDFLAGS)

install: $(BIN)
	mkdir -p $(PREFIX)
	install $(BIN) $(PREFIX)

dist: clean all
	mkdir -p dist/src
	find . -maxdepth 1 ! -name '$(BIN)' -a ! -name dist -a ! -name . -exec cp -r {} dist/src \;
	cp $(BIN) *.md *.txt *.html dist

clean:
	$(RM) -r *.o $(BIN) dist

.PHONY: all install dist clean
