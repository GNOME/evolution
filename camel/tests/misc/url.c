#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <camel/camel-url.h>

#include "camel-test.h"

char *base = "http://a/b/c/d;p?q#f";

struct {
	char *url_string, *result;
} tests[] = {
	{ "g:h", "g:h" },
	{ "g", "http://a/b/c/g" },
	{ "./g", "http://a/b/c/g" },
	{ "g/", "http://a/b/c/g/" },
	{ "/g", "http://a/g" },
	{ "//g", "http://g" },
	{ "?y", "http://a/b/c/d;p?y" },
	{ "g?y", "http://a/b/c/g?y" },
	{ "g?y/./x", "http://a/b/c/g?y/./x" },
	{ "#s", "http://a/b/c/d;p?q#s" },
	{ "g#s", "http://a/b/c/g#s" },
	{ "g#s/./x", "http://a/b/c/g#s/./x" },
	{ "g?y#s", "http://a/b/c/g?y#s" },
	{ ";x", "http://a/b/c/d;x" },
	{ "g;x", "http://a/b/c/g;x" },
	{ "g;x?y#s", "http://a/b/c/g;x?y#s" },
	{ ".", "http://a/b/c/" },
	{ "./", "http://a/b/c/" },
	{ "..", "http://a/b/" },
	{ "../", "http://a/b/" },
	{ "../g", "http://a/b/g" },
	{ "../..", "http://a/" },
	{ "../../", "http://a/" },
	{ "../../g", "http://a/g" },
	{ "", "http://a/b/c/d;p?q#f" },
	{ "../../../g", "http://a/../g" },
	{ "../../../../g", "http://a/../../g" },
	{ "/./g", "http://a/./g" },
	{ "/../g", "http://a/../g" },
	{ "g.", "http://a/b/c/g." },
	{ ".g", "http://a/b/c/.g" },
	{ "g..", "http://a/b/c/g.." },
	{ "..g", "http://a/b/c/..g" },
	{ "./../g", "http://a/b/g" },
	{ "./g/.", "http://a/b/c/g/" },
	{ "g/./h", "http://a/b/c/g/h" },
	{ "g/../h", "http://a/b/c/h" },
	{ "http:g", "http:g" },
	{ "http:", "http:" }
};
int num_tests = sizeof (tests) / sizeof (tests[0]);

int
main (int argc, char **argv)
{
	CamelURL *base_url, *url;
	char *url_string;
	int i;

	camel_test_init (argc, argv);

	camel_test_start ("RFC1808 relative URL parsing");

	camel_test_push ("base URL parsing");
	base_url = camel_url_new (base);
	if (!base_url)
		camel_test_fail ("Could not parse %s\n", base);
	camel_test_pull ();

	camel_test_push ("base URL unparsing");
	url_string = camel_url_to_string (base_url, TRUE);
	if (strcmp (url_string, base) != 0) {
		camel_test_fail ("URL <%s> unparses to <%s>\n",
				 base, url_string);
	}
	camel_test_pull ();
	g_free (url_string);

	for (i = 0; i < num_tests; i++) {
		camel_test_push ("<%s> + <%s> = <%s>?", base, tests[i].url_string, tests[i].result);
		url = camel_url_new_with_base (base_url, tests[i].url_string);
		if (!url) {
			camel_test_fail ("could not parse");
			camel_test_pull ();
			continue;
		}

		url_string = camel_url_to_string (url, TRUE);
		if (strcmp (url_string, tests[i].result) != 0)
			camel_test_fail ("got <%s>!", url_string);
		g_free (url_string);
		camel_test_pull ();
	}

	camel_test_end ();

	return 0;
}
