/*
 * test-mail-autoconfig.c
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
 */

#include <stdlib.h>

#include "e-mail-autoconfig.h"

gint
main (gint argc,
      gchar **argv)
{
	EMailAutoconfig *autoconfig;
	GError *error = NULL;

	if (argc < 2) {
		g_printerr ("USAGE: %s EMAIL-ADDRESS\n", argv[0]);
		exit (EXIT_FAILURE);
	}

	autoconfig = e_mail_autoconfig_new_sync (argv[1], NULL, &error);

	if (error != NULL) {
		g_warn_if_fail (autoconfig == NULL);
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}

	g_assert (E_IS_MAIL_AUTOCONFIG (autoconfig));

	e_mail_autoconfig_dump_results (autoconfig);

	g_object_unref (autoconfig);

	return EXIT_SUCCESS;
}
