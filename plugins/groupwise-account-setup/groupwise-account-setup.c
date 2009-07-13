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
 *		Sivaiah Nallagatla <snallagatla@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "camel-gw-listener.h"
#include <gtk/gtk.h>
#include "mail/em-config.h"
#include <gconf/gconf-client.h>
#include "shell/es-event.h"
#include <string.h>

#define GROUPWISE_BASE_URI "groupwise://"

static CamelGwListener *config_listener = NULL;

gint e_plugin_lib_enable (EPluginLib *ep, gint enable);
GtkWidget* org_gnome_gw_account_setup(struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data);
void ensure_mandatory_esource_properties (EPlugin *ep, ESEventTargetUpgrade *target);

static gboolean
is_groupwise_account (EAccount *account)
{
	if (account->source->url != NULL) {
		return g_str_has_prefix (account->source->url, GROUPWISE_BASE_URI);
	} else {
		return FALSE;
	}
}

static void
set_esource_props (const gchar *path, EAccount *a, GConfClient *client, const gchar *name)
{
	ESourceList *list;
        GSList *groups;

        list = e_source_list_new_for_gconf (client, path);
	groups = e_source_list_peek_groups (list);

	for (; groups != NULL; groups = g_slist_next (groups)) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);

		if (strcmp (e_source_group_peek_name (group), name) == 0 &&
		    strcmp (e_source_group_peek_base_uri (group), GROUPWISE_BASE_URI) == 0) {
			GSList *sources = e_source_group_peek_sources (group);

			for (; sources != NULL; sources = g_slist_next (sources)) {
				ESource *source = E_SOURCE (sources->data);

				if (a->source->auto_check) {
					gchar *str = g_strdup_printf ("%d",a->source->auto_check_time);

					e_source_set_property (source, "refresh", str);
					g_free (str);
				} else
					e_source_set_property (source, "refresh", NULL);
				break;
			}
		}
	}
	e_source_list_sync (list, NULL);

	g_object_unref (list);
}

void
ensure_mandatory_esource_properties (EPlugin *ep, ESEventTargetUpgrade *target)
{
        GConfClient* client;
	EAccountList *al;
	EIterator *it;

	client = gconf_client_get_default();
	al = e_account_list_new (client);

	for (it = e_list_get_iterator((EList *)al);
			e_iterator_is_valid(it);
			e_iterator_next(it)) {
		EAccount *a;

		a = (EAccount *) e_iterator_get(it);
		if (!a->enabled || !is_groupwise_account (a))
			continue;
		set_esource_props ("/apps/evolution/calendar/sources", a, client, a->name);
		set_esource_props ("/apps/evolution/tasks/sources", a, client, a->name);
		set_esource_props ("/apps/evolution/memos/sources", a, client, a->name);
	}
	g_object_unref (al);
	g_object_unref (client);
}

static void
free_groupwise_listener ( void )
{
	g_object_unref (config_listener);
}

gint
e_plugin_lib_enable (EPluginLib *ep, gint enable)
{
	if (!config_listener) {
		config_listener = camel_gw_listener_new ();
		g_atexit ( free_groupwise_listener );
	}

	return 0;
}

GtkWidget * org_gnome_groupwise_account_setup(struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data);

GtkWidget *
org_gnome_groupwise_account_setup(struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data)
{
	if (data->old)
		return data->old;
        /* FIXME, with new soap camel provider we don't need extra settings in receiving options page, Remove them
	   from camel-groupwise-provider.c once soap provider is ready and add any groupwise sepcific settings like "add contacts automatically to Frequent contacts folder" here*/

	return NULL;
}
