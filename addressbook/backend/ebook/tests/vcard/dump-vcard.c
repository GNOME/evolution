/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <stdio.h>
#include "ebook/e-vcard.h"

FILE *fp;

int
main(int argc, char **argv)
{
	EVCard *vcard;
	GString *str = g_string_new ("");

	if (argc < 2)
	  return 0;

	g_type_init_with_debug_flags (G_TYPE_DEBUG_OBJECTS);

	fp = fopen (argv[1], "r");

	while (!feof (fp)) {
		char buf[1024];
		if (fgets (buf, sizeof(buf), fp))
			str = g_string_append (str, buf);
	}

	vcard = e_vcard_new_from_string (str->str);

	e_vcard_dump_structure (vcard);

	return 0;
}
