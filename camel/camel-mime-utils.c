/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2003 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* dont touch this file without my permission - Michael */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>  /* for MAXHOSTNAMELEN */
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <regex.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 1024
#endif

#include <glib.h>
#include <libedataserver/e-iconv.h>
#include <libedataserver/e-time-utils.h>

#include "camel-mime-utils.h"
#include "camel-charset-map.h"
#include "camel-net-utils.h"
#include "camel-utf8.h"

#ifndef CLEAN_DATE
#include "broken-date-parser.h"
#endif

#if 0
int strdup_count = 0;
int malloc_count = 0;
int free_count = 0;

#define g_strdup(x) (strdup_count++, g_strdup(x))
#define g_malloc(x) (malloc_count++, g_malloc(x))
#define g_free(x) (free_count++, g_free(x))
#endif

/* for all non-essential warnings ... */
#define w(x)

#define d(x)
#define d2(x)

#define CAMEL_UUENCODE_CHAR(c)  ((c) ? (c) + ' ' : '`')
#define	CAMEL_UUDECODE_CHAR(c)	(((c) - ' ') & 077)

static char *base64_alphabet =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char tohex[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

unsigned short camel_mime_special_table[256];
static unsigned char camel_mime_base64_rank[256];

/* Used by table initialisation code for special characters */
#define CHARS_LWSP " \t\n\r"
#define CHARS_TSPECIAL "()<>@,;:\\\"/[]?="
#define CHARS_SPECIAL "()<>@,;:\\\".[]"
#define CHARS_CSPECIAL "()\\\r"	/* not in comments */
#define CHARS_DSPECIAL "[]\\\r \t"	/* not in domains */
#define CHARS_ESPECIAL "()<>@,;:\"/[]?.=_" /* list of characters that must be encoded.
					      encoded word in text specials: rfc 2047 5(1)*/
#define CHARS_PSPECIAL "!*+-/" /* list of additional characters that can be left unencoded.
				  encoded word in phrase specials: rfc 2047 5(3) */
#define CHARS_ATTRCHAR "*\'% "	/* extra non-included attribute-chars */

static void
header_remove_bits(unsigned short bit, unsigned char *vals)
{
	int i;

	for (i=0;vals[i];i++)
		camel_mime_special_table[vals[i]] &= ~ bit;
}

static void
header_init_bits(unsigned short bit, unsigned short bitcopy, int remove, unsigned char *vals)
{
	int i;
	int len = strlen(vals);

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

	for (i=0;i<256;i++) {
		camel_mime_special_table[i] = 0;
		if (i<32 || i==127)
			camel_mime_special_table[i] |= CAMEL_MIME_IS_CTRL;
		else if (i < 127)
			camel_mime_special_table[i] |= CAMEL_MIME_IS_ATTRCHAR;
		if ((i>=32 && i<=60) || (i>=62 && i<=126) || i==9)
			camel_mime_special_table[i] |= (CAMEL_MIME_IS_QPSAFE|CAMEL_MIME_IS_ESAFE);
		if ((i>='0' && i<='9') || (i>='a' && i<='z') || (i>='A' && i<= 'Z'))
			camel_mime_special_table[i] |= CAMEL_MIME_IS_PSAFE;
	}
	camel_mime_special_table[' '] |= CAMEL_MIME_IS_SPACE;
	header_init_bits(CAMEL_MIME_IS_LWSP, 0, 0, CHARS_LWSP);
	header_init_bits(CAMEL_MIME_IS_TSPECIAL, CAMEL_MIME_IS_CTRL, 0, CHARS_TSPECIAL);
	header_init_bits(CAMEL_MIME_IS_SPECIAL, 0, 0, CHARS_SPECIAL);
	header_init_bits(CAMEL_MIME_IS_DSPECIAL, 0, FALSE, CHARS_DSPECIAL);
	header_remove_bits(CAMEL_MIME_IS_ESAFE, CHARS_ESPECIAL);
	header_remove_bits(CAMEL_MIME_IS_ATTRCHAR, CHARS_TSPECIAL CHARS_ATTRCHAR);
	header_init_bits(CAMEL_MIME_IS_PSAFE, 0, 0, CHARS_PSPECIAL);
}

static void
base64_init(void)
{
	int i;

	memset(camel_mime_base64_rank, 0xff, sizeof(camel_mime_base64_rank));
	for (i=0;i<64;i++) {
		camel_mime_base64_rank[(unsigned int)base64_alphabet[i]] = i;
	}
	camel_mime_base64_rank['='] = 0;
}

/* call this when finished encoding everything, to
   flush off the last little bit */
size_t
camel_base64_encode_close(unsigned char *in, size_t inlen, gboolean break_lines, unsigned char *out, int *state, int *save)
{
	int c1, c2;
	unsigned char *outptr = out;

	if (inlen>0)
		outptr += camel_base64_encode_step(in, inlen, break_lines, outptr, state, save);

	c1 = ((unsigned char *)save)[1];
	c2 = ((unsigned char *)save)[2];
	
	d(printf("mode = %d\nc1 = %c\nc2 = %c\n",
		 (int)((char *)save)[0],
		 (int)((char *)save)[1],
		 (int)((char *)save)[2]));

	switch (((char *)save)[0]) {
	case 2:
		outptr[2] = base64_alphabet[ ( (c2 &0x0f) << 2 ) ];
		g_assert(outptr[2] != 0);
		goto skip;
	case 1:
		outptr[2] = '=';
	skip:
		outptr[0] = base64_alphabet[ c1 >> 2 ];
		outptr[1] = base64_alphabet[ c2 >> 4 | ( (c1&0x3) << 4 )];
		outptr[3] = '=';
		outptr += 4;
		break;
	}
	if (break_lines)
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
size_t
camel_base64_encode_step(unsigned char *in, size_t len, gboolean break_lines, unsigned char *out, int *state, int *save)
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
		
		/* yes, we jump into the loop, no i'm not going to change it, it's beautiful! */
		while (inptr < inend) {
			c1 = *inptr++;
		skip1:
			c2 = *inptr++;
		skip2:
			c3 = *inptr++;
			*outptr++ = base64_alphabet[ c1 >> 2 ];
			*outptr++ = base64_alphabet[ c2 >> 4 | ( (c1&0x3) << 4 ) ];
			*outptr++ = base64_alphabet[ ( (c2 &0x0f) << 2 ) | (c3 >> 6) ];
			*outptr++ = base64_alphabet[ c3 & 0x3f ];
			/* this is a bit ugly ... */
			if (break_lines && (++already)>=19) {
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
 * camel_base64_decode_step: decode a chunk of base64 encoded data
 * @in: input stream
 * @len: max length of data to decode
 * @out: output stream
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been decoded
 *
 * Decodes a chunk of base64 encoded data
 **/
size_t
camel_base64_decode_step(unsigned char *in, size_t len, unsigned char *out, int *state, unsigned int *save)
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
			if (*inptr == '=' && outptr>out)
				outptr--;
			i--;
		}
	}

	/* if i!= 0 then there is a truncation error! */
	return outptr-out;
}

char *
camel_base64_encode_simple (const char *data, size_t len)
{
	unsigned char *out;
	int state = 0, outlen;
	unsigned int save = 0;
	
	out = g_malloc (len * 4 / 3 + 5);
	outlen = camel_base64_encode_close ((unsigned char *)data, len, FALSE,
				      out, &state, &save);
	out[outlen] = '\0';
	return (char *)out;
}

size_t
camel_base64_decode_simple (char *data, size_t len)
{
	int state = 0;
	unsigned int save = 0;

	return camel_base64_decode_step ((unsigned char *)data, len,
				   (unsigned char *)data, &state, &save);
}

/**
 * camel_uuencode_close: uuencode a chunk of data
 * @in: input stream
 * @len: input stream length
 * @out: output stream
 * @uubuf: temporary buffer of 60 bytes
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been encoded
 *
 * Returns the number of bytes encoded. Call this when finished
 * encoding data with camel_uuencode_step to flush off the last little
 * bit.
 **/
size_t
camel_uuencode_close (unsigned char *in, size_t len, unsigned char *out, unsigned char *uubuf, int *state, guint32 *save)
{
	register unsigned char *outptr, *bufptr;
	register guint32 saved;
	int uulen, uufill, i;
	
	outptr = out;
	
	if (len > 0)
		outptr += camel_uuencode_step (in, len, out, uubuf, state, save);
	
	uufill = 0;
	
	saved = *save;
	i = *state & 0xff;
	uulen = (*state >> 8) & 0xff;
	
	bufptr = uubuf + ((uulen / 3) * 4);
	
	if (i > 0) {
		while (i < 3) {
			saved <<= 8 | 0;
			uufill++;
			i++;
		}
		
		if (i == 3) {
			/* convert 3 normal bytes into 4 uuencoded bytes */
			unsigned char b0, b1, b2;
			
			b0 = saved >> 16;
			b1 = saved >> 8 & 0xff;
			b2 = saved & 0xff;
			
			*bufptr++ = CAMEL_UUENCODE_CHAR ((b0 >> 2) & 0x3f);
			*bufptr++ = CAMEL_UUENCODE_CHAR (((b0 << 4) | ((b1 >> 4) & 0xf)) & 0x3f);
			*bufptr++ = CAMEL_UUENCODE_CHAR (((b1 << 2) | ((b2 >> 6) & 0x3)) & 0x3f);
			*bufptr++ = CAMEL_UUENCODE_CHAR (b2 & 0x3f);
			
			i = 0;
			saved = 0;
			uulen += 3;
		}
	}
	
	if (uulen > 0) {
		int cplen = ((uulen / 3) * 4);
		
		*outptr++ = CAMEL_UUENCODE_CHAR ((uulen - uufill) & 0xff);
		memcpy (outptr, uubuf, cplen);
		outptr += cplen;
		*outptr++ = '\n';
		uulen = 0;
	}
	
	*outptr++ = CAMEL_UUENCODE_CHAR (uulen & 0xff);
	*outptr++ = '\n';
	
	*save = 0;
	*state = 0;
	
	return outptr - out;
}


/**
 * camel_uuencode_step: uuencode a chunk of data
 * @in: input stream
 * @len: input stream length
 * @out: output stream
 * @uubuf: temporary buffer of 60 bytes
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been encoded
 *
 * Returns the number of bytes encoded. Performs an 'encode step',
 * only encodes blocks of 45 characters to the output at a time, saves
 * left-over state in @uubuf, @state and @save (initialize to 0 on first
 * invocation).
 **/
size_t
camel_uuencode_step (unsigned char *in, size_t len, unsigned char *out, unsigned char *uubuf, int *state, guint32 *save)
{
	register unsigned char *inptr, *outptr, *bufptr;
	unsigned char *inend;
	register guint32 saved;
	int uulen, i;
	
	saved = *save;
	i = *state & 0xff;
	uulen = (*state >> 8) & 0xff;
	
	inptr = in;
	inend = in + len;
	
	outptr = out;
	
	bufptr = uubuf + ((uulen / 3) * 4);
	
	while (inptr < inend) {
		while (uulen < 45 && inptr < inend) {
			while (i < 3 && inptr < inend) {
				saved = (saved << 8) | *inptr++;
				i++;
			}
			
			if (i == 3) {
				/* convert 3 normal bytes into 4 uuencoded bytes */
				unsigned char b0, b1, b2;
				
				b0 = saved >> 16;
				b1 = saved >> 8 & 0xff;
				b2 = saved & 0xff;
				
				*bufptr++ = CAMEL_UUENCODE_CHAR ((b0 >> 2) & 0x3f);
				*bufptr++ = CAMEL_UUENCODE_CHAR (((b0 << 4) | ((b1 >> 4) & 0xf)) & 0x3f);
				*bufptr++ = CAMEL_UUENCODE_CHAR (((b1 << 2) | ((b2 >> 6) & 0x3)) & 0x3f);
				*bufptr++ = CAMEL_UUENCODE_CHAR (b2 & 0x3f);
				
				i = 0;
				saved = 0;
				uulen += 3;
			}
		}
		
		if (uulen >= 45) {
			*outptr++ = CAMEL_UUENCODE_CHAR (uulen & 0xff);
			memcpy (outptr, uubuf, ((uulen / 3) * 4));
			outptr += ((uulen / 3) * 4);
			*outptr++ = '\n';
			uulen = 0;
			bufptr = uubuf;
		}
	}
	
	*save = saved;
	*state = ((uulen & 0xff) << 8) | (i & 0xff);
	
	return outptr - out;
}


/**
 * camel_uudecode_step: uudecode a chunk of data
 * @in: input stream
 * @inlen: max length of data to decode ( normally strlen(in) ??)
 * @out: output stream
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been decoded
 *
 * Returns the number of bytes decoded. Performs a 'decode step' on
 * a chunk of uuencoded data. Assumes the "begin <mode> <file name>"
 * line has been stripped off.
 **/
size_t
camel_uudecode_step (unsigned char *in, size_t len, unsigned char *out, int *state, guint32 *save)
{
	register unsigned char *inptr, *outptr;
	unsigned char *inend, ch;
	register guint32 saved;
	gboolean last_was_eoln;
	int uulen, i;
	
	if (*state & CAMEL_UUDECODE_STATE_END)
		return 0;
	
	saved = *save;
	i = *state & 0xff;
	uulen = (*state >> 8) & 0xff;
	if (uulen == 0)
		last_was_eoln = TRUE;
	else
		last_was_eoln = FALSE;
	
	inend = in + len;
	outptr = out;
	
	inptr = in;
	while (inptr < inend) {
		if (*inptr == '\n' || last_was_eoln) {
			if (last_was_eoln && *inptr != '\n') {
				uulen = CAMEL_UUDECODE_CHAR (*inptr);
				last_was_eoln = FALSE;
				if (uulen == 0) {
					*state |= CAMEL_UUDECODE_STATE_END;
					break;
				}
			} else {
				last_was_eoln = TRUE;
			}
			
			inptr++;
			continue;
		}
		
		ch = *inptr++;
		
		if (uulen > 0) {
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
				
				if (uulen >= 3) {
					*outptr++ = CAMEL_UUDECODE_CHAR (b0) << 2 | CAMEL_UUDECODE_CHAR (b1) >> 4;
					*outptr++ = CAMEL_UUDECODE_CHAR (b1) << 4 | CAMEL_UUDECODE_CHAR (b2) >> 2;
				        *outptr++ = CAMEL_UUDECODE_CHAR (b2) << 6 | CAMEL_UUDECODE_CHAR (b3);
				} else {
					if (uulen >= 1) {
						*outptr++ = CAMEL_UUDECODE_CHAR (b0) << 2 | CAMEL_UUDECODE_CHAR (b1) >> 4;
					}
					if (uulen >= 2) {
						*outptr++ = CAMEL_UUDECODE_CHAR (b1) << 4 | CAMEL_UUDECODE_CHAR (b2) >> 2;
					}
				}
				
				i = 0;
				saved = 0;
				uulen -= 3;
			}
		} else {
			break;
		}
	}
	
	*save = saved;
	*state = (*state & CAMEL_UUDECODE_STATE_MASK) | ((uulen & 0xff) << 8) | (i & 0xff);
	
	return outptr - out;
}


