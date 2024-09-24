/****************************************************************************
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Luna code.
 *
 * The Initial Developer of the Original Code is Olivier ARMAND
 * <olivier.calc@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2011-2014
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "DES.h"
#include <zlib.h>
#include "minizip-1.1/zip.h"

#define LUNA_VER "2.1"

#ifndef min
 #define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

const char *gnu_basename(const char *path)
{
	char *base = strrchr(path, '/');
	return base ? base+1 : path;
}

/* Reads an UTF-8 character from in to *c. Doesn't read at or after end. Returns a pointer to the next character. */
char *utf82unicode(char *in, char *end, unsigned long *c) {
	if (in == end) {
		*c = 0;
		return in;
	}
	if (!(*in & 0b10000000)) {
		*c = *in;
		return in + 1;
	}
	if ((*in & 0b11100000) == 0b11000000) {
		*c = (*in & 0b00011111) << 6;
		if (end > in + 1)
			*c |= *(in + 1) & 0b00111111;
		return min(end, in + 2);
	}
	if ((*in & 0b11110000) == 0b11100000) {
		*c = (*in & 0b00001111) << 12;
		if (end > in + 1)
			*c |= (*(in + 1) & 0b00111111) << 6;
		if (end > in + 2)
			*c |= *(in + 2) & 0b00111111;
		return min(end, in + 3);
	}
	if ((*in & 0b11111000) == 0b11110000) {
		*c = (*in & 0b00000111) << 18;
		if (end > in + 1)
			*c |= (*(in + 1) & 0b00111111) << 12;
		if (end > in + 2)
			*c |= (*(in + 2) & 0b00111111) << 6;
		if (end > in + 3)
			*c |= *(in + 3) & 0b00111111;
		return min(end, in + 4);
	}
	*c = 0;
	return in + 1;
}

/* sub-routine of read_file_and_xml_compress() to escape the unicode characters as required by the XML compression. Returns the new in_buf or NULL */
void *escape_unicode(char *in_buf, size_t header_size, size_t footer_size, size_t in_size, size_t *obuf_size) {
	char *p, *op;
	char *out_buf = malloc(header_size + in_size * 4 /* worst case */ + footer_size);
	if (!out_buf) {
		puts("escape_unicode: can't malloc");
		return NULL;
	}
	memcpy(out_buf, in_buf, header_size);

	p = in_buf + header_size;
	if (!memcmp(in_buf + header_size, "\xEF\xBB\xBF", 3)) // skip the UTF-8 BOM if any
		p += 3;
	for (op = out_buf + header_size; p < in_buf + header_size + in_size;) {
		unsigned long uc;
		p = utf82unicode(p, in_buf + header_size + in_size, &uc);
		if (uc < 0x80) {
			*op++ = (char)uc;
		} else if (uc < 0x800) {
			*op++ = (char)(uc >> 8);
			*op++ = (char)(uc);
		} else if (uc < 0x10000) {
			*op++ = 0b10000000;
			*op++ = (char)(uc >> 8);
			*op++ = (char)(uc);
		} else {
			*op++ = 0b00001000;
			*op++ = (char)(uc >> 16);
			*op++ = (char)(uc >> 8);
			*op++ = (char)(uc);
		}
	}
	*obuf_size = op - out_buf + footer_size;
	char *out_buf2 = realloc(out_buf, *obuf_size);
	if (!out_buf2) {
		free(out_buf);
		puts("escape_unicode: can't realloc");
		return NULL;
	}
	return out_buf2;
}

/* sub-routine of xml_compress() to fix occurrences of CDATA end sequence "]]>" in Lua scripts
 * by spitting them between two CDATA sections. Returns the new in_buf, or NULL if out of memory. */
static const char cdata_restart[] = "]]><![CDATA[";
void *fix_cdata_end_seq(char *in_buf, size_t header_size, size_t in_size, size_t *obuf_size) {
	for (size_t offset = header_size; offset < header_size + in_size - 2; offset++) {
		if (!memcmp(in_buf + offset, "]]>", 3)) {
			// Skip "]]"
			offset += 2;
			// Insert "]]><![CDATA["
			*obuf_size += sizeof(cdata_restart) - 1;
			char *new_in_buf;
			if (!(new_in_buf = realloc(in_buf, *obuf_size))) {
				puts("can't realloc in_buf");
				free(in_buf);
				return NULL;
			}
			in_buf = new_in_buf;
			memmove(in_buf + offset + sizeof(cdata_restart) - 1, in_buf + offset, header_size + in_size - offset);
			memcpy(in_buf + offset, cdata_restart, sizeof(cdata_restart) - 1);
			in_size += sizeof(cdata_restart) - 1;
			offset += sizeof(cdata_restart) - 1;
		}
	}
	return in_buf;
}

