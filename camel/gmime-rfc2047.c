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

static void 
decode_quoted (const gchar *text, gchar *to) 
{
	while (*text) {
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
	*to = 0;
}

static void 
decode_base64 (const gchar *what, gchar *where) 
{
	unsigned short pattern = 0;
	int bits = 0;
	int delimiter = '=';
	gchar x;
	gchar *t = where;
	int Q = 0;
	while (*what != delimiter) {
		x = base64_rank[(unsigned char)(*what++)];
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

gchar 
*gmime_rfc2047_decode (const gchar *data, const gchar *into_what) 
{
	gchar buffer[4096] /* FIXME : constant sized buffer */, *b = buffer;
	
	build_base64_rank_table ();
	
	while (*data) {
		
		/* If we encounter an error we just break out of the loop and copy the rest
		 * of the text as-is */
		
		if (*data=='=') {
			data++;
			if (*data=='?') {
				gchar *charset, *encoding, *text, *end;
				gchar dc[4096];
				charset = data+1;
				encoding = strchr(charset, '?');
				
				if (!encoding) break;
				encoding++;
				text = strchr (encoding, '?');
				if (!text) break;
				text++;
				end = strstr (text, "?=");
				if (!end) break;
				end++;
				
				*(encoding-1)=0;
				*(text-1)=0;
				*(end-1)=0;
				
				if (strcasecmp (encoding, "q") == 0) {
					decode_quoted(text, dc);
				} else if (strcasecmp (encoding, "b") == 0) {
					decode_base64 (text, dc);
				} else {
					/* What to do here? */
					break;
				}
				
				{
					int f;
					iconv_t i;
					const gchar *d2 = dc;
					int l = strlen (d2), l2 = 4000;
					
					i = unicode_iconv_open (into_what, charset);
					if (!i) 
						break;
					
					unicode_iconv (i, &d2, &l, &b, &l2);
					
					unicode_iconv_close (i);
					data = end;
				}
			}
		} else {
			*b = *data;
			b++;
		}
		
		data++;
		
	}
	
	while (*data) {
		*b = *data;
		b++;
		data++;
	}
	
	*b = 0;
	
	return g_strdup (buffer);
}

gchar 
*rfc2047_encode (const gchar *string, const gchar *charset) 
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
				b += sprintf (b, "=%2x", *s);
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
