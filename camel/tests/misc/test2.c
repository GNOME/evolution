

#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <camel/camel-object.h>
#include <camel/camel-mime-utils.h>

#include "camel-test.h"

/* NB: We know which order the params will be decoded in, plain in the order they come,
   and rfc2184 encoded following those, sorted lexigraphically */
struct {
	char *list;
	int count;
	char *params[8];
} test1[] = {
	{ "; charset=\"iso-8859-1\"",
	  1,
	  { "charset", "iso-8859-1" }, },
	{ "; charset=iso-8859-1",
	  1,
	  { "charset", "iso-8859-1" }, },
	{ "; charset=\"iso-8859-1\"; boundary=\"foo\"",
	  2,
	  { "charset", "iso-8859-1",
	    "boundary", "foo" }, },
	{ "; charset*1 = 8859; charset*0=\"iso-8859-1'en'iso-\";charset*2=\"-1\" ",
	  1,
	  { "charset", "iso-8859-1" }, },
	{ "; charset*1 = 8859; boundary=foo; charset*0=\"iso-8859-1'en'iso-\";charset*2=\"-1\" ",
	  2,
	  { "boundary", "foo",
	    "charset", "iso-8859-1", }, },
	{ "; charset*1 = 8859; boundary*0=f; charset*0=\"iso-8859-1'en'iso-\"; boundary*2=\"o\" ; charset*2=\"-1\"; boundary*1=o ",
	  2,
	  { "boundary", "foo",
	    "charset", "iso-8859-1", }, },
	{ "; charset*1 = 8859; boundary*0=\"iso-8859-1'en'f\"; charset*0=\"iso-8859-1'en'iso-\"; boundary*2=\"o\" ; charset*2=\"-1\"; boundary*1=o ",
	  2,
	  { "boundary", "foo",
	    "charset", "iso-8859-1", }, },
};

struct {
	int count;
	char *params[8];
	char *list;
} test2[] = {
	{ 1,
	  { "name", "Doul\xC3\xADk01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123457890123456789123456789" },
	  ";\n"
	  "\tname*0*=iso-8859-1''Doul%EDk012345678901234567890123456789012345678901234;\n"
	  "\tname*1*=56789012345678901234567890123456789012345678901234567890123457890;\n"
	  "\tname*2*=123456789123456789" },
	{ 1,
	  { "name", "\"%$#@ special chars?;; !" },
	  "; name=\"\\\"%$#@ special chars?;; !\"" },
	{ 1,
	  { "name", "\"%$#@ special chars?;; !\xC3\xAD" },
	  "; name*=iso-8859-1''%22%25$#%40%20special%20chars%3F%3B%3B%20!%ED" },
};

int
main (int argc, char **argv)
{
	int i, j;

	camel_test_init(argc, argv);

	camel_test_start("Param list decoding");

	for (i=0;i<sizeof(test1)/sizeof(test1[0]);i++) {
		struct _camel_header_param *head, *node;

		camel_test_push("param decoding[%d] '%s'", i, test1[i].list);
		head = camel_header_param_list_decode(test1[i].list);
		check(head != NULL);
		node = head;
		for (j=0;j<test1[i].count;j++) {
			check_msg(node != NULL, "didn't find all params");
			check(strcmp(node->name, test1[i].params[j*2]) == 0);
			check(strcmp(node->value, test1[i].params[j*2+1]) == 0);
			node = node->next;
		}
		check_msg(node == NULL, "found more params than should have");
		camel_header_param_list_free(head);
		camel_test_pull();
	}

	camel_test_end();

	camel_test_start("Param list encoding");

	for (i=0;i<sizeof(test2)/sizeof(test2[0]);i++) {
		struct _camel_header_param *head = NULL, *scan;
		char *text;

		camel_test_push("param encoding[%d]", i);

		for (j=0;j<test2[i].count;j++)
			camel_header_set_param(&head, test2[i].params[j*2], test2[i].params[j*2+1]);
		scan = head;
		for (j=0;scan;j++)
			scan = scan->next;
		check(j == test2[i].count);

		text = camel_header_param_list_format(head);
		check(strcmp(text, test2[i].list) == 0);
		camel_header_param_list_free(head);

		camel_test_pull();
	}

	camel_test_end();

	return 0;
}