/* sub-routine of read_file_and_xml_compress() in case of an XML problem as input. Returns the new in_buf or NULL */
void *reformat_xml_doc(char *in_buf, size_t header_size, size_t in_size, size_t *obuf_size) {
	char *out_buf = malloc(header_size + in_size);
	if (!out_buf) {
		puts("reformat_xml_doc: can't alloc");
		return NULL;
	}
	memcpy(out_buf, in_buf, header_size);
	char *in_ptr = in_buf;
	char *xml_start = NULL;
	while(in_ptr < in_buf + in_size + header_size - 5) {
		if (!memcmp(in_ptr, "<prob", 5) || !memcmp(in_ptr, "<doc", 4)) {
			xml_start = in_ptr;
			break;
		}
		in_ptr++;
	}
	if (!xml_start) {
		puts("input is not a TI-Nspire problem/document");
reformat_xml_quit:
		free(out_buf);
		return NULL;
	}
	int size_written = 0;
	size_t read_offset = 0;
	size_t size_to_read = in_size + header_size - (xml_start - in_buf);
	unsigned tagid_stack[100];
	unsigned tagid_head_index = 0;
	unsigned last_tagid = 0;
	char *out_ptr = out_buf + header_size;
	// very weak XML parsing: all < must be escaped
	while(read_offset <= size_to_read - 1) {
		if (xml_start[read_offset] == '<') {
			if (read_offset + 1 >= size_to_read) {
invalid_problem:
				puts("input problem/document is not a valid XML document");
				goto reformat_xml_quit;
			}
			if (xml_start[read_offset + 1] == '/') { // closing tag
				if (tagid_head_index == 0) {
					goto invalid_problem;
				}
				read_offset++;
				unsigned index = tagid_stack[--tagid_head_index];
				if(index < 256) {
					out_ptr[size_written++] = 0x0E;
					out_ptr[size_written++] = index;
					// skip the closing tag
					while(++read_offset <= size_to_read - 1 && xml_start[read_offset] != '>')
						;
				}
				else {
					// Too big for the tag store, copy in full
					const char *tag_first = &xml_start[read_offset - 1];
					const char *tag_last = memchr(&xml_start[read_offset], '>', size_to_read - read_offset);
					if(!tag_last)
						goto invalid_problem;

					memcpy(out_ptr + size_written, tag_first, tag_last - tag_first + 1);
					size_written += tag_last - tag_first + 1;
					read_offset = tag_last - xml_start + 1;
					continue;
				}
				if (read_offset > size_to_read - 1) {
					goto invalid_problem;
				}
			}
			else if (xml_start[read_offset + 1] == '!') { // special
				if (read_offset + 2 >= size_to_read)
					goto invalid_problem;

				read_offset += 2;

				if (xml_start[read_offset] == '-') {
					puts("XML comments not supported");
					goto reformat_xml_quit;
				}

				if (read_offset + sizeof("[CDATA[]]>") >= size_to_read)
					goto invalid_problem;

				if (strncmp(&xml_start[read_offset], "[CDATA[", strlen("[CDATA")))
					goto invalid_problem;

				const char *cdata_first = &xml_start[read_offset - 2];
				read_offset += strlen("[CDATA[");

				const char *cdata_last = NULL;
				for(;;) {
					const char *maybe_last = memchr(xml_start + read_offset, '>', size_to_read - read_offset);
					if(!maybe_last)
						break;

					read_offset = maybe_last - xml_start + 1;

					if(!strncmp("]]>", maybe_last - 2, strlen("]]>")))
						cdata_last = maybe_last;
				}

				if(!cdata_last)
					goto invalid_problem;

				// Copy everything including the start and end tags
				memcpy(out_ptr + size_written, cdata_first, cdata_last - cdata_first + 1);
				size_written += cdata_last - cdata_first + 1;
				read_offset = cdata_last - xml_start + 1;
				continue;
			}
			else { // opening tag
				if (tagid_head_index >= sizeof(tagid_stack) / sizeof(*tagid_stack)) {
					puts("input problem/document XML too deep");
					goto reformat_xml_quit;
				}
				tagid_stack[tagid_head_index++] = last_tagid++;
				out_ptr[size_written++] = xml_start[read_offset];
			}
		} else {
			out_ptr[size_written++] = xml_start[read_offset];
		}
		read_offset++;
	}
	*obuf_size = header_size + size_written;
	out_buf = realloc(out_buf, *obuf_size);
	if (!out_buf) {
		puts("reformat_xml_doc: can't realloc out_buf");
		goto reformat_xml_quit;
	}
	free(in_buf);
	return out_buf;
}

