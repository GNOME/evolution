/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <libsoup/soup.h>

#include "e-proxy.h"

static void
set_proxy (GConfClient *client)
{
	SoupContext *context;
	char *proxy_server, *proxy_user, *proxy_pw, *uri;
	gboolean use_auth, use_proxy;
	int proxy_port;
	gboolean new_proxy_exists;

	new_proxy_exists = gconf_client_dir_exists (client, "/system/http_proxy", NULL);

	if (new_proxy_exists) {
		use_proxy = gconf_client_get_bool (client, "/system/http_proxy/use_http_proxy", NULL);
	} else {
		use_proxy = gconf_client_get_bool (client, "/system/gnome-vfs/use-http-proxy", NULL);
	}

	if (use_proxy == FALSE) {
		return;
	}

	if (new_proxy_exists) {
		proxy_server = gconf_client_get_string (client, "/system/http_proxy/host", NULL);
		proxy_port = gconf_client_get_int (client, "/system/http_proxy/port", NULL);

		use_auth = gconf_client_get_bool (client, "/system/http_proxy/use_authentication", NULL);
	} else {
		proxy_server = gconf_client_get_string (client, "/system/gnome-vfs/http-proxy-host", NULL);
		proxy_port = gconf_client_get_int (client, "/system/gnome-vfs/http-proxy-port", NULL);
	
		use_auth = gconf_client_get_bool (client, "/system/gnome-vfs/use-http-proxy-authorization", NULL);
	}

	if (use_auth == TRUE) {
		if (new_proxy_exists) {
			proxy_user = gconf_client_get_string (client, "/system/http_proxy/authentication_user", NULL);
			proxy_pw = gconf_client_get_string (client, "/system/http_proxy/authentication_password", NULL);
		} else {
			proxy_user = gconf_client_get_string (client, "/system/gnome-vfs/http-proxy-authorization-user", NULL);
			proxy_pw = gconf_client_get_string (client, "/system/gnome-vfs/http-proxy-authorization-password", NULL);
		}
		uri = g_strdup_printf ("http://%s:%s@%s:%d", proxy_user, proxy_pw, proxy_server, proxy_port);
	} else {
		uri = g_strdup_printf ("http://%s:%d", proxy_server, proxy_port);
	}
	
	context = soup_context_get (uri);
	soup_set_proxy (context);
	soup_context_unref (context);
	g_free (uri);
}

static void
proxy_setting_changed (GConfClient *client, guint32 cnxn_id,
		       GConfEntry *entry, gpointer user_data)
{
	set_proxy (client);
}

void
e_proxy_init ()
{
	GConfClient *client;
	gboolean new_proxy_exists;

	/* We get the gnome-vfs proxy keys here
	   set soup up to use the proxy,
	   and listen to any changes */
	
	if (!(client = gconf_client_get_default ()))
		return;
	
	new_proxy_exists = gconf_client_dir_exists (client, "/system/http_proxy", NULL);

	/* Listen to the changes in the gnome-vfs path */
	if (new_proxy_exists) {
		gconf_client_add_dir (client, "/system/http_proxy",
				      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
		gconf_client_notify_add (client, "/system/http_proxy/",
					 proxy_setting_changed, NULL,
					 NULL, NULL);
	} else {
		gconf_client_add_dir (client, "/system/gnome-vfs",
				      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
		gconf_client_notify_add (client, "/system/gnome-vfs/",
					 proxy_setting_changed, NULL,
					 NULL, NULL);
	}
	
	set_proxy (client);
}
