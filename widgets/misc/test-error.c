/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>  
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-ui-init.h>
#include "e-error.h"

int
main (int argc, char **argv)
{
	gnome_init ("test-error", "0.0", argc, argv);

	argc--;
	switch (argc) {
	case 1:
		e_error_run(NULL, argv[1], NULL);
		break;
	case 2:
		e_error_run(NULL, argv[1], argv[2], NULL);
		break;
	case 3:
		e_error_run(NULL, argv[1], argv[2], argv[3], NULL);
		break;
	case 4:
		e_error_run(NULL, argv[1], argv[2], argv[3], argv[4], NULL);
		break;
	case 5:
		e_error_run(NULL, argv[1], argv[2], argv[3], argv[4], argv[5], NULL);
		break;
	case 6:
		e_error_run(NULL, argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], NULL);
		break;
	default:
		printf("Error: too many or too few arguments\n");
		printf("Usage:\n %s domain:error-id [ args ... ]\n", argv[0]);
	}

	return 0;
}