/* complete qp encoding */
size_t
camel_quoted_decode_close(unsigned char *in, size_t len, unsigned char *out, int *state, int *save)
{
	register unsigned char *outptr = out;
	int last;

	if (len>0)
		outptr += camel_quoted_encode_step(in, len, outptr, state, save);

	last = *state;
	if (last != -1) {
		/* space/tab must be encoded if it's the last character on
		   the line */
		if (camel_mime_is_qpsafe(last) && last!=' ' && last!=9) {
			*outptr++ = last;
		} else {
			*outptr++ = '=';
			*outptr++ = tohex[(last>>4) & 0xf];
			*outptr++ = tohex[last & 0xf];
		}
	}

	*save = 0;
	*state = -1;

	return outptr-out;
}

/* perform qp encoding, initialise state to -1 and save to 0 on first invocation */
size_t
camel_quoted_encode_step (unsigned char *in, size_t len, unsigned char *out, int *statep, int *save)
{
	register guchar *inptr, *outptr, *inend;
	unsigned char c;
	register int sofar = *save;  /* keeps track of how many chars on a line */
	register int last = *statep; /* keeps track if last char to end was a space cr etc */
	
	inptr = in;
	inend = in + len;
	outptr = out;
	while (inptr < inend) {
		c = *inptr++;
		if (c == '\r') {
			if (last != -1) {
				*outptr++ = '=';
				*outptr++ = tohex[(last >> 4) & 0xf];
				*outptr++ = tohex[last & 0xf];
				sofar += 3;
			}
			last = c;
		} else if (c == '\n') {
			if (last != -1 && last != '\r') {
				*outptr++ = '=';
				*outptr++ = tohex[(last >> 4) & 0xf];
				*outptr++ = tohex[last & 0xf];
			}
			*outptr++ = '\n';
			sofar = 0;
			last = -1;
		} else {
			if (last != -1) {
				if (camel_mime_is_qpsafe(last)) {
					*outptr++ = last;
					sofar++;
				} else {
					*outptr++ = '=';
					*outptr++ = tohex[(last >> 4) & 0xf];
					*outptr++ = tohex[last & 0xf];
					sofar += 3;
				}
			}
			
			if (camel_mime_is_qpsafe(c)) {
				if (sofar > 74) {
					*outptr++ = '=';
					*outptr++ = '\n';
					sofar = 0;
				}
				
				/* delay output of space char */
				if (c==' ' || c=='\t') {
					last = c;
				} else {
					*outptr++ = c;
					sofar++;
					last = -1;
				}
			} else {
				if (sofar > 72) {
					*outptr++ = '=';
					*outptr++ = '\n';
					sofar = 3;
				} else
					sofar += 3;
				
				*outptr++ = '=';
				*outptr++ = tohex[(c >> 4) & 0xf];
				*outptr++ = tohex[c & 0xf];
				last = -1;
			}
		}
	}
	*save = sofar;
	*statep = last;
	
	return (outptr - out);
}

/*
  FIXME: this does not strip trailing spaces from lines (as it should, rfc 2045, section 6.7)
  Should it also canonicalise the end of line to CR LF??

  Note: Trailing rubbish (at the end of input), like = or =x or =\r will be lost.
*/ 

size_t
camel_quoted_decode_step(unsigned char *in, size_t len, unsigned char *out, int *savestate, int *saveme)
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
  which is slightly different than plain quoted-printable (mainly by allowing 0x20 <> _)
*/
static size_t
quoted_decode(const unsigned char *in, size_t len, unsigned char *out)
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
	return 0;
}

/* rfc2047 version of quoted-printable */
/* safemask is the mask to apply to the camel_mime_special_table to determine what
   characters can safely be included without encoding */
static size_t
quoted_encode (const unsigned char *in, size_t len, unsigned char *out, unsigned short safemask)
{
	register const unsigned char *inptr, *inend;
	unsigned char *outptr;
	unsigned char c;

	inptr = in;
	inend = in + len;
	outptr = out;
	while (inptr < inend) {
		c = *inptr++;
		if (c==' ') {
			*outptr++ = '_';
		} else if (camel_mime_special_table[c] & safemask) {
			*outptr++ = c;
		} else {
			*outptr++ = '=';
			*outptr++ = tohex[(c >> 4) & 0xf];
			*outptr++ = tohex[c & 0xf];
		}
	}

	d(printf("encoding '%.*s' = '%.*s'\n", len, in, outptr-out, out));

	return (outptr - out);
}


