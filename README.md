Luna 2.0
========

Luna is a portable command-line converter of Lua programs to TNS TI-Nspire documents, compatible with OS 3.0.2 and later.

It can also be used to convert any TI-Nspire problems in XML format to TNS documents.

## Usage

* Lua program conversion:    `luna INFILE.lua OUTFILE.tns`
* Problem conversion:        `luna Problem1.xml OUTFILE.tns`
* Multiple files:            `luna Document.xml Problem1.xml [Problem2.xml...] OUTFILE.tns`

If the input is `-`, it reads the file from the standard input.  
Make sure to encode your Lua or Problem file in UTF-8 if it contains special characters. You can also pack arbitrary files like images into the TNS.

## Bugs, feedback...

Please use the GitHub [issue tracker](https://github.com/ndless-nspire/Luna/issues).

## License

Luna is licensed under the Mozilla Public License v1.1.  
Luna is based on a derived version of MiniZip. See minizip-1.1/MiniZip64_info.txt for more information.  
Luna includes cryptographic software written by Eric Young (eay@cryptsoft.com), see the DES.* files for more information.  
The JS version uses FileSaver.js, MIT-licensed.

## Building it yourself

You need the zlib (zlib1g-dev/zlib-devel) development library. On macOS, you can use [brew](http://brew.sh/): `brew install zlib`  
Then you can just run `make`.

## History

2016-12-28: v2.0
 * NEW: No OpenSSL library dependency anymore, just use the relevant DES code.
 * FIX: Fix crash when parsing deeply nested XML documents
 
2016-12-27: v1.0.1
 * NEW: Show Luna version in usage message
 * FIX: Some minor warnings

2016-12-27: v1.0
 * NEW: Install target in Makefile
 * NEW: Add support for .BMP resources
 * NEW: License under Mozilla Public License v1.1
 * FIX: Fix build with OpenSSL 1.1 using newer DES API

2012-06-26: v0.3a
 * FIX: build options for Mac/Linux
 * FIX: skip UTF-8 BOM if any
 * FIX: compatibility with some Unicode characters such as Chinese ones

2012-06-24: v0.3
 * FIX: support all UTF-8 characters
 * NEW: Lua program produced now default to OS v3.2's Lua API level 2 (see
        https://wiki.inspired-lua.org/Changes_in_OS_3.2).

2011-11-10: v0.2b
 * FIX: buffer overflow

2011-09-27: v0.2a
 * FIX: characters at the end of a Lua script might no be correctly XML-escaped
        (thanks Goplat)
 * FIX: wrong buffer size handling might cause '<' and '&' characters in Lua
        scripts to produce bad TNS files (thanks Goplat)
 * FIX: TNS documents were abnormally big because of trailing garbage data being
        encrypted (thanks Levak)

2011-09-19: v0.2
 * NEW: supports arbitrary TI-Nspire problem conversion, useful to build
        third-party document generators

2011-08-08: v0.1b
 * NEW: lua file can be provided from stdin
 * FIX: crash if input file doesn't exist

2011-08-06: v0.1a
 * Can be built on Linux

2011-08-05: v0.1
 * First release
