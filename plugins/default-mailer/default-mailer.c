/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Author: Jonathan Dieter <jdieter99@gmx.net>
 *
 *  Copyright 2005 Jonathan Dieter
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
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

void org_gnome_default_mailer_check_default (EPlugin *ep, ESEventTargetUpgrade *target);

void
org_gnome_default_mailer_check_default (EPlugin *ep, ESEventTargetUpgrade *target)
{
	GConfClient *client;
	gchar       *mailer;
	GConfValue  *is_key;

	client = gconf_client_get_default ();
	
	/* See whether the check default mailer key has already been set */
	is_key = gconf_client_get(client, GCONF_KEY_CHECKDEFAULT, NULL);
	if(!is_key)
		gconf_client_set_bool(client, GCONF_KEY_CHECKDEFAULT, TRUE, NULL);
	g_free(is_key);
	
	/* Check whether we're supposed to check whether or not we are the default mailer */
	if(gconf_client_get_bool(client, GCONF_KEY_CHECKDEFAULT, NULL)) { 
		mailer  = gconf_client_get_string(client, GCONF_KEY_MAILTO_COMMAND, NULL);

		/* Check whether we are the default mailer */
		if(mailer == NULL || (strcmp(mailer, "@evolution %s") != 0 && strcmp(mailer, "evolution %s") != 0)) { 
			/* Ask whether we should be the default mailer */
			if(em_utils_prompt_user(NULL, GCONF_KEY_CHECKDEFAULT, "org.gnome.default.mailer:check-default", NULL)) {
				gconf_client_set_bool(client, GCONF_KEY_MAILTO_ENABLED, TRUE, NULL);
				gconf_client_set_string(client, GCONF_KEY_MAILTO_COMMAND, "evolution %s", NULL);
			}
		}
		
		g_free(mailer);
	}
}
