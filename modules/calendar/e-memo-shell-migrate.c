/*
 * e-memo-shell-migrate.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-memo-shell-migrate.h"

#include <string.h>
#include <glib/gi18n.h>
#include <camel/camel.h>
#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-group.h>
#include <libedataserver/e-source-list.h>

#include "calendar/gui/calendar-config.h"
#include "calendar/gui/calendar-config-keys.h"
#include "shell/e-shell.h"

#define LOCAL_BASE_URI "local:"
#define WEBCAL_BASE_URI "webcal://"
#define PERSONAL_RELATIVE_URI "system"
#define GROUPWISE_BASE_URI "groupwise://"

static void
create_memo_sources (EShellBackend *shell_backend,
		     ESourceList *source_list,
		     ESourceGroup **on_this_computer,
		     ESourceGroup **on_the_web,
		     ESource **personal_source)
{
	EShell *shell;
	EShellSettings *shell_settings;
	GSList *groups;
	ESourceGroup *group;

	*on_this_computer = NULL;
	*on_the_web = NULL;
	*personal_source = NULL;

	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	groups = e_source_list_peek_groups (source_list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		GSList *g;
		gchar *base_dir, *base_uri;

		base_dir = g_build_filename (e_shell_backend_get_data_dir (shell_backend), "local", NULL);
		base_uri = g_filename_to_uri (base_dir, NULL, NULL);

		for (g = groups; g; g = g->next) {
			group = E_SOURCE_GROUP (g->data);

			if (strcmp (base_uri, e_source_group_peek_base_uri (group)) == 0)
				e_source_group_set_base_uri (group, LOCAL_BASE_URI);

			if (!*on_this_computer && !strcmp (LOCAL_BASE_URI, e_source_group_peek_base_uri (group)))
				*on_this_computer = g_object_ref (group);
			else if (!*on_the_web && !strcmp (WEBCAL_BASE_URI, e_source_group_peek_base_uri (group)))
				*on_the_web = g_object_ref (group);
		}

		g_free (base_dir);
		g_free (base_uri);
	}

	if (*on_this_computer) {
		/* make sure "Personal" shows up as a source under
		   this group */
		GSList *sources = e_source_group_peek_sources (*on_this_computer);
		GSList *s;
		for (s = sources; s; s = s->next) {
			ESource *source = E_SOURCE (s->data);
			const gchar *relative_uri;

			relative_uri = e_source_peek_relative_uri (source);
			if (relative_uri == NULL)
				continue;
			if (!strcmp (PERSONAL_RELATIVE_URI, relative_uri)) {
				*personal_source = g_object_ref (source);
				break;
			}
		}
	} else {
		/* create the local source group */
		group = e_source_group_new (_("On This Computer"), LOCAL_BASE_URI);
		e_source_list_add_group (source_list, group, -1);

		*on_this_computer = group;
	}

	if (!*personal_source) {
		gchar *primary_memo_list;

		/* Create the default Person memo list */
		ESource *source = e_source_new (_("Personal"), PERSONAL_RELATIVE_URI);
		e_source_group_add_source (*on_this_computer, source, -1);

		primary_memo_list = e_shell_settings_get_string (
			shell_settings, "cal-primary-memo-list");

		if (!primary_memo_list && !calendar_config_get_memos_selected ()) {
			GSList selected;

			e_shell_settings_set_string (
				shell_settings, "cal-primary-memo-list",
				e_source_peek_uid (source));

			selected.data = (gpointer)e_source_peek_uid (source);
			selected.next = NULL;
			calendar_config_set_memos_selected (&selected);
		}

		e_source_set_color_spec (source, "#BECEDD");
		*personal_source = source;
	}

	if (!*on_the_web) {
		/* Create the Webcal source group */
		group = e_source_group_new (_("On The Web"), WEBCAL_BASE_URI);
		e_source_list_add_group (source_list, group, -1);

		*on_the_web = group;
	}
}

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
add_gw_esource (ESourceList *source_list, const gchar *group_name,  const gchar *source_name, CamelURL *url, GConfClient *client)
{
	ESourceGroup *group;
	ESource *source;
	GSList *ids, *temp;
	GError *error = NULL;
	gchar *relative_uri;
	const gchar *soap_port;
	const gchar * use_ssl;
	const gchar *poa_address;
	const gchar *offline_sync;

	poa_address = url->host;
	if (!poa_address || strlen (poa_address) ==0)
		return;
	soap_port = camel_url_get_param (url, "soap_port");

	if (soap_port == NULL || *soap_port == '\0')
		soap_port = "7191";

	use_ssl = camel_url_get_param (url, "use_ssl");
	offline_sync = camel_url_get_param (url, "offline_sync");

	group = e_source_group_new (group_name,  GROUPWISE_BASE_URI);
	if (!e_source_list_add_group (source_list, group, -1))
		return;
	relative_uri = g_strdup_printf ("%s@%s/", url->user, poa_address);

	source = e_source_new (source_name, relative_uri);
	e_source_set_property (source, "auth", "1");
	e_source_set_property (source, "username", url->user);
	e_source_set_property (source, "port", soap_port);
	e_source_set_property (source, "auth-domain", "Groupwise");
	e_source_set_property (source, "use_ssl", use_ssl);
	e_source_set_property (source, "offline_sync", offline_sync ? "1" : "0" );

	e_source_set_color_spec (source, "#EEBC60");
	e_source_group_add_source (group, source, -1);

	ids = gconf_client_get_list (client, CALENDAR_CONFIG_MEMOS_SELECTED_MEMOS, GCONF_VALUE_STRING, &error);
	if (error != NULL) {
		g_warning("%s (%s) %s\n", G_STRLOC, G_STRFUNC, error->message);
		g_error_free (error);
	}
	ids = g_slist_append (ids, g_strdup (e_source_peek_uid (source)));
	gconf_client_set_list (client, CALENDAR_CONFIG_MEMOS_SELECTED_MEMOS, GCONF_VALUE_STRING, ids, NULL);
	temp  = ids;
	for (; temp != NULL; temp = g_slist_next (temp))
		g_free (temp->data);

	g_slist_free (ids);
	g_object_unref (source);
	g_object_unref (group);
	g_free (relative_uri);
}

