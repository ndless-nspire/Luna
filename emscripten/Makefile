CC      = emcc

EMFLAGS := -s WASM=1 -s MODULARIZE=1 -s EXPORT_ES6=1 -s EXPORT_NAME="'WebLuna'" -s INVOKE_RUN=0 -s NO_EXIT_RUNTIME=1 -s EXPORTED_RUNTIME_METHODS="['FS','callMain']"

CFLAGS  = -O3 -flto -W -Wall -Wextra -DNOCRYPT -DUSE_FILE32API -Wno-unused-parameter
CFLAGS += -s USE_ZLIB=1
LFLAGS := -flto $(EMFLAGS)

VPATH   = ../ ../minizip-1.1

OBJS = luna.bc zip.bc ioapi.bc DES.bc

OUTPUT := WebLuna

wasm:  $(OUTPUT).js

all: wasm

%.bc: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT).js: $(OBJS)
	$(CC) $(CFLAGS) $(LFLAGS) $^ -o $@

clean:
	$(RM) -f $(OBJS) $(OUTPUT).js* $(OUTPUT).was*

.PHONY: all clean wasm
