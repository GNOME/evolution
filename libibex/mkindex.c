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

/* mkindex.c: a simple client, part 1 */

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
	fprintf (stderr, "Usage: mkindex [-f indexfile] file ...\n");
	exit (1);
}

int
main (int argc, char **argv)
{
	ibex *ib;
	int opt;
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

	ib = ibex_open (file, TRUE);
	if (!ib) {
		fprintf (stderr, "Couldn't open index file %s: %s\n",
			 file, strerror (errno));
		exit (1);
	}

	while (argc--) {
		if (ibex_index_file (ib, argv[argc]) == -1) {
			fprintf (stderr, "Couldn't index %s: %s\n",
				 argv[argc], strerror (errno));
			exit (1);
		}
	}

	if (ibex_close (ib) != 0) {
		fprintf (stderr, "Failed to write index file %s: %s\n",
			 file, strerror (errno));
		exit (1);
	}
	exit (0);
}
