#include "camel-test.h"
#include "messages.h"
#include "addresses.h"

/* for stat */
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <iconv.h>

#include <camel/camel-internet-address.h>
#include <camel/camel-address.h>

#include "address-data.h"

static char *convert(const char *in, const char *from, const char *to)
{
	iconv_t ic = iconv_open(to, from);
	char *out, *outp;
	const char *inp;
	size_t inlen, outlen;

	if (ic == (iconv_t)-1)
		return g_strdup(in);

	inlen = strlen(in);
	outlen = inlen*5 + 16;

	outp = out = g_malloc(outlen);
	inp = in;

	if (iconv(ic, &inp, &inlen, &outp, &outlen) == -1) {
		test_free(out);
		iconv_close(ic);
		return g_strdup(in);
	}

	if (iconv(ic, NULL, 0, &outp, &outlen) == -1) {
		test_free(out);
		iconv_close(ic);
		return g_strdup(in);
	}

	iconv_close(ic);

	*outp = 0;

#if 0
	/* lets see if we can convert back again? */
	{
		char *nout, *noutp;
		iconv_t ic = iconv_open(from, to);

		inp = out;
		inlen = strlen(out);
		outlen = inlen*5 + 16;
		noutp = nout = g_malloc(outlen);
		if (iconv(ic, &inp, &inlen, &noutp, &outlen) == -1
		    || iconv(ic, NULL, 0, &noutp, &outlen) == -1) {
			g_warning("Cannot convert '%s' \n from %s to %s: %s\n", in, to, from, strerror(errno));
		}
		iconv_close(ic);
	}

	/* and lets see what camel thinks out optimal charset is */
	{
		printf("Camel thinks the best encoding of '%s' is %s, although we converted from %s\n",
		       in, camel_charset_best(out, strlen(out)), from);
	}
#endif

	return out;
}

#define to_utf8(in, type) convert(in, type, "utf-8")
#define from_utf8(in, type) convert(in, "utf-8", type)

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

