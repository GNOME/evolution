/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "e-util/e-trie.h"
#include "camel-utf8.h"
#include "camel-url-scanner.h"


struct _CamelUrlScanner {
	GPtrArray *patterns;
	ETrie *trie;
};


CamelUrlScanner *
camel_url_scanner_new (void)
{
	CamelUrlScanner *scanner;
	
	scanner = g_new (CamelUrlScanner, 1);
	scanner->patterns = g_ptr_array_new ();
	scanner->trie = e_trie_new (TRUE);
	
	return scanner;
}


void
camel_url_scanner_free (CamelUrlScanner *scanner)
{
	g_return_if_fail (scanner != NULL);
	
	g_ptr_array_free (scanner->patterns, TRUE);
	e_trie_free (scanner->trie);
	g_free (scanner);
}


void
camel_url_scanner_add (CamelUrlScanner *scanner, urlpattern_t *pattern)
{
	g_return_if_fail (scanner != NULL);
	
	e_trie_add (scanner->trie, pattern->pattern, scanner->patterns->len);
	g_ptr_array_add (scanner->patterns, pattern);
}


gboolean
camel_url_scanner_scan (CamelUrlScanner *scanner, const char *in, size_t inlen, urlmatch_t *match)
{
	const char *pos, *inptr, *inend;
	urlpattern_t *pat;
	int pattern;
	
	g_return_val_if_fail (scanner != NULL, FALSE);
	g_return_val_if_fail (in != NULL, FALSE);
	
	inptr = in;
	inend = in + inlen;
	
	do {
		if (!(pos = e_trie_search (scanner->trie, inptr, inlen, &pattern)))
			return FALSE;
		
		pat = g_ptr_array_index (scanner->patterns, pattern);
		
		match->pattern = pat->pattern;
		match->prefix = pat->prefix;
		
		if (pat->start (in, pos, inend, match) && pat->end (in, pos, inend, match))
			return TRUE;
		
		inptr = pos;
		if (camel_utf8_getc_limit ((const unsigned char **) &inptr, inend) == 0xffff)
			break;
		
		inlen = inend - inptr;
	} while (inptr < inend);
	
	return FALSE;
}


static unsigned char url_scanner_table[256] = {
	  1,  1,  1,  1,  1,  1,  1,  1,  1,  9,  9,  1,  1,  9,  1,  1,
	  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	 24,128,160,128,128,128,128,128,160,160,128,128,160,192,160,160,
	 68, 68, 68, 68, 68, 68, 68, 68, 68, 68,160,160, 32,128, 32,128,
	160, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
	 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,160,160,160,128,128,
	128, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
	 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,128,128,128,128,  1,
	  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1
};

enum {
	IS_CTRL		= (1 << 0),
	IS_ALPHA        = (1 << 1),
	IS_DIGIT        = (1 << 2),
	IS_LWSP		= (1 << 3),
	IS_SPACE	= (1 << 4),
	IS_SPECIAL	= (1 << 5),
	IS_DOMAIN       = (1 << 6),
	IS_URLSAFE      = (1 << 7),
};

#define is_ctrl(x) ((url_scanner_table[(unsigned char)(x)] & IS_CTRL) != 0)
#define is_lwsp(x) ((url_scanner_table[(unsigned char)(x)] & IS_LWSP) != 0)
#define is_atom(x) ((url_scanner_table[(unsigned char)(x)] & (IS_SPECIAL|IS_SPACE|IS_CTRL)) == 0)
#define is_alpha(x) ((url_scanner_table[(unsigned char)(x)] & IS_ALPHA) != 0)
#define is_digit(x) ((url_scanner_table[(unsigned char)(x)] & IS_DIGIT) != 0)
#define is_domain(x) ((url_scanner_table[(unsigned char)(x)] & IS_DOMAIN) != 0)
#define is_urlsafe(x) ((url_scanner_table[(unsigned char)(x)] & (IS_ALPHA|IS_DIGIT|IS_URLSAFE)) != 0)


static struct {
	char open;
	char close;
} url_braces[] = {
	{ '(', ')' },
	{ '{', '}' },
	{ '[', ']' },
	{ '<', '>' },
	{ '|', '|' },
};

