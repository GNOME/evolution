/*
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
 *		Christian Kellner <gicmo@gnome.org>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <glib.h>

#include <gtk/gtk.h>

#include <e-util/e-config.h>
#include <e-util/e-plugin.h>
#include <e-util/e-plugin-util.h>
#include <calendar/gui/e-cal-config.h>
#include <libedataserver/e-account-list.h>
#include <libecal/e-cal.h>

#include <string.h>

#include "caldav-browse-server.h"

#define d(x)

/*****************************************************************************/
/* prototypes */
gint              e_plugin_lib_enable      (EPlugin                 *ep,
					   gint                         enable);

GtkWidget *      oge_caldav               (EPlugin                    *epl,
					   EConfigHookItemFactoryData *data);

/*****************************************************************************/
/* plugin intialization */

static void
ensure_caldav_source_group (ECalSourceType source_type)
{
	ESourceList  *slist;

	if (!e_cal_get_sources (&slist, source_type, NULL)) {
		g_warning ("Could not get calendar source list from GConf!");
		return;
	}

	e_source_list_ensure_group (slist, _("CalDAV"), "caldav://", FALSE);
	g_object_unref (slist);
}

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{

	if (enable) {
		d(g_print ("CalDAV Eplugin starting up ...\n"));
		ensure_caldav_source_group (E_CAL_SOURCE_TYPE_EVENT);
		ensure_caldav_source_group (E_CAL_SOURCE_TYPE_TODO);
		ensure_caldav_source_group (E_CAL_SOURCE_TYPE_JOURNAL);
	}

	return 0;
}

/*****************************************************************************/

static void
location_changed_cb (GtkEntry *editable, ESource *source)
{
	SoupURI     *suri;
	gchar       *ruri;
	const gchar *uri, *username;

	uri = gtk_entry_get_text (editable);

	suri = soup_uri_new (uri);
	if (!suri)
		return;

	username = e_source_get_property (source, "username");
	if (username && !*username)
		username = NULL;

	soup_uri_set_user (suri, username);

	ruri = e_plugin_util_uri_no_proto (suri);
	e_source_set_relative_uri (source, ruri);
	g_free (ruri);
	soup_uri_free (suri);
}

static void
user_changed_cb (GtkEntry *editable, ESource *source)
{
	SoupURI     *suri;
	gchar       *uri, *ruri;
	const gchar *user;

	uri = e_source_get_uri (source);
	user = gtk_entry_get_text (editable);

	if (uri == NULL)
		return;

	suri = soup_uri_new (uri);
	g_free (uri);

	if (suri == NULL)
		return;

	soup_uri_set_user (suri, NULL);

	if (user != NULL && *user) {
		soup_uri_set_user (suri, user);
		e_source_set_property (source, "auth", "1");
	} else {
		e_source_set_property (source, "auth", NULL);
	}

	e_source_set_property (source, "username", user);
	ruri = e_plugin_util_uri_no_proto (suri);
	e_source_set_relative_uri (source, ruri);
	g_free (ruri);
	soup_uri_free (suri);
}

static void
browse_cal_clicked_cb (GtkButton *button, gpointer user_data)
{
	GtkEntry *url, *username;
	GtkToggleButton *ssl;
	gchar *new_url;

	g_return_if_fail (button != NULL);

	url = g_object_get_data (G_OBJECT (button), "caldav-url");
	username = g_object_get_data (G_OBJECT (button), "caldav-username");
	ssl = g_object_get_data (G_OBJECT (button), "caldav-ssl");

	g_return_if_fail (url != NULL);
	g_return_if_fail (GTK_IS_ENTRY (url));
	g_return_if_fail (username != NULL);
	g_return_if_fail (GTK_IS_ENTRY (username));
	g_return_if_fail (ssl != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (ssl));

	new_url = caldav_browse_server (
		GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
		gtk_entry_get_text (url),
		gtk_entry_get_text (username),
		gtk_toggle_button_get_active (ssl),
		GPOINTER_TO_INT (user_data));

	if (new_url) {
		gtk_entry_set_text (url, new_url);
		g_free (new_url);
	}
}

GtkWidget *
oge_caldav  (EPlugin                    *epl,
	     EConfigHookItemFactoryData *data)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource      *source;
	SoupURI      *suri;
	GtkWidget    *parent, *location, *ssl, *user, *browse_cal;
	gchar        *uri, *username;
	guint         n_rows;

	source = t->source;

	if (!e_plugin_util_is_group_proto (e_source_peek_group (source), "caldav")) {
		return NULL;
	}

	/* Extract the username from the uri so we can prefill the
	 * dialog right, remove the username from the url then */
	uri = e_source_get_uri (source);
	suri = soup_uri_new (uri);
	g_free (uri);

	if (suri) {
		soup_uri_set_user (suri, NULL);
		soup_uri_set_password (suri, NULL);
		uri = soup_uri_to_string (suri, FALSE);
		soup_uri_free (suri);
	} else {
		uri = g_strdup ("");
	}

	username = e_source_get_duped_property (source, "username");

	/* Build up the UI */
	parent = data->parent;

	location = e_plugin_util_add_entry (parent, _("_URL:"), NULL, NULL);
	gtk_entry_set_text (GTK_ENTRY (location), uri);

	g_signal_connect (
		location, "changed",
		G_CALLBACK (location_changed_cb), source);

	ssl = e_plugin_util_add_check (parent, _("Use _SSL"), source, "ssl", "1", "0");

	user = e_plugin_util_add_entry (parent, _("User_name:"), NULL, NULL);
	gtk_entry_set_text (GTK_ENTRY (user), username ? username : "");

	g_signal_connect (
		user, "changed",
		G_CALLBACK (user_changed_cb), source);

        g_free (uri);
	g_free (username);

	browse_cal = gtk_button_new_with_mnemonic (_("Brows_e server for a calendar"));
	gtk_widget_show (browse_cal);
	g_object_get (parent, "n-rows", &n_rows, NULL);
	gtk_table_attach (
		GTK_TABLE (parent), browse_cal, 1, 2,
		n_rows, n_rows + 1, GTK_FILL, 0, 0, 0);

	g_object_set_data (G_OBJECT (browse_cal), "caldav-url", location);
	g_object_set_data (G_OBJECT (browse_cal), "caldav-username", user);
	g_object_set_data (G_OBJECT (browse_cal), "caldav-ssl", ssl);

	g_signal_connect  (
		browse_cal, "clicked",
		G_CALLBACK (browse_cal_clicked_cb),
		GINT_TO_POINTER (t->source_type));

	e_plugin_util_add_refresh (parent, _("Re_fresh:"), source, "refresh");

	return location;
}