// ext must start with "."
int has_ext(const char *filepath, const char *ext) {
	return strlen(filepath) > strlen(ext) && !strcasecmp(ext, filepath + strlen(filepath) - strlen(ext));
}

/* Returns the output buffer, NULL on error. Fills obuf_size.
 * Don't compress anything if not Lua/XML */
void *read_file_and_xml_compress(const char *inf_path, size_t *obuf_size, const char **filename) {
	static const char lua_header[] =
		"\x54\x49\x58\x43\x30\x31\x30\x30\x2D\x31\x2E\x30\x3F\x3E\x3C\x70\x72"
		"\x6F\x62\x20\x78\x6D\x6C\x6E\x73\x3D\x22\x75\x72\x6E\x3A\x54\x49\x2E"
		"\x50\xA8\x5F\x5B\x1F\x0A\x22\x20\x76\x65\x72\x3D\x22\x31\x2E\x30\x22"
		"\x20\x70\x62\x6E\x61\x6D\x65\x3D\x22\x22\x3E\x3C\x73\x79\x6D\x3E\x0E"
		"\x01\x3C\x63\x61\x72\x64\x20\x63\x6C\x61\x79\x3D\x22\x30\x22\x20\x68"
		"\x31\x3D\x22\xF1\x00\x00\xFF\x22\x20\x68\x32\x3D\x22\xF1\x00\x00\xFF"
		"\x22\x20\x77\x31\x3D\x22\xF1\x00\x00\xFF\x22\x20\x77\x32\x3D\x22\xF1"
		"\x00\x00\xFF\x22\x3E\x3C\x69\x73\x44\x75\x6D\x6D\x79\x43\x61\x72\x64"
		"\x3E\x30\x0E\x03\x3C\x66\x6C\x61\x67\x3E\x30\x0E\x04\x3C\x77\x64\x67"
		"\x74\x20\x78\x6D\x6C\x6E\x73\x3A\x73\x63\x3D\x22\x75\x72\x6E\x3A\x54"
		"\x49\x2E\x53\xAC\x84\xF2\x2A\x41\x70\x70\x22\x20\x74\x79\x70\x65\x3D"
		"\x22\x54\x49\x2E\x53\xAC\x84\xF2\x2A\x41\x70\x70\x22\x20\x76\x65\x72"
		"\x3D\x22\x31\x2E\x30\x22\x3E\x3C\x73\x63\x3A\x6D\x46\x6C\x61\x67\x73"
		"\x3E\x30\x0E\x06\x3C\x73\x63\x3A\x76\x61\x6C\x75\x65\x3E\x2D\x31\x0E"
		"\x07\x3C\x73\x63\x3A\x73\x63\x72\x69\x70\x74\x20\x76\x65\x72\x73\x69"
		"\x6F\x6E\x3D\x22\x35\x31\x32\x22\x20\x69\x64\x3D\x22\x30\x22\x3E"
		"<![CDATA[";
	static const char lua_footer[] = "]]>\x0E\x08\x0E\x05\x0E\x02\x0E\x00";
	static const char xml_header[] =
		"\x54\x49\x58\x43\x30\x31\x30\x30\x2D\x31\x2E\x30\x3F\x3E";

	int infile_is_xml = has_ext(inf_path, ".xml");
	int infile_is_lua = has_ext(inf_path, ".lua") || !strcmp("-", inf_path);

	*filename = gnu_basename(inf_path);
	if (infile_is_lua)
		*filename = "Problem1.xml";

	const char *header;
	size_t header_size;
	size_t footer_size;
	if (infile_is_xml) {
		header = xml_header;
		header_size = sizeof(xml_header) - 1;
		footer_size = 0;
	} else if (infile_is_lua) {
		header = lua_header;
		header_size = sizeof(lua_header) - 1;
		footer_size = sizeof(lua_footer) - 1;
	}
	else {
		header = NULL;
		header_size = 0;
		footer_size = 0;
	}

	FILE *inf;
	if (!strcmp(inf_path, "-"))
		inf = stdin;
	else
		inf = fopen(inf_path, "rb");
	if (!inf) {
		puts("can't open input file");
		return NULL;
	}
	#define FREAD_BLOCK_SIZE 1024
	*obuf_size = header_size + FREAD_BLOCK_SIZE + footer_size;
	char *in_buf = malloc(*obuf_size);
	if (!in_buf) {
		puts("can't realloc in_buf");
		fclose(inf);
		return NULL;
	}
	memcpy(in_buf, header, header_size);
	size_t in_offset = header_size;
	while(1) {
		size_t read_size;
		if ((read_size = fread(in_buf + in_offset, 1, FREAD_BLOCK_SIZE, inf)) != FREAD_BLOCK_SIZE) {
			*obuf_size -= FREAD_BLOCK_SIZE - read_size;
			if (!(in_buf = realloc(in_buf, *obuf_size))) {
				puts("can't realloc in_buf");
				free(in_buf);
				return NULL;
			}
			break;
		}
		*obuf_size += FREAD_BLOCK_SIZE;
		if (!(in_buf = realloc(in_buf, *obuf_size))) {
			puts("can't realloc in_buf");
			free(in_buf);
			return NULL;
		}
		in_offset += read_size;
	}
	size_t in_size = *obuf_size - header_size - footer_size;
	fclose(inf);

	if (!infile_is_xml && !infile_is_lua) {
		return in_buf;
	}

	if (infile_is_xml) {
		in_buf = escape_unicode(in_buf, header_size, footer_size, in_size, obuf_size);
		if (!in_buf) return NULL;
		return reformat_xml_doc(in_buf, header_size, in_size, obuf_size);
	} else {
		if (!(in_buf = fix_cdata_end_seq(in_buf, header_size, in_size, obuf_size)))
			return NULL;
		in_size = *obuf_size - header_size - footer_size;
		memcpy(in_buf + header_size + in_size, lua_footer, sizeof(lua_footer) - 1);
		return in_buf;
	}
}

