/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* test for the RFC 2047 encoder */

#include <string.h>
#include <unicode.h>

#include "gmime-utils.h"
#include "stdio.h"
#include "camel-log.h"
#include "camel-mime-message.h"
#include "camel-mime-part.h"
#include "camel-stream.h"
#include "camel-stream-fs.h"
#include "camel.h"
#include "gmime-rfc2047.h"

#define TERMINAL_CHARSET "UTF-8"

/* 
 * Info on many unicode issues, including, utf-8 xterms from :
 * 
 *   http://www.cl.cam.ac.uk/~mgk25/unicode.html
 *
 */

const char *tests[] = 
{ 
  "�is is a test", "ISO-8859-1",
  "I�t�r��ti�n�l��ation", "ISO-8859-1",
  "Καλημέρα κόσμε", "UTF-8",
  "コンニチハ", "UTF-8",
  "ði ıntəˈnæʃənəl fəˈnɛtık əsoʊsiˈeıʃn", "UTF-8",
  NULL
};
  

int
main (int argc, char**argv)
{      
	const char **b = tests;
	while (*b) {
		char *e = gmime_rfc2047_encode(b[0], b[1]);
		printf("%s\t%s\n", e, gmime_rfc2047_decode(e, TERMINAL_CHARSET));
		b+=2;
	}

	return 0;

}