static void
header_decode_lwsp(const char **in)
{
	const char *inptr = *in;
	char c;

	d2(printf("is ws: '%s'\n", *in));

	while (camel_mime_is_lwsp(*inptr) || (*inptr =='(' && *inptr != '\0')) {
		while (camel_mime_is_lwsp(*inptr) && inptr != '\0') {
			d2(printf("(%c)", *inptr));
			inptr++;
		}
		d2(printf("\n"));

		/* check for comments */
		if (*inptr == '(') {
			int depth = 1;
			inptr++;
			while (depth && (c=*inptr) && *inptr != '\0') {
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
rfc2047_decode_word(const char *in, size_t len)
{
	const char *inptr = in+2;
	const char *inend = in+len-2;
	const char *inbuf;
	const char *charset;
	char *encname, *p;
	int tmplen;
	size_t ret;
	char *decword = NULL;
	char *decoded = NULL;
	char *outbase = NULL;
	char *outbuf;
	size_t inlen, outlen;
	gboolean retried = FALSE;
	iconv_t ic;
	
	d(printf("rfc2047: decoding '%.*s'\n", len, in));

	/* quick check to see if this could possibly be a real encoded word */
	if (len < 8 || !(in[0] == '=' && in[1] == '?' && in[len-1] == '=' && in[len-2] == '?')) {
		d(printf("invalid\n"));
		return NULL;
	}
	
	/* skip past the charset to the encoding type */
	inptr = memchr (inptr, '?', inend-inptr);
	if (inptr != NULL && inptr < inend + 2 && inptr[2] == '?') {
		d(printf("found ?, encoding is '%c'\n", inptr[0]));
		inptr++;
		tmplen = inend-inptr-2;
		decword = g_alloca (tmplen); /* this will always be more-than-enough room */
		switch(toupper(inptr[0])) {
		case 'Q':
			inlen = quoted_decode(inptr+2, tmplen, decword);
			break;
		case 'B': {
			int state = 0;
			unsigned int save = 0;
			
			inlen = camel_base64_decode_step((char *)inptr+2, tmplen, decword, &state, &save);
			/* if state != 0 then error? */
			break;
		}
		default:
			/* uhhh, unknown encoding type - probably an invalid encoded word string */
			return NULL;
		}
		d(printf("The encoded length = %d\n", inlen));
		if (inlen > 0) {
			/* yuck, all this snot is to setup iconv! */
			tmplen = inptr - in - 3;
			encname = g_alloca (tmplen + 1);
			memcpy (encname, in + 2, tmplen);
			encname[tmplen] = '\0';
			
			/* rfc2231 updates rfc2047 encoded words...
			 * The ABNF given in RFC 2047 for encoded-words is:
			 *   encoded-word := "=?" charset "?" encoding "?" encoded-text "?="
			 * This specification changes this ABNF to:
			 *   encoded-word := "=?" charset ["*" language] "?" encoding "?" encoded-text "?="
			 */
			
			/* trim off the 'language' part if it's there... */
			p = strchr (encname, '*');
			if (p)
				*p = '\0';
			
			charset = e_iconv_charset_name (encname);
			
			inbuf = decword;
			
			outlen = inlen * 6 + 16;
			outbase = g_alloca (outlen);
			outbuf = outbase;
			
		retry:
			ic = e_iconv_open ("UTF-8", charset);
			if (ic != (iconv_t) -1) {
				ret = e_iconv (ic, &inbuf, &inlen, &outbuf, &outlen);
				if (ret != (size_t) -1) {
					e_iconv (ic, NULL, 0, &outbuf, &outlen);
					*outbuf = 0;
					decoded = g_strdup (outbase);
				}
				e_iconv_close (ic);
			} else {
				w(g_warning ("Cannot decode charset, header display may be corrupt: %s: %s",
					     charset, strerror (errno)));
				
				if (!retried) {
					charset = e_iconv_locale_charset ();
					if (!charset)
						charset = "iso-8859-1";
					
					retried = TRUE;
					goto retry;
				}
				
				/* we return the encoded word here because we've got to return valid utf8 */
				decoded = g_strndup (in, inlen);
			}
		}
	}
	
	d(printf("decoded '%s'\n", decoded));
	
	return decoded;
}

/* ok, a lot of mailers are BROKEN, and send iso-latin1 encoded
   headers, when they should just be sticking to US-ASCII
   according to the rfc's.  Anyway, since the conversion to utf-8
   is trivial, just do it here without iconv */
static GString *
append_latin1 (GString *out, const char *in, size_t len)
{
	unsigned int c;
	
	while (len) {
		c = (unsigned int)*in++;
		len--;
		if (c & 0x80) {
			out = g_string_append_c (out, 0xc0 | ((c >> 6) & 0x3));  /* 110000xx */
			out = g_string_append_c (out, 0x80 | (c & 0x3f));        /* 10xxxxxx */
		} else {
			out = g_string_append_c (out, c);
		}
	}
	return out;
}

static int
append_8bit (GString *out, const char *inbuf, size_t inlen, const char *charset)
{
	char *outbase, *outbuf;
	size_t outlen;
	iconv_t ic;
	
	ic = e_iconv_open ("UTF-8", charset);
	if (ic == (iconv_t) -1)
		return FALSE;

	outlen = inlen * 6 + 16;
	outbuf = outbase = g_malloc(outlen);
	
	if (e_iconv (ic, &inbuf, &inlen, &outbuf, &outlen) == (size_t) -1) {
		w(g_warning("Conversion to '%s' failed: %s", charset, strerror (errno)));
		g_free(outbase);
		e_iconv_close (ic);
		return FALSE;
	}
	
	e_iconv (ic, NULL, NULL, &outbuf, &outlen);
	
	*outbuf = 0;
	g_string_append(out, outbase);
	g_free(outbase);
	e_iconv_close (ic);

	return TRUE;
	
}

static void
append_quoted_pair (GString *str, const char *in, int inlen)
{
	register const char *inptr = in;
	const char *inend = in + inlen;
	char c;
	
	while (inptr < inend) {
		c = *inptr++;
		if (c == '\\' && inptr < inend)
			g_string_append_c (str, *inptr++);
		else
			g_string_append_c (str, c);
	}
}

/* decodes a simple text, rfc822 + rfc2047 */
static char *
header_decode_text (const char *in, size_t inlen, int ctext, const char *default_charset)
{
	GString *out;
	const char *inptr, *inend, *start, *chunk, *locale_charset;
	void (* append) (GString *, const char *, int);
	char *dword = NULL;
	guint32 mask;
	
	locale_charset = e_iconv_locale_charset ();
	
	if (ctext) {
		mask = (CAMEL_MIME_IS_SPECIAL | CAMEL_MIME_IS_SPACE | CAMEL_MIME_IS_CTRL);
		append = append_quoted_pair;
	} else {
		mask = (CAMEL_MIME_IS_LWSP);
		append = g_string_append_len;
	}
	
	out = g_string_new ("");
	inptr = in;
	inend = inptr + inlen;
	chunk = NULL;

	while (inptr < inend) {
		start = inptr;
		while (inptr < inend && camel_mime_is_type (*inptr, mask))
			inptr++;

		if (inptr == inend) {
			append (out, start, inptr - start);
			break;
		} else if (dword == NULL) {
			append (out, start, inptr - start);
		} else {
			chunk = start;
		}

		start = inptr;
		while (inptr < inend && !camel_mime_is_type (*inptr, mask))
			inptr++;

		dword = rfc2047_decode_word(start, inptr-start);
		if (dword) {
			g_string_append(out, dword);
			g_free(dword);
		} else {
			if (!chunk)
				chunk = start;
			
			if ((default_charset == NULL || !append_8bit (out, chunk, inptr-chunk, default_charset))
			    && (locale_charset == NULL || !append_8bit(out, chunk, inptr-chunk, locale_charset)))
				append_latin1(out, chunk, inptr-chunk);
		}
		
		chunk = NULL;
	}

	dword = out->str;
	g_string_free (out, FALSE);
	
	return dword;
}

char *
camel_header_decode_string (const char *in, const char *default_charset)
{
	if (in == NULL)
		return NULL;
	return header_decode_text (in, strlen (in), FALSE, default_charset);
}

char *
camel_header_format_ctext (const char *in, const char *default_charset)
{
	if (in == NULL)
		return NULL;
	return header_decode_text (in, strlen (in), TRUE, default_charset);
}

/* how long a sequence of pre-encoded words should be less than, to attempt to 
   fit into a properly folded word.  Only a guide. */
#define CAMEL_FOLD_PREENCODED (24)

/* FIXME: needs a way to cache iconv opens for different charsets? */
static void
rfc2047_encode_word(GString *outstring, const char *in, size_t len, const char *type, unsigned short safemask)
{
	iconv_t ic = (iconv_t) -1;
	char *buffer, *out, *ascii;
	size_t inlen, outlen, enclen, bufflen;
	const char *inptr, *p;
	int first = 1;

	d(printf("Converting [%d] '%.*s' to %s\n", len, len, in, type));

	/* convert utf8->encoding */
	bufflen = len * 6 + 16;
	buffer = g_alloca (bufflen);
	inlen = len;
	inptr = in;
	
	ascii = g_alloca (bufflen);
	
	if (strcasecmp (type, "UTF-8") != 0)
		ic = e_iconv_open (type, "UTF-8");
	
	while (inlen) {
		size_t convlen, proclen;
		int i;
		
		/* break up words into smaller bits, what we really want is encoded + overhead < 75,
		   but we'll just guess what that means in terms of input chars, and assume its good enough */

		out = buffer;
		outlen = bufflen;

		if (ic == (iconv_t) -1) {
			/* native encoding case, the easy one (?) */
			/* we work out how much we can convert, and still be in length */
			/* proclen will be the result of input characters that we can convert, to the nearest
			   (approximated) valid utf8 char */
			convlen = 0;
			proclen = 0;
			p = inptr;
			i = 0;
			while (p < (in+len) && convlen < (75 - strlen("=?utf-8?q\?\?="))) {
				unsigned char c = *p++;

				if (c >= 0xc0)
					proclen = i;
				i++;
				if (c < 0x80)
					proclen = i;
				if (camel_mime_special_table[c] & safemask)
					convlen += 1;
				else
					convlen += 3;
			}
			/* well, we probably have broken utf8, just copy it anyway what the heck */
			if (proclen == 0) {
				w(g_warning("Appear to have truncated utf8 sequence"));
				proclen = inlen;
			}
			memcpy(out, inptr, proclen);
			inptr += proclen;
			inlen -= proclen;
			out += proclen;
		} else {
			/* well we could do similar, but we can't (without undue effort), we'll just break it up into
			   hopefully-small-enough chunks, and leave it at that */
			convlen = MIN(inlen, CAMEL_FOLD_PREENCODED);
			p = inptr;
			if (e_iconv (ic, &inptr, &convlen, &out, &outlen) == (size_t) -1 && errno != EINVAL) {
				w(g_warning("Conversion problem: conversion truncated: %s", strerror (errno)));
				/* blah, we include it anyway, better than infinite loop ... */
				inptr = p + convlen;
			} else {
				/* make sure we flush out any shift state */
				e_iconv (ic, NULL, 0, &out, &outlen);
			}
			inlen -= (inptr - p);
		}
		
		enclen = out-buffer;
		
		if (enclen) {
			/* create token */
			out = ascii;
			if (first)
				first = 0;
			else
				*out++ = ' ';
			out += sprintf (out, "=?%s?Q?", type);
			out += quoted_encode (buffer, enclen, out, safemask);
			sprintf (out, "?=");
			
			d(printf("converted part = %s\n", ascii));
			
			g_string_append (outstring, ascii);
		}
	}
	
	if (ic != (iconv_t) -1)
		e_iconv_close (ic);
}


/* TODO: Should this worry about quotes?? */
char *
camel_header_encode_string (const unsigned char *in)
{
	const unsigned char *inptr = in, *start, *word;
	gboolean last_was_encoded = FALSE;
	gboolean last_was_space = FALSE;
	int encoding;
	GString *out;
	char *outstr;

	g_return_val_if_fail (g_utf8_validate (in, -1, NULL), NULL);
	
	if (in == NULL)
		return NULL;
	
	/* do a quick us-ascii check (the common case?) */
	while (*inptr) {
		if (*inptr > 127)
			break;
		inptr++;
	}
	if (*inptr == '\0')
		return g_strdup (in);
	
	/* This gets each word out of the input, and checks to see what charset
	   can be used to encode it. */
	/* TODO: Work out when to merge subsequent words, or across word-parts */
	out = g_string_new ("");
	inptr = in;
	encoding = 0;
	word = NULL;
	start = inptr;
	while (inptr && *inptr) {
		gunichar c;
		const char *newinptr;
		
		newinptr = g_utf8_next_char (inptr);
		c = g_utf8_get_char (inptr);
		if (newinptr == NULL || !g_unichar_validate (c)) {
			w(g_warning ("Invalid UTF-8 sequence encountered (pos %d, char '%c'): %s",
				     (inptr-in), inptr[0], in));
			inptr++;
			continue;
		}
		
		if (c < 256 && camel_mime_is_lwsp (c) && !last_was_space) {
			/* we've reached the end of a 'word' */
			if (word && !(last_was_encoded && encoding)) {
				/* output lwsp between non-encoded words */
				g_string_append_len (out, start, word - start);
				start = word;
			}
			
			switch (encoding) {
			case 0:
				g_string_append_len (out, start, inptr - start);
				last_was_encoded = FALSE;
				break;
			case 1:
				if (last_was_encoded)
					g_string_append_c (out, ' ');
				
				rfc2047_encode_word (out, start, inptr - start, "ISO-8859-1", CAMEL_MIME_IS_ESAFE);
				last_was_encoded = TRUE;
				break;
			case 2:
				if (last_was_encoded)
					g_string_append_c (out, ' ');
				
				rfc2047_encode_word (out, start, inptr - start,
						     camel_charset_best (start, inptr - start), CAMEL_MIME_IS_ESAFE);
				last_was_encoded = TRUE;
				break;
			}
			
			last_was_space = TRUE;
			start = inptr;
			word = NULL;
			encoding = 0;
		} else if (c > 127 && c < 256) {
			encoding = MAX (encoding, 1);
			last_was_space = FALSE;
		} else if (c >= 256) {
			encoding = MAX (encoding, 2);
			last_was_space = FALSE;
		} else if (!camel_mime_is_lwsp (c)) {
			last_was_space = FALSE;
		}
		
		if (!(c < 256 && camel_mime_is_lwsp (c)) && !word)
			word = inptr;
		
		inptr = newinptr;
	}
	
	if (inptr - start) {
		if (word && !(last_was_encoded && encoding)) {
			g_string_append_len (out, start, word - start);
			start = word;
		}
		
		switch (encoding) {
		case 0:
			g_string_append_len (out, start, inptr - start);
			break;
		case 1:
			if (last_was_encoded)
				g_string_append_c (out, ' ');
			
			rfc2047_encode_word (out, start, inptr - start, "ISO-8859-1", CAMEL_MIME_IS_ESAFE);
			break;
		case 2:
			if (last_was_encoded)
				g_string_append_c (out, ' ');
			
			rfc2047_encode_word (out, start, inptr - start,
					     camel_charset_best (start, inptr - start - 1), CAMEL_MIME_IS_ESAFE);
			break;
		}
	}
	
	outstr = out->str;
	g_string_free (out, FALSE);
	
	return outstr;
}

/* apply quoted-string rules to a string */
static void
quote_word(GString *out, gboolean do_quotes, const char *start, size_t len)
{
	int i, c;

	/* TODO: What about folding on long lines? */
	if (do_quotes)
		g_string_append_c(out, '"');
	for (i=0;i<len;i++) {
		c = *start++;
		if (c == '\"' || c=='\\' || c=='\r')
			g_string_append_c(out, '\\');
		g_string_append_c(out, c);
	}
	if (do_quotes)
		g_string_append_c(out, '"');
}

/* incrementing possibility for the word type */
enum _phrase_word_t {
	WORD_ATOM,
	WORD_QSTRING,
	WORD_2047
};

struct _phrase_word {
	const unsigned char *start, *end;
	enum _phrase_word_t type;
	int encoding;
};

static gboolean
word_types_compatable (enum _phrase_word_t type1, enum _phrase_word_t type2)
{
	switch (type1) {
	case WORD_ATOM:
		return type2 == WORD_QSTRING;
	case WORD_QSTRING:
		return type2 != WORD_2047;
	case WORD_2047:
		return type2 == WORD_2047;
	default:
		return FALSE;
	}
}

/* split the input into words with info about each word
 * merge common word types clean up */
static GList *
header_encode_phrase_get_words (const unsigned char *in)
{
	const unsigned char *inptr = in, *start, *last;
	struct _phrase_word *word;
	enum _phrase_word_t type;
	int encoding, count = 0;
	GList *words = NULL;
	
	/* break the input into words */
	type = WORD_ATOM;
	last = inptr;
	start = inptr;
	encoding = 0;
	while (inptr && *inptr) {
		gunichar c;
		const char *newinptr;
		
		newinptr = g_utf8_next_char (inptr);
		c = g_utf8_get_char (inptr);
		
		if (!g_unichar_validate (c)) {
			w(g_warning ("Invalid UTF-8 sequence encountered (pos %d, char '%c'): %s",
				     (inptr - in), inptr[0], in));
			inptr++;
			continue;
		}
		
		inptr = newinptr;
		if (g_unichar_isspace (c)) {
			if (count > 0) {
				word = g_new0 (struct _phrase_word, 1);
				word->start = start;
				word->end = last;
				word->type = type;
				word->encoding = encoding;
				words = g_list_append (words, word);
				count = 0;
			}
			
			start = inptr;
			type = WORD_ATOM;
			encoding = 0;
		} else {
			count++;
			if (c < 128) {
				if (!camel_mime_is_atom (c))
					type = MAX (type, WORD_QSTRING);
			} else if (c > 127 && c < 256) {
				type = WORD_2047;
				encoding = MAX (encoding, 1);
			} else if (c >= 256) {
				type = WORD_2047;
				encoding = MAX (encoding, 2);
			}
		}
		
		last = inptr;
	}
	
	if (count > 0) {
		word = g_new0 (struct _phrase_word, 1);
		word->start = start;
		word->end = last;
		word->type = type;
		word->encoding = encoding;
		words = g_list_append (words, word);
	}
	
	return words;
}

#define MERGED_WORD_LT_FOLDLEN(wordlen, type) ((type) == WORD_2047 ? (wordlen) < CAMEL_FOLD_PREENCODED : (wordlen) < (CAMEL_FOLD_SIZE - 8))

static gboolean
header_encode_phrase_merge_words (GList **wordsp)
{
	GList *wordl, *nextl, *words = *wordsp;
	struct _phrase_word *word, *next;
	gboolean merged = FALSE;
	
	/* scan the list, checking for words of similar types that can be merged */
	wordl = words;
	while (wordl) {
		word = wordl->data;
		nextl = g_list_next (wordl);
		
		while (nextl) {
			next = nextl->data;
			/* merge nodes of the same type AND we are not creating too long a string */
			if (word_types_compatable (word->type, next->type)) {
				if (MERGED_WORD_LT_FOLDLEN (next->end - word->start, MAX (word->type, next->type))) {
					/* the resulting word type is the MAX of the 2 types */
					word->type = MAX(word->type, next->type);
					
					word->end = next->end;
					words = g_list_remove_link (words, nextl);
					g_list_free_1 (nextl);
					g_free (next);
					
					nextl = g_list_next (wordl);
					
					merged = TRUE;
				} else {
					/* if it is going to be too long, make sure we include the
					   separating whitespace */
					word->end = next->start;
					break;
				}
			} else {
				break;
			}
		}
		
		wordl = g_list_next (wordl);
	}
	
	*wordsp = words;
	
	return merged;
}

/* encodes a phrase sequence (different quoting/encoding rules to strings) */
char *
camel_header_encode_phrase (const unsigned char *in)
{
	struct _phrase_word *word = NULL, *last_word = NULL;
	GList *words, *wordl;
	GString *out;
	char *outstr;
	
	if (in == NULL)
		return NULL;
	
	words = header_encode_phrase_get_words (in);
	if (!words)
		return NULL;
	
	while (header_encode_phrase_merge_words (&words))
		;
	
	out = g_string_new ("");
	
	/* output words now with spaces between them */
	wordl = words;
	while (wordl) {
		const char *start;
		size_t len;
		
		word = wordl->data;
		
		/* append correct number of spaces between words */
		if (last_word && !(last_word->type == WORD_2047 && word->type == WORD_2047)) {
			/* one or both of the words are not encoded so we write the spaces out untouched */
			len = word->start - last_word->end;
			out = g_string_append_len (out, last_word->end, len);
		}
		
		switch (word->type) {
		case WORD_ATOM:
			out = g_string_append_len (out, word->start, word->end - word->start);
			break;
		case WORD_QSTRING:
			quote_word (out, TRUE, word->start, word->end - word->start);
			break;
		case WORD_2047:
			if (last_word && last_word->type == WORD_2047) {
				/* include the whitespace chars between these 2 words in the
                                   resulting rfc2047 encoded word. */
				len = word->end - last_word->end;
				start = last_word->end;
				
				/* encoded words need to be separated by linear whitespace */
				g_string_append_c (out, ' ');
			} else {
				len = word->end - word->start;
				start = word->start;
			}
			
			if (word->encoding == 1)
				rfc2047_encode_word (out, start, len, "ISO-8859-1", CAMEL_MIME_IS_PSAFE);
			else
				rfc2047_encode_word (out, start, len,
						     camel_charset_best (start, len), CAMEL_MIME_IS_PSAFE);
			break;
		}
		
		g_free (last_word);
		wordl = g_list_next (wordl);
		
		last_word = word;
	}
	
	/* and we no longer need the list */
	g_free (word);
	g_list_free (words);
	
	outstr = out->str;
	g_string_free (out, FALSE);
	
	return outstr;
}


/* these are all internal parser functions */

static char *
decode_token (const char **in)
{
	const char *inptr = *in;
	const char *start;
	
	header_decode_lwsp (&inptr);
	start = inptr;
	while (camel_mime_is_ttoken (*inptr))
		inptr++;
	if (inptr > start) {
		*in = inptr;
		return g_strndup (start, inptr - start);
	} else {
		return NULL;
	}
}

char *
camel_header_token_decode(const char *in)
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
	size_t outlen;
	int c;

	header_decode_lwsp(&inptr);
	if (*inptr == '"') {
		const char *intmp;
		int skip = 0;

		/* first, calc length */
		inptr++;
		intmp = inptr;
		while ( (c = *intmp++) && c!= '"') {
			if (c=='\\' && *intmp) {
				intmp++;
				skip++;
			}
		}
		outlen = intmp-inptr-skip;
		out = outptr = g_malloc(outlen+1);
		while ( (c = *inptr++) && c!= '"') {
			if (c=='\\' && *inptr) {
				c = *inptr++;
			}
			*outptr++ = c;
		}
		*outptr = '\0';
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
	while (camel_mime_is_atom(*inptr))
		inptr++;
	*in = inptr;
	if (inptr > start)
		return g_strndup(start, inptr-start);
	else
		return NULL;
}

static char *
header_decode_word (const char **in)
{
	const char *inptr = *in;
	
	header_decode_lwsp (&inptr);
	if (*inptr == '"') {
		*in = inptr;
		return header_decode_quoted_string (in);
	} else {
		*in = inptr;
		return header_decode_atom (in);
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
	} else if (camel_mime_is_ttoken(*inptr)) {
		d(printf("decoding token\n"));
		/* this may not have the right specials for all params? */
		return decode_token(in);
	}
	return NULL;
}

/* should this return -1 for no int? */
int
camel_header_decode_int(const char **in)
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

#define HEXVAL(c) (isdigit (c) ? (c) - '0' : tolower (c) - 'a' + 10)

static char *
hex_decode (const char *in, size_t len)
{
	const unsigned char *inend = in + len;
	unsigned char *inptr, *outptr;
	char *outbuf;
	
	outptr = outbuf = g_malloc (len + 1);
	
	inptr = (unsigned char *) in;
	while (inptr < inend) {
		if (*inptr == '%') {
			if (isxdigit (inptr[1]) && isxdigit (inptr[2])) {
				*outptr++ = HEXVAL (inptr[1]) * 16 + HEXVAL (inptr[2]);
				inptr += 3;
			} else
				*outptr++ = *inptr++;
		} else
			*outptr++ = *inptr++;
	}
	
	*outptr = '\0';
	
	return outbuf;
}

/* Tries to convert @in @from charset @to charset.  Any failure, we get no data out rather than partial conversion */
static char *
header_convert(const char *to, const char *from, const char *in, size_t inlen)
{
	iconv_t ic;
	size_t outlen, ret;
	char *outbuf, *outbase, *result = NULL;

	ic = e_iconv_open(to, from);
	if (ic == (iconv_t) -1)
		return NULL;

	outlen = inlen * 6 + 16;
	outbuf = outbase = g_malloc(outlen);
			
	ret = e_iconv(ic, &in, &inlen, &outbuf, &outlen);
	if (ret != (size_t) -1) {
		e_iconv(ic, NULL, 0, &outbuf, &outlen);
		*outbuf = '\0';
		result = g_strdup(outbase);
	}
	e_iconv_close(ic);
	g_free(outbase);

	return result;
}

/* an rfc2184 encoded string looks something like:
 * us-ascii'en'This%20is%20even%20more%20
 */

static char *
rfc2184_decode (const char *in, size_t len)
{
	const char *inptr = in;
	const char *inend = in + len;
	const char *charset;
	char *decoded, *decword, *encoding;
	
	inptr = memchr (inptr, '\'', len);
	if (!inptr)
		return NULL;

	encoding = g_alloca(inptr-in+1);
	memcpy(encoding, in, inptr-in);
	encoding[inptr-in] = 0;
	charset = e_iconv_charset_name (encoding);
	
	inptr = memchr (inptr + 1, '\'', inend - inptr - 1);
	if (!inptr)
		return NULL;
	inptr++;
	if (inptr >= inend)
		return NULL;

	decword = hex_decode (inptr, inend - inptr);
	decoded = header_convert("UTF-8", charset, decword, strlen(decword));
	g_free(decword);

	return decoded;
}

char *
camel_header_param (struct _camel_header_param *p, const char *name)
{
	while (p && strcasecmp (p->name, name) != 0)
		p = p->next;
	if (p)
		return p->value;
	return NULL;
}

struct _camel_header_param *
camel_header_set_param (struct _camel_header_param **l, const char *name, const char *value)
{
	struct _camel_header_param *p = (struct _camel_header_param *)l, *pn;
	
	if (name == NULL)
		return NULL;
	
	while (p->next) {
		pn = p->next;
		if (!strcasecmp (pn->name, name)) {
			g_free (pn->value);
			if (value) {
				pn->value = g_strdup (value);
				return pn;
			} else {
				p->next = pn->next;
				g_free (pn->name);
				g_free (pn);
				return NULL;
			}
		}
		p = pn;
	}

	if (value == NULL)
		return NULL;

	pn = g_malloc (sizeof (*pn));
	pn->next = 0;
	pn->name = g_strdup (name);
	pn->value = g_strdup (value);
	p->next = pn;

	return pn;
}

const char *
camel_content_type_param (CamelContentType *t, const char *name)
{
	if (t==NULL)
		return NULL;
	return camel_header_param (t->params, name);
}

void
camel_content_type_set_param (CamelContentType *t, const char *name, const char *value)
{
	camel_header_set_param (&t->params, name, value);
}

/**
 * camel_content_type_is:
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
camel_content_type_is(CamelContentType *ct, const char *type, const char *subtype)
{
	/* no type == text/plain or text/"*" */
	if (ct==NULL || (ct->type == NULL && ct->subtype == NULL)) {
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
camel_header_param_list_free(struct _camel_header_param *p)
{
	struct _camel_header_param *n;

	while (p) {
		n = p->next;
		g_free(p->name);
		g_free(p->value);
		g_free(p);
		p = n;
	}
}

CamelContentType *
camel_content_type_new(const char *type, const char *subtype)
{
	CamelContentType *t = g_malloc(sizeof(*t));

	t->type = g_strdup(type);
	t->subtype = g_strdup(subtype);
	t->params = NULL;
	t->refcount = 1;
	return t;
}

void
camel_content_type_ref(CamelContentType *ct)
{
	if (ct)
		ct->refcount++;
}


void
camel_content_type_unref(CamelContentType *ct)
{
	if (ct) {
		if (ct->refcount <= 1) {
			camel_header_param_list_free(ct->params);
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
			domain = g_string_append_c(domain, '[');
			inptr++;
			header_decode_lwsp(&inptr);
			start = inptr;
			while (camel_mime_is_dtext(*inptr)) {
				domain = g_string_append_c(domain, *inptr);
				inptr++;
			}
			if (*inptr == ']') {
				domain = g_string_append_c(domain, ']');
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
	word = header_decode_word (&inptr);
	if (word) {
		addr = g_string_append(addr, word);
		header_decode_lwsp(&inptr);
		g_free(word);
		while (*inptr == '.' && word) {
			inptr++;
			addr = g_string_append_c(addr, '.');
			word = header_decode_word (&inptr);
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

static struct _camel_header_address *
header_decode_mailbox(const char **in, const char *charset)
{
	const char *inptr = *in;
	char *pre;
	int closeme = FALSE;
	GString *addr;
	GString *name = NULL;
	struct _camel_header_address *address = NULL;
	const char *comment = NULL;

	addr = g_string_new("");

	/* for each address */
	pre = header_decode_word (&inptr);
	header_decode_lwsp(&inptr);
	if (!(*inptr == '.' || *inptr == '@' || *inptr==',' || *inptr=='\0')) {
		/* ',' and '\0' required incase it is a simple address, no @ domain part (buggy writer) */
		name = g_string_new ("");
		while (pre) {
			char *text, *last;

			/* perform internationalised decoding, and append */
			text = camel_header_decode_string (pre, charset);
			g_string_append (name, text);
			last = pre;
			g_free(text);

			pre = header_decode_word (&inptr);
			if (pre) {
				size_t l = strlen (last);
				size_t p = strlen (pre);
				
				/* dont append ' ' between sucsessive encoded words */
				if ((l>6 && last[l-2] == '?' && last[l-1] == '=')
				    && (p>6 && pre[0] == '=' && pre[1] == '?')) {
					/* dont append ' ' */
				} else {
					name = g_string_append_c(name, ' ');
				}
			} else {
				/* Fix for stupidly-broken-mailers that like to put '.''s in names unquoted */
				/* see bug #8147 */
				while (!pre && *inptr && *inptr != '<') {
					w(g_warning("Working around stupid mailer bug #5: unescaped characters in names"));
					name = g_string_append_c(name, *inptr++);
					pre = header_decode_word (&inptr);
				}
			}
			g_free(last);
		}
		header_decode_lwsp(&inptr);
		if (*inptr == '<') {
			closeme = TRUE;
		try_address_again:
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
			pre = header_decode_word (&inptr);
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
		pre = header_decode_word (&inptr);
		addr = g_string_append_c(addr, '.');
		if (pre)
			addr = g_string_append(addr, pre);
		comment = inptr;
		header_decode_lwsp(&inptr);
	}
	g_free(pre);

	/* now at '@' domain part */
	if (*inptr == '@') {
		char *dom;

		inptr++;
		addr = g_string_append_c(addr, '@');
		comment = inptr;
		dom = header_decode_domain(&inptr);
		addr = g_string_append(addr, dom);
		g_free(dom);
	} else if (*inptr != '>' || !closeme) {
		/* If we get a <, the address was probably a name part, lets try again shall we? */
		/* Another fix for seriously-broken-mailers */
		if (*inptr && *inptr != ',') {
			char *text;

			w(g_warning("We didn't get an '@' where we expected in '%s', trying again", *in));
			w(g_warning("Name is '%s', Addr is '%s' we're at '%s'\n", name?name->str:"<UNSET>", addr->str, inptr));

			/* need to keep *inptr, as try_address_again will drop the current character */
			if (*inptr == '<')
				closeme = TRUE;
			else
				g_string_append_c(addr, *inptr);

			/* check for address is encoded word ... */
			text = camel_header_decode_string(addr->str, charset);
			if (name == NULL) {
				name = addr;
				addr = g_string_new("");
				if (text) {
					g_string_truncate(name, 0);
					g_string_append(name, text);
				}
			} else {
				g_string_append(name, text?text:addr->str);
				g_string_truncate(addr, 0);
			}
			g_free(text);

			/* or maybe that we've added up a bunch of broken bits to make an encoded word */
			text = rfc2047_decode_word(name->str, name->len);
			if (text) {
				g_string_truncate(name, 0);
				g_string_append(name, text);
				g_free(text);
			}

			goto try_address_again;
		}
		w(g_warning("invalid address, no '@' domain part at %c: %s", *inptr, *in));
	}

	if (closeme) {
		header_decode_lwsp(&inptr);
		if (*inptr == '>') {
			inptr++;
		} else {
			w(g_warning("invalid route address, no closing '>': %s", *in));
		} 
	} else if (name == NULL && comment != NULL && inptr>comment) { /* check for comment after address */
		char *text, *tmp;
		const char *comstart, *comend;

		/* this is a bit messy, we go from the last known position, because
		   decode_domain/etc skip over any comments on the way */
		/* FIXME: This wont detect comments inside the domain itself,
		   but nobody seems to use that feature anyway ... */

		d(printf("checking for comment from '%s'\n", comment));

		comstart = strchr(comment, '(');
		if (comstart) {
			comstart++;
			header_decode_lwsp(&inptr);
			comend = inptr-1;
			while (comend > comstart && comend[0] != ')')
				comend--;
			
			if (comend > comstart) {
				d(printf("  looking at subset '%.*s'\n", comend-comstart, comstart));
				tmp = g_strndup (comstart, comend-comstart);
				text = camel_header_decode_string (tmp, charset);
				name = g_string_new (text);
				g_free (tmp);
				g_free (text);
			}
		}
	}
	
	*in = inptr;
	
	if (addr->len > 0) {
		if (!g_utf8_validate (addr->str, addr->len, NULL)) {
			/* workaround for invalid addr-specs containing 8bit chars (see bug #42170 for details) */
			const char *locale_charset;
			GString *out;
			
			locale_charset = e_iconv_locale_charset ();
			
			out = g_string_new ("");
			
			if ((charset == NULL || !append_8bit (out, addr->str, addr->len, charset))
			    && (locale_charset == NULL || !append_8bit (out, addr->str, addr->len, locale_charset)))
				append_latin1 (out, addr->str, addr->len);
			
			g_string_free (addr, TRUE);
			addr = out;
		}
		
		address = camel_header_address_new_name(name ? name->str : "", addr->str);
	}
	
	d(printf("got mailbox: %s\n", addr->str));
	
	g_string_free(addr, TRUE);
	if (name)
		g_string_free(name, TRUE);
	
	return address;
}

static struct _camel_header_address *
header_decode_address(const char **in, const char *charset)
{
	const char *inptr = *in;
	char *pre;
	GString *group = g_string_new("");
	struct _camel_header_address *addr = NULL, *member;

	/* pre-scan, trying to work out format, discard results */
	header_decode_lwsp(&inptr);
	while ((pre = header_decode_word (&inptr))) {
		group = g_string_append(group, pre);
		group = g_string_append(group, " ");
		g_free(pre);
	}
	header_decode_lwsp(&inptr);
	if (*inptr == ':') {
		d(printf("group detected: %s\n", group->str));
		addr = camel_header_address_new_group(group->str);
		/* that was a group spec, scan mailbox's */
		inptr++;
		/* FIXME: check rfc 2047 encodings of words, here or above in the loop */
		header_decode_lwsp(&inptr);
		if (*inptr != ';') {
			int go = TRUE;
			do {
				member = header_decode_mailbox(&inptr, charset);
				if (member)
					camel_header_address_add_member(addr, member);
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
		addr = header_decode_mailbox(in, charset);
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
camel_header_msgid_decode(const char *in)
{
	if (in == NULL)
		return NULL;

	return header_msgid_decode_internal(&in);
}

char *
camel_header_contentid_decode (const char *in)
{
	const char *inptr = in;
	gboolean at = FALSE;
	GString *addr;
	char *buf;
	
	d(printf("decoding Content-ID: '%s'\n", in));
	
	header_decode_lwsp (&inptr);
	
	/* some lame mailers quote the Content-Id */
	if (*inptr == '"')
		inptr++;
	
	/* make sure the content-id is not "" which can happen if we get a
	 * content-id such as <.@> (which Eudora likes to use...) */
	if ((buf = camel_header_msgid_decode (inptr)) != NULL && *buf)
		return buf;
	
	g_free (buf);
	
	/* ugh, not a valid msg-id - try to get something useful out of it then? */
	inptr = in;
	header_decode_lwsp (&inptr);
	if (*inptr == '<') {
		inptr++;
		header_decode_lwsp (&inptr);
	}
	
	/* Eudora has been known to use <.@> as a content-id */
	if (!(buf = header_decode_word (&inptr)) && !strchr (".@", *inptr))
		return NULL;
	
	addr = g_string_new ("");
	header_decode_lwsp (&inptr);
	while (buf != NULL || *inptr == '.' || (*inptr == '@' && !at)) {
		if (buf != NULL) {
			g_string_append (addr, buf);
			g_free (buf);
			buf = NULL;
		}
		
		if (!at) {
			if (*inptr == '.') {
				g_string_append_c (addr, *inptr++);
				buf = header_decode_word (&inptr);
			} else if (*inptr == '@') {
				g_string_append_c (addr, *inptr++);
				buf = header_decode_word (&inptr);
				at = TRUE;
			}
		} else if (strchr (".[]", *inptr)) {
			g_string_append_c (addr, *inptr++);
			buf = header_decode_atom (&inptr);
		}
		
		header_decode_lwsp (&inptr);
	}
	
	buf = addr->str;
	g_string_free (addr, FALSE);
	
	return buf;
}

void
camel_header_references_list_append_asis(struct _camel_header_references **list, char *ref)
{
	struct _camel_header_references *w = (struct _camel_header_references *)list, *n;
	while (w->next)
		w = w->next;
	n = g_malloc(sizeof(*n));
	n->id = ref;
	n->next = 0;
	w->next = n;
}

int
camel_header_references_list_size(struct _camel_header_references **list)
{
	int count = 0;
	struct _camel_header_references *w = *list;
	while (w) {
		count++;
		w = w->next;
	}
	return count;
}

void
camel_header_references_list_clear(struct _camel_header_references **list)
{
	struct _camel_header_references *w = *list, *n;
	while (w) {
		n = w->next;
		g_free(w->id);
		g_free(w);
		w = n;
	}
	*list = NULL;
}

static void
header_references_decode_single (const char **in, struct _camel_header_references **head)
{
	struct _camel_header_references *ref;
	const char *inptr = *in;
	char *id, *word;
	
	while (*inptr) {
		header_decode_lwsp (&inptr);
		if (*inptr == '<') {
			id = header_msgid_decode_internal (&inptr);
			if (id) {
				ref = g_malloc (sizeof (struct _camel_header_references));
				ref->next = *head;
				ref->id = id;
				*head = ref;
				break;
			}
		} else {
			word = header_decode_word (&inptr);
			if (word)
				g_free (word);
			else if (*inptr != '\0')
				inptr++; /* Stupid mailer tricks */
		}
	}
	
	*in = inptr;
}

/* TODO: why is this needed?  Can't the other interface also work? */
struct _camel_header_references *
camel_header_references_inreplyto_decode (const char *in)
{
	struct _camel_header_references *ref = NULL;
	
	if (in == NULL || in[0] == '\0')
		return NULL;
	
	header_references_decode_single (&in, &ref);
	
	return ref;
}

/* generate a list of references, from most recent up */
struct _camel_header_references *
camel_header_references_decode (const char *in)
{
	struct _camel_header_references *refs = NULL;
	
	if (in == NULL || in[0] == '\0')
		return NULL;
	
	while (*in)
		header_references_decode_single (&in, &refs);
	
	return refs;
}

struct _camel_header_references *
camel_header_references_dup(const struct _camel_header_references *list)
{
	struct _camel_header_references *new = NULL, *tmp;

	while (list) {
		tmp = g_new(struct _camel_header_references, 1);
		tmp->next = new;
		tmp->id = g_strdup(list->id);
		new = tmp;
		list = list->next;
	}
	return new;
}

struct _camel_header_address *
camel_header_mailbox_decode(const char *in, const char *charset)
{
	if (in == NULL)
		return NULL;

	return header_decode_mailbox(&in, charset);
}

struct _camel_header_address *
camel_header_address_decode(const char *in, const char *charset)
{
	const char *inptr = in, *last;
	struct _camel_header_address *list = NULL, *addr;

	d(printf("decoding To: '%s'\n", in));

	if (in == NULL)
		return NULL;

	header_decode_lwsp(&inptr);
	if (*inptr == 0)
		return NULL;

	do {
		last = inptr;
		addr = header_decode_address(&inptr, charset);
		if (addr)
			camel_header_address_list_append(&list, addr);
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

struct _camel_header_newsgroup *
camel_header_newsgroups_decode(const char *in)
{
	const char *inptr = in;
	register char c;
	struct _camel_header_newsgroup *head, *last, *ng;
	const char *start;

	head = NULL;
	last = (struct _camel_header_newsgroup *)&head;

	do {
		header_decode_lwsp(&inptr);
		start = inptr;
		while ((c = *inptr++) && !camel_mime_is_lwsp(c) && c != ',')
			;
		if (start != inptr-1) {
			ng = g_malloc(sizeof(*ng));
			ng->newsgroup = g_strndup(start, inptr-start-1);
			ng->next = NULL;
			last->next = ng;
			last = ng;
		}
	} while (c);

	return head;
}

void
camel_header_newsgroups_free(struct _camel_header_newsgroup *ng)
{
	while (ng) {
		struct _camel_header_newsgroup *nng = ng->next;

		g_free(ng->newsgroup);
		g_free(ng);
		ng = nng;
	}
}

/* this must be kept in sync with the header */
static const char *encodings[] = {
	"",
	"7bit",
	"8bit",
	"base64",
	"quoted-printable",
	"binary",
	"x-uuencode",
};

const char *
camel_transfer_encoding_to_string (CamelTransferEncoding encoding)
{
	if (encoding >= sizeof (encodings) / sizeof (encodings[0]))
		encoding = 0;
	
	return encodings[encoding];
}

CamelTransferEncoding
camel_transfer_encoding_from_string (const char *string)
{
	int i;
	
	if (string != NULL) {
		for (i = 0; i < sizeof (encodings) / sizeof (encodings[0]); i++)
			if (!strcasecmp (string, encodings[i]))
				return i;
	}
	
	return CAMEL_TRANSFER_ENCODING_DEFAULT;
}

void
camel_header_mime_decode(const char *in, int *maj, int *min)
{
	const char *inptr = in;
	int major=-1, minor=-1;

	d(printf("decoding MIME-Version: '%s'\n", in));

	if (in != NULL) {
		header_decode_lwsp(&inptr);
		if (isdigit(*inptr)) {
			major = camel_header_decode_int(&inptr);
			header_decode_lwsp(&inptr);
			if (*inptr == '.') {
				inptr++;
				header_decode_lwsp(&inptr);
				if (isdigit(*inptr))
					minor = camel_header_decode_int(&inptr);
			}
		}
	}

	if (maj)
		*maj = major;
	if (min)
		*min = minor;

	d(printf("major = %d, minor = %d\n", major, minor));
}

struct _rfc2184_param {
	struct _camel_header_param param;
	int index;
};

static int
rfc2184_param_cmp(const void *ap, const void *bp)
{
	const struct _rfc2184_param *a = *(void **)ap;
	const struct _rfc2184_param *b = *(void **)bp;
	int res;

	res = strcmp(a->param.name, b->param.name);
	if (res == 0) {
		if (a->index > b->index)
			res = 1;
		else if (a->index < b->index)
			res = -1;
	}
		
	return res;
}

/* NB: Steals name and value */
static struct _camel_header_param *
header_append_param(struct _camel_header_param *last, char *name, char *value)
{
	struct _camel_header_param *node;

	/* This handles -
	    8 bit data in parameters, illegal, tries to convert using locale, or just safens it up.
	    rfc2047 ecoded parameters, illegal, decodes them anyway.  Some Outlook & Mozilla do this?
	*/
	node = g_malloc(sizeof(*node));
	last->next = node;
	node->next = NULL;
	node->name = name;
	if (strncmp(value, "=?", 2) == 0
	    && (node->value = header_decode_text(value, strlen(value), FALSE, NULL))) {
		g_free(value);
	} else if (!g_utf8_validate(value, -1, NULL)) {
		const char * charset = e_iconv_locale_charset();

		if ((node->value = header_convert("UTF-8", charset?charset:"ISO-8859-1", value, strlen(value)))) {
			g_free(value);
		} else {
			node->value = value;
			for (;*value;value++)
				if (!isascii((unsigned char)*value))
					*value = '_';
		}
	} else
		node->value = value;

	return node;
}

static struct _camel_header_param *
header_decode_param_list (const char **in)
{
	struct _camel_header_param *head = NULL, *last = (struct _camel_header_param *)&head;
	GPtrArray *split = NULL;
	const char *inptr = *in;
	struct _rfc2184_param *work;
	char *tmp;

	/* Dump parameters into the output list, in the order found.  RFC 2184 split parameters are kept in an array */
	header_decode_lwsp(&inptr);
	while (*inptr == ';') {
		char *name;
		char *value = NULL;

		inptr++;
		name = decode_token(&inptr);
		header_decode_lwsp(&inptr);
		if (*inptr == '=') {
			inptr++;
			value = header_decode_value(&inptr);
		}

		if (name && value) {
			char *index = strchr(name, '*');

			if (index) {
				if (index[1] == 0) {
					/* VAL*="foo", decode immediately and append */
					*index = 0;
					tmp = rfc2184_decode(value, strlen(value));
					if (tmp) {
						g_free(value);
						value = tmp;
					}
					last = header_append_param(last, name, value);
				} else {
					/* VAL*1="foo", save for later */
					*index++ = 0;
					work = g_malloc(sizeof(*work));
					work->param.name = name;
					work->param.value = value;
					work->index = atoi(index);
					if (split == NULL)
						split = g_ptr_array_new();
					g_ptr_array_add(split, work);
				}
			} else {
				last = header_append_param(last, name, value);
			}
		} else {
			g_free(name);
			g_free(value);
		}

		header_decode_lwsp(&inptr);
	}

	/* Rejoin any RFC 2184 split parameters in the proper order */
	/* Parameters with the same index will be concatenated in undefined order */
	if (split) {
		GString *value = g_string_new("");
		struct _rfc2184_param *first;
		int i;

		qsort(split->pdata, split->len, sizeof(split->pdata[0]), rfc2184_param_cmp);
		first = split->pdata[0];
		for (i=0;i<split->len;i++) {
			work = split->pdata[i];
			if (split->len-1 == i)
				g_string_append(value, work->param.value);
			if (split->len-1 == i || strcmp(work->param.name, first->param.name) != 0) {
				tmp = rfc2184_decode(value->str, value->len);
				if (tmp == NULL)
					tmp = g_strdup(value->str);

				last = header_append_param(last, g_strdup(first->param.name), tmp);
				g_string_truncate(value, 0);
				first = work;
			}
			if (split->len-1 != i)
				g_string_append(value, work->param.value);
		}
		g_string_free(value, TRUE);
		for (i=0;i<split->len;i++) {
			work = split->pdata[i];
			g_free(work->param.name);
			g_free(work->param.value);
			g_free(work);
		}
		g_ptr_array_free(split, TRUE);
	}

	*in = inptr;

	return head;
}

struct _camel_header_param *
camel_header_param_list_decode(const char *in)
{
	if (in == NULL)
		return NULL;

	return header_decode_param_list(&in);
}

static char *
header_encode_param (const unsigned char *in, gboolean *encoded)
{
	const unsigned char *inptr = in;
	unsigned char *outbuf = NULL;
	const char *charset;
	int encoding;
	GString *out;
	guint32 c;

	*encoded = FALSE;
	
	g_return_val_if_fail (in != NULL, NULL);
	
	/* do a quick us-ascii check (the common case?) */
	while (*inptr) {
		if (*inptr > 127)
			break;
		inptr++;
	}
	
	if (*inptr == '\0')
		return g_strdup (in);
	
	inptr = in;
	encoding = 0;
	while ( encoding !=2 && (c = camel_utf8_getc(&inptr)) ) {
		if (c > 127 && c < 256)
			encoding = MAX (encoding, 1);
		else if (c >= 256)
			encoding = MAX (encoding, 2);
	}

	if (encoding == 2)
		charset = camel_charset_best(in, strlen(in));
	else
		charset = "iso-8859-1";
	
	if (g_ascii_strcasecmp(charset, "UTF-8") != 0
	    && (outbuf = header_convert(charset, "UTF-8", in, strlen(in)))) {
		inptr = outbuf;
	} else {
		charset = "UTF-8";
		inptr = in;
	}
	
	/* FIXME: set the 'language' as well, assuming we can get that info...? */
	out = g_string_new (charset);
	g_string_append(out, "''");

	while ( (c = *inptr++) ) {
		if (camel_mime_is_attrchar(c))
			g_string_append_c (out, c);
		else
			g_string_append_printf (out, "%%%c%c", tohex[(c >> 4) & 0xf], tohex[c & 0xf]);
	}
	g_free (outbuf);
	
	outbuf = out->str;
	g_string_free (out, FALSE);
	*encoded = TRUE;
	
	return outbuf;
}

void
camel_header_param_list_format_append (GString *out, struct _camel_header_param *p)
{
	int used = out->len;
	
	while (p) {
		gboolean encoded = FALSE;
		gboolean quote = FALSE;
		int here = out->len;
		size_t nlen, vlen;
		char *value;
		
		if (!p->value) {
			p = p->next;
			continue;
		}
		
		value = header_encode_param (p->value, &encoded);
		if (!value) {
			w(g_warning ("appending parameter %s=%s violates rfc2184", p->name, p->value));
			value = g_strdup (p->value);
		}
		
		if (!encoded) {
			char *ch;
			
			for (ch = value; *ch; ch++) {
				if (camel_mime_is_tspecial (*ch) || camel_mime_is_lwsp (*ch))
					break;
			}
			
			quote = ch && *ch;
		}
		
		nlen = strlen (p->name);
		vlen = strlen (value);
		
		if (used + nlen + vlen > CAMEL_FOLD_SIZE - 8) {
			out = g_string_append (out, ";\n\t");
			here = out->len;
			used = 0;
		} else
			out = g_string_append (out, "; ");
		
		if (nlen + vlen > CAMEL_FOLD_SIZE - 8) {
			/* we need to do special rfc2184 parameter wrapping */
			int maxlen = CAMEL_FOLD_SIZE - (nlen + 8);
			char *inptr, *inend;
			int i = 0;
			
			inptr = value;
			inend = value + vlen;
			
			while (inptr < inend) {
				char *ptr = inptr + MIN (inend - inptr, maxlen);
				
				if (encoded && ptr < inend) {
					/* be careful not to break an encoded char (ie %20) */
					char *q = ptr;
					int j = 2;
					
					for ( ; j > 0 && q > inptr && *q != '%'; j--, q--);
					if (*q == '%')
						ptr = q;
				}
				
				if (i != 0) {
					g_string_append (out, ";\n\t");
					here = out->len;
					used = 0;
				}
				
				g_string_append_printf (out, "%s*%d%s=", p->name, i++, encoded ? "*" : "");
				if (encoded || !quote)
					g_string_append_len (out, inptr, ptr - inptr);
				else
					quote_word (out, TRUE, inptr, ptr - inptr);
				
				d(printf ("wrote: %s\n", out->str + here));
				
				used += (out->len - here);
				
				inptr = ptr;
			}
		} else {
			g_string_append_printf (out, "%s%s=", p->name, encoded ? "*" : "");
			
			if (encoded || !quote)
				g_string_append (out, value);
			else
				quote_word (out, TRUE, value, vlen);
			
			used += (out->len - here);
		}
		
		g_free (value);
		
		p = p->next;
	}
}

char *
camel_header_param_list_format(struct _camel_header_param *p)
{
	GString *out = g_string_new("");
	char *ret;

	camel_header_param_list_format_append(out, p);
	ret = out->str;
	g_string_free(out, FALSE);
	return ret;
}

CamelContentType *
camel_content_type_decode(const char *in)
{
	const char *inptr = in;
	char *type, *subtype = NULL;
	CamelContentType *t = NULL;

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

		t = camel_content_type_new(type, subtype);
		t->params = header_decode_param_list(&inptr);
		g_free(type);
		g_free(subtype);
	} else {
		g_free(type);
		d(printf("cannot find MIME type in header (2) '%s'", in));
	}
	return t;
}

void
camel_content_type_dump(CamelContentType *ct)
{
	struct _camel_header_param *p;

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
camel_content_type_format (CamelContentType *ct)
{
	GString *out;
	char *ret;
	
	if (ct == NULL)
		return NULL;
	
	out = g_string_new ("");
	if (ct->type == NULL) {
		g_string_append_printf (out, "text/plain");
		w(g_warning ("Content-Type with no main type"));
	} else if (ct->subtype == NULL) {
		w(g_warning ("Content-Type with no sub type: %s", ct->type));
		if (!strcasecmp (ct->type, "multipart"))
			g_string_append_printf (out, "%s/mixed", ct->type);
		else
			g_string_append_printf (out, "%s", ct->type);
	} else {
		g_string_append_printf (out, "%s/%s", ct->type, ct->subtype);
	}
	camel_header_param_list_format_append (out, ct->params);
	
	ret = out->str;
	g_string_free (out, FALSE);
	
	return ret;
}

char *
camel_content_type_simple (CamelContentType *ct)
{
	if (ct->type == NULL) {
		w(g_warning ("Content-Type with no main type"));
		return g_strdup ("text/plain");
	} else if (ct->subtype == NULL) {
		w(g_warning ("Content-Type with no sub type: %s", ct->type));
		if (!strcasecmp (ct->type, "multipart"))
			return g_strdup_printf ("%s/mixed", ct->type);
		else
			return g_strdup (ct->type);
	} else
		return g_strdup_printf ("%s/%s", ct->type, ct->subtype);
}

char *
camel_content_transfer_encoding_decode (const char *in)
{
	if (in)
		return decode_token (&in);
	
	return NULL;
}

CamelContentDisposition *
camel_content_disposition_decode(const char *in)
{
	CamelContentDisposition *d = NULL;
	const char *inptr = in;

	if (in == NULL)
		return NULL;

	d = g_malloc(sizeof(*d));
	d->refcount = 1;
	d->disposition = decode_token(&inptr);
	if (d->disposition == NULL)
		w(g_warning("Empty disposition type"));
	d->params = header_decode_param_list(&inptr);
	return d;
}

void
camel_content_disposition_ref(CamelContentDisposition *d)
{
	if (d)
		d->refcount++;
}

void
camel_content_disposition_unref(CamelContentDisposition *d)
{
	if (d) {
		if (d->refcount<=1) {
			camel_header_param_list_free(d->params);
			g_free(d->disposition);
			g_free(d);
		} else {
			d->refcount--;
		}
	}
}

char *
camel_content_disposition_format(CamelContentDisposition *d)
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
	camel_header_param_list_format_append(out, d->params);

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

static char *tz_days [] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

char *
camel_header_format_date(time_t time, int offset)
{
	struct tm tm;

	d(printf("offset = %d\n", offset));

	d(printf("converting date %s", ctime(&time)));

	time += ((offset / 100) * (60*60)) + (offset % 100)*60;

	d(printf("converting date %s", ctime(&time)));
	
	gmtime_r (&time, &tm);
	
	return g_strdup_printf("%s, %02d %s %04d %02d:%02d:%02d %+05d",
			       tz_days[tm.tm_wday],
			       tm.tm_mday, tz_months[tm.tm_mon],
			       tm.tm_year + 1900,
			       tm.tm_hour, tm.tm_min, tm.tm_sec,
			       offset);
}

/* convert a date to time_t representation */
/* this is an awful mess oh well */
time_t
camel_header_decode_date(const char *in, int *saveoffset)
{
	const char *inptr = in;
	char *monthname;
	gboolean foundmonth;
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
		/* we dont really care about the day, it's only for display */
		if (day) {
			d(printf ("got day: %s\n", day));
			g_free (day);
			header_decode_lwsp (&inptr);
			if (*inptr == ',') {
				inptr++;
			} else {
#ifndef CLEAN_DATE
				return parse_broken_date (in, saveoffset);
#else
				if (saveoffset)
					*saveoffset = 0;
				return 0;
#endif /* ! CLEAN_DATE */
			}
		}
	}
	tm.tm_mday = camel_header_decode_int(&inptr);
#ifndef CLEAN_DATE
	if (tm.tm_mday == 0) {
		return parse_broken_date (in, saveoffset);
	}
#endif /* ! CLEAN_DATE */

	monthname = decode_token(&inptr);
	foundmonth = FALSE;
	if (monthname) {
		for (i=0;i<sizeof(tz_months)/sizeof(tz_months[0]);i++) {
			if (!strcasecmp(tz_months[i], monthname)) {
				tm.tm_mon = i;
				foundmonth = TRUE;
				break;
			}
		}
		g_free(monthname);
	}
#ifndef CLEAN_DATE
	if (!foundmonth) {
		return parse_broken_date (in, saveoffset);
	}
#endif /* ! CLEAN_DATE */

	year = camel_header_decode_int(&inptr);
	if (year < 69) {
		tm.tm_year = 100 + year;
	} else if (year < 100) {
		tm.tm_year = year;
	} else if (year >= 100 && year < 1900) {
		tm.tm_year = year;
	} else {
		tm.tm_year = year - 1900;
	}
	/* get the time ... yurck */
	tm.tm_hour = camel_header_decode_int(&inptr);
	header_decode_lwsp(&inptr);
	if (*inptr == ':')
		inptr++;
	tm.tm_min = camel_header_decode_int(&inptr);
	header_decode_lwsp(&inptr);
	if (*inptr == ':')
		inptr++;
	tm.tm_sec = camel_header_decode_int(&inptr);
	header_decode_lwsp(&inptr);
	if (*inptr == '+'
	    || *inptr == '-') {
		offset = (*inptr++)=='-'?-1:1;
		offset = offset * camel_header_decode_int(&inptr);
		d(printf("abs signed offset = %d\n", offset));
		if (offset < -1200 || offset > 1400)
			offset = 0;
	} else if (isdigit(*inptr)) {
		offset = camel_header_decode_int(&inptr);
		d(printf("abs offset = %d\n", offset));
		if (offset < -1200 || offset > 1400)
			offset = 0;
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
			offset = offset + (camel_header_decode_int(&inptr)*sign);
		}
		d(printf("named offset = %d\n", offset));
	}

	t = e_mktime_utc(&tm);

	/* t is now GMT of the time we want, but not offset by the timezone ... */

	d(printf(" gmt normalized? = %s\n", ctime(&t)));

	/* this should convert the time to the GMT equiv time */
	t -= ( (offset/100) * 60*60) + (offset % 100)*60;

	d(printf(" gmt normalized for timezone? = %s\n", ctime(&t)));

	d({
		char *tmp;
		tmp = camel_header_format_date(t, offset);
		printf(" encoded again: %s\n", tmp);
		g_free(tmp);
	});

	if (saveoffset)
		*saveoffset = offset;

	return t;
}

char *
camel_header_location_decode(const char *in)
{
	int quote = 0;
	GString *out = g_string_new("");
	char c, *res;

	/* Sigh. RFC2557 says:
	 *   content-location =   "Content-Location:" [CFWS] URI [CFWS]
	 *      where URI is restricted to the syntax for URLs as
	 *      defined in Uniform Resource Locators [URL] until
	 *      IETF specifies other kinds of URIs.
	 *
	 * But Netscape puts quotes around the URI when sending web
	 * pages.
	 *
	 * Which is required as defined in rfc2017 [3.1].  Although
	 * outlook doesn't do this.
	 *
	 * Since we get headers already unfolded, we need just drop
	 * all whitespace.  URL's cannot contain whitespace or quoted
	 * characters, even when included in quotes.
	 */

	header_decode_lwsp(&in);
	if (*in == '"') {
		in++;
		quote = 1;
	}

	while ( (c = *in++) ) {
		if (quote && c=='"')
			break;
		if (!camel_mime_is_lwsp(c))
			g_string_append_c(out, c);
	}

	res = g_strdup(out->str);
	g_string_free(out, TRUE);

	return res;
}

/* extra rfc checks */
#define CHECKS

#ifdef CHECKS
static void
check_header(struct _camel_header_raw *h)
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
camel_header_raw_append_parse(struct _camel_header_raw **list, const char *header, int offset)
{
	register const char *in;
	size_t fieldlen;
	char *name;

	in = header;
	while (camel_mime_is_fieldname(*in) || *in==':')
		in++;
	fieldlen = in-header-1;
	while (camel_mime_is_lwsp(*in))
		in++;
	if (fieldlen == 0 || header[fieldlen] != ':') {
		printf("Invalid header line: '%s'\n", header);
		return;
	}
	name = g_alloca (fieldlen + 1);
	memcpy(name, header, fieldlen);
	name[fieldlen] = 0;

	camel_header_raw_append(list, name, in, offset);
}

void
camel_header_raw_append(struct _camel_header_raw **list, const char *name, const char *value, int offset)
{
	struct _camel_header_raw *l, *n;

	d(printf("Header: %s: %s\n", name, value));

	n = g_malloc(sizeof(*n));
	n->next = NULL;
	n->name = g_strdup(name);
	n->value = g_strdup(value);
	n->offset = offset;
#ifdef CHECKS
	check_header(n);
#endif
	l = (struct _camel_header_raw *)list;
	while (l->next) {
		l = l->next;
	}
	l->next = n;

	/* debug */
#if 0
	if (!strcasecmp(name, "To")) {
		printf("- Decoding To\n");
		camel_header_to_decode(value);
	} else if (!strcasecmp(name, "Content-type")) {
		printf("- Decoding content-type\n");
		camel_content_type_dump(camel_content_type_decode(value));		
	} else if (!strcasecmp(name, "MIME-Version")) {
		printf("- Decoding mime version\n");
		camel_header_mime_decode(value);
	}
#endif
}

static struct _camel_header_raw *
header_raw_find_node(struct _camel_header_raw **list, const char *name)
{
	struct _camel_header_raw *l;

	l = *list;
	while (l) {
		if (!strcasecmp(l->name, name))
			break;
		l = l->next;
	}
	return l;
}

const char *
camel_header_raw_find(struct _camel_header_raw **list, const char *name, int *offset)
{
	struct _camel_header_raw *l;

	l = header_raw_find_node(list, name);
	if (l) {
		if (offset)
			*offset = l->offset;
		return l->value;
	} else
		return NULL;
}

const char *
camel_header_raw_find_next(struct _camel_header_raw **list, const char *name, int *offset, const char *last)
{
	struct _camel_header_raw *l;

	if (last == NULL || name == NULL)
		return NULL;

	l = *list;
	while (l && l->value != last)
		l = l->next;
	return camel_header_raw_find(&l, name, offset);
}

static void
header_raw_free(struct _camel_header_raw *l)
{
	g_free(l->name);
	g_free(l->value);
	g_free(l);
}

void
camel_header_raw_remove(struct _camel_header_raw **list, const char *name)
{
	struct _camel_header_raw *l, *p;

	/* the next pointer is at the head of the structure, so this is safe */
	p = (struct _camel_header_raw *)list;
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
camel_header_raw_replace(struct _camel_header_raw **list, const char *name, const char *value, int offset)
{
	camel_header_raw_remove(list, name);
	camel_header_raw_append(list, name, value, offset);
}

void
camel_header_raw_clear(struct _camel_header_raw **list)
{
	struct _camel_header_raw *l, *n;
	l = *list;
	while (l) {
		n = l->next;
		header_raw_free(l);
		l = n;
	}
	*list = NULL;
}

char *
camel_header_msgid_generate (void)
{
	static pthread_mutex_t count_lock = PTHREAD_MUTEX_INITIALIZER;
#define COUNT_LOCK() pthread_mutex_lock (&count_lock)
#define COUNT_UNLOCK() pthread_mutex_unlock (&count_lock)
	char host[MAXHOSTNAMELEN];
	char *name;
	static int count = 0;
	char *msgid;
	int retval;
	struct addrinfo *ai = NULL, hints = { 0 };

	retval = gethostname (host, sizeof (host));
	if (retval == 0 && *host) {
		hints.ai_flags = AI_CANONNAME;
		ai = camel_getaddrinfo(host, NULL, &hints, NULL);
		if (ai && ai->ai_canonname)
			name = ai->ai_canonname;
		else
			name = host;
	} else
		name = "localhost.localdomain";
	
	COUNT_LOCK ();
	msgid = g_strdup_printf ("%d.%d.%d.camel@%s", (int) time (NULL), getpid (), count++, name);
	COUNT_UNLOCK ();
	
	if (ai)
		camel_freeaddrinfo(ai);
	
	return msgid;
}


static struct {
	char *name;
	char *pattern;
	regex_t regex;
} mail_list_magic[] = {
	/* List-Post: <mailto:gnome-hackers@gnome.org> */
	/* List-Post: <mailto:gnome-hackers> */
	{ "List-Post", "[ \t]*<mailto:([^@>]+)@?([^ \n\t\r>]*)" },
	/* List-Id: GNOME stuff <gnome-hackers.gnome.org> */
	/* List-Id: <gnome-hackers.gnome.org> */
	/* List-Id: <gnome-hackers> */
	/* This old one wasn't very useful: { "List-Id", " *([^<]+)" },*/
	{ "List-Id", "[^<]*<([^\\.>]+)\\.?([^ \n\t\r>]*)" },
	/* Mailing-List: list gnome-hackers@gnome.org; contact gnome-hackers-owner@gnome.org */
	{ "Mailing-List", "[ \t]*list ([^@]+)@?([^ \n\t\r>;]*)" },
	/* Originator: gnome-hackers@gnome.org */
	{ "Originator", "[ \t]*([^@]+)@?([^ \n\t\r>]*)" },
	/* X-Mailing-List: <gnome-hackers@gnome.org> arcive/latest/100 */
	/* X-Mailing-List: gnome-hackers@gnome.org */
	/* X-Mailing-List: gnome-hackers */
	/* X-Mailing-List: <gnome-hackers> */
	{ "X-Mailing-List", "[ \t]*<?([^@>]+)@?([^ \n\t\r>]*)" },
	/* X-Loop: gnome-hackers@gnome.org */
	{ "X-Loop", "[ \t]*([^@]+)@?([^ \n\t\r>]*)" },
	/* X-List: gnome-hackers */
	/* X-List: gnome-hackers@gnome.org */
	{ "X-List", "[ \t]*([^@]+)@?([^ \n\t\r>]*)" },	
	/* Sender: owner-gnome-hackers@gnome.org */
	/* Sender: owner-gnome-hacekrs */
	{ "Sender", "[ \t]*owner-([^@]+)@?([^ @\n\t\r>]*)" },
	/* Sender: gnome-hackers-owner@gnome.org */
	/* Sender: gnome-hackers-owner */
	{ "Sender", "[ \t]*([^@]+)-owner@?([^ @\n\t\r>]*)" },
	/* Delivered-To: mailing list gnome-hackers@gnome.org */
	/* Delivered-To: mailing list gnome-hackers */
	{ "Delivered-To", "[ \t]*mailing list ([^@]+)@?([^ \n\t\r>]*)" },
	/* Sender: owner-gnome-hackers@gnome.org */
	/* Sender: <owner-gnome-hackers@gnome.org> */
	/* Sender: owner-gnome-hackers */
	/* Sender: <owner-gnome-hackers> */
	{ "Return-Path", "[ \t]*<?owner-([^@>]+)@?([^ \n\t\r>]*)" },
	/* X-BeenThere: gnome-hackers@gnome.org */
	/* X-BeenThere: gnome-hackers */
	{ "X-BeenThere", "[ \t]*([^@]+)@?([^ \n\t\r>]*)" },
	/* List-Unsubscribe:  <mailto:gnome-hackers-unsubscribe@gnome.org> */
	{ "List-Unsubscribe", "<mailto:(.+)-unsubscribe@([^ \n\t\r>]*)" },
};

char *
camel_header_raw_check_mailing_list(struct _camel_header_raw **list)
{
	const char *v;
	regmatch_t match[3];
	int i, j;
	
	for (i = 0; i < sizeof (mail_list_magic) / sizeof (mail_list_magic[0]); i++) {
		v = camel_header_raw_find (list, mail_list_magic[i].name, NULL);
		for (j=0;j<3;j++) {
			match[j].rm_so = -1;
			match[j].rm_eo = -1;
		}
		if (v != NULL && regexec (&mail_list_magic[i].regex, v, 3, match, 0) == 0 && match[1].rm_so != -1) {
			char *list;
			int len1, len2;

			len1 = match[1].rm_eo - match[1].rm_so;
			len2 = match[2].rm_eo - match[2].rm_so;

			list = g_malloc(len1+len2+2);
			memcpy(list, v + match[1].rm_so, len1);
			if (len2) {
				list[len1] = '@';
				memcpy(list+len1+1, v+match[2].rm_so, len2);
				list[len1+len2+1]=0;
			} else {
				list[len1] = 0;
			}

			return list;
		}
	}

	return NULL;
}

/* ok, here's the address stuff, what a mess ... */
struct _camel_header_address *
camel_header_address_new (void)
{
	struct _camel_header_address *h;
	h = g_malloc0(sizeof(*h));
	h->type = CAMEL_HEADER_ADDRESS_NONE;
	h->refcount = 1;
	return h;
}

struct _camel_header_address *
camel_header_address_new_name(const char *name, const char *addr)
{
	struct _camel_header_address *h;
	h = camel_header_address_new();
	h->type = CAMEL_HEADER_ADDRESS_NAME;
	h->name = g_strdup(name);
	h->v.addr = g_strdup(addr);
	return h;
}

struct _camel_header_address *
camel_header_address_new_group (const char *name)
{
	struct _camel_header_address *h;

	h = camel_header_address_new();
	h->type = CAMEL_HEADER_ADDRESS_GROUP;
	h->name = g_strdup(name);
	return h;
}

void
camel_header_address_ref(struct _camel_header_address *h)
{
	if (h)
		h->refcount++;
}

void
camel_header_address_unref(struct _camel_header_address *h)
{
	if (h) {
		if (h->refcount <= 1) {
			if (h->type == CAMEL_HEADER_ADDRESS_GROUP) {
				camel_header_address_list_clear(&h->v.members);
			} else if (h->type == CAMEL_HEADER_ADDRESS_NAME) {
				g_free(h->v.addr);
			}
			g_free(h->name);
			g_free(h);
		} else {
			h->refcount--;
		}
	}
}

void
camel_header_address_set_name(struct _camel_header_address *h, const char *name)
{
	if (h) {
		g_free(h->name);
		h->name = g_strdup(name);
	}
}

void
camel_header_address_set_addr(struct _camel_header_address *h, const char *addr)
{
	if (h) {
		if (h->type == CAMEL_HEADER_ADDRESS_NAME
		    || h->type == CAMEL_HEADER_ADDRESS_NONE) {
			h->type = CAMEL_HEADER_ADDRESS_NAME;
			g_free(h->v.addr);
			h->v.addr = g_strdup(addr);
		} else {
			g_warning("Trying to set the address on a group");
		}
	}
}

void
camel_header_address_set_members(struct _camel_header_address *h, struct _camel_header_address *group)
{
	if (h) {
		if (h->type == CAMEL_HEADER_ADDRESS_GROUP
		    || h->type == CAMEL_HEADER_ADDRESS_NONE) {
			h->type = CAMEL_HEADER_ADDRESS_GROUP;
			camel_header_address_list_clear(&h->v.members);
			/* should this ref them? */
			h->v.members = group;
		} else {
			g_warning("Trying to set the members on a name, not group");
		}
	}
}

void
camel_header_address_add_member(struct _camel_header_address *h, struct _camel_header_address *member)
{
	if (h) {
		if (h->type == CAMEL_HEADER_ADDRESS_GROUP
		    || h->type == CAMEL_HEADER_ADDRESS_NONE) {
			h->type = CAMEL_HEADER_ADDRESS_GROUP;
			camel_header_address_list_append(&h->v.members, member);
		}		    
	}
}

void
camel_header_address_list_append_list(struct _camel_header_address **l, struct _camel_header_address **h)
{
	if (l) {
		struct _camel_header_address *n = (struct _camel_header_address *)l;

		while (n->next)
			n = n->next;
		n->next = *h;
	}
}


void
camel_header_address_list_append(struct _camel_header_address **l, struct _camel_header_address *h)
{
	if (h) {
		camel_header_address_list_append_list(l, &h);
		h->next = NULL;
	}
}

void
camel_header_address_list_clear(struct _camel_header_address **l)
{
	struct _camel_header_address *a, *n;
	a = *l;
	while (a) {
		n = a->next;
		camel_header_address_unref(a);
		a = n;
	}
	*l = NULL;
}

/* if encode is true, then the result is suitable for mailing, otherwise
   the result is suitable for display only (and may not even be re-parsable) */
static void
header_address_list_encode_append (GString *out, int encode, struct _camel_header_address *a)
{
	char *text;
	
	while (a) {
		switch (a->type) {
		case CAMEL_HEADER_ADDRESS_NAME:
			if (encode)
				text = camel_header_encode_phrase (a->name);
			else
				text = a->name;
			if (text && *text)
				g_string_append_printf (out, "%s <%s>", text, a->v.addr);
			else
				g_string_append (out, a->v.addr);
			if (encode)
				g_free (text);
			break;
		case CAMEL_HEADER_ADDRESS_GROUP:
			if (encode)
				text = camel_header_encode_phrase (a->name);
			else
				text = a->name;
			g_string_append_printf (out, "%s: ", text);
			header_address_list_encode_append (out, encode, a->v.members);
			g_string_append_printf (out, ";");
			if (encode)
				g_free (text);
			break;
		default:
			g_warning ("Invalid address type");
			break;
		}
		a = a->next;
		if (a)
			g_string_append (out, ", ");
	}
}

char *
camel_header_address_list_encode (struct _camel_header_address *a)
{
	GString *out;
	char *ret;
	
	if (a == NULL)
		return NULL;
	
	out = g_string_new ("");
	header_address_list_encode_append (out, TRUE, a);
	ret = out->str;
	g_string_free (out, FALSE);
	
	return ret;
}

char *
camel_header_address_list_format (struct _camel_header_address *a)
{
	GString *out;
	char *ret;
	
	if (a == NULL)
		return NULL;
	
	out = g_string_new ("");
	
	header_address_list_encode_append (out, FALSE, a);
	ret = out->str;
	g_string_free (out, FALSE);
	
	return ret;
}

char *
camel_header_address_fold (const char *in, size_t headerlen)
{
	size_t len, outlen;
	const char *inptr = in, *space, *p, *n;
	GString *out;
	char *ret;
	int i, needunfold = FALSE;
	
	if (in == NULL)
		return NULL;
	
	/* first, check to see if we even need to fold */
	len = headerlen + 2;
	p = in;
	while (*p) {
		n = strchr (p, '\n');
		if (n == NULL) {
			len += strlen (p);
			break;
		}
		
		needunfold = TRUE;
		len += n-p;
		
		if (len >= CAMEL_FOLD_SIZE)
			break;
		len = 0;
		p = n + 1;
	}
	if (len < CAMEL_FOLD_SIZE)
		return g_strdup (in);
	
	/* we need to fold, so first unfold (if we need to), then process */
	if (needunfold)
		inptr = in = camel_header_unfold (in);
	
	out = g_string_new ("");
	outlen = headerlen + 2;
	while (*inptr) {
		space = strchr (inptr, ' ');
		if (space) {
			len = space - inptr + 1;
		} else {
			len = strlen (inptr);
		}
		
		d(printf("next word '%.*s'\n", len, inptr));
		
		if (outlen + len > CAMEL_FOLD_SIZE) {
			d(printf("outlen = %d wordlen = %d\n", outlen, len));
			/* strip trailing space */
			if (out->len > 0 && out->str[out->len-1] == ' ')
				g_string_truncate (out, out->len-1);
			g_string_append (out, "\n\t");
			outlen = 1;
		}
		
		outlen += len;
		for (i = 0; i < len; i++) {
			g_string_append_c (out, inptr[i]);
		}
		
		inptr += len;
	}
	ret = out->str;
	g_string_free (out, FALSE);
	
	if (needunfold)
		g_free ((char *)in);
	
	return ret;	
}

/* simple header folding */
/* will work even if the header is already folded */
char *
camel_header_fold(const char *in, size_t headerlen)
{
	size_t len, outlen, i;
	const char *inptr = in, *space, *p, *n;
	GString *out;
	char *ret;
	int needunfold = FALSE;

	if (in == NULL)
		return NULL;

	/* first, check to see if we even need to fold */
	len = headerlen + 2;
	p = in;
	while (*p) {
		n = strchr(p, '\n');
		if (n == NULL) {
			len += strlen (p);
			break;
		}

		needunfold = TRUE;
		len += n-p;
		
		if (len >= CAMEL_FOLD_SIZE)
			break;
		len = 0;
		p = n + 1;
	}
	if (len < CAMEL_FOLD_SIZE)
		return g_strdup(in);

	/* we need to fold, so first unfold (if we need to), then process */
	if (needunfold)
		inptr = in = camel_header_unfold(in);

	out = g_string_new("");
	outlen = headerlen+2;
	while (*inptr) {
		space = strchr(inptr, ' ');
		if (space) {
			len = space-inptr+1;
		} else {
			len = strlen(inptr);
		}
		d(printf("next word '%.*s'\n", len, inptr));
		if (outlen + len > CAMEL_FOLD_SIZE) {
			d(printf("outlen = %d wordlen = %d\n", outlen, len));
			/* strip trailing space */
			if (out->len > 0 && out->str[out->len-1] == ' ')
				g_string_truncate(out, out->len-1);
			g_string_append(out, "\n\t");
			outlen = 1;
			/* check for very long words, just cut them up */
			while (outlen+len > CAMEL_FOLD_MAX_SIZE) {
				for (i=0;i<CAMEL_FOLD_MAX_SIZE-outlen;i++)
					g_string_append_c(out, inptr[i]);
				inptr += CAMEL_FOLD_MAX_SIZE-outlen;
				len -= CAMEL_FOLD_MAX_SIZE-outlen;
				g_string_append(out, "\n\t");
				outlen = 1;
			}
		}
		outlen += len;
		for (i=0;i<len;i++) {
			g_string_append_c(out, inptr[i]);
		}
		inptr += len;
	}
	ret = out->str;
	g_string_free(out, FALSE);

	if (needunfold)
		g_free((char *)in);

	return ret;	
}

char *
camel_header_unfold(const char *in)
{
	char *out = g_malloc(strlen(in)+1);
	const char *inptr = in;
	char c, *o = out;

	o = out;
	while ((c = *inptr++)) {
		if (c == '\n') {
			if (camel_mime_is_lwsp(*inptr)) {
				do {
					inptr++;
				} while (camel_mime_is_lwsp(*inptr));
				*o++ = ' ';
			} else {
				*o++ = c;
			}
		} else {
			*o++ = c;
		}
	}
	*o = 0;

	return out;
}

void
camel_mime_utils_init(void)
{
	int i, errcode, regex_compilation_failed=0;

	/* Init tables */
	header_decode_init();
	base64_init();

	/* precompile regex's for speed at runtime */
	for (i = 0; i < G_N_ELEMENTS (mail_list_magic); i++) {
		errcode = regcomp(&mail_list_magic[i].regex, mail_list_magic[i].pattern, REG_EXTENDED|REG_ICASE);
		if (errcode != 0) {
			char *errstr;
			size_t len;
		
			len = regerror(errcode, &mail_list_magic[i].regex, NULL, 0);
			errstr = g_malloc0(len + 1);
			regerror(errcode, &mail_list_magic[i].regex, errstr, len);
		
			g_warning("Internal error, compiling regex failed: %s: %s", mail_list_magic[i].pattern, errstr);
			g_free(errstr);
			regex_compilation_failed++;
		}
	}

	g_assert(regex_compilation_failed == 0);
}


void
camel_mime_utils_shutdown (void)
{
	int i;
	
	for (i = 0; i < G_N_ELEMENTS (mail_list_magic); i++)
		regfree (&mail_list_magic[i].regex);
}