int doccrypt(uint8_t *inout, long in_size) {
	unsigned i;
	DES_key_schedule ks1, ks2, ks3;
	DES_cblock cbc_data;
	/* Compatible with tien_crypted_header below from which they are derived */
	static unsigned char cbc1_key[8] = {0x16, 0xA7, 0xA7, 0x32, 0x68, 0xA7, 0xBA, 0x73};
	static unsigned char cbc2_key[8] = {0xD9, 0xA8, 0x86, 0xA4, 0x34, 0x45, 0x94, 0x10};
	static unsigned char cbc3_key[8] = {0x3D, 0x80, 0x8C, 0xB5, 0xDF, 0xB3, 0x80, 0x6B};
	unsigned char ivec[8] = {0x00, 0x00, 0x00, 0x00}; /* the last 4 bytes are incremented each time, LSB first */
	unsigned ivec_incr = 0;
	/* As stored in tien_crypted_header below */
	#define IVEC_BASE 0x6fe21307

	DES_set_key((DES_cblock*)&cbc1_key, &ks1);
	DES_set_key((DES_cblock*)&cbc2_key, &ks2);
	DES_set_key((DES_cblock*)&cbc3_key, &ks3);

	do {
		unsigned current_ivec = IVEC_BASE + ivec_incr++;
		if (ivec_incr == 1024)
			ivec_incr = 0;
		ivec[4] = (unsigned char)(current_ivec >> 0);
		ivec[5] = (unsigned char)(current_ivec >> 8);
		ivec[6] = (unsigned char)(current_ivec >> 16);
		ivec[7] = (unsigned char)(current_ivec >> 24);
		memcpy(&cbc_data, ivec, sizeof(DES_cblock));
		DES_ecb3_encrypt(&cbc_data, &cbc_data, &ks1, &ks2, &ks3, DES_ENCRYPT);
 		for (i = 0; i < ((unsigned)in_size >= sizeof(DES_cblock) ? sizeof(DES_cblock) : (unsigned)in_size); i++) {
			*inout++ ^= cbc_data.bytes[i];
		}
		in_size -= sizeof(DES_cblock);
	} while (in_size > 0);
	return 0;
}

