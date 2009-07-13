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
 *		Harish Krishnaswamy <kharish@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "camel-hula-listener.h"
#include <gtk/gtk.h>
#include "mail/em-config.h"

static CamelHulaListener *config_listener = NULL;

gint e_plugin_lib_enable (EPluginLib *ep, gint enable);
GtkWidget* org_gnome_evolution_hula_account_setup (struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data);

static void
free_hula_listener ( void )
{
	g_object_unref (config_listener);
}

gint
e_plugin_lib_enable (EPluginLib *ep, gint enable)
{
	if (!config_listener) {
		config_listener = camel_hula_listener_new ();
		g_atexit ( free_hula_listener );
	}

	return 0;
}

GtkWidget *
org_gnome_evolution_hula_account_setup (struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data)
{
	if (data->old)
		return data->old;
	return NULL;
}
