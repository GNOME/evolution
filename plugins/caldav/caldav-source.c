/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 * ex: set ts=8: */
/* Evolution calendar - caldav backend
 *
 * Copyright (C) 2005 Novell, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Christian Kellner <gicmo@gnome.org> 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <glib.h>

#include <gtk/gtk.h>

#include <e-util/e-config.h>
#include <e-util/e-plugin.h>
#include <calendar/gui/e-cal-config.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-account-list.h>
#include <libecal/e-cal.h>

#include <string.h>

/*****************************************************************************/
/* prototypes */
int              e_plugin_lib_enable      (EPluginLib                 *ep,
					   int                         enable);

GtkWidget *      oge_caldav               (EPlugin                    *epl, 
					   EConfigHookItemFactoryData *data);

/*****************************************************************************/
/* plugin intialization */

static void
ensure_caldav_source_group ()
{
	ESourceList  *slist;
	ESourceGroup *group;

	
	if (!e_cal_get_sources (&slist, E_CAL_SOURCE_TYPE_EVENT, NULL)) {
		g_warning ("Could not get calendar source list from GConf!");
		return;
	}

	group = e_source_list_peek_group_by_name (slist, _("CalDAV"));

	if (group == NULL) {
		gboolean res;
		group = e_source_group_new (_("CalDAV"), "caldav://");
		res = e_source_list_add_group (slist, group, -1);
		
		if (res == FALSE) {
			g_warning ("Could not add CalDAV source group!");	
		} else {
			e_source_list_sync (slist, NULL);
		}
		
		g_object_unref (group);
		g_object_unref (slist);
	}
}

int
e_plugin_lib_enable (EPluginLib *ep, int enable)
{

	if (enable) {
		g_print ("CalDAV Eplugin starting up ...\n");
		ensure_caldav_source_group ();
	}

	return 0;
}


/*****************************************************************************/
/* the location field for caldav sources */

/* stolen from calendar-weather eplugin */
static gchar *
print_uri_noproto (EUri *uri)
{
	gchar *uri_noproto;

	if (uri->port != 0)
		uri_noproto = g_strdup_printf (
			"%s%s%s%s%s%s%s:%d%s%s%s",
			uri->user ? uri->user : "",
			uri->authmech ? ";auth=" : "",
			uri->authmech ? uri->authmech : "",
			uri->passwd ? ":" : "",
			uri->passwd ? uri->passwd : "",
			uri->user ? "@" : "",
			uri->host ? uri->host : "",
			uri->port,
			uri->path ? uri->path : "",
			uri->query ? "?" : "",
			uri->query ? uri->query : "");
	else
		uri_noproto = g_strdup_printf (
			"%s%s%s%s%s%s%s%s%s%s",
			uri->user ? uri->user : "",
			uri->authmech ? ";auth=" : "",
			uri->authmech ? uri->authmech : "",
			uri->passwd ? ":" : "",
			uri->passwd ? uri->passwd : "",
			uri->user ? "@" : "",
			uri->host ? uri->host : "",
			uri->path ? uri->path : "",
			uri->query ? "?" : "",
			uri->query ? uri->query : "");
	return uri_noproto;
}

static void
location_changed (GtkEntry *editable, ESource *source)
{
	EUri       *euri;
	char       *ruri;
	const char *uri;

	uri = gtk_entry_get_text (GTK_ENTRY (editable));
	
	euri = e_uri_new (uri);
	ruri = print_uri_noproto (euri);
	e_source_set_relative_uri (source, ruri);
	g_free (ruri);
	e_uri_free (euri);
}

static void
ssl_changed (GtkToggleButton *button, ESource *source)
{
	e_source_set_property(source, "ssl", 
			      gtk_toggle_button_get_active(button) ? "1" : "0");	
}