static zipFile zipF = 0;

// stateful, keep the zipFile opened
int add_processed_file_to_tns(const char *infile_name, void const *in_buf, long in_size, const char *outfile_path, unsigned tiversion) {
	zip_fileinfo zi;
	if (!zipF && !(zipF = zipOpen(outfile_path, 0))) {
		puts("can't open zip-TNS file for writing");
		return 1;
	}

	zi.tmz_date.tm_sec = zi.tmz_date.tm_min = zi.tmz_date.tm_hour = 0;
	zi.tmz_date.tm_mday = zi.tmz_date.tm_mon = zi.tmz_date.tm_year = 0;
	zi.dosDate = 0; zi.internal_fa = 0; zi.external_fa = 0;
	int method = 0xD; // TI encrypted
	int level = 0;
	if (!has_ext(infile_name, ".xml")) {
		method = Z_DEFLATED; // just deflated
		level = Z_DEFAULT_COMPRESSION;
	}
	if (zipOpenNewFileInZip2(zipF, infile_name, &zi, NULL, 0, NULL, 0, NULL, method, level, 0, tiversion) != ZIP_OK) {
		puts("can't open file in zip-TNS file for writing");
close_quit:
		zipClose(zipF, NULL);
unlink_quit:
		unlink(outfile_path);
		return 1;
	}
	if (zipWriteInFileInZip(zipF, in_buf, in_size) != ZIP_OK) {
		puts("can't write file in zip-TNS file");
		goto close_quit;
	}
	if (zipCloseFileInZip(zipF) != ZIP_OK) {
		puts("can't close file in zip-TNS file");
		goto unlink_quit;
	}
	return 0;
}

void close_tns(const char *outfile_path) {
	if (zipClose(zipF, NULL) != ZIP_OK) {
		puts("can't close file in zip-TNS file");
		unlink(outfile_path);
	}
}

// returns the deflated size
long deflate_compressed_xml(void *def_buf, size_t def_size, const void *xmlc_buf, size_t xmlc_buf_size) {
	z_stream zstream;
	zstream.next_in = (Bytef*)xmlc_buf;
	zstream.next_out = (Bytef*)def_buf;
	zstream.avail_in = xmlc_buf_size;
	zstream.avail_out = def_size;
	zstream.zalloc = Z_NULL;
	zstream.zfree  = Z_NULL;
	/* -windowBits=-15: no zlib header */
	if (deflateInit2(&zstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY)) {
		puts("can't deflateInit2");
		exit(1);
	}
	if (deflate(&zstream, Z_FINISH) != Z_STREAM_END) {
		puts("can't deflate");
		exit(1);
	}
	if (deflateEnd(&zstream)) {
		puts("can't deflateEnd");
		exit(1);
	}
	return zstream.total_out;
}

