/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Helix Code Inc.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* lookup.c: a simple client, part 2 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ibex.h"

extern int optind;
extern char *optarg;

static void
usage (void)
{
	fprintf (stderr, "Usage: lookup [-f indexfile] word ...\n");
	exit (1);
}

int
main (int argc, char **argv)
{
	ibex *ib;
	GPtrArray *ans, *words;
	int opt, i;
	char *file = "INDEX";

	while ((opt = getopt (argc, argv, "f:")) != -1) {
		switch (opt) {
		case 'f':
			file = optarg;
			break;

		default:
			usage ();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage ();

	ib = ibex_open (file, O_RDWR|O_CREAT, 0600);
	if (!ib) {
		printf ("Couldn't open %s: %s\n", file, strerror (errno));
		exit (1);
	}

	words = g_ptr_array_new ();
	while (argc--)
		g_ptr_array_add (words, argv[argc]);

	ans = ibex_find_all (ib, words);
	if (ans) {
		for (i = 0; i < ans->len; i++)
			printf ("%s\n", (char *)g_ptr_array_index (ans, i));
		exit (0);
	} else {
		printf ("Nope.\n");
		exit (1);
	}
}
