/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* main.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Iain Holmes
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkrgb.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-init.h>

#include <bonobo/bonobo-main.h>
#include <liboaf/liboaf.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <glade/glade.h>
#include <libsoup/soup.h>

#include "component-factory.h"

static void
set_proxy (GConfClient *client)
{
	SoupContext *context;
	char *proxy_server, *proxy_user, *proxy_pw, *uri;
	gboolean use_auth, use_proxy;
	int proxy_port;

	use_proxy = gconf_client_get_bool (client, "/system/gnome-vfs/use-http-proxy", NULL);
	if (use_proxy == FALSE) {
		return;
	}
	
	proxy_server = gconf_client_get_string (client,
						"/system/gnome-vfs/http-proxy-host", NULL);
	proxy_port = gconf_client_get_int (client,
					   "/system/gnome-vfs/http-proxy-port",
					   NULL);

	use_auth = gconf_client_get_bool (client,
					  "/system/gnome-vfs/use-http-proxy-authorization", NULL);
	if (use_auth == TRUE) {
		proxy_user = gconf_client_get_string (client,
						      "/system/gnome-vfs/http-proxy-authorization-user", NULL);
		proxy_pw = gconf_client_get_string (client,
						    "/system/gnome-vfs/http-proxy-authorization-password", NULL);

		uri = g_strdup_printf ("http://%s:%s@%s:%d",
				       proxy_user, proxy_pw, proxy_server,
				       proxy_port);
	} else {
		uri = g_strdup_printf ("http://%s:%d", proxy_server, proxy_port);
	}

	g_print ("Using proxy: %s\n", uri);
	context = soup_context_get (uri);
	soup_set_proxy (context);
	soup_context_unref (context);
	g_free (uri);
}

static void
proxy_setting_changed (GConfClient *client,
		       guint32 cnxn_id,
		       GConfEntry *entry,
		       gpointer user_data)
{
	set_proxy (client);
}

static void
init_soup_proxy (void)
{
	GConfClient *client;
	
	/* We get the gnome-vfs proxy keys here
	   set soup up to use the proxy,
	   and listen to any changes */

	client = gconf_client_get_default ();
	if (client == NULL) {
		return;
	}

	/* Listen to the changes in the gnome-vfs path */
	gconf_client_add_dir (client, "/system/gnome-vfs",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	
	gconf_client_notify_add (client, "/system/gnome-vfs/",
				 proxy_setting_changed, NULL, NULL, NULL);

	set_proxy (client);
}

int
main (int argc,
      char **argv)
{
	CORBA_ORB orb;

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	gnome_init_with_popt_table ("Evolution Executive Summary", VERSION,
				    argc, argv, oaf_popt_options, 0, NULL);
	orb = oaf_init (argc, argv);

	gdk_rgb_init ();
	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE) {
		g_error (_("Executive summary component could not initialize Bonobo.\n"));
		exit (1);
	}

	gconf_init (argc, argv, NULL);

	glade_gnome_init ();

	e_cursors_init ();

	init_soup_proxy ();
	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	/* Start our component */
	component_factory_init ();

	bonobo_main ();

	return 0;
}