int add_default_document_to_tns(const char *tnsfile_path, unsigned tiversion) {
	static const char default_processed_document_xml[] =
		"\x0F\xCE\xD8\xD2\x81\x06\x86\x5B\x4A\x4A\xC5\xCE\xA9\x16\xF2\xD5\x1D\xA8\x2F\x6E"
		"\x00\x22\xF2\xF0\xC1\xA6\x06\x77\x4D\x7E\xA6\xC0\x3A\xF0\x5C\x74\xBA\xAA\x44\x60"
		"\xCD\x58\xE6\x70\xD7\x40\xF6\x9C\x17\xDC\xF0\x94\x77\xBF\xCA\xDE\xF7\x02\x09\xC9"
		"\x62\xB1\x5D\xEF\x22\xFA\x51\x37\xA0\x81\x91\x48\xE1\x83\x4D\xAD\x08\x31\x2D\xD0"
		"\xD3\xE3\x2D\x60\xAB\x13\xC2\x98\x2B\xED\x39\x5B\x09\x24\x39\x92\x2F\x0C\x7A\x4C"
		"\x95\x74\x91\x3B\x0C\xF4\x60\xCC\x73\x27\xCB\x07\x7E\x7F\xA9\x17\x87\xE2\xAC\xA2"
		"\x3B\xCC\xA0\xC4\xE3\x8E\x89\xF0\xC0\x51\x9F\xC2\xBE\xCE\x28\x45\xC3\xD4\x11\x90"
		"\xA6\xEC\x53\xA0\xFB\x5B\x46\x6B\x41\xAD\xE9\x53\xBB\x97\xDB\xB1\xD2\x68\xE2\xF6"
		"\x36\x0F\x26\x36\x75\x9B\xE9\x1F\x48\xAD\xE9\x29\x67\x00\x58\x19\xC3\xC0\x12\x76"
		"\xA0\x4A\x73\xF3\xB1\xD3\x09\x18\xD6\x06\xDD\x97\x24\x53\x3E\x22\xA4\xFB\x82\x50"
		"\x7B\x7C\x12\x88\x4E\x7D\x41\x80\xFE\x72\x92\x29\x87\xE8\x5C\x56\x72\xFF\x29\x16"
		"\x8C\x42\x5B\x8B\x9B\xA7\xD2\x08\x6D\xD3\x98\xFF\x91\xA9\x9E\xF3\x93\xA8\x2E\x1C"
		"\xB2\xA9\x6B\x6A\xDF\xF6\xCE\x2D\x15\x17\xCE\x6E\xC0\x4F\x9A\x9C\x0E\xDF\x19\x8D"
		"\x2D\xFA\x69\x9F\x11\xD2\x20\x12\xE0\x79\x14\x04\x4E\x62\x8F\x0A\x2A\x18\x72\x5A"
		"\x8B\x80\xB3\x3C\x9B\xD5\x67\x59\x4B\x51\x4D\xE0\xC3\x38\x28\xC3\xDC\xCD\x39\x22"
		"\x12\x8C\x40\x55";
	return add_processed_file_to_tns("Document.xml", default_processed_document_xml, sizeof(default_processed_document_xml) - 1, tnsfile_path, tiversion);
}

int add_compressed_xml_to_tns(const char *tnsfile_path, const char *filename, const void *xmlc_buf, size_t xmlc_buf_size, unsigned tiversion) {
	static const char tien_crypted_header[] =
		"\x0F\xCE\xD8\xD2\x81\x06\x86\x5B\x99\xDD\xA2\x3D\xD9\xE9\x4B\xD4\x31\xBB\x50\xB6"
		"\x4D\xB3\x29\x24\x70\x60\x49\x38\x1C\x30\xF8\x99\x00\x4B\x92\x64\xE4\x58\xE6\xBC";

	/* As expected by zlib */
	size_t def_size = (size_t) (xmlc_buf_size + (xmlc_buf_size * 0.1) + 12);
	size_t header_size = sizeof(tien_crypted_header) - 1;
	uint8_t *header_and_deflated_buf = malloc(def_size + header_size);
	if (!header_and_deflated_buf) {
		puts("can't malloc header_and_deflated_buf");
add_compressed_xml_err:
		free(header_and_deflated_buf);
		return 1;
	}

	uint8_t *def_buf = header_and_deflated_buf + header_size;
	long deflated_size = deflate_compressed_xml(def_buf, def_size, xmlc_buf, xmlc_buf_size);
	if (doccrypt(def_buf, deflated_size))
		goto add_compressed_xml_err;
	memcpy(header_and_deflated_buf, tien_crypted_header, header_size);
	if (add_processed_file_to_tns(filename, header_and_deflated_buf, header_size + deflated_size, tnsfile_path, tiversion))
		goto add_compressed_xml_err;
	free(header_and_deflated_buf);
	return 0;
}

int add_infile_to_tns(const char *infile_path, const char *tnsfile_path, unsigned tiversion) {
	size_t xmlc_buf_size;
	const char *filename = NULL;
	void *xmlc_buf = read_file_and_xml_compress(infile_path, &xmlc_buf_size, &filename);
	int ret;
	if (!xmlc_buf)
		return 1;

	if (has_ext(filename, ".xml")) // Only .xml files are encrypted
		ret = add_compressed_xml_to_tns(tnsfile_path, filename, xmlc_buf, xmlc_buf_size, tiversion);
	else // don't crypt, don't deflate: will be deflated by minizip
		ret = add_processed_file_to_tns(filename, xmlc_buf, xmlc_buf_size, tnsfile_path, tiversion);
	free(xmlc_buf);
	return ret;
}

