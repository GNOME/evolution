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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-passwords.h>
#include "authentication.h"
#include <libedataserver/e-url.h>

static GHashTable *source_lists_hash = NULL;

static gchar *
auth_func_cb (ECal *ecal,
              const gchar *prompt,
              const gchar *key,
              gpointer user_data)
{
	gboolean remember;
	gchar *password, *auth_domain;
	ESource *source;
	const gchar *component_name;

	source = e_cal_get_source (ecal);
	auth_domain = e_source_get_duped_property (source, "auth-domain");
	component_name = auth_domain ? auth_domain : "Calendar";
	password = e_passwords_get_password (component_name, key);

	if (!password)
		password = e_passwords_ask_password (
			_("Enter password"),
			component_name, key, prompt,
			E_PASSWORDS_REMEMBER_FOREVER |
			E_PASSWORDS_SECRET |
			E_PASSWORDS_ONLINE,
			&remember, NULL);

	g_free (auth_domain);

	return password;
}

static gchar *
build_pass_key (ECal *ecal)
{
	gchar *euri_str;
	const gchar *uri;
	EUri *euri;

	uri = e_cal_get_uri (ecal);

	euri = e_uri_new (uri);
	euri_str = e_uri_to_string (euri, FALSE);

	e_uri_free (euri);
	return euri_str;
}

void
e_auth_cal_forget_password (ECal *ecal)
{
	ESource *source = NULL;
	const gchar *auth_domain = NULL, *component_name = NULL,  *auth_type = NULL;

	source = e_cal_get_source (ecal);
	auth_domain = e_source_get_property (source, "auth-domain");
	component_name = auth_domain ? auth_domain : "Calendar";

	auth_type = e_source_get_property (source, "auth-type");
	if (auth_type) {
		gchar *key = NULL;

		key = build_pass_key (ecal);
		e_passwords_forget_password (component_name, key);
		g_free (key);
	}

	e_passwords_forget_password (component_name, e_source_get_uri (source));
}

ECal *
e_auth_new_cal_from_default (ECalSourceType type)
{
	ECal *ecal = NULL;

	if (!e_cal_open_default (&ecal, type, auth_func_cb, NULL, NULL))
		return NULL;

	return ecal;
}

ECal *
e_auth_new_cal_from_source (ESource *source, ECalSourceType type)
{
	ECal *cal;

	cal = e_cal_new (source, type);
	if (cal)
		e_cal_set_auth_func (cal, (ECalAuthFunc) auth_func_cb, NULL);

	return cal;
}