static gboolean
is_open_brace (char c)
{
	int i;
	
	for (i = 0; i < G_N_ELEMENTS (url_braces); i++) {
		if (c == url_braces[i].open)
			return TRUE;
	}
	
	return FALSE;
}

static char
url_stop_at_brace (const char *in, size_t so)
{
	int i;
	
	if (so > 0) {
		for (i = 0; i < G_N_ELEMENTS (url_braces); i++) {
			if (in[so - 1] == url_braces[i].open)
				return url_braces[i].close;
		}
	}
	
	return '\0';
}


gboolean
camel_url_addrspec_start (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	register const char *inptr = pos;
	
	g_assert (*inptr == '@');
	
	inptr--;
	
	while (inptr > in) {
		if (is_atom (*inptr))
			inptr--;
		else
			break;
		
		while (inptr > in && is_atom (*inptr))
			inptr--;
		
		if (inptr > in && *inptr == '.')
			inptr--;
	}
	
	if (!is_atom (*inptr) || is_open_brace (*inptr))
		inptr++;
	
	if (inptr == pos)
		return FALSE;
	
	match->um_so = (inptr - in);
	
	return TRUE;
}

gboolean
camel_url_addrspec_end (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	const char *inptr = pos;
	int parts = 0, digits;
	gboolean got_dot = FALSE;
	
	g_assert (*inptr == '@');
	
	inptr++;
	
	if (*inptr == '[') {
		/* domain literal */
		do {
			inptr++;
			
			digits = 0;
			while (inptr < inend && is_digit (*inptr) && digits < 3) {
				inptr++;
				digits++;
			}
			
			parts++;
			
			if (*inptr != '.' && parts != 4)
				return FALSE;
		} while (parts < 4);
		
		if (*inptr == ']')
			inptr++;
		else
			return FALSE;
		
		got_dot = TRUE;
	} else {
		while (inptr < inend) {
			if (is_domain (*inptr))
				inptr++;
			else
				break;
			
			while (inptr < inend && is_domain (*inptr))
				inptr++;
			
			if (inptr < inend && *inptr == '.' && is_domain (inptr[1])) {
				if (*inptr == '.')
					got_dot = TRUE;
				inptr++;
			}
		}
	}
	
	/* don't allow toplevel domains */
	if (inptr == pos + 1 || !got_dot)
		return FALSE;
	
	match->um_eo = (inptr - in);
	
	return TRUE;
}

gboolean
camel_url_file_start (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	match->um_so = (pos - in);
	
	return TRUE;
}

gboolean
camel_url_file_end (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	register const char *inptr = pos;
	char close_brace;
	
	inptr += strlen (match->pattern);
	
	if (*inptr == '/')
		inptr++;
	
	close_brace = url_stop_at_brace (in, match->um_so);
	
	while (inptr < inend && is_urlsafe (*inptr) && *inptr != close_brace)
		inptr++;
	
	if (inptr == pos)
		return FALSE;
	
	match->um_eo = (inptr - in);
	
	return TRUE;
}

gboolean
camel_url_web_start (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	if (pos > in && !strncmp (pos, "www", 3)) {
		/* make sure we aren't actually part of another word */
		if (!is_open_brace (pos[-1]) && !isspace (pos[-1]))
			return FALSE;
	}
	
	match->um_so = (pos - in);
	
	return TRUE;
}

