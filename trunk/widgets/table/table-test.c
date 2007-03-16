/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * table-test.c
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <gnome.h>

#include "misc/e-cursors.h"

#include "table-test.h"

int
main (int argc, char *argv [])
{

	if (isatty (0)){
		int fd;

		close (0);
		fd = open ("sample.table", O_RDONLY);
		if (fd == -1){
			fprintf (stderr, "Could not find sample.table, try feeding a table on stdin");
			exit (1);
		}
		dup2 (fd, 0);
	}

	gnome_init ("TableTest", "TableTest", argc, argv);
	e_cursors_init ();


/*  	table_browser_test (); */
/*  	multi_cols_test (); */
/*  	check_test (); */

	e_table_test ();

	gtk_main ();

	e_cursors_shutdown ();
	return 0;
}
