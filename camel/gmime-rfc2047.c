/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* gmime-rfc2047.c: implemention of RFC2047 */

/*
 * Author : 
 *  Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org>
 *
 * Copyright 1999 International GNOME Support (http://www.gnome-support.com) .
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
decode_quoted (const gchar *text, const gchar *end) 
{
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
decode_base64 (const gchar *data, const gchar *end) 
{
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


static gchar *
rfc2047_decode_word (const gchar *data, const gchar *into_what) 
{
	const char *charset = strstr (data, "=?"), *encoding, *text, *end;

	char *buffer, *b, *cooked_data;
	
	buffer = g_malloc (strlen(data) * 2);
	b = buffer;

	if (!charset) return strdup (data);
	charset+=2;

	encoding = strchr (charset, '?');
	if (!encoding) return strdup (data);
	encoding++;

	text = strchr(encoding, '?');
	if (!text) return strdup (data);
	text++;

	end = strstr(text, "?=");
	if (!end) return strdup (data);

	b[0] = 0;

	if (toupper(*encoding)=='Q')
		cooked_data = decode_quoted (text, end);
	else if (toupper (*encoding)=='B')
		cooked_data = decode_base64 (text, end);
	else
		return g_strdup(data);

	{
		char *c = strchr (charset, '?');
		char *q = g_malloc (c - charset + 1);
		char *cook_2 = cooked_data;
		int cook_len = strlen (cook_2);
		int b_len = 4096;
		unicode_iconv_t i;
		strncpy (q, charset, c - charset);
		q[c - charset] = 0;
		i = unicode_iconv_open (into_what, q);
		if (!i) {
			g_free (q);
			return g_strdup (buffer);
		}
		if (unicode_iconv (i, &cook_2, &cook_len, &b, &b_len)==-1)
			/* FIXME : use approximation code if we can't convert it properly. */
			;
		unicode_iconv_close (i);
		*b = 0;
	}

	return g_strdup (buffer);
}

static const gchar *
find_end_of_encoded_word (const gchar *data) 
{
	/* We can't just search for ?=,
           because of the case :
           "=?charset?q?=ff?=" :( */
	if (!data) return NULL;
	data = strstr (data, "=?");
	if (!data) return NULL;
	data = strchr(data+2, '?');
	if (!data) return NULL;
	data = strchr (data+1, '?');
	if (!data) return NULL;
	data = strstr (data+1, "?=");
	if (!data) return NULL;
	return data + 2;
}

gchar *
gmime_rfc2047_decode (const gchar *data, const gchar *into_what) 
{
	char *buffer = malloc (strlen(data) * 4), *b = buffer;

	int was_encoded_word = 0;

	build_base64_rank_table ();

	while (data && *data) {
		char *word_start = strstr (data, "=?"), *decoded;
		if (!word_start) {
			strcpy (b, data);
			b[strlen (data)] = 0;
			return buffer;
		}
		if (word_start != data) {

			if (strspn (data, " \t\n\r") != (word_start - data)) {
				strncpy (b, data, word_start - data);
				b += word_start - data;
				*b = 0;
			}
		}
		decoded = rfc2047_decode_word (word_start, into_what);
		strcpy (b, decoded);
		b += strlen (decoded);
		*b = 0;
		g_free (decoded);

		data = find_end_of_encoded_word (data);
	}

	*b = 0;
	return buffer;
}

#define isnt_ascii(a) ((a) <= 0x1f || (a) >= 0x7f)

static int 
rfc2047_clean (const gchar *string, const gchar *max)
{
	/* if (strstr (string, "?=")) return 1; */
	while (string < max) {
		if (isnt_ascii ((unsigned char)*string))
			return 0;
		string++;
	}
	return 1;
}

static gchar *
encode_word (const gchar *string, int length, const gchar *said_charset) 
{
	const gchar *max = string + length;
	if (rfc2047_clean(string, max)) {
		/* don't bother encoding it if it has no odd characters in it */
		return g_strndup (string, length);
	}
	{
		char *temp = malloc (length * 4 + 1), *t = temp;
		t += sprintf (t, "=?%s?q?", said_charset);
		while (string < max) {
			if (*string == ' ')
				*(t++) = '_';
			else if ((*string <= 0x1f) || (*string >= 0x7f) || (*string == '=') || (*string == '?')) 
				t += sprintf (t, "=%2x", (unsigned char)*string);
			else
				*(t++) = *string;
			      
			string++;
		}
		t += sprintf (t, "?=");
		*t = 0;
	        return temp;
	}
}

static int
words_in(char *a) 
{
	int words = 1;
	while (*a) {
		if (*(a++)==' ')
			words++;
	}
	return words;
}