gboolean
e_memo_shell_backend_migrate (EShellBackend *shell_backend,
                             gint major,
                             gint minor,
                             gint revision,
                             GError **error)
{
	ESourceGroup *on_this_computer = NULL;
	ESourceGroup *on_the_web = NULL;
	ESource *personal_source = NULL;
	ESourceList *source_list = NULL;
	gboolean retval = FALSE;

	g_object_get (shell_backend, "source-list", &source_list, NULL);

	/* we call this unconditionally now - create_groups either
	   creates the groups/sources or it finds the necessary
	   groups/sources. */
	create_memo_sources (
		shell_backend, source_list, &on_this_computer,
		&on_the_web, &personal_source);

	/* Migration for Gw accounts between versions < 2.8 */
	if (major == 2 && minor < 8) {
		EAccountList *al;
		EAccount *a;
		CamelURL *url;
		EIterator *it;
		GConfClient *gconf_client = gconf_client_get_default ();
		al = e_account_list_new (gconf_client);
		for (it = e_list_get_iterator ((EList *)al);
				e_iterator_is_valid (it);
				e_iterator_next (it)) {
			a = (EAccount *) e_iterator_get (it);
			if (!a->enabled || !is_groupwise_account (a))
				continue;
			url = camel_url_new (a->source->url, NULL);
			add_gw_esource (source_list, a->name, _("Notes"), url, gconf_client);
			camel_url_free (url);
		}
		g_object_unref (al);
		g_object_unref (gconf_client);
	}

	e_source_list_sync (source_list, NULL);
	retval = TRUE;

	if (on_this_computer)
		g_object_unref (on_this_computer);
	if (on_the_web)
		g_object_unref (on_the_web);
	if (personal_source)
		g_object_unref (personal_source);

        return retval;
}
