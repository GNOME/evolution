/*
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
 *		Jonathan Dieter <jdieter99@gmx.net>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright 2005 Jonathan Dieter
 *
 */

#include <stdlib.h>
#include <string.h>

#include <gconf/gconf-client.h>
#include <e-util/e-error.h>

#include <mail/em-utils.h>
#include <shell/es-event.h>

#define GCONF_KEY_CHECKDEFAULT   "/apps/evolution/mail/prompts/checkdefault"
#define GCONF_KEY_MAILTO_ENABLED "/desktop/gnome/url-handlers/mailto/enabled"
#define GCONF_KEY_MAILTO_COMMAND "/desktop/gnome/url-handlers/mailto/command"
#define EVOLUTION_MAILTO_COMMAND "evolution --component=mail %s"

void org_gnome_default_mailer_check_default (EPlugin *ep, ESEventTargetUpgrade *target);

static gboolean
evolution_is_default_mailer (const gchar *mailto_command)
{
        gint argc;
        gchar **argv;
        gchar *basename;
        gboolean is_default;

        if (mailto_command == NULL)
                return FALSE;

        g_debug ("mailto URL command: %s", mailto_command);

        /* tokenize the mailto command */
        if (!g_shell_parse_argv (mailto_command, &argc, &argv, NULL))
                return FALSE;

        g_assert (argc > 0);

        /* check the basename of the first token */
        basename = g_path_get_basename (argv[0]);
        g_debug ("mailto URL program: %s", basename);
        is_default = g_str_has_prefix (basename, "evolution");
        g_free (basename);

        g_strfreev (argv);

        return is_default;
}

void
org_gnome_default_mailer_check_default (EPlugin *ep, ESEventTargetUpgrade *target)
{
	GConfClient *client;
	gchar       *mailer;
	GConfValue  *is_key;

	client = gconf_client_get_default ();

	/* See whether the check default mailer key has already been set */
	is_key = gconf_client_get(client, GCONF_KEY_CHECKDEFAULT, NULL);
	if (!is_key)
		gconf_client_set_bool(client, GCONF_KEY_CHECKDEFAULT, TRUE, NULL);
	else
		gconf_value_free (is_key);

	/* Check whether we're supposed to check whether or not we are the default mailer */
	if (gconf_client_get_bool(client, GCONF_KEY_CHECKDEFAULT, NULL)) {
		mailer  = gconf_client_get_string(client, GCONF_KEY_MAILTO_COMMAND, NULL);

		/* Check whether we are the default mailer */
		if (!evolution_is_default_mailer (mailer)) {
			/* Ask whether we should be the default mailer */
			if (em_utils_prompt_user(NULL, GCONF_KEY_CHECKDEFAULT, "org.gnome.default.mailer:check-default", NULL)) {
				gconf_client_set_bool(client, GCONF_KEY_MAILTO_ENABLED, TRUE, NULL);
				gconf_client_set_string(client, GCONF_KEY_MAILTO_COMMAND, EVOLUTION_MAILTO_COMMAND, NULL);
			}
		}

		g_free(mailer);
	}

	g_object_unref (client);
}
