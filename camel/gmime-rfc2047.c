/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* gmime-rfc2047.c: implemention of RFC2047 */

/*
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
 * 
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

/* 
 * Authors:  Robert Brady <rwb197@ecs.soton.ac.uk>
 */

#include <stdio.h>
#include <ctype.h>
#include <unicode.h>
#include <string.h>

#include "gmime-rfc2047.h"

#define NOT_RANKED -1

/* This should be changed ASAP to use the base64 code Miguel comitted */

const char *base64_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char base64_rank[256];
static int base64_rank_table_built;
static void build_base64_rank_table (void);

static int 
hexval (gchar c) {
	if (isdigit (c)) return c-'0';
	c = tolower (c);
	return c - 'a' + 10;
}

static gchar *
decode_quoted(const gchar *text, const gchar *end) {
	gchar *to = malloc(end - text + 1), *to_2 = to;
        if (!to) return NULL;
	while (*text && text < end) {
		if (*text == '=') {
			gchar a = hexval (text[1]);
			gchar b = hexval (text[2]);
			int c = (a << 4) + b;
			*to = c;
			to++;
			text+=3;
		} else if (*text == '_') {
			*to = ' ';
			to++;
			text++;
		} else {
			*to = *text;
			to++;
			text++;
		}
	}
	return to_2;
}

static gchar *
decode_base64(const gchar *data, const gchar *end) {
	unsigned short pattern = 0;
	int bits = 0;
	int delimiter = '=';
	gchar x;
	gchar *buffer = g_malloc((end - data) * 3);
	gchar *t = buffer;
	int Q = 0;

	if (!buffer) return NULL;

	while (*data != delimiter) {
		x = base64_rank[(unsigned char)(*data++)];
		if (x == NOT_RANKED)
			continue;
		pattern <<= 6;
		pattern |= x;
		bits += 6;
		if (bits >= 8) {
			x = (pattern >> (bits - 8)) & 0xff;
			*t++ = x;
			Q++;
			bits -= 8;
		}
	}
	*t = 0;
	return buffer;
}

static void
build_base64_rank_table (void)
{
	int i;
	
	if (!base64_rank_table_built) {
		for (i = 0; i < 256; i++)
			base64_rank[i] = NOT_RANKED;
		for (i = 0; i < 64; i++)
			base64_rank[(int) base64_alphabet[i]] = i;
		base64_rank_table_built = 1;
	}
}


gchar*
rfc2047_decode_word (const gchar *data, const gchar *into_what) 
{
	const char *charset = strstr(data, "=?"), *encoding, *text, *end;

	char *buffer, *b, *cooked_data;
	
	buffer = g_malloc(strlen(data) * 2);
	b = buffer;

	if (!charset) return strdup(data);
	charset+=2;

	encoding = strchr(charset, '?');
	if (!encoding) return strdup(data);
	encoding++;

	text = strchr(encoding, '?');
	if (!text) return strdup(data);
	text++;

	end = strstr(text, "?=");
	if (!end) return strdup(data);

	b[0] = 0;

	if (toupper(*encoding)=='Q')
		cooked_data = decode_quoted(text, end);
	else if (toupper(*encoding)=='B')
		cooked_data = decode_base64(text, end);
	else
		return g_strdup(data);

	{
		char *c = strchr(charset, '?');
		char *q = g_malloc(c - charset + 1);
		char *cook_2 = cooked_data;
		int cook_len = strlen(cook_2);
		int b_len = 4096;
		iconv_t i;
		strncpy(q, charset, c - charset);
		i = unicode_iconv_open(into_what, q);
		if (!i) {
			g_free(q);
			return g_strdup(buffer);
		}
		unicode_iconv(i, &cook_2, &cook_len, &b, &b_len);
		unicode_iconv_close(i);
	}

	return g_strdup(buffer);
}

gchar *
gmime_rfc2047_decode (const gchar *data, const gchar *into_what) 
{
	char *buffer = malloc(strlen(data) * 4), *b = buffer;

	int was_encoded_word = 0;

	build_base64_rank_table ();

	while (data && *data) {
		char *word_start = strstr(data, "=?"), *decoded;
		if (!word_start) {
			strcpy(b, data);
			return buffer;
		}
		if (word_start != data) {

			if (strspn(data, " \t\n\r") != (word_start - data)) {
				strncpy(b, data, word_start - data);
				b += word_start - data;
			}
		}
		decoded = rfc2047_decode_word(word_start, into_what);
		strcpy(b, decoded);
		b += strlen(decoded);
		g_free(decoded);

		data = strstr(data, "?=") + 2;
	}

	*b = 0;
	return buffer;
}

gchar 
*gmime_rfc2047_encode (const gchar *string, const gchar *charset) 
{
	gchar buffer[4096] /* FIXME : constant sized buffer */;
	gchar *b = buffer;
	const gchar *s = string;
	int not_ascii = 0, not_latin1 = 0;
	while (*s) {
		if (*s <= 20 || *s >= 0x7f || *s == '=') { not_ascii = 1; }
		s++;
	}
	
	if (!not_ascii) {
		b += sprintf (b, "%s", string);
	}
	
	else {
		b += sprintf (b, "=?%s?Q?", charset);
		s = string;
		while (*s) {
			if (*s == ' ') b += sprintf (b, "_");
			else if (*s < 0x20 || *s >= 0x7f || *s == '=' || *s == '?' || *s == '_') {
				b += sprintf (b, "=%2x", (unsigned char)*s);
			} else {
				b += sprintf (b, "%c", *s);
			}
			s++;
		}
		b += sprintf (b, "?=");
	}
	
	*b = 0;
	
	return g_strdup (buffer);
}
