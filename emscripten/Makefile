CC      = emcc

CFLAGS  = -O3 -W -Wall -Wextra -DNOCRYPT -DUSE_FILE32API -Wno-unused-parameter
CFLAGS += --llvm-lto 3 -s USE_ZLIB=1 -s INVOKE_RUN=0 -s NO_EXIT_RUNTIME=1 --pre-js luna-utils.js

asmjs: CFLAGS += --closure 1 -s WASM=0
wasm:  CFLAGS += --closure 0 -s WASM=1 -s "BINARYEN_METHOD='native-wasm,asmjs'"

VPATH   = ../ ../minizip-1.1

OBJECTS = luna.bc zip.bc ioapi.bc DES.bc

OUTPUT := luna

asmjs: $(OUTPUT).js
wasm: $(OUTPUT).js

all: asmjs

%.bc: %.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT).js: $(OBJECTS)
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^

clean:
	$(RM) -f $(OUTPUT).js* $(OUTPUT).asm.js $(OUTPUT).was* $(OUTPUT).data $(OBJECTS)

.PHONY: all clean asmjs wasm
