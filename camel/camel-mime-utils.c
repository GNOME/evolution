/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *           Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <unicode.h>

#include <glib.h>
#include <time.h>

#include <ctype.h>
#include <errno.h>

#include "camel-mime-utils.h"

#include "broken-date-parser.h"

#if 0
int strdup_count = 0;
int malloc_count = 0;
int free_count = 0;

#define g_strdup(x) (strdup_count++, g_strdup(x))
#define g_malloc(x) (malloc_count++, g_malloc(x))
#define g_free(x) (free_count++, g_free(x))
#endif

/* for all warnings ... */
#define w(x) x

#define d(x)
#define d2(x)

#define	CAMEL_UUDECODE_CHAR(c)	(((c) - ' ') & 077)

static char *base64_alphabet =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char tohex[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

static unsigned char camel_mime_special_table[256] = {
	  5,  5,  5,  5,  5,  5,  5,  5,  5,167,  7,  5,  5, 39,  5,  5,
	  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
	178,128,140,128,128,128,128,128,140,140,128,128,140,128,136,132,
	128,128,128,128,128,128,128,128,128,128,204,140,140,  4,140,132,
	140,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
	128,128,128,128,128,128,128,128,128,128,128,172,172,172,128,128,
	128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
	128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,  5,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static unsigned char camel_mime_base64_rank[256] = {
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
	 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
	255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
	255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};

/*
  if any of these change, then the tables above should be regenerated
  by compiling this with -DBUILD_TABLE, and running.

  gcc -o buildtable `glib-config --cflags --libs` -DBUILD_TABLE camel-mime-utils.c
  ./buildtable

*/
enum {
	IS_CTRL		= 1<<0,
	IS_LWSP		= 1<<1,
	IS_TSPECIAL	= 1<<2,
	IS_SPECIAL	= 1<<3,
	IS_SPACE	= 1<<4,
	IS_DSPECIAL	= 1<<5,
	IS_COLON	= 1<<6,	/* rather wasteful of space ... */
	IS_QPSAFE	= 1<<7
};

#define is_ctrl(x) ((camel_mime_special_table[(unsigned char)(x)] & IS_CTRL) != 0)
#define is_lwsp(x) ((camel_mime_special_table[(unsigned char)(x)] & IS_LWSP) != 0)
#define is_tspecial(x) ((camel_mime_special_table[(unsigned char)(x)] & IS_TSPECIAL) != 0)
#define is_type(x, t) ((camel_mime_special_table[(unsigned char)(x)] & (t)) != 0)
#define is_ttoken(x) ((camel_mime_special_table[(unsigned char)(x)] & (IS_TSPECIAL|IS_LWSP|IS_CTRL)) == 0)
#define is_atom(x) ((camel_mime_special_table[(unsigned char)(x)] & (IS_SPECIAL|IS_SPACE|IS_CTRL)) == 0)
#define is_dtext(x) ((camel_mime_special_table[(unsigned char)(x)] & IS_DSPECIAL) == 0)
#define is_fieldname(x) ((camel_mime_special_table[(unsigned char)(x)] & (IS_CTRL|IS_SPACE|IS_COLON)) == 0)
#define is_qpsafe(x) ((camel_mime_special_table[(unsigned char)(x)] & IS_QPSAFE) != 0)

/* only needs to be run to rebuild the tables above */
#ifdef BUILD_TABLE

#define CHARS_LWSP " \t\n\r"
#define CHARS_TSPECIAL "()<>@,;:\\\"/[]?="
#define CHARS_SPECIAL "()<>@,;:\\\".[]"
#define CHARS_CSPECIAL "()\\\r"	/* not in comments */
#define CHARS_DSPECIAL "[]\\\r \t"	/* not in domains */

static void
header_init_bits(unsigned char bit, unsigned char bitcopy, int remove, unsigned char *vals, int len)
{
	int i;

	if (!remove) {
		for (i=0;i<len;i++) {
			camel_mime_special_table[vals[i]] |= bit;
		}
		if (bitcopy) {
			for (i=0;i<256;i++) {
				if (camel_mime_special_table[i] & bitcopy)
					camel_mime_special_table[i] |= bit;
			}
		}
	} else {
		for (i=0;i<256;i++)
			camel_mime_special_table[i] |= bit;
		for (i=0;i<len;i++) {
			camel_mime_special_table[vals[i]] &= ~bit;
		}
		if (bitcopy) {
			for (i=0;i<256;i++) {
				if (camel_mime_special_table[i] & bitcopy)
					camel_mime_special_table[i] &= ~bit;
			}
		}
	}
}

static void
header_decode_init(void)
{
	int i;

	for (i=0;i<256;i++) camel_mime_special_table[i] = 0;
	for (i=0;i<32;i++) camel_mime_special_table[i] |= IS_CTRL;
	camel_mime_special_table[127] = IS_CTRL;
	camel_mime_special_table[' '] = IS_SPACE;
	camel_mime_special_table[':'] = IS_COLON;
	header_init_bits(IS_LWSP, 0, 0, CHARS_LWSP, sizeof(CHARS_LWSP)-1);
	header_init_bits(IS_TSPECIAL, IS_CTRL, 0, CHARS_TSPECIAL, sizeof(CHARS_TSPECIAL)-1);
	header_init_bits(IS_SPECIAL, 0, 0, CHARS_SPECIAL, sizeof(CHARS_SPECIAL)-1);
	header_init_bits(IS_DSPECIAL, 0, FALSE, CHARS_DSPECIAL, sizeof(CHARS_DSPECIAL)-1);
	for (i=0;i<256;i++) if ((i>=33 && i<=60) || (i>=62 && i<=126) || i==32 || i==9) camel_mime_special_table[i] |= IS_QPSAFE;
}

void
base64_init(void)
{
	int i;

	memset(camel_mime_base64_rank, 0xff, sizeof(camel_mime_base64_rank));
	for (i=0;i<64;i++) {
		camel_mime_base64_rank[(unsigned int)base64_alphabet[i]] = i;
	}
	camel_mime_base64_rank['='] = 0;
}

int main(int argc, char **argv)
{
	int i;
	void run_test(void);

	header_decode_init();
	base64_init();

	printf("static unsigned char camel_mime_special_table[256] = {\n\t");
	for (i=0;i<256;i++) {
		printf("%3d,", camel_mime_special_table[i]);
		if ((i&15) == 15) {
			printf("\n");
			if (i!=255) {
				printf("\t");
			}
		}
	}
	printf("};\n");

	printf("static unsigned char camel_mime_base64_rank[256] = {\n\t");
	for (i=0;i<256;i++) {
		printf("%3d,", camel_mime_base64_rank[i]);
		if ((i&15) == 15) {
			printf("\n");
			if (i!=255) {
				printf("\t");
			}
		}
	}
	printf("};\n");

	run_test();

	return 0;
}

#endif


/* call this when finished encoding everything, to
   flush off the last little bit */
int
base64_encode_close(unsigned char *in, int inlen, unsigned char *out, int *state, int *save)
{
	int c1, c2;
	unsigned char *outptr = out;

	if (inlen>0)
		outptr += base64_encode_step(in, inlen, outptr, state, save);

	c1 = ((char *)save)[1];
	c2 = ((char *)save)[2];

	switch (((char *)save)[0]) {
	case 2:
		outptr[2] = base64_alphabet [ ( (c2 &0x0f) << 2 ) ];
		goto skip;
	case 1:
		outptr[2] = '=';
	skip:
		outptr[0] = base64_alphabet [ c1 >> 2 ];
		outptr[1] = base64_alphabet [ c2 >> 4 | ( (c1&0x3) << 4 )];
		outptr[3] = '=';
		outptr += 4;
		break;
	}
	*outptr++ = '\n';

	*save = 0;
	*state = 0;

	return outptr-out;
}

/*
  performs an 'encode step', only encodes blocks of 3 characters to the
  output at a time, saves left-over state in state and save (initialise to
  0 on first invocation).
*/
int
base64_encode_step(unsigned char *in, int len, unsigned char *out, int *state, int *save)
{
	register unsigned char *inptr, *outptr;

	if (len<=0)
		return 0;

	inptr = in;
	outptr = out;

	d(printf("we have %d chars, and %d saved chars\n", len, ((char *)save)[0]));

	if (len + ((char *)save)[0] > 2) {
		unsigned char *inend = in+len-2;
		register int c1, c2, c3;
		register int already;

		already = *state;

		switch (((char *)save)[0]) {
		case 1:	c1 = ((unsigned char *)save)[1]; goto skip1;
		case 2:	c1 = ((unsigned char *)save)[1];
			c2 = ((unsigned char *)save)[2]; goto skip2;
		}
		
		/* yes, we jump into the loop, no i'm not going to change it, its beautiful! */
		while (inptr < inend) {
			c1 = *inptr++;
		skip1:
			c2 = *inptr++;
		skip2:
			c3 = *inptr++;
			*outptr++ = base64_alphabet [ c1 >> 2 ];
			*outptr++ = base64_alphabet [ c2 >> 4 | ( (c1&0x3) << 4 ) ];
			*outptr++ = base64_alphabet [ ( (c2 &0x0f) << 2 ) | (c3 >> 6) ];
			*outptr++ = base64_alphabet [ c3 & 0x3f ];
			/* this is a bit ugly ... */
			if ((++already)>=19) {
				*outptr++='\n';
				already = 0;
			}
		}

		((char *)save)[0] = 0;
		len = 2-(inptr-inend);
		*state = already;
	}

	d(printf("state = %d, len = %d\n",
		 (int)((char *)save)[0],
		 len));

	if (len>0) {
		register char *saveout;

		/* points to the slot for the next char to save */
		saveout = & (((char *)save)[1]) + ((char *)save)[0];

		/* len can only be 0 1 or 2 */
		switch(len) {
		case 2:	*saveout++ = *inptr++;
		case 1:	*saveout++ = *inptr++;
		}
		((char *)save)[0]+=len;
	}

	d(printf("mode = %d\nc1 = %c\nc2 = %c\n",
		 (int)((char *)save)[0],
		 (int)((char *)save)[1],
		 (int)((char *)save)[2]));

	return outptr-out;
}


/**
 * base64_decode_step: decode a chunk of base64 encoded data
 * @in: input stream
 * @len: max length of data to decode ( normally strlen(in) ??)
 * @out: output stream
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been decoded
 *
 * Decodes a chunk of base64 encoded data
 **/
int
base64_decode_step(unsigned char *in, int len, unsigned char *out, int *state, unsigned int *save)
{
	register unsigned char *inptr, *outptr;
	unsigned char *inend, c;
	register unsigned int v;
	int i;

	inend = in+len;
	outptr = out;

	/* convert 4 base64 bytes to 3 normal bytes */
	v=*save;
	i=*state;
	inptr = in;
	while (inptr<inend) {
		c = camel_mime_base64_rank[*inptr++];
		if (c != 0xff) {
			v = (v<<6) | c;
			i++;
			if (i==4) {
				*outptr++ = v>>16;
				*outptr++ = v>>8;
				*outptr++ = v;
				i=0;
			}
		}
	}

	*save = v;
	*state = i;

	/* quick scan back for '=' on the end somewhere */
	/* fortunately we can drop 1 output char for each trailing = (upto 2) */
	i=2;
	while (inptr>in && i) {
		inptr--;
		if (camel_mime_base64_rank[*inptr] != 0xff) {
			if (*inptr == '=')
				outptr--;
			i--;
		}
	}

	/* if i!= 0 then there is a truncation error! */
	return outptr-out;
}


/**
 * uudecode_step: uudecode a chunk of data
 * @in: input stream
 * @len: max length of data to decode ( normally strlen(in) ??)
 * @out: output stream
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been decoded
 * @uulen: holds the value of the length-char which is used to calculate
 *         how many more chars need to be decoded for that 'line'
 *
 * uudecodes a chunk of data. Assumes the "begin <mode> <file name>" line
 * has been stripped off.
 **/
int
uudecode_step (unsigned char *in, int len, unsigned char *out, int *state, guint32 *save, char *uulen)
{
	register unsigned char *inptr, *outptr;
	unsigned char *inend, ch;
	register guint32 saved;
	gboolean last_was_eoln;
	int i;

	if (*uulen <= 0)
		last_was_eoln = TRUE;
	else
		last_was_eoln = FALSE;
	
	inend = in + len;
	outptr = out;
	saved = *save;
	i = *state;
	inptr = in;
	while (inptr < inend && *inptr) {
		if (*inptr == '\n' || last_was_eoln) {
			if (last_was_eoln) {
				*uulen = CAMEL_UUDECODE_CHAR (*inptr);
				last_was_eoln = FALSE;
			} else {
				last_was_eoln = TRUE;
			}

			inptr++;
			continue;
		}

		ch = *inptr++;
		
		if (*uulen > 0) {
			/* save the byte */
			saved = (saved << 8) | ch;
			i++;
			if (i == 4) {
				/* convert 4 uuencoded bytes to 3 normal bytes */
				unsigned char b0, b1, b2, b3;

				b0 = saved >> 24;
				b1 = saved >> 16 & 0xff;
				b2 = saved >> 8 & 0xff;
				b3 = saved & 0xff;

				if (*uulen >= 3) {
					*outptr++ = CAMEL_UUDECODE_CHAR (b0) << 2 | CAMEL_UUDECODE_CHAR (b1) >> 4;
					*outptr++ = CAMEL_UUDECODE_CHAR (b1) << 4 | CAMEL_UUDECODE_CHAR (b2) >> 2;
				        *outptr++ = CAMEL_UUDECODE_CHAR (b2) << 6 | CAMEL_UUDECODE_CHAR (b3);
				} else {
					if (*uulen >= 1) {
						*outptr++ = CAMEL_UUDECODE_CHAR (b0) << 2 | CAMEL_UUDECODE_CHAR (b1) >> 4;
					}
					if (*uulen >= 2) {
						*outptr++ = CAMEL_UUDECODE_CHAR (b1) << 4 | CAMEL_UUDECODE_CHAR (b2) >> 2;
					}
				}

				i = 0;
				saved = 0;
				*uulen -= 3;
			}
		} else {
			break;
		}
	}

	*save = saved;
	*state = i;

	return outptr - out;
}

int
quoted_encode_close(unsigned char *in, int len, unsigned char *out, int *state, int *save)
{
	register unsigned char *outptr = out;

	if (len>0)
		outptr += quoted_encode_step(in, len, outptr, state, save);

	/* hmm, not sure if this should really be added here, we dont want
	   to add it to the content, afterall ...? */
	*outptr++ = '\n';

	*save = 0;
	*state = 0;

	return outptr-out;
}

/*
  FIXME: does not handle trailing spaces/tabs before end of line
*/
int
quoted_encode_step(unsigned char *in, int len, unsigned char *out, int *state, int *save)
{
	register unsigned char *inptr, *outptr, *inend;
	unsigned char c;
	register int sofar = *state;

	inptr = in;
	inend = in+len;
	outptr = out;
	while (inptr<inend) {
		c = *inptr++;
		if (is_qpsafe(c)) {
				/* check for soft line-break */
			if ((++sofar)>74) {
				*outptr++='=';
				*outptr++='\n';
				sofar = 1;
			}
			*outptr++=c;
		} else {
			if ((++sofar)>72) {
				*outptr++='=';
				*outptr++='\n';
				sofar = 3;
			}
			*outptr++ = '=';
			*outptr++ = tohex[(c>>4) & 0xf];
			*outptr++ = tohex[c & 0xf];
		}
	}
	*state = sofar;
	return outptr-out;
}

/*
  FIXME: this does not strip trailing spaces from lines (as it should, rfc 2045, section 6.7)
  Should it also canonicalise the end of line to CR LF??

  Note: Trailing rubbish (at the end of input), like = or =x or =\r will be lost.
*/ 

int
quoted_decode_step(unsigned char *in, int len, unsigned char *out, int *savestate, int *saveme)
{
	register unsigned char *inptr, *outptr;
	unsigned char *inend, c;
	int state, save;

	inend = in+len;
	outptr = out;

	d(printf("quoted-printable, decoding text '%.*s'\n", len, in));

	state = *savestate;
	save = *saveme;
	inptr = in;
	while (inptr<inend) {
		switch (state) {
		case 0:
			while (inptr<inend) {
				c = *inptr++;
				/* FIXME: use a specials table to avoid 3 comparisons for the common case */
				if (c=='=') { 
					state = 1;
					break;
				}
#ifdef CANONICALISE_EOL
				/*else if (c=='\r') {
					state = 3;
				} else if (c=='\n') {
					*outptr++ = '\r';
					*outptr++ = c;
					} */
#endif
				else {
					*outptr++ = c;
				}
			}
			break;
		case 1:
			c = *inptr++;
			if (c=='\n') {
				/* soft break ... unix end of line */
				state = 0;
			} else {
				save = c;
				state = 2;
			}
			break;
		case 2:
			c = *inptr++;
			if (isxdigit(c) && isxdigit(save)) {
				c = toupper(c);
				save = toupper(save);
				*outptr++ = (((save>='A'?save-'A'+10:save-'0')&0x0f) << 4)
					| ((c>='A'?c-'A'+10:c-'0')&0x0f);
			} else if (c=='\n' && save == '\r') {
				/* soft break ... canonical end of line */
			} else {
				/* just output the data */
				*outptr++ = '=';
				*outptr++ = save;
				*outptr++ = c;
			}
			state = 0;
			break;
#ifdef CANONICALISE_EOL
		case 3:
			/* convert \r -> to \r\n, leaves \r\n alone */
			c = *inptr++;
			if (c=='\n') {
				*outptr++ = '\r';
				*outptr++ = c;
			} else {
				*outptr++ = '\r';
				*outptr++ = '\n';
				*outptr++ = c;
			}
			state = 0;
			break;
#endif
		}
	}

	*savestate = state;
	*saveme = save;

	return outptr-out;
}

/*
  this is for the "Q" encoding of international words,
  which is slightly different than plain quoted-printable
*/
static int
quoted_decode(const unsigned char *in, int len, unsigned char *out)
{
	register const unsigned char *inptr;
	register unsigned char *outptr;
	unsigned const char *inend;
	unsigned char c, c1;
	int ret = 0;

	inend = in+len;
	outptr = out;

	d(printf("decoding text '%.*s'\n", len, in));

	inptr = in;
	while (inptr<inend) {
		c = *inptr++;
		if (c=='=') {
			/* silently ignore truncated data? */
			if (inend-in>=2) {
				c = toupper(*inptr++);
				c1 = toupper(*inptr++);
				*outptr++ = (((c>='A'?c-'A'+10:c-'0')&0x0f) << 4)
					| ((c1>='A'?c1-'A'+10:c1-'0')&0x0f);
			} else {
				ret = -1;
				break;
			}
		} else if (c=='_') {
			*outptr++ = 0x20;
		} else if (c==' ' || c==0x09) {
			/* FIXME: this is an error! ignore for now ... */
			ret = -1;
			break;
		} else {
			*outptr++ = c;
		}
	}
	if (ret==0) {
		return outptr-out;
	}
	return -1;
}

/* rfc2047 version of quoted-printable */
static int
quoted_encode(const unsigned char *in, int len, unsigned char *out)
{
	register const unsigned char *inptr, *inend;
	unsigned char *outptr;
	unsigned char c;

	inptr = in;
	inend = in+len;
	outptr = out;
	while (inptr<inend) {
		c = *inptr++;
		if (is_qpsafe(c) && !(c=='_' || c=='?')) {
			if (c==' ')
				c='_';
			*outptr++=c;
		} else {
			*outptr++ = '=';
			*outptr++ = tohex[(c>>4) & 0xf];
			*outptr++ = tohex[c & 0xf];
		}
	}

	printf("encoding '%.*s' = '%.*s'\n", len, in, outptr-out, out);

	return outptr-out;
}


static void
header_decode_lwsp(const char **in)
{
	const char *inptr = *in;
	char c;

	d2(printf("is ws: '%s'\n", *in));

	while (is_lwsp(*inptr) || *inptr =='(') {
		while (is_lwsp(*inptr)) {
			d2(printf("(%c)", *inptr));
			inptr++;
		}
		d2(printf("\n"));

		/* check for comments */
		if (*inptr == '(') {
			int depth = 1;
			inptr++;
			while (depth && (c=*inptr)) {
				if (c=='\\' && inptr[1]) {
					inptr++;
				} else if (c=='(') {
					depth++;
				} else if (c==')') {
					depth--;
				}
				inptr++;
			}
		}
	}
	*in = inptr;
}

/* decode rfc 2047 encoded string segment */
static char *
rfc2047_decode_word(const char *in, int len)
{
	const char *inptr = in+2;
	const char *inend = in+len-2;
	char *encname;
	int tmplen;
	int ret;
	char *decword = NULL;
	char *decoded = NULL;
	char *outbase = NULL;
	char *inbuf, *outbuf;
	int inlen, outlen;
	unicode_iconv_t ic;

	d(printf("decoding '%.*s'\n", len, in));

	/* just make sure we're not passed shit */
	if (len<7
	    || !(in[0]=='=' && in[1]=='?' && in[len-1]=='=' && in[len-2]=='?')) {
		d(printf("invalid\n"));
		return NULL;
	}

	inptr = memchr(inptr, '?', inend-inptr);
	if (inptr!=NULL
	    && inptr<inend+2
	    && inptr[2]=='?') {
		d(printf("found ?, encoding is '%c'\n", inptr[0]));
		inptr++;
		tmplen = inend-inptr-2;
		decword = alloca(tmplen); /* this will always be more-than-enough room */
		switch(toupper(inptr[0])) {
		case 'Q':
			inlen = quoted_decode(inptr+2, tmplen, decword);
			break;
		case 'B': {
			int state=0;
			unsigned int save=0;
			inlen = base64_decode_step((char *)inptr+2, tmplen, decword, &state, &save);
			/* if state != 0 then error? */
			break;
		}
		}
		d(printf("The encoded length = %d\n", inlen));
		if (inlen>0) {
			/* yuck, all this snot is to setup iconv! */
			tmplen = inptr-in-3;
			encname = alloca(tmplen+1);
			encname[tmplen]=0;
			memcpy(encname, in+2, tmplen);

			inbuf = decword;

			outlen = inlen*6;
			outbase = alloca(outlen);
			outbuf = outbase;

			/* TODO: Should this cache iconv converters? */
			ic = unicode_iconv_open("iso-8859-1", encname);
			if (ic != (unicode_iconv_t)-1) {
				ret = unicode_iconv(ic, (const char **)&inbuf, &inlen, &outbuf, &outlen);
				unicode_iconv_close(ic);
				if (ret>=0) {
					*outbuf = 0;
					decoded = g_strdup(outbase);
				}
			} else {
				w(g_warning("Cannot decode charset, header display may be corrupt: %s: %s", encname, strerror(errno)));
				/* TODO: Should this do this, or just leave the encoded strings? */
				decword[inlen] = 0;
				decoded = g_strdup(decword);
			}
		}
	}

	d(printf("decoded '%s'\n", decoded));

	return decoded;
}

/* grrr, glib should have this ! */
static GString *
g_string_append_len(GString *st, const char *s, int l)
{
	char *tmp;

	tmp = alloca(l+1);
	tmp[l]=0;
	memcpy(tmp, s, l);
	return g_string_append(st, tmp);
}

/* decodes a simple text, rfc822 */
static char *
header_decode_text(const char *in, int inlen)
{
	GString *out;
	const char *inptr = in;
	const char *inend = in+inlen;
	char *encstart, *encend;
	char *decword;

	out = g_string_new("");
	while ( (encstart = strstr(inptr, "=?"))
		&& (encend = strstr(encstart+2, "?=")) ) {

		decword = rfc2047_decode_word(encstart, encend-encstart+2);
		if (decword) {
			out = g_string_append_len(out, inptr, encstart-inptr);
			out = g_string_append_len(out, decword, strlen(decword));
			free(decword);
		} else {
			out = g_string_append_len(out, inptr, encend-inptr+2);
		}
		inptr = encend+2;
	}
	out = g_string_append_len(out, inptr, inend-inptr);

	encstart = out->str;
	g_string_free(out, FALSE);

	return encstart;
}

char *
header_decode_string(const char *in)
{
	if (in == NULL)
		return NULL;
	return header_decode_text(in, strlen(in));
}

static char *encoding_map[] = {
	"US-ASCII",
	"ISO-8859-1",
	"UTF-8"
};

/* FIXME: needs a way to cache iconv opens for different charsets? */
static
char *rfc2047_encode_word(const char *in, int len, char *type)
{
	unicode_iconv_t ic;
	char *buffer, *out, *ascii;
	size_t inlen, outlen, enclen;

	d(printf("Converting '%.*s' to %s\n", len, in, type));

	/* convert utf8->encoding */
	outlen = len*6;
	buffer = alloca(outlen);
	inlen = len;
	out = buffer;

	/* if we can't convert from utf-8, just encode as utf-8 */
	if (!strcasecmp(type, "UTF-8")
	    || (ic = unicode_iconv_open(type, "UTF-8")) == (unicode_iconv_t)-1) {
		memcpy(buffer, in, len);
		out = buffer+len;
		type = "UTF-8";
	} else {
		if (unicode_iconv(ic, &in, &inlen, &out, &outlen) == -1) {
			w(g_warning("Conversion problem: conversion truncated: %s", strerror(errno)));
		}
		unicode_iconv_close(ic);
	}
	enclen = out-buffer;

	/* now create qp version */
	ascii = alloca(enclen*3 + strlen(type) + 8);
	out = ascii;
	/* should determine which encoding is smaller, and use that? */
	out += sprintf(out, "=?%s?Q?", type);
	out += quoted_encode(buffer, enclen, out);
	sprintf(out, "?=");

	d(printf("converted = %s\n", ascii));
	return g_strdup(ascii);
}


/* TODO: Should this worry about quotes?? */
char *
header_encode_string(const unsigned char *in)
{
	GString *out;
	const unsigned char *inptr = in, *start;
	int encoding;
	char *outstr;

	if (in == NULL)
		return NULL;

	/* do a quick us-ascii check (the common case?) */
	while (*inptr) {
		if (*inptr > 127)
			break;
		inptr++;
	}
	if (*inptr == 0)
		return g_strdup(in);

	/* This gets each word out of the input, and checks to see what charset
	   can be used to encode it. */
	/* TODO: Work out when to merge subsequent words, or across word-parts */
	/* FIXME: Make sure a converted word is less than the encoding size */
	out = g_string_new("");
	inptr = in;
	encoding = 0;
	start = inptr;
	while (inptr && *inptr) {
		unicode_char_t c;
		const char *newinptr;
		newinptr = unicode_get_utf8(inptr, &c);
		if (newinptr == NULL) {
			w(g_warning("Invalid UTF-8 sequence encountered (pos %d, char '%c'): %s", (inptr-in), inptr[0], in));
			inptr++;
			continue;
		}
		inptr = newinptr;
		if (unicode_isspace(c)) {
			if (encoding == 0) {
				out = g_string_append_len(out, start, inptr-start);
			} else {
				char *text = rfc2047_encode_word(start, inptr-start-1, encoding_map[encoding]);
				out = g_string_append(out, text);
				out = g_string_append_c(out, c);
				g_free(text);
			}
			start = inptr;
			encoding = 0;
		} else if (c>127 && c < 256) {
			encoding = MAX(encoding, 1);
		} else if (c >=256) {
			encoding = MAX(encoding, 2);
		}
	}
	if (inptr-start) {
		if (encoding == 0) {
			out = g_string_append_len(out, start, inptr-start);
		} else {
			char *text = rfc2047_encode_word(start, inptr-start, encoding_map[encoding]);
			out = g_string_append(out, text);
			g_free(text);
		}
	}
	outstr = out->str;
	g_string_free(out, FALSE);
	return outstr;
}


/* these are all internal parser functions */

static char *
decode_token(const char **in)
{
	const char *inptr = *in;
	const char *start;

	header_decode_lwsp(&inptr);
	start = inptr;
	while (is_ttoken(*inptr))
		inptr++;
	if (inptr>start) {
		*in = inptr;
		return g_strndup(start, inptr-start);
	} else {
		return NULL;
	}
}

char *
header_token_decode(const char *in)
{
	if (in == NULL)
		return NULL;

	return decode_token(&in);
}

/*
   <"> * ( <any char except <"> \, cr  /  \ <any char> ) <">
*/
static char *
header_decode_quoted_string(const char **in)
{
	const char *inptr = *in;
	char *out = NULL, *outptr;
	int outlen;
	int c;

	header_decode_lwsp(&inptr);
	if (*inptr == '"') {
		const char *intmp;
		int skip = 0;

		/* first, calc length */
		inptr++;
		intmp = inptr;
		while ( (c = *intmp++) && c!= '"' ) {
			if (c=='\\' && *intmp) {
				intmp++;
				skip++;
			}
		}
		outlen = intmp-inptr-skip;
		out = outptr = g_malloc(outlen+1);
		while ( (c = *inptr++) && c!= '"' ) {
			if (c=='\\' && *inptr) {
				c = *inptr++;
			}
			*outptr++ = c;
		}
		*outptr = 0;
	}
	*in = inptr;
	return out;
}

static char *
header_decode_atom(const char **in)
{
	const char *inptr = *in, *start;

	header_decode_lwsp(&inptr);
	start = inptr;
	while (is_atom(*inptr))
		inptr++;
	*in = inptr;
	if (inptr > start)
		return g_strndup(start, inptr-start);
	else
		return NULL;
}

static char *
header_decode_word(const char **in)
{
	const char *inptr = *in;

	header_decode_lwsp(&inptr);
	if (*inptr == '"') {
		*in = inptr;
		return header_decode_quoted_string(in);
	} else {
		*in = inptr;
		return header_decode_atom(in);
	}
}

static char *
header_decode_value(const char **in)
{
	const char *inptr = *in;

	header_decode_lwsp(&inptr);
	if (*inptr == '"') {
		d(printf("decoding quoted string\n"));
		return header_decode_quoted_string(in);
	} else if (is_ttoken(*inptr)) {
		d(printf("decoding token\n"));
		/* this may not have the right specials for all params? */
		return decode_token(in);
	}
	return NULL;
}

/* shoudl this return -1 for no int? */
static int
header_decode_int(const char **in)
{
	const char *inptr = *in;
	int c, v=0;

	header_decode_lwsp(&inptr);
	while ( (c=*inptr++ & 0xff)
		&& isdigit(c) ) {
		v = v*10+(c-'0');
	}
	*in = inptr-1;
	return v;
}

static int
header_decode_param(const char **in, char **paramp, char **valuep)
{
	const char *inptr = *in;
	char *param, *value=NULL;

	param = decode_token(&inptr);
	header_decode_lwsp(&inptr);
	if (*inptr == '=') {
		inptr++;
		value = header_decode_value(&inptr);
	}

	if (param && value) {
		*paramp = param;
		*valuep = value;
		*in = inptr;
		return 0;
	} else {
		g_free(param);
		g_free(value);
		return 1;
	}
}

char *
header_param(struct _header_param *p, const char *name)
{
	while (p && strcasecmp(p->name, name) != 0)
		p = p->next;
	if (p)
		return p->value;
	return NULL;
}

struct _header_param *
header_set_param(struct _header_param **l, const char *name, const char *value)
{
	struct _header_param *p = (struct _header_param *)l, *pn;

	while (p->next) {
		pn = p->next;
		if (!strcasecmp(pn->name, name)) {
			g_free(pn->value);
			if (value) {
				pn->value = g_strdup(value);
				return pn;
			} else {
				p->next = pn->next;
				g_free(pn);
				return NULL;
			}
		}
		p = pn;
	}

	if (value == NULL)
		return NULL;

	pn = g_malloc(sizeof(*pn));
	pn->next = 0;
	pn->name = g_strdup(name);
	pn->value = g_strdup(value);
	p->next = pn;

	return pn;
}

const char *
header_content_type_param(struct _header_content_type *t, const char *name)
{
	if (t==NULL)
		return NULL;
	return header_param(t->params, name);
}

void header_content_type_set_param(struct _header_content_type *t, const char *name, const char *value)
{
	header_set_param(&t->params, name, value);
}

/**
 * header_content_type_is:
 * @ct: A content type specifier, or #NULL.
 * @type: A type to check against.
 * @subtype: A subtype to check against, or "*" to match any subtype.
 * 
 * Returns #TRUE if the content type @ct is of type @type/@subtype.
 * The subtype of "*" will match any subtype.  If @ct is #NULL, then
 * it will match the type "text/plain".
 * 
 * Return value: #TRUE or #FALSE depending on the matching of the type.
 **/
int
header_content_type_is(struct _header_content_type *ct, const char *type, const char *subtype)
{
	/* no type == text/plain or text/"*" */
	if (ct==NULL) {
		return (!strcasecmp(type, "text")
			&& (!strcasecmp(subtype, "plain")
			    || !strcasecmp(subtype, "*")));
	}

	return (ct->type != NULL
		&& (!strcasecmp(ct->type, type)
		    && ((ct->subtype != NULL
			 && !strcasecmp(ct->subtype, subtype))
			|| !strcasecmp("*", subtype))));
}

void
header_param_list_free(struct _header_param *p)
{
	struct _header_param *n;

	while (p) {
		n = p->next;
		g_free(p->name);
		g_free(p->value);
		g_free(p);
		p = n;
	}
}

struct _header_content_type *
header_content_type_new(const char *type, const char *subtype)
{
	struct _header_content_type *t = g_malloc(sizeof(*t));

	t->type = g_strdup(type);
	t->subtype = g_strdup(subtype);
	t->params = NULL;
	t->refcount = 1;
	return t;
}

void
header_content_type_ref(struct _header_content_type *ct)
{
	if (ct)
		ct->refcount++;
}


void
header_content_type_unref(struct _header_content_type *ct)
{
	if (ct) {
		if (ct->refcount <= 1) {
			header_param_list_free(ct->params);
			g_free(ct->type);
			g_free(ct->subtype);
			g_free(ct);
		} else {
			ct->refcount--;
		}
	}
}

/* for decoding email addresses, canonically */
static char *
header_decode_domain(const char **in)
{
	const char *inptr = *in, *start;
	int go = TRUE;
	char *ret;
	GString *domain = g_string_new("");

				/* domain ref | domain literal */
	header_decode_lwsp(&inptr);
	while (go) {
		if (*inptr == '[') { /* domain literal */
			domain = g_string_append(domain, "[ ");
			inptr++;
			header_decode_lwsp(&inptr);
			start = inptr;
			while (is_dtext(*inptr)) {
				domain = g_string_append_c(domain, *inptr);
				inptr++;
			}
			if (*inptr == ']') {
				domain = g_string_append(domain, " ]");
				inptr++;
			} else {
				w(g_warning("closing ']' not found in domain: %s", *in));
			}
		} else {
			char *a = header_decode_atom(&inptr);
			if (a) {
				domain = g_string_append(domain, a);
				g_free(a);
			} else {
				w(g_warning("missing atom from domain-ref"));
				break;
			}
		}
		header_decode_lwsp(&inptr);
		if (*inptr == '.') { /* next sub-domain? */
			domain = g_string_append_c(domain, '.');
			inptr++;
			header_decode_lwsp(&inptr);
		} else
			go = FALSE;
	}

	*in = inptr;

	ret = domain->str;
	g_string_free(domain, FALSE);
	return ret;
}

static char *
header_decode_addrspec(const char **in)
{
	const char *inptr = *in;
	char *word;
	GString *addr = g_string_new("");

	header_decode_lwsp(&inptr);

	/* addr-spec */
	word = header_decode_word(&inptr);
	if (word) {
		addr = g_string_append(addr, word);
		header_decode_lwsp(&inptr);
		g_free(word);
		while (*inptr == '.' && word) {
			inptr++;
			addr = g_string_append_c(addr, '.');
			word = header_decode_word(&inptr);
			if (word) {
				addr = g_string_append(addr, word);
				header_decode_lwsp(&inptr);
				g_free(word);
			} else {
				w(g_warning("Invalid address spec: %s", *in));
			}
		}
		if (*inptr == '@') {
			inptr++;
			addr = g_string_append_c(addr, '@');
			word = header_decode_domain(&inptr);
			if (word) {
				addr = g_string_append(addr, word);
				g_free(word);
			} else {
				w(g_warning("Invalid address, missing domain: %s", *in));
			}
		} else {
			w(g_warning("Invalid addr-spec, missing @: %s", *in));
		}
	} else {
		w(g_warning("invalid addr-spec, no local part"));
	}

	/* FIXME: return null on error? */

	*in = inptr;
	word = addr->str;
	g_string_free(addr, FALSE);
	return word;
}

/*
  address:
   word *('.' word) @ domain |
   *(word) '<' [ *('@' domain ) ':' ] word *( '.' word) @ domain |

   1*word ':' [ word ... etc (mailbox, as above) ] ';'
 */

/* mailbox:
   word *( '.' word ) '@' domain
   *(word) '<' [ *('@' domain ) ':' ] word *( '.' word) @ domain
   */

static struct _header_address *
header_decode_mailbox(const char **in)
{
	const char *inptr = *in;
	char *pre;
	int closeme = FALSE;
	GString *addr;
	GString *name = NULL;
	struct _header_address *address = NULL;

	addr = g_string_new("");

	/* for each address */
	pre = header_decode_word(&inptr);
	header_decode_lwsp(&inptr);
	if (!(*inptr == '.' || *inptr == '@' || *inptr==',' || *inptr=='\0')) {
		/* ',' and '\0' required incase it is a simple address, no @ domain part (buggy writer) */
		name = g_string_new("");
		while (pre) {
			char *text;

			text = header_decode_string(pre);
			name = g_string_append(name, text);
			g_free(pre);
			g_free(text);

			/* rfc_decode(pre) */
			pre = header_decode_word(&inptr);
			if (pre)
				name = g_string_append_c(name, ' ');
		}
		header_decode_lwsp(&inptr);
		if (*inptr == '<') {
			closeme = TRUE;
			inptr++;
			header_decode_lwsp(&inptr);
			if (*inptr == '@') {
				while (*inptr == '@') {
					inptr++;
					header_decode_domain(&inptr);
					header_decode_lwsp(&inptr);
					if (*inptr == ',') {
						inptr++;
						header_decode_lwsp(&inptr);
					}
				}
				if (*inptr == ':') {
					inptr++;
				} else {
					w(g_warning("broken route-address, missing ':': %s", *in));
				}
			}
			pre = header_decode_word(&inptr);
			header_decode_lwsp(&inptr);
		} else {
			w(g_warning("broken address? %s", *in));
		}
	}

	if (pre) {
		addr = g_string_append(addr, pre);
	} else {
		w(g_warning("No local-part for email address: %s", *in));
	}

	/* should be at word '.' localpart */
	while (*inptr == '.' && pre) {
		inptr++;
		g_free(pre);
		pre = header_decode_word(&inptr);
		if (pre) {
			addr = g_string_append_c(addr, '.');
			addr = g_string_append(addr, pre);
		}
		header_decode_lwsp(&inptr);
	}
	g_free(pre);

	/* now at '@' domain part */
	if (*inptr == '@') {
		char *dom;

		inptr++;
		addr = g_string_append_c(addr, '@');
		dom = header_decode_domain(&inptr);
		addr = g_string_append(addr, dom);
		g_free(dom);
	} else {
		w(g_warning("invalid address, no '@' domain part at %c: %s", *inptr, *in));
	}

	if (closeme) {
		header_decode_lwsp(&inptr);
		if (*inptr == '>') {
			inptr++;
		} else {
			w(g_warning("invalid route address, no closing '>': %s", *in));
		} 
	} else if (name == NULL) { /* check for comment after address */
		char *text, *tmp;
		const char *comment = inptr;

		header_decode_lwsp(&inptr);
		if (inptr-comment > 3) { /* just guess ... */
			tmp = g_strndup(comment, inptr-comment);
			text = header_decode_string(tmp);
			name = g_string_new(text);
			g_free(tmp);
			g_free(text);
		}
	}

	*in = inptr;

	if (addr->len > 0) {
		address = header_address_new_name(name?name->str:"", addr->str);
	}

	g_string_free(addr, TRUE);
	if (name)
		g_string_free(name, TRUE);

	d(printf("got mailbox: %s\n", addr->str));
	return address;
}

static struct _header_address *
header_decode_address(const char **in)
{
	const char *inptr = *in;
	char *pre;
	GString *group = g_string_new("");
	struct _header_address *addr = NULL, *member;

	/* pre-scan, trying to work out format, discard results */
	header_decode_lwsp(&inptr);
	while ( (pre = header_decode_word(&inptr)) ) {
		group = g_string_append(group, pre);
		group = g_string_append(group, " ");
		g_free(pre);
	}
	header_decode_lwsp(&inptr);
	if (*inptr == ':') {
		d(printf("group detected: %s\n", group->str));
		addr = header_address_new_group(group->str);
		/* that was a group spec, scan mailbox's */
		inptr++;
		/* FIXME: check rfc 2047 encodings of words, here or above in the loop */
		header_decode_lwsp(&inptr);
		if (*inptr != ';') {
			int go = TRUE;
			do {
				member = header_decode_mailbox(&inptr);
				if (member)
					header_address_add_member(addr, member);
				header_decode_lwsp(&inptr);
				if (*inptr == ',')
					inptr++;
				else
					go = FALSE;
			} while (go);
			if (*inptr == ';') {
				inptr++;
			} else {
				w(g_warning("Invalid group spec, missing closing ';': %s", *in));
			}
		} else {
			inptr++;
		}
		*in = inptr;
	} else {
		addr = header_decode_mailbox(in);
	}

	g_string_free(group, TRUE);

	return addr;
}

static char *
header_msgid_decode_internal(const char **in)
{
	const char *inptr = *in;
	char *msgid = NULL;

	d(printf("decoding Message-ID: '%s'\n", *in));

	header_decode_lwsp(&inptr);
	if (*inptr == '<') {
		inptr++;
		header_decode_lwsp(&inptr);
		msgid = header_decode_addrspec(&inptr);
		if (msgid) {
			header_decode_lwsp(&inptr);
			if (*inptr == '>') {
				inptr++;
			} else {
				w(g_warning("Missing closing '>' on message id: %s", *in));
			}
		} else {
			w(g_warning("Cannot find message id in: %s", *in));
		}
	} else {
		w(g_warning("missing opening '<' on message id: %s", *in));
	}
	*in = inptr;

	return msgid;
}

char *
header_msgid_decode(const char *in)
{
	if (in == NULL)
		return NULL;

	return header_msgid_decode_internal(&in);
}

void
header_references_list_append_asis(struct _header_references **list, char *ref)
{
	struct _header_references *w = (struct _header_references *)list, *n;
	while (w->next)
		w = w->next;
	n = g_malloc(sizeof(*n));
	n->id = ref;
	n->next = 0;
	w->next = n;
}

int
header_references_list_size(struct _header_references **list)
{
	int count = 0;
	struct _header_references *w = *list;
	while (w) {
		count++;
		w = w->next;
	}
	return count;
}

void
header_references_list_clear(struct _header_references **list)
{
	struct _header_references *w = *list, *n;
	while (w) {
		n = w->next;
		g_free(w->id);
		g_free(w);
		w = n;
	}
	*list = NULL;
}

/* generate a list of references, from most recent up */
struct _header_references *
header_references_decode(const char *in)
{
	const char *inptr = in, *intmp;
	struct _header_references *head = NULL, *node;
	char *id, *word;

	if (in == NULL)
		return NULL;

	while (*inptr) {
		header_decode_lwsp(&inptr);
		if (*inptr == '<') {
			id = header_msgid_decode_internal(&inptr);
			if (id) {
				node = g_malloc(sizeof(*node));
				node->next = head;
				head = node;
				node->id = id;
			}
		} else {
			word = header_decode_word(&inptr);
			if (word)
				g_free (word);
			else
				break;
		}
	}

	return head;
}

struct _header_address *
header_mailbox_decode(const char *in)
{
	if (in == NULL)
		return NULL;

	return header_decode_mailbox(&in);
}

struct _header_address *
header_address_decode(const char *in)
{
	const char *inptr = in, *last;
	struct _header_address *list = NULL, *addr;

	d(printf("decoding To: '%s'\n", in));

#warning header_to_decode needs to return some structure

	if (in == NULL)
		return NULL;

	do {
		last = inptr;
		addr = header_decode_address(&inptr);
		if (addr)
			header_address_list_append(&list, addr);
		header_decode_lwsp(&inptr);
		if (*inptr == ',')
			inptr++;
		else
			break;
	} while (inptr != last);

	if (*inptr) {
		w(g_warning("Invalid input detected at %c (%d): %s\n or at: %s", *inptr, inptr-in, in, inptr));
	}

	if (inptr == last) {
		w(g_warning("detected invalid input loop at : %s", last));
	}

	return list;
}

void
header_mime_decode(const char *in, int *maj, int *min)
{
	const char *inptr = in;
	int major=-1, minor=-1;

	d(printf("decoding MIME-Version: '%s'\n", in));

	if (in != NULL) {
		header_decode_lwsp(&inptr);
		if (isdigit(*inptr)) {
			major = header_decode_int(&inptr);
			header_decode_lwsp(&inptr);
			if (*inptr == '.') {
				inptr++;
				header_decode_lwsp(&inptr);
				if (isdigit(*inptr))
					minor = header_decode_int(&inptr);
			}
		}
	}

	if (maj)
		*maj = major;
	if (min)
		*min = minor;

	d(printf("major = %d, minor = %d\n", major, minor));
}

static struct _header_param *
header_param_list_decode(const char **in)
{
	const char *inptr = *in;
	struct _header_param *head = NULL, *tail = NULL;

	header_decode_lwsp(&inptr);
	while (*inptr == ';') {
		char *param, *value;
		struct _header_param *p;

		inptr++;
		/* invalid format? */
		if (header_decode_param(&inptr, &param, &value) != 0)
			break;

		p = g_malloc(sizeof(*p));
		p->name = param;
		p->value = value;
		p->next = NULL;
		if (head == NULL)
			head = p;
		if (tail)
			tail->next = p;
		tail = p;
		header_decode_lwsp(&inptr);
	}
	*in = inptr;
	return head;
}

static void
header_param_list_format_append(GString *out, struct _header_param *p)
{
	int len = out->len;
	while (p) {
		int here = out->len;
		if (len+strlen(p->name)+strlen(p->value)>60) {
			out = g_string_append(out, "\n\t");
			len = 0;
		}
		/* FIXME: format the value properly */
		g_string_sprintfa(out, " ; %s=\"%s\"", p->name, p->value);
		len += (out->len - here);
		p = p->next;
	}
}

struct _header_content_type *
header_content_type_decode(const char *in)
{
	const char *inptr = in;
	char *type, *subtype = NULL;
	struct _header_content_type *t = NULL;

	if (in==NULL)
		return NULL;

	type = decode_token(&inptr);
	header_decode_lwsp(&inptr);
	if (type) {
		if  (*inptr == '/') {
			inptr++;
			subtype = decode_token(&inptr);
		}
		if (subtype == NULL && (!strcasecmp(type, "text"))) {
			w(g_warning("text type with no subtype, resorting to text/plain: %s", in));
			subtype = g_strdup("plain");
		}
		if (subtype == NULL) {
			w(g_warning("MIME type with no subtype: %s", in));
		}

		t = header_content_type_new(type, subtype);
		t->params = header_param_list_decode(&inptr);
		g_free(type);
		g_free(subtype);
	} else {
		g_free(type);
		d(printf("cannot find MIME type in header (2) '%s'", in));
	}
	return t;
}

void
header_content_type_dump(struct _header_content_type *ct)
{
	struct _header_param *p;

	printf("Content-Type: ");
	if (ct==NULL) {
		printf("<NULL>\n");
		return;
	}
	printf("%s / %s", ct->type, ct->subtype);
	p = ct->params;
	if (p) {
		while (p) {
			printf(";\n\t%s=\"%s\"", p->name, p->value);
			p = p->next;
		}
	}
	printf("\n");
}

char *
header_content_type_format(struct _header_content_type *ct)
{
	GString *out;
	char *ret;

	if (ct==NULL)
		return NULL;

	out = g_string_new("");
	if (ct->type == NULL) {
		g_string_sprintfa(out, "text/plain");
		w(g_warning("Content-Type with no main type"));
	} else if (ct->subtype == NULL) {
		w(g_warning("Content-Type with no sub type: %s", ct->type));
		if (!strcasecmp(ct->type, "multipart"))
			g_string_sprintfa(out, "%s/mixed", ct->type);
		else
			g_string_sprintfa(out, "%s", ct->type);
	} else {
		g_string_sprintfa(out, "%s/%s", ct->type, ct->subtype);
	}
	header_param_list_format_append(out, ct->params);

	ret = out->str;
	g_string_free(out, FALSE);
	return ret;
}

char *
header_content_encoding_decode(const char *in)
{
	if (in)
		return decode_token(&in);
	return NULL;
}

CamelMimeDisposition *header_disposition_decode(const char *in)
{
	CamelMimeDisposition *d = NULL;
	const char *inptr = in;

	if (in == NULL)
		return NULL;

	d = g_malloc(sizeof(*d));
	d->refcount = 1;
	d->disposition = decode_token(&inptr);
	if (d->disposition == NULL)
		w(g_warning("Empty disposition type"));
	d->params = header_param_list_decode(&inptr);
	return d;
}

void header_disposition_ref(CamelMimeDisposition *d)
{
	if (d)
		d->refcount++;
}
void header_disposition_unref(CamelMimeDisposition *d)
{
	if (d) {
		if (d->refcount<=1) {
			header_param_list_free(d->params);
			g_free(d->disposition);
			g_free(d);
		} else {
			d->refcount--;
		}
	}
}

char *header_disposition_format(CamelMimeDisposition *d)
{
	GString *out;
	char *ret;

	if (d==NULL)
		return NULL;

	out = g_string_new("");
	if (d->disposition)
		out = g_string_append(out, d->disposition);
	else
		out = g_string_append(out, "attachment");
	header_param_list_format_append(out, d->params);

	ret = out->str;
	g_string_free(out, FALSE);
	return ret;
}

/* hrm, is there a library for this shit? */
static struct {
	char *name;
	int offset;
} tz_offsets [] = {
	{ "UT", 0 },
	{ "GMT", 0 },
	{ "EST", -500 },	/* these are all US timezones.  bloody yanks */
	{ "EDT", -400 },
	{ "CST", -600 },
	{ "CDT", -500 },
	{ "MST", -700 },
	{ "MDT", -600 },
	{ "PST", -800 },
	{ "PDT", -700 },
	{ "Z", 0 },
	{ "A", -100 },
	{ "M", -1200 },
	{ "N", 100 },
	{ "Y", 1200 },
};

static char *tz_months [] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

char *
header_format_date(time_t time, int offset)
{
	struct tm tm;

	d(printf("offset = %d\n", offset));

	d(printf("converting date %s", ctime(&time)));

	time += ((offset / 100) * (60*60)) + (offset % 100)*60;

	d(printf("converting date %s", ctime(&time)));

	memcpy(&tm, gmtime(&time), sizeof(tm));

	return g_strdup_printf("%02d %s %04d %02d:%02d:%02d %+05d",
			       tm.tm_mday, tz_months[tm.tm_mon],
			       tm.tm_year + 1900,
			       tm.tm_hour, tm.tm_min, tm.tm_sec,
			       offset);
}

/* convert a date to time_t representation */
/* this is an awful mess oh well */
time_t
header_decode_date(const char *in, int *saveoffset)
{
	const char *inptr = in;
	char *monthname;
	int year, offset = 0;
	struct tm tm;
	int i;
	time_t t;

	if (in == NULL) {
		if (saveoffset)
			*saveoffset = 0;
		return 0;
	}

	d(printf ("\ndecoding date '%s'\n", inptr));

	memset (&tm, 0, sizeof(tm));

	header_decode_lwsp (&inptr);
	if (!isdigit (*inptr)) {
		char *day = decode_token (&inptr);
		/* we dont really care about the day, its only for display */
		if (day) {
			d(printf ("got day: %s\n", day));
			g_free (day);
			header_decode_lwsp (&inptr);
			if (*inptr == ',') {
				inptr++;
			} else {
				gchar *newdate;
				
				w(g_warning("day not followed by ',' its probably a broken mail client, so we'll ignore its date entirely"));
				printf ("Giving it one last chance...\n");
				newdate = parse_broken_date (in);
				if (newdate) {
					printf ("Got: %s\n", newdate);
					if (saveoffset)
						*saveoffset = 0;					
					t = header_decode_date (newdate, NULL);
					g_free (newdate);
				}
				
				if (saveoffset)
					*saveoffset = 0;
				return 0;
			}
		}
	}
	tm.tm_mday = header_decode_int(&inptr);
	monthname = decode_token(&inptr);
	if (monthname) {
		for (i=0;i<sizeof(tz_months)/sizeof(tz_months[0]);i++) {
			if (!strcasecmp(tz_months[i], monthname)) {
				tm.tm_mon = i;
				break;
			}
		}
		g_free(monthname);
	}
	year = header_decode_int(&inptr);
	if (year<100) {
		tm.tm_year = year;
	} else {
		tm.tm_year = year-1900;
	}
	/* get the time ... yurck */
	tm.tm_hour = header_decode_int(&inptr);
	header_decode_lwsp(&inptr);
	if (*inptr == ':')
		inptr++;
	tm.tm_min = header_decode_int(&inptr);
	header_decode_lwsp(&inptr);
	if (*inptr == ':')
		inptr++;
	tm.tm_sec = header_decode_int(&inptr);
	header_decode_lwsp(&inptr);
	if (*inptr == '+'
	    || *inptr == '-') {
		offset = (*inptr++)=='-'?-1:1;
		offset = offset * header_decode_int(&inptr);
		d(printf("abs signed offset = %d\n", offset));
	} else if (isdigit(*inptr)) {
		offset = header_decode_int(&inptr);
		d(printf("abs offset = %d\n", offset));
	} else {
		char *tz = decode_token(&inptr);

		if (tz) {
			for (i=0;i<sizeof(tz_offsets)/sizeof(tz_offsets[0]);i++) {
				if (!strcasecmp(tz_offsets[i].name, tz)) {
					offset = tz_offsets[i].offset;
					break;
				}
			}
			g_free(tz);
		}
		/* some broken mailers seem to put in things like GMT+1030 instead of just +1030 */
		header_decode_lwsp(&inptr);
		if (*inptr == '+' || *inptr == '-') {
			int sign = (*inptr++)=='-'?-1:1;
			offset = offset + (header_decode_int(&inptr)*sign);
		}
		d(printf("named offset = %d\n", offset));
	}

	t = mktime(&tm);
#if defined(HAVE_TIMEZONE)
	t -= timezone;
#elif defined(HAVE_TM_GMTOFF)
	t += tm.tm_gmtoff;
#else
#error Neither HAVE_TIMEZONE nor HAVE_TM_GMTOFF defined. Rerun autoheader, autoconf, etc.
#endif

	/* t is now GMT of the time we want, but not offset by the timezone ... */

	d(printf(" gmt normalized? = %s\n", ctime(&t)));

	/* this should convert the time to the GMT equiv time */
	t -= ( (offset/100) * 60*60) + (offset % 100)*60;

	d(printf(" gmt normalized for timezone? = %s\n", ctime(&t)));

	d({
		char *tmp;
		tmp = header_format_date(t, offset);
		printf(" encoded again: %s\n", tmp);
		g_free(tmp);
	});

	if (saveoffset)
		*saveoffset = offset;

	return t;
}

/* extra rfc checks */
#define CHECKS

#ifdef CHECKS
static void
check_header(struct _header_raw *h)
{
	unsigned char *p;

	p = h->value;
	while (p && *p) {
		if (!isascii(*p)) {
			w(g_warning("Appending header violates rfc: %s: %s", h->name, h->value));
			return;
		}
		p++;
	}
}
#endif

void
header_raw_append_parse(struct _header_raw **list, const char *header, int offset)
{
	register const char *in;
	int fieldlen;
	char *name;

	in = header;
	while (is_fieldname(*in))
		in++;
	fieldlen = in-header;
	while (is_lwsp(*in))
		in++;
	if (fieldlen == 0 || *in != ':') {
		printf("Invalid header line: '%s'\n", header);
		return;
	}
	in++;
	name = alloca(fieldlen+1);
	memcpy(name, header, fieldlen);
	name[fieldlen] = 0;

	header_raw_append(list, name, in, offset);
}

void
header_raw_append(struct _header_raw **list, const char *name, const char *value, int offset)
{
	struct _header_raw *l, *n;

	d(printf("Header: %s: %s\n", name, value));

	n = g_malloc(sizeof(*n));
	n->next = NULL;
	n->name = g_strdup(name);
	n->value = g_strdup(value);
	n->offset = offset;
#ifdef CHECKS
	check_header(n);
#endif
	l = (struct _header_raw *)list;
	while (l->next) {
		l = l->next;
	}
	l->next = n;

	/* debug */
#if 0
	if (!strcasecmp(name, "To")) {
		printf("- Decoding To\n");
		header_to_decode(value);
	} else if (!strcasecmp(name, "Content-type")) {
		printf("- Decoding content-type\n");
		header_content_type_dump(header_content_type_decode(value));		
	} else if (!strcasecmp(name, "MIME-Version")) {
		printf("- Decoding mime version\n");
		header_mime_decode(value);
	}
#endif
}

static struct _header_raw *
header_raw_find_node(struct _header_raw **list, const char *name)
{
	struct _header_raw *l;

	l = *list;
	while (l) {
		if (!strcasecmp(l->name, name))
			break;
		l = l->next;
	}
	return l;
}

const char *
header_raw_find(struct _header_raw **list, const char *name, int *offset)
{
	struct _header_raw *l;

	l = header_raw_find_node(list, name);
	if (l) {
		if (offset)
			*offset = l->offset;
		return l->value;
	} else
		return NULL;
}

const char *
header_raw_find_next(struct _header_raw **list, const char *name, int *offset, const char *last)
{
	struct _header_raw *l;

	if (last == NULL || name == NULL)
		return NULL;

	l = *list;
	while (l && l->value != last)
		l = l->next;
	return header_raw_find(&l, name, offset);
}

static void
header_raw_free(struct _header_raw *l)
{
	g_free(l->name);
	g_free(l->value);
	g_free(l);
}

void
header_raw_remove(struct _header_raw **list, const char *name)
{
	struct _header_raw *l, *p;

	/* the next pointer is at the head of the structure, so this is safe */
	p = (struct _header_raw *)list;
	l = *list;
	while (l) {
		if (!strcasecmp(l->name, name)) {
			p->next = l->next;
			header_raw_free(l);
			l = p->next;
		} else {
			p = l;
			l = l->next;
		}
	}
}

void
header_raw_replace(struct _header_raw **list, const char *name, const char *value, int offset)
{
	header_raw_remove(list, name);
	header_raw_append(list, name, value, offset);
}

void
header_raw_clear(struct _header_raw **list)
{
	struct _header_raw *l, *n;
	l = *list;
	while (l) {
		n = l->next;
		header_raw_free(l);
		l = n;
	}
	*list = NULL;
}


/* ok, here's the address stuff, what a mess ... */
struct _header_address *header_address_new(void)
{
	struct _header_address *h;
	h = g_malloc0(sizeof(*h));
	h->type = HEADER_ADDRESS_NONE;
	h->refcount = 1;
	return h;
}

struct _header_address *header_address_new_name(const char *name, const char *addr)
{
	struct _header_address *h;

	h = header_address_new();
	h->type = HEADER_ADDRESS_NAME;
	h->name = g_strdup(name);
	h->v.addr = g_strdup(addr);
	return h;
}

struct _header_address *header_address_new_group(const char *name)
{
	struct _header_address *h;

	h = header_address_new();
	h->type = HEADER_ADDRESS_GROUP;
	h->name = g_strdup(name);
	return h;
}

void header_address_ref(struct _header_address *h)
{
	if (h)
		h->refcount++;
}

void header_address_unref(struct _header_address *h)
{
	if (h) {
		if (h->refcount <= 1) {
			if (h->type == HEADER_ADDRESS_GROUP) {
				header_address_list_clear(&h->v.members);
			} else if (h->type == HEADER_ADDRESS_NAME) {
				g_free(h->v.addr);
			}
			g_free(h->name);
			g_free(h);
		} else {
			h->refcount--;
		}
	}
}

void header_address_set_name(struct _header_address *h, const char *name)
{
	if (h) {
		g_free(h->name);
		h->name = g_strdup(name);
	}
}

void header_address_set_addr(struct _header_address *h, const char *addr)
{
	if (h) {
		if (h->type == HEADER_ADDRESS_NAME
		    || h->type == HEADER_ADDRESS_NONE) {
			h->type = HEADER_ADDRESS_NAME;
			g_free(h->v.addr);
			h->v.addr = g_strdup(addr);
		} else {
			g_warning("Trying to set the address on a group");
		}
	}
}

void header_address_set_members(struct _header_address *h, struct _header_address *group)
{
	if (h) {
		if (h->type == HEADER_ADDRESS_GROUP
		    || h->type == HEADER_ADDRESS_NONE) {
			h->type = HEADER_ADDRESS_GROUP;
			header_address_list_clear(&h->v.members);
			/* should this ref them? */
			h->v.members = group;
		} else {
			g_warning("Trying to set the members on a name, not group");
		}
	}
}

void header_address_add_member(struct _header_address *h, struct _header_address *member)
{
	if (h) {
		if (h->type == HEADER_ADDRESS_GROUP
		    || h->type == HEADER_ADDRESS_NONE) {
			h->type = HEADER_ADDRESS_GROUP;
			header_address_list_append(&h->v.members, member);
		}		    
	}
}

void header_address_list_append_list(struct _header_address **l, struct _header_address **h)
{
	if (l) {
		struct _header_address *n = (struct _header_address *)l;

		while (n->next)
			n = n->next;
		n->next = *h;
	}
}


void header_address_list_append(struct _header_address **l, struct _header_address *h)
{
	if (h) {
		header_address_list_append_list(l, &h);
		h->next = NULL;
	}
}

void header_address_list_clear(struct _header_address **l)
{
	struct _header_address *a, *n;
	a = *l;
	while (a) {
		n = a->next;
		header_address_unref(a);
		a = n;
	}
	*l = NULL;
}

static void
header_address_list_format_append(GString *out, struct _header_address *a)
{
	char *text;

	while (a) {
		switch (a->type) {
		case HEADER_ADDRESS_NAME:
#warning needs to rfc2047 encode address phrase
			/* FIXME: 2047 encoding?? */
			if (a->name && *a->name)
				g_string_sprintfa(out, "\"%s\" <%s>", a->name, a->v.addr);
			else
				g_string_append(out, a->v.addr);
			break;
		case HEADER_ADDRESS_GROUP:
			text = header_encode_string(a->name);
			g_string_sprintfa(out, "%s:\n ", text);
			header_address_list_format_append(out, a->v.members);
			g_string_sprintfa(out, ";");
			break;
		default:
			g_warning("Invalid address type");
			break;
		}
		a = a->next;
	}
}

/* FIXME: need a 'display friendly' version, as well as a 'rfc friendly' version? */
char *
header_address_list_format(struct _header_address *a)
{
	GString *out;
	char *ret;

	if (a == NULL)
		return NULL;

	out = g_string_new("");

	header_address_list_format_append(out, a);
	ret = out->str;
	g_string_free(out, FALSE);
	return ret;
}

#ifdef BUILD_TABLE

/* for debugging tests */
/* should also have some regression tests somewhere */

void run_test(void)
{
	char *to = "gnome hacker dudes: license-discuss@opensource.org,
        \"Richard M. Stallman\" <rms@gnu.org>,
        Barry Chester <barry_che@antdiv.gov.au>,
        Michael Zucchi <zucchi.michael(this (is a nested) comment)@zedzone.mmc.com.au>,
        Miguel de Icaza <miguel@gnome.org>;,
	zucchi@zedzone.mmc.com.au, \"Foo bar\" <zed@zedzone>,
	<frob@frobzone>";

	header_to_decode(to);

	header_mime_decode("1.0");
	header_mime_decode("1.3 (produced by metasend V1.0)");
	header_mime_decode("(produced by metasend V1.0) 5.2");
	header_mime_decode("7(produced by metasend 1.0) . (produced by helix/send/1.0) 9 . 5");
	header_mime_decode("3.");
	header_mime_decode(".");
	header_mime_decode(".5");
	header_mime_decode("c.d");
	header_mime_decode("");

	header_msgid_decode(" <\"L3x2i1.0.Nm5.Xd-Wu\"@lists.redhat.com>");
	header_msgid_decode("<200001180446.PAA02065@beaker.htb.com.au>");

}

#endif /* BUILD_TABLE */