static void
user_changed (GtkEntry *editable, ESource *source)
{
	EUri       *euri;
	char       *uri;
	char       *ruri;
	const char *user;

	uri = e_source_get_uri (source); 
	user = gtk_entry_get_text (GTK_ENTRY (editable));
	
	if (uri == NULL) {
		g_free (uri);
		return;
	}
	
	euri = e_uri_new (uri);
	g_free (euri->user);

	if (user != NULL) {
		euri->user = g_strdup (user);
		e_source_set_property (source, "auth", "1");
	} else {
		e_source_set_property (source, "auth", NULL);
	}

	e_source_set_property (source, "username", euri->user);	
	ruri = print_uri_noproto (euri);
	e_source_set_relative_uri (source, ruri);
	g_free (ruri);
	e_uri_free (euri);
}

GtkWidget *
oge_caldav  (EPlugin                    *epl, 
	     EConfigHookItemFactoryData *data)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource      *source;
	ESourceGroup *group;
	EUri         *euri;
	GtkWidget    *parent;
	GtkWidget    *lurl;
	GtkWidget    *cssl;
	GtkWidget    *location;
	GtkWidget    *widget;
	GtkWidget    *luser;
	GtkWidget    *user;
	char         *uri;
	char         *username;
	const char   *ssl_prop;
	gboolean      ssl_enabled;
	int           row;
	
	source = t->source;
	group = e_source_peek_group (source);
	
	widget = NULL;
	
	if (!g_str_has_prefix (e_source_group_peek_base_uri (group), 
			       "caldav")) {
		return NULL;
	}
	
	/* Extract the username from the uri so we can prefill the
	 * dialog right, remove the username from the url then */
	uri = e_source_get_uri (source);
	euri = e_uri_new (uri);
	g_free (uri);

	if (euri == NULL) {
		return NULL;
	}
	
	username = euri->user;
	euri->user = NULL;
	uri = e_uri_to_string (euri, FALSE);

	ssl_prop = e_source_get_property (source, "ssl");	
	if (ssl_prop && ssl_prop[0] == '1') {
		ssl_enabled = TRUE;
	} else {
		ssl_enabled = FALSE;
	}

	/* Build up the UI */
	parent = data->parent;
	
	row = GTK_TABLE (parent)->nrows;

	lurl = gtk_label_new_with_mnemonic (_("_URL:"));
	gtk_widget_show (lurl);
	gtk_misc_set_alignment (GTK_MISC (lurl), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent), 
			  lurl, 0, 1, 
			  row, row+1, 
			  GTK_FILL, 0, 0, 0);

	location = gtk_entry_new ();
	gtk_widget_show (location);
	gtk_entry_set_text (GTK_ENTRY (location), uri);
	gtk_table_attach (GTK_TABLE (parent), location,
			  1, 2, row, row+1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);

	gtk_label_set_mnemonic_widget (GTK_LABEL (lurl), location);
	
	g_signal_connect (G_OBJECT (location), 
			  "changed", 
			  G_CALLBACK (location_changed),
			  source);

	cssl = gtk_check_button_new_with_mnemonic (_("Use _SSL"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cssl), ssl_enabled);
	gtk_widget_show (cssl);
	gtk_table_attach (GTK_TABLE (parent), 
			  cssl, 1, 2, 
			  row + 1, row + 2, 
			  GTK_FILL, 0, 0, 0);

	g_signal_connect (G_OBJECT (cssl), 
			  "toggled", 
			  G_CALLBACK (ssl_changed),
			  source);

	luser = gtk_label_new_with_mnemonic (_("User_name:"));
	gtk_widget_show (luser);
	gtk_misc_set_alignment (GTK_MISC (luser), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent), 
			  luser, 0, 1, 
			  row + 2, row + 3, 
			  GTK_FILL, 0, 0, 0);

	user = gtk_entry_new ();
	gtk_widget_show (user);
	gtk_entry_set_text (GTK_ENTRY (user), username);
	gtk_table_attach (GTK_TABLE (parent), user,
			  1, 2, row + 2, row + 3,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);

	gtk_label_set_mnemonic_widget (GTK_LABEL (luser), user);
	
	g_signal_connect (G_OBJECT (user), 
			  "changed", 
			  G_CALLBACK (user_changed),
			  source);
	
	
        g_free (uri);
	g_free (username);
	
	return widget;	
}


