#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <camel/camel-utf8.h>
#include <camel/camel-exception.h>

#include "camel-test.h"

static struct {
	unsigned char *utf8;
	char *utf7;
	guint32 unicode[200];
} tests[] = {
	/* the escape char */
 	{ "&", "&-",
	  {  0x0026, } },
	/* part of set D */
	{ "+", "+",
	  {  0x002b, } },
	{ "plain ascii text", "plain ascii text",
	  {  0x0070, 0x006c, 0x0061, 0x0069, 0x006e, 0x0020, 0x0061, 0x0073, 0x0063, 0x0069, 0x0069, 0x0020, 0x0074, 0x0065, 0x0078, 0x0074, } },
	/* part of set O */
	{ "'(),-./:?", "'(),-./:?",
	  {  0x0027, 0x0028, 0x0029, 0x002c, 0x002d, 0x002e, 0x002f, 0x003a, 0x003f, } },
	{ "!\"#$%*+-;<=>@[]^_`{|}", "!\"#$%*+-;<=>@[]^_`{|}",
	  {  0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x002a, 0x002b, 0x002d, 0x003b, 0x003c, 0x003d, 0x003e, 0x0040, 0x005b, 0x005d, 0x005e, 0x005f, 0x0060, 0x007b, 0x007c, 0x007d, } },
	/* example strings from rfc1642 (modified for imap utf7) */
	{ "A\xe2\x89\xa2\xce\x91" ".", "A&ImIDkQ-.",
	  {  0x0041, 0x2262, 0x0391, 0x002e, } },
	{ "Hi Mum \xe2\x98\xba!", "Hi Mum &Jjo-!",
	  {  0x0048, 0x0069, 0x0020, 0x004d, 0x0075, 0x006d, 0x0020, 0x263a, 0x0021, } },
	{ "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", "&ZeVnLIqe-",
	  {  0x65e5, 0x672c, 0x8a9e, } },
	{ "Item 3 is \xc2\xa3" "1.", "Item 3 is &AKM-1.",
	  {  0x0049, 0x0074, 0x0065, 0x006d, 0x0020, 0x0033, 0x0020, 0x0069, 0x0073, 0x0020, 0x00a3, 0x0031, 0x002e, } },
	{ "\"The sayings of Confucius,\" James R. Ware, trans.  \xe5\x8f\xb0\xe5\x8c\x97:\xe6\x96\x87\xe8\x87\xb4\xe5\x87\xba\xe7\x89\x88\xe7\xa4\xbe, 1980.  (Chinese text with English translation)\xe5\x9b\x9b\xe6\x9b\xb8\xe4\xba\x94\xe7\xb6\x93, \xe5\xae\x8b\xe5\x85\x83\xe4\xba\xba\xe6\xb3\xa8, \xe5\x8c\x97\xe4\xba\xac:  \xe4\xb8\xad\xe5\x9c\x8b\xe6\x9b\xb8\xe5\xba\x97, 1990.",
	  "\"The sayings of Confucius,\" James R. Ware, trans.  &U,BTFw-:&ZYeB9FH6ckh5Pg-, 1980.  (Chinese text with English translation)&Vttm+E6UfZM-, &W4tRQ066bOg-, &UxdOrA-:  &Ti1XC2b4Xpc-, 1990.",
	  {  0x0022, 0x0054, 0x0068, 0x0065, 0x0020, 0x0073, 0x0061, 0x0079, 0x0069, 0x006e, 0x0067, 0x0073, 0x0020, 0x006f, 0x0066, 0x0020, 0x0043, 0x006f, 0x006e, 0x0066, 0x0075, 0x0063, 0x0069, 0x0075, 0x0073, 0x002c, 0x0022, 0x0020, 0x004a, 0x0061, 0x006d, 0x0065, 0x0073, 0x0020, 0x0052, 0x002e, 0x0020, 0x0057, 0x0061, 0x0072, 0x0065, 0x002c, 0x0020, 0x0074, 0x0072, 0x0061, 0x006e, 0x0073, 0x002e, 0x0020, 0x0020, 0x53f0, 0x5317, 0x003a, 0x6587, 0x81f4, 0x51fa, 0x7248, 0x793e, 0x002c, 0x0020, 0x0031, 0x0039, 0x0038, 0x0030, 0x002e, 0x0020, 0x0020, 0x0028, 0x0043, 0x0068, 0x0069, 0x006e, 0x0065, 0x0073, 0x0065, 0x0020, 0x0074, 0x0065, 0x0078, 0x0074, 0x0020, 0x0077, 0x0069, 0x0074, 0x0068, 0x0020, 0x0045, 0x006e, 0x0067, 0x006c, 0x0069, 0x0073, 0x0068, 0x0020, 0x0074, 0x0072, 0x0061, 0x006e, 0x0073, 0x006c, 0x0061, 0x0074, 0x0069, 0x006f, 0x006e, 0x0029, 0x56db, 0x66f8, 0x4e94, 0x7d93, 0x002c, 0x0020, 0x5b8b, 0x5143, 0x4eba, 0x6ce8, 0x002c, 0x0020, 0x5317, 0x4eac, 0x003a, 0x0020, 0x0020, 0x4e2d, 0x570b, 0x66f8, 0x5e97, 0x002c, 0x0020, 0x0031, 0x0039, 0x0039, 0x0030, 0x002e, } },
};
#define TESTS_LENGTH (sizeof(tests)/sizeof(tests[0]))

int
main (int argc, char **argv)
{
	char *utf8, *utf7, *p;
	int i, j;
	guint32 u;
	char utf8enc[256];
	GString *out;

	camel_test_init(argc, argv);

	out = g_string_new("");

	camel_test_start("UTF8, UTF7 RFC1642+RFC2060");

	for (i=0; i<TESTS_LENGTH; i++) {

		camel_test_push("%2d: %s utf8 decode", i, tests[i].utf7);
		p = tests[i].utf8;
		j = 0;
		do {
			u = camel_utf8_getc((const unsigned char **)&p);
			check(u == tests[i].unicode[j]);
			j++;
		} while (u);
		camel_test_pull();

		camel_test_push("%2d: %s utf7->utf8", i, tests[i].utf7);
		utf8 = camel_utf7_utf8(tests[i].utf7);
		check_msg(strcmp(utf8, tests[i].utf8) == 0, "utf8 = '%s'", utf8);
		camel_test_pull();

		camel_test_push("%2d: %s utf7->utf8->utf7", i, tests[i].utf7);
		utf7 = camel_utf8_utf7(utf8);
		check_msg(strcmp(utf7, tests[i].utf7) == 0, "utf7 = '%s'", utf7);
		camel_test_pull();

		g_free(utf7);
		g_free(utf8);

		camel_test_push("%2d: %s utf8 encode", i, tests[i].utf7);

		g_string_truncate(out, 0);
		p = utf8enc;
		j = 0;
		do {
			u = tests[i].unicode[j++];
			camel_utf8_putc((unsigned char **)&p, u);
			g_string_append_u(out, u);
		} while (u);

		check(strcmp(utf8enc, out->str) == 0);
		check(strcmp(utf8enc, tests[i].utf8) == 0);

		camel_test_pull();
	}

	camel_test_end();

	return 0;
}