gboolean
camel_url_web_end (const char *in, const char *pos, const char *inend, urlmatch_t *match)
{
	register const char *inptr = pos;
	gboolean passwd = FALSE;
	const char *save;
	char close_brace;
	int port;
	
	inptr += strlen (match->pattern);
	
	close_brace = url_stop_at_brace (in, match->um_so);
	
	/* find the end of the domain */
	if (is_atom (*inptr)) {
		/* might be a domain or user@domain */
		save = inptr;
		while (inptr < inend) {
			if (!is_atom (*inptr))
				break;
			
			inptr++;
			
			while (inptr < inend && is_atom (*inptr))
				inptr++;
			
			if ((inptr + 1) < inend && *inptr == '.' && is_atom (inptr[1]))
				inptr++;
		}
		
		if (*inptr != '@')
			inptr = save;
		else
			inptr++;
		
		goto domain;
	} else if (is_domain (*inptr)) {
	domain:
		while (inptr < inend) {
			if (!is_domain (*inptr))
				break;
			
			inptr++;
			
			while (inptr < inend && is_domain (*inptr))
				inptr++;
			
			if ((inptr + 1) < inend && *inptr == '.' && is_domain (inptr[1]))
				inptr++;
		}
	} else {
		return FALSE;
	}
	
	if (inptr < inend) {
		switch (*inptr) {
		case ':': /* we either have a port or a password */
			inptr++;
			
			if (is_digit (*inptr) || passwd) {
				port = (*inptr++ - '0');
				
				while (inptr < inend && is_digit (*inptr) && port < 65536)
					port = (port * 10) + (*inptr++ - '0');
				
				if (!passwd && (port >= 65536 || *inptr == '@')) {
					if (inptr < inend) {
						/* this must be a password? */
						goto passwd;
					}
					
					inptr--;
				}
			} else {
			passwd:
				passwd = TRUE;
				save = inptr;
				
				while (inptr < inend && is_atom (*inptr))
					inptr++;
				
				if ((inptr + 2) < inend) {
					if (*inptr == '@') {
						inptr++;
						if (is_domain (*inptr))
							goto domain;
					}
					
					return FALSE;
				}
			}
			
			if (inptr >= inend || *inptr != '/')
				break;
			
			/* we have a '/' so there could be a path - fall through */
		case '/': /* we've detected a path component to our url */
			inptr++;
			
			while (inptr < inend && is_urlsafe (*inptr) && *inptr != close_brace)
				inptr++;
			
			break;
		default:
			break;
		}
	}
	
	/* urls are extremely unlikely to end with any
	 * punctuation, so strip any trailing
	 * punctuation off. Also strip off any closing
	 * braces or quotes. */
	while (inptr > pos && strchr (",.:;?!-|)}]'\"", inptr[-1]))
		inptr--;
	
	match->um_eo = (inptr - in);
	
	return TRUE;
}



#ifdef BUILD_TABLE

#include <stdio.h>

/* got these from rfc1738 */
#define CHARS_LWSP " \t\n\r"               /* linear whitespace chars */
#define CHARS_SPECIAL "()<>@,;:\\\".[]"

/* got these from rfc1738 */
#define CHARS_URLSAFE "$-_.+!*'(),{}|\\^~[]`#%\";/?:@&="


static void
table_init_bits (unsigned int mask, const unsigned char *vals)
{
	int i;
	
	for (i = 0; vals[i] != '\0'; i++)
		url_scanner_table[vals[i]] |= mask;
}

static void
url_scanner_table_init (void)
{
	int i;
	
	for (i = 0; i < 256; i++) {
		url_scanner_table[i] = 0;
		if (i < 32)
			url_scanner_table[i] |= IS_CTRL;
		if ((i >= '0' && i <= '9'))
			url_scanner_table[i] |= IS_DIGIT | IS_DOMAIN;
		if ((i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z'))
			url_scanner_table[i] |= IS_ALPHA | IS_DOMAIN;
		if (i >= 127)
			url_scanner_table[i] |= IS_CTRL;
	}
	
	url_scanner_table[' '] |= IS_SPACE;
	url_scanner_table['-'] |= IS_DOMAIN;
	
	/* not defined to be special in rfc0822, but when scanning
           backwards to find the beginning of the email address we do
           not want to include this char if we come accross it - so
           this is kind of a hack */
	url_scanner_table['/'] |= IS_SPECIAL;
	
	table_init_bits (IS_LWSP, CHARS_LWSP);
	table_init_bits (IS_SPECIAL, CHARS_SPECIAL);
	table_init_bits (IS_URLSAFE, CHARS_URLSAFE);
}

int main (int argc, char **argv)
{
	int i;
	
	url_scanner_table_init ();
	
	printf ("static unsigned char url_scanner_table[256] = {");
	for (i = 0; i < 256; i++) {
		printf ("%s%3d%s", (i % 16) ? "" : "\n\t",
			url_scanner_table[i], i != 255 ? "," : "\n");
	}
	printf ("};\n\n");
	
	return 0;
}

#endif /* BUILD_TABLE */
