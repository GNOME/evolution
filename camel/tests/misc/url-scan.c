/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2004 Ximian, Inc. (www.ximian.com)
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
#include <stdlib.h>
#include <string.h>

#include <camel/camel-mime-filter-tohtml.h>

#include "camel-test.h"

struct {
	char *text, *url;
} url_tests[] = {
	{ "bob@foo.com", "mailto:bob@foo.com" },
	{ "Ends with bob@foo.com", "mailto:bob@foo.com" },
	{ "bob@foo.com at start", "mailto:bob@foo.com" },
	{ "bob@foo.com.", "mailto:bob@foo.com" },
	{ "\"bob@foo.com\"", "mailto:bob@foo.com" },
	{ "<bob@foo.com>", "mailto:bob@foo.com" },
	{ "(bob@foo.com)", "mailto:bob@foo.com" },
	{ "bob@foo.com, 555-9999", "mailto:bob@foo.com" },
	{ "|bob@foo.com|555-9999|", "mailto:bob@foo.com" },
	{ "bob@ no match bob@", NULL },
	{ "@foo.com no match @foo.com", NULL },
	{ "\"bob\"@foo.com", NULL },
	{ "M@ke money fast!", NULL },
	{ "ASCII art @_@ @>->-", NULL },

	{ "http://www.foo.com", "http://www.foo.com" },
	{ "Ends with http://www.foo.com", "http://www.foo.com" },
	{ "http://www.foo.com at start", "http://www.foo.com" },
	{ "http://www.foo.com.", "http://www.foo.com" },
	{ "http://www.foo.com/.", "http://www.foo.com/" },
	{ "<http://www.foo.com>", "http://www.foo.com" },
	{ "(http://www.foo.com)", "http://www.foo.com" },
	{ "http://www.foo.com, 555-9999", "http://www.foo.com" },
	{ "|http://www.foo.com|555-9999|", "http://www.foo.com" },
	{ "foo http://www.foo.com/ bar", "http://www.foo.com/" },
	{ "foo http://www.foo.com/index.html bar", "http://www.foo.com/index.html" },
	{ "foo http://www.foo.com/q?99 bar", "http://www.foo.com/q?99" },
	{ "foo http://www.foo.com/;foo=bar&baz=quux bar", "http://www.foo.com/;foo=bar&baz=quux" },
	{ "foo http://www.foo.com/index.html#anchor bar", "http://www.foo.com/index.html#anchor" },
	{ "http://www.foo.com/index.html; foo", "http://www.foo.com/index.html" },
	{ "http://www.foo.com/index.html: foo", "http://www.foo.com/index.html" },
	{ "http://www.foo.com/index.html-- foo", "http://www.foo.com/index.html" },
	{ "http://www.foo.com/index.html?", "http://www.foo.com/index.html" },
	{ "http://www.foo.com/index.html!", "http://www.foo.com/index.html" },
	{ "\"http://www.foo.com/index.html\"", "http://www.foo.com/index.html" },
	{ "'http://www.foo.com/index.html'", "http://www.foo.com/index.html" },
	{ "http://bob@www.foo.com/bar/baz/", "http://bob@www.foo.com/bar/baz/" },
	{ "http no match http", NULL },
	{ "http: no match http:", NULL },
	{ "http:// no match http://", NULL },
	{ "unrecognized://bob@foo.com/path", "mailto:bob@foo.com" },

	{ "src/www.c", NULL },
	{ "Ewwwwww.Gross.", NULL },

};

static int num_url_tests = G_N_ELEMENTS (url_tests);

int main (int argc, char **argv)
{
	char *html, *url, *p;
	int i, errors = 0;
	guint32 flags;
	
	camel_test_init (argc, argv);
	
	camel_test_start ("URL scanning");
	
	flags = CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS | CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;
	for (i = 0; i < num_url_tests; i++) {
		camel_test_push ("'%s' => '%s'", url_tests[i].text, url_tests[i].url ? url_tests[i].url : "None");
		
		html = camel_text_to_html (url_tests[i].text, flags, 0);
		
		url = strstr (html, "href=\"");
		if (url) {
			url += 6;
			p = strchr (url, '"');
			if (p)
				*p = '\0';
			
			while ((p = strstr (url, "&amp;")))
				memmove (p + 1, p + 5, strlen (p + 5) + 1);
		}
		
		if ((url && (!url_tests[i].url || strcmp (url, url_tests[i].url) != 0)) ||
		    (!url && url_tests[i].url)) {
			printf ("FAILED on \"%s\" -> %s\n  (got %s)\n\n",
				url_tests[i].text,
				url_tests[i].url ? url_tests[i].url : "(nothing)",
				url ? url : "(nothing)");
			errors++;
		}
		
		g_free (html);
	}
	
	printf ("\n%d errors\n", errors);
	
	camel_test_end ();
	
	return errors;
}