struct word_data {
	const char *word;
	int word_length;
	const char *to_encode_in;
	char *encoded;
	enum {
		wt_None,
		wt_Address,
	} type;
};

static int string_can_fit_in(const char *a, int length, const char *charset) 
{
	while (length--) {
		if (*a < 0x1f || *a >= 0x7f) return 0;
		a++;
	}
	return 1;
}

static void
show_entry(struct word_data *a) 
{
	a->type = wt_None;
	
	if (string_can_fit_in(a->word, a->word_length, "US-ASCII"))
		a->to_encode_in = "US-ASCII";

	if (a->word[0]=='<' && a->word[a->word_length-1]=='>') {
		a->type = wt_Address;
	}
}

static void
break_into_words(const char *string, struct word_data *a, int words) 
{
	int i;
	for (i=0;i<words;i++) {
		
		char *next_space = strchr(string, ' ');

		if (!next_space) {
			a[i].word = string;
			a[i].word_length = strlen(string);
			a[i].to_encode_in = NULL; /* i.e. the default */

			show_entry(a+i);

			return;
		}

		a[i].word = string;
		a[i].word_length = next_space - string;
		a[i].to_encode_in = NULL;

		show_entry(a+i);

		string = next_space + 1;

	}
}

static void
join_words(struct word_data *a, int words)
{
	int i;
	for (i=(words-1);i>0;i--) {
		if (a[i].to_encode_in == a[i-1].to_encode_in) {
			a[i-1].word_length += 1 + a[i].word_length;
			a[i].word = 0;
			a[i].word_length = 0;
		}

	}
}

static void show_words(struct word_data *words, int count) 
{
	int i;
	for (i=0;i<count;i++)
		if (words[i].word)
			show_entry(words+i);
}

gchar *
gmime_rfc2047_encode (const gchar *string, const gchar *charset) 
{
	int temp_len = strlen (string)*4 + 1, word_count;
	char *temp = g_malloc (temp_len), *temp_2 = temp;
	int string_length = strlen (string);
	char *encoded = NULL, *p;
	struct word_data *words;

	/* first, let us convert to UTF-8 */
	unicode_iconv_t i = unicode_iconv_open ("UTF-8", charset);
	unicode_iconv (i, &string, &string_length, &temp_2, &temp_len);
	unicode_iconv_close (i);
	
	/* null terminate it */
	*temp_2 = 0;

	/* now encode it as if it were a single word */
	
	word_count = words_in ( temp );

        words = g_malloc(sizeof (struct word_data) * word_count);
	break_into_words(temp, words, word_count);
	
	join_words(words, word_count);

	show_words(words, word_count);

	{
		size_t len = 0;
		int c = 0;
		for (c = 0;c<word_count;c++) {
			if (words[c].word)
				{
					words[c].encoded = encode_word(words[c].word, words[c].word_length, 
							      words[c].to_encode_in ? words[c].to_encode_in :
							      "UTF-8");
					len += strlen(words[c].encoded) + 1;
				}
		}

		{ 
		        encoded = g_malloc(len+1);
			p = encoded;
			for (c = 0; c < word_count;c++) if (words[c].word) {
				strcpy(p, words[c].encoded);
				p += strlen(p);
				strcpy(p, " ");
				p++;
			}
			*p = 0;
		}
	}


	/*
	  
	  real algorithm :
	  
	  we need to 
	  
	  split it into words
	  
	  identify portions that have NOT to be encoded (i.e. <> and the comment starter/ender )
	  
	  identify the best character set for each word
	  
	  merge words which share a character set, allow jumping and merging with words which 
	  would be ok to encode in non-US-ASCII.
	  
	  if we have to use 2 character sets, try and collapse them into one.
	  
	  (e.g. if one word contains letters in latin-1, and another letters in latin-2, use
	  latin-2 for the first word as well if possible).
	  
	  finally :
	  
	  if utf-8 will still be used, use it for everything.
	  
	  and then, at last, generate the encoded text, using base64/quoted-printable for
	  each word depending upon which is more efficient.
	  
	  TODO :
	  create a priority list of encodings
	  
	  i.e.

            US-ASCII, ISO-8859-1, ISO-8859-2, ISO-8859-3, KOI8, 

          Should survey for most popular charsets :
          what do people usually use for the following scripts?

	  * Chinese/Japanese/Korean
          * Greek
          * Cyrillic

          (any other scripts commonly used in mail/news?)

	  This algorithm is probably far from optimal, but should be
	  reasonably efficient for simple cases. (and almost free if
	  the text is just in US-ASCII : like 99% of the text that will
	  pass through it)
	  


	  current status :

	    Algorithm now partially implemented.

	*/

	g_free(words);
        g_free(temp);
	
	return encoded;
}
