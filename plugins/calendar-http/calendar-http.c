/*
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <gtk/gtk.h>
#include <e-util/e-config.h>
#include <e-util/e-plugin-util.h>
#include <calendar/gui/e-cal-config.h>
#include <libedataserver/e-source.h>
#include <libsoup/soup.h>
#include <glib/gi18n.h>
#include <string.h>

GtkWidget *e_calendar_http_url (EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *e_calendar_http_refresh (EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean   e_calendar_http_check (EPlugin *epl, EConfigHookPageCheckData *data);
GtkWidget * e_calendar_http_secure (EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *e_calendar_http_auth (EPlugin *epl, EConfigHookItemFactoryData *data);

gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
	return 0;
}

static void
url_changed (GtkEntry *entry, ESource *source)
{
	SoupURI *uri;
	gchar *relative_uri;

	uri = soup_uri_new (gtk_entry_get_text (GTK_ENTRY (entry)));

	if (!uri)
		return;

	if (uri->scheme && strncmp (uri->scheme, "https", sizeof ("https") - 1) == 0) {
		gpointer secure_checkbox;

		secure_checkbox = g_object_get_data (G_OBJECT (gtk_widget_get_parent (GTK_WIDGET (entry))),
						     "secure_checkbox");

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (secure_checkbox), TRUE);
	}

	soup_uri_set_user (uri, e_source_get_property (source, "username"));
	relative_uri = e_plugin_util_uri_no_proto (uri);
	e_source_set_relative_uri (source, relative_uri);
	g_free (relative_uri);
	soup_uri_free (uri);
}

GtkWidget *
e_calendar_http_url (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	GtkWidget *entry;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	SoupURI *uri;
	gchar *uri_text;

	if ((!e_plugin_util_is_source_proto (t->source, "http") &&
	     !e_plugin_util_is_source_proto (t->source, "https") &&
	     !e_plugin_util_is_source_proto (t->source, "webcal"))) {
		return NULL;
	}

	uri_text = e_source_get_uri (t->source);
	uri = soup_uri_new (uri_text);
	g_free (uri_text);

	if (uri) {
		soup_uri_set_user (uri, NULL);
		soup_uri_set_password (uri, NULL);

		uri_text = soup_uri_to_string (uri, FALSE);
		soup_uri_free (uri);
	} else {
		uri_text = g_strdup ("");
	}

	entry = e_plugin_util_add_entry (data->parent, _("_URL:"), NULL, NULL);
	gtk_entry_set_text (GTK_ENTRY (entry), uri_text);
	g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (url_changed), t->source);
	g_free (uri_text);

	return entry;
}

GtkWidget *
e_calendar_http_refresh (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;

	if ((!e_plugin_util_is_source_proto (t->source, "http") &&
	     !e_plugin_util_is_source_proto (t->source, "https") &&
	     !e_plugin_util_is_source_proto (t->source, "webcal"))) {
		return NULL;
	}

	return e_plugin_util_add_refresh (data->parent, _("Re_fresh:"), t->source, "refresh");
}

GtkWidget *
e_calendar_http_secure (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	GtkWidget *secure_setting;

	if ((!e_plugin_util_is_source_proto (t->source, "http") &&
	     !e_plugin_util_is_source_proto (t->source, "https") &&
	     !e_plugin_util_is_source_proto (t->source, "webcal"))) {
		return NULL;
	}

	secure_setting = e_plugin_util_add_check (data->parent, _("_Secure connection"), t->source, "use_ssl", "1", "0");

	/* Store pointer to secure checkbox so we can retrieve it in url_changed() */
	g_object_set_data (G_OBJECT (data->parent), "secure_checkbox", (gpointer)secure_setting);

	return secure_setting;
}

static void
username_changed (GtkEntry *entry, ESource *source)
{
	const gchar *username;
	gchar *uri;

	username = gtk_entry_get_text (GTK_ENTRY (entry));

	if (username && username[0]) {
		e_source_set_property (source, "auth", "1");
		e_source_set_property (source, "username", username);
	} else {
		e_source_set_property (source, "auth", NULL);
		e_source_set_property (source, "username", NULL);
		username = NULL;
	}

	uri = e_source_get_uri (source);
	if (uri != NULL) {
		SoupURI *suri;
		gchar *ruri;

		suri = soup_uri_new (uri);
		if (!suri)
			return;

		soup_uri_set_user (suri, username);
		soup_uri_set_password (suri, NULL);

		ruri = e_plugin_util_uri_no_proto (suri);
		e_source_set_relative_uri (source, ruri);
		soup_uri_free (suri);
		g_free (ruri);
		g_free (uri);
	}
}

GtkWidget *
e_calendar_http_auth (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	GtkWidget *entry;
	const gchar *username;

	if ((!e_plugin_util_is_source_proto (t->source, "http") &&
	     !e_plugin_util_is_source_proto (t->source, "https") &&
	     !e_plugin_util_is_source_proto (t->source, "webcal"))) {
		return NULL;
	}

	username = e_source_get_property (t->source, "username");

	entry = e_plugin_util_add_entry (data->parent, _("Userna_me:"), NULL, NULL);
	gtk_entry_set_text (GTK_ENTRY (entry), username ? username : "");
	g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (username_changed), t->source);

	return entry;
}

gboolean
e_calendar_http_check (EPlugin *epl, EConfigHookPageCheckData *data)
{
	/* FIXME - check pageid */
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	gboolean ok = FALSE;

	if (!e_plugin_util_is_group_proto (e_source_peek_group (t->source), "webcal"))
		return TRUE;

	if (e_plugin_util_is_source_proto (t->source, "file"))
		return FALSE;

	ok = e_plugin_util_is_source_proto (t->source, "webcal") ||
	     e_plugin_util_is_source_proto (t->source, "http")   ||
	     e_plugin_util_is_source_proto (t->source, "https")  ||
	     e_plugin_util_is_source_proto (t->source, "file");

	return ok;
}
