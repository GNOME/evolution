/*
 * test-mail-autoconfig.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <stdlib.h>
#include <libedataserver/libedataserver.h>

#include "e-mail-autoconfig.h"

gint
main (gint argc,
      gchar **argv)
{
	ESourceRegistry *registry;
	EMailAutoconfig *autoconfig = NULL;
	GError *error = NULL;

	if (argc < 2) {
		g_printerr ("USAGE: %s EMAIL-ADDRESS\n", argv[0]);
		exit (EXIT_FAILURE);
	}

	registry = e_source_registry_new_sync (NULL, &error);

	if (registry != NULL) {
		autoconfig = e_mail_autoconfig_new_sync (
			registry, argv[1], NULL, NULL, &error);
		g_object_unref (registry);
	} else {
		autoconfig = NULL;
	}

	/* Sanity check. */
	g_return_val_if_fail (
		((autoconfig != NULL) && (error == NULL)) ||
		((autoconfig == NULL) && (error != NULL)), -1);

	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}

	e_mail_autoconfig_dump_results (autoconfig);

	g_clear_object (&autoconfig);

	return EXIT_SUCCESS;
}