int main(int argc, char **argv)
{
	int i;
	CamelInternetAddress *addr, *addr2;
	char *name;
	char *charset;
	const char *real, *where;
	char *enc, *enc2, *format, *format2;

	camel_test_init(argc, argv);

	camel_test_start("CamelInternetAddress, basics");

	addr = camel_internet_address_new();

	push("Test blank address");
	check(camel_address_length(CAMEL_ADDRESS(addr)) == 0);
	check(camel_internet_address_get(addr, 0, &real, &where) == FALSE);
	pull();

	push("Test blank clone");
	addr2 = CAMEL_INTERNET_ADDRESS(camel_address_new_clone(CAMEL_ADDRESS(addr)));
	test_address_compare(addr, addr2);
	check_unref(addr2, 1);
	pull();

	push("Test add 1");
	camel_internet_address_add(addr, "Zed", "nowhere@here.com.au");
	check(camel_address_length(CAMEL_ADDRESS(addr)) == 1);
	check(camel_internet_address_get(addr, 0, &real, &where) == TRUE);
	check_msg(string_equal("Zed", real), "real = '%s'", real);
	check(strcmp(where, "nowhere@here.com.au") == 0);
	pull();

	push("Test clone 1");
	addr2 = CAMEL_INTERNET_ADDRESS(camel_address_new_clone(CAMEL_ADDRESS(addr)));
	test_address_compare(addr, addr2);
	check_unref(addr2, 1);
	pull();

	push("Test add many");
	for (i=1;i<10;i++) {
		char name[16], a[32];
		sprintf(name, "Zed %d", i);
		sprintf(a, "nowhere@here-%d.com.au", i);
		camel_internet_address_add(addr, name, a);
		check(camel_address_length(CAMEL_ADDRESS(addr)) == i+1);
		check(camel_internet_address_get(addr, i, &real, &where) == TRUE);
		check_msg(string_equal(name, real), "name = '%s' real = '%s'", name, real);
		check(strcmp(where, a) == 0);
	}
	pull();

	/* put a few of these in to make it look like its doing something impressive ... :) */
	camel_test_end();
	camel_test_start("CamelInternetAddress, search");

	push("Test search");
	camel_test_nonfatal("Address comparisons should ignore whitespace??");
	check(camel_internet_address_find_name(addr, "Zed 1", &where) == 1);
	check(camel_internet_address_find_name(addr, "Zed 9", &where) == 9);
	check(camel_internet_address_find_name(addr, "Zed", &where) == 0);
	check(camel_internet_address_find_name(addr, " Zed", &where) == 0);
	check(camel_internet_address_find_name(addr, "Zed ", &where) == 0);
	check(camel_internet_address_find_name(addr, "  Zed ", &where) == 0);
	check(camel_internet_address_find_name(addr, "Zed 20", &where) == -1);
	check(camel_internet_address_find_name(addr, "", &where) == -1);
	/* interface dont handle nulls :) */
	/*check(camel_internet_address_find_name(addr, NULL, &where) == -1);*/

	check(camel_internet_address_find_address(addr, "nowhere@here-1.com.au", &where) == 1);
	check(camel_internet_address_find_address(addr, "nowhere@here-1 . com.au", &where) == 1);
	check(camel_internet_address_find_address(addr, "nowhere@here-2 .com.au ", &where) == 2);
	check(camel_internet_address_find_address(addr, " nowhere @here-3.com.au", &where) == 3);
	check(camel_internet_address_find_address(addr, "nowhere@here-20.com.au ", &where) == -1);
	check(camel_internet_address_find_address(addr, "", &where) == -1);
	/*check(camel_internet_address_find_address(addr, NULL, &where) == -1);*/
	camel_test_fatal();
	pull();

	camel_test_end();
	camel_test_start("CamelInternetAddress, copy/cat/clone");

	push("Test clone many");
	addr2 = CAMEL_INTERNET_ADDRESS(camel_address_new_clone(CAMEL_ADDRESS(addr)));
	test_address_compare(addr, addr2);
	pull();

	push("Test remove items");
	camel_address_remove(CAMEL_ADDRESS(addr2), 0);
	check(camel_address_length(CAMEL_ADDRESS(addr2)) == 9);
	camel_address_remove(CAMEL_ADDRESS(addr2), 0);
	check(camel_address_length(CAMEL_ADDRESS(addr2)) == 8);
	camel_address_remove(CAMEL_ADDRESS(addr2), 5);
	check(camel_address_length(CAMEL_ADDRESS(addr2)) == 7);
	camel_address_remove(CAMEL_ADDRESS(addr2), 10);
	check(camel_address_length(CAMEL_ADDRESS(addr2)) == 7);
	camel_address_remove(CAMEL_ADDRESS(addr2), -1);
	check(camel_address_length(CAMEL_ADDRESS(addr2)) == 0);
	check_unref(addr2, 1);
	pull();

	push("Testing copy/cat");
	push("clone + cat");
	addr2 = CAMEL_INTERNET_ADDRESS(camel_address_new_clone(CAMEL_ADDRESS(addr)));
	camel_address_cat(CAMEL_ADDRESS(addr2), CAMEL_ADDRESS(addr));
	check(camel_address_length(CAMEL_ADDRESS(addr)) == 10);
	check(camel_address_length(CAMEL_ADDRESS(addr2)) == 20);
	check_unref(addr2, 1);
	pull();

	push("cat + cat + copy");
	addr2 = camel_internet_address_new();
	camel_address_cat(CAMEL_ADDRESS(addr2), CAMEL_ADDRESS(addr));
	test_address_compare(addr, addr2);
	camel_address_cat(CAMEL_ADDRESS(addr2), CAMEL_ADDRESS(addr));
	check(camel_address_length(CAMEL_ADDRESS(addr)) == 10);
	check(camel_address_length(CAMEL_ADDRESS(addr2)) == 20);
	camel_address_copy(CAMEL_ADDRESS(addr2), CAMEL_ADDRESS(addr));
	test_address_compare(addr, addr2);
	check_unref(addr2, 1);
	pull();

	push("copy");
	addr2 = camel_internet_address_new();
	camel_address_copy(CAMEL_ADDRESS(addr2), CAMEL_ADDRESS(addr));
	test_address_compare(addr, addr2);
	check_unref(addr2, 1);
	pull();

	pull();

	check_unref(addr, 1);

	camel_test_end();

	camel_test_start("CamelInternetAddress, I18N");

	for (i=0;i<ARRAY_LEN(test_lines);i++) {
		push("Testing text line %d (%s) '%s'", i, test_lines[i].type, test_lines[i].line);

		addr = camel_internet_address_new();

		/* first, convert to api format (utf-8) */
		charset = test_lines[i].type;
		name = to_utf8(test_lines[i].line, charset);

		push("Address setup");
		camel_internet_address_add(addr, name, "nobody@nowhere.com");
		check(camel_internet_address_get(addr, 0, &real, &where) == TRUE);
		check_msg(string_equal(name, real), "name = '%s' real = '%s'", name, real);
		check(strcmp(where, "nobody@nowhere.com") == 0);
		test_free(name);

		check(camel_internet_address_get(addr, 1, &real, &where) == FALSE);
		check(camel_address_length(CAMEL_ADDRESS(addr)) == 1);
		pull();

		push("Address encode/decode");
		enc = camel_address_encode(CAMEL_ADDRESS(addr));

		addr2 = camel_internet_address_new();
		check(camel_address_decode(CAMEL_ADDRESS(addr2), enc) == 1);
		check(camel_address_length(CAMEL_ADDRESS(addr2)) == 1);

		enc2 = camel_address_encode(CAMEL_ADDRESS(addr2));
		check_msg(string_equal(enc, enc2), "enc = '%s' enc2 = '%s'", enc, enc2);
		test_free(enc2);

		push("Compare addresses");
		test_address_compare(addr, addr2);
		pull();
		check_unref(addr2, 1);
		test_free(enc);
		pull();

		/* FIXME: format/unformat arne't guaranteed to be reversible, at least at the moment */
		camel_test_nonfatal("format/unformat not (yet) reversible for all cases");

		push("Address format/unformat");
		format = camel_address_format(CAMEL_ADDRESS(addr));

		addr2 = camel_internet_address_new();
		check(camel_address_unformat(CAMEL_ADDRESS(addr2), format) == 1);
		check(camel_address_length(CAMEL_ADDRESS(addr2)) == 1);

		format2 = camel_address_format(CAMEL_ADDRESS(addr2));
		check_msg(string_equal(format, format2), "format = '%s\n\tformat2 = '%s'", format, format2);
		test_free(format2);

		/* currently format/unformat doesn't handle ,'s and other special chars at all */
		if (camel_address_length(CAMEL_ADDRESS(addr2)) == 1) {
			push("Compare addresses");
			test_address_compare(addr, addr2);
			pull();
		}

		test_free(format);
		pull();

		camel_test_fatal();

		check_unref(addr2, 1);

		check_unref(addr, 1);
		pull();

	}

	camel_test_end();

	camel_test_start("CamelInternetAddress, I18N decode");

	for (i=0;i<ARRAY_LEN(test_address);i++) {
		push("Testing address line %d '%s'", i, test_address[i].addr);

		addr = camel_internet_address_new();
		push("checking decoded");
		check(camel_address_decode(CAMEL_ADDRESS(addr), test_address[i].addr) == test_address[i].count);
		format = camel_address_format(CAMEL_ADDRESS(addr));
		check(strcmp(format, test_address[i].utf8) == 0);
		test_free(format);
		pull();
		
		push("Comparing re-encoded output");
		addr2 = CAMEL_INTERNET_ADDRESS(camel_internet_address_new());
		enc = camel_address_encode(CAMEL_ADDRESS(addr));
		check_msg(camel_address_decode(CAMEL_ADDRESS(addr2), enc) == test_address[i].count, "enc = '%s'", enc);
		test_free(enc);
		test_address_compare(addr, addr2);
		check_unref(addr2, 1);
		pull();

		check_unref(addr, 1);

		pull();
	}

	camel_test_end();

	/* FIXME: Add test of decoding of broken addresses */

	return 0;
}


