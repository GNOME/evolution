/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Sivaiah Nallagatla <snallagatla@novell.com>
 *  Copyright (C) 2004 Novell, Inc.
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */


#include "camel-gw-listener.h"
#include <gtk/gtk.h>
#include "mail/em-config.h"
#include <gconf/gconf-client.h>
#include "shell/es-event.h"
#include <string.h>

#define GROUPWISE_BASE_URI "groupwise://"

static CamelGwListener *config_listener = NULL;

int e_plugin_lib_enable (EPluginLib *ep, int enable);
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
set_esource_props (const char *path, EAccount *a, GConfClient *client, const char *name)
{
	ESourceList *list;
        GSList *groups;
	char *old_relative_uri;
	const char *poa_address;
	CamelURL *url = camel_url_new (a->source->url, NULL);
	
	old_relative_uri =  g_strdup_printf ("%s@%s/", url->user, poa_address);
        list = e_source_list_new_for_gconf (client, path);
	groups = e_source_list_peek_groups (list);
	poa_address = url->host;

	if (!poa_address || !*poa_address)
		return;

	for ( ; groups != NULL; groups = g_slist_next (groups)) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);

		if (strcmp (e_source_group_peek_name (group), name) == 0 &&
		    strcmp (e_source_group_peek_base_uri (group), GROUPWISE_BASE_URI) == 0) {
			GSList *sources = e_source_group_peek_sources (group);

			for ( ; sources != NULL; sources = g_slist_next (sources)) {
				ESource *source = E_SOURCE (sources->data);

				if (a->source->auto_check) {
					char *str = g_strdup_printf ("%d",a->source->auto_check_time);
				
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
	g_free (old_relative_uri);
	camel_url_free (url);

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

int
e_plugin_lib_enable (EPluginLib *ep, int enable)
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