// Add XML that refers to the Python script. The Python script itself is added separately.
int add_python_xml_to_tns(const char *python_path, const char *tnsfile_path, unsigned tiversion) {
	static const char py_header[] =
		"TIXC0100-1.0?><prob xmlns=\"urn:TI.Problem\" ver=\"1.0\" pbname=\"\">"
		"<sym>\x0E\x01<card clay=\"0\" h1=\"10000\" h2=\"10000\" w1=\"10000\" "
		"w2=\"10000\"><isDummyCard>0\x0E\x03<flag>0\x0E\x04<wdgt xmlns:py=\"urn:"
		"TI.PythonEditor\" type=\"TI.PythonEditor\" ver=\"1.0\"><py:data><py:name>";
	static const char py_footer[] =
		"\x0E\x07<py:dirf>-10000000\x0E\x08\x0E\x06<py:mFlags>1024\x0E\x09"
		"<py:value>10\x0E\x0A\x0E\x05\x0E\x02\x0E\x00";

	const char *filename = gnu_basename(python_path);
	size_t filename_len = strlen(filename);
	if (filename_len > 240) {
		puts("Python script filenames limited to 240 characters");
		return 1;
	}

	// Filename goes sandwiched between the header and footer
	char xmlc_buf[(sizeof(py_header) - 1) + 240 + (sizeof(py_footer) - 1)];
	snprintf(xmlc_buf, sizeof(xmlc_buf), "%s%s%s", py_header, filename, py_footer);

	size_t total_size = (sizeof(py_header) - 1) + filename_len + (sizeof(py_footer) - 1);
	return add_compressed_xml_to_tns(tnsfile_path, "Problem1.xml", xmlc_buf, total_size, tiversion);
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		puts("Luna v" LUNA_VER " usage:\n"
				 "  luna [INFILE.lua|-] [OUTFILE.tns]\n"
				 "  luna [INFILE.py]* [OUTFILE.tns]\n"
				 "  luna [Problem1.xml|Document.xml|ABCD.BMP]* [OUTFILE.tns]\n"
				 "Converts a Lua script, Python scripts, or XML problems/documents/resources to a TNS document.\n"
				 "If the input file '-', reads it as Lua from the standard input.\n"
				 "For Python, the first script will be the one that shows when the TNS document is opened.\n"
				 "A default Document.xml will be generated if not specified."
		);
		return 0;
	}
	unsigned tiversion = 0x0500; // default to document version 5
	for (int i = 1; i <= argc - 2; i++) { // infiles: the args except the last one
		if (has_ext(argv[i], ".bmp")) {
			tiversion = 0x0700; // bitmap files require the document type to be bumped up to 7
			break;
		}
	}

	char *outfile_path = argv[argc - 1];
	unlink(outfile_path);

	zipF = 0; // Only useful for emscripten (possible multiple main() calls)

	// Document.xml must be added first to the TNS
	int has_processed_documentxml = 0;
	for (int i = 1; i <= argc - 2; i++) { // infiles: the args except the last one
		if (!strcmp("Document.xml", gnu_basename(argv[i]))) {
			printf("processing '%s'...\n", argv[i]);
			int ret = add_infile_to_tns(argv[i], outfile_path, tiversion);
			if (ret) return ret;
			has_processed_documentxml = 1;
		}
	}
	if (!has_processed_documentxml) {
		int ret = add_default_document_to_tns(outfile_path, tiversion);
		if (ret) return ret;
	}

	// Then add all the other files
	int is_converting_lua = 0;
	int added_python_xml = 0;
	int ret;
	for (int i = 1; i <= argc - 2; i++) { // infiles: the args except the last one
		if (!strcmp("Document.xml", gnu_basename(argv[i])))
			continue;
		printf("processing '%s'...\n", argv[i]);
		if (has_ext(argv[i], ".lua") || !strcmp("-", argv[i])) {
			if (is_converting_lua) {
				puts("[WARN] skipping it, can only add a single Lua script to the TNS file");
				continue;
			}
			is_converting_lua = 1;
		}
		if (!added_python_xml && has_ext(argv[i], ".py")) { // Add XML for just the first Python file
			ret = add_python_xml_to_tns(argv[i], outfile_path, tiversion);
			if (ret) return ret;
			added_python_xml = 1;
		}
		ret = add_infile_to_tns(argv[i], outfile_path, tiversion);
		if (ret) return ret;
	}

	close_tns(outfile_path);
	return 0;
}
