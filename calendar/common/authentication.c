/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Authors : 
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2003, Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include "e-util/e-passwords.h"
#include "authentication.h"

static char *
auth_func_cb (ECal *ecal, const char *prompt, const char *key, gpointer user_data)
{
	gboolean remember;
	char *password;
	ESource *source;
	gchar *auth_domain;
	gchar *component_name;

	source = e_cal_get_source (ecal);
	auth_domain = e_source_get_property (source, "auth-domain");
	component_name = auth_domain ? auth_domain : "Calendar";
	password = e_passwords_get_password (component_name, key);
	
	if (!password)
		password = e_passwords_ask_password (_("Enter password"), component_name, key, prompt, TRUE,
						     E_PASSWORDS_REMEMBER_FOREVER, &remember,
						     NULL);

	return password;
}

ECal *
auth_new_cal_from_default (ECalSourceType type)
{
	ECal *ecal = NULL;
	
	if (!e_cal_open_default (&ecal, type, auth_func_cb, NULL, NULL))
		return NULL;


	return ecal;
}

ECal *
auth_new_cal_from_source (ESource *source, ECalSourceType type)
{
	ECal *cal;

	cal = e_cal_new (source, type);
	if (cal)
		e_cal_set_auth_func (cal, (ECalAuthFunc) auth_func_cb, NULL);

	return cal;
}

ECal *
auth_new_cal_from_uri (const char *uri, ECalSourceType type)
{
	ESourceGroup *group;
	ESource *source;
	ECal *cal;

	group = e_source_group_new ("", uri);
	source = e_source_new ("", "");
	e_source_set_group (source, group);

	/* we explicitly check for groupwise:// uris, to force authentication on them */
	if (!strncmp (uri, "groupwise://", strlen ("groupwise://"))) {
		e_source_set_property (source, "auth", "yes");
		/* FIXME: need to retrieve the username */
	}

	cal = auth_new_cal_from_source (source, type);

	g_object_unref (source);
	g_object_unref (group);

	return cal;
}
