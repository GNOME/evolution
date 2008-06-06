/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors :
 *  Ebby Wiselyn <ebbywiselyn@gmail.com>
 *
 * Copyright 2007, Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <glib/gi18n-lib.h>
#include <glib.h>

#include <gtk/gtk.h>

#include <e-util/e-config.h>
#include <e-util/e-plugin.h>

#include <calendar/gui/e-cal-config.h>

#include <libedataserver/e-url.h>
#include <libedataserver/e-account-list.h>
#include <libecal/e-cal.h>

#define CALENDAR_LOCATION "http://www.google.com/calendar/feeds/"

#define d(x)

/*****************************************************************************/
/* prototypes */
int              e_plugin_lib_enable      (EPluginLib                 *ep,
					   int                         enable);

GtkWidget *      plugin_google               (EPlugin                    *epl,
					   EConfigHookItemFactoryData *data);

/*****************************************************************************/
/* plugin intialization */

static void
ensure_google_source_group ()
{
	ESourceList  *slist;
	ESourceGroup *group;


	if (!e_cal_get_sources (&slist, E_CAL_SOURCE_TYPE_EVENT, NULL)) {
		g_warning ("Could not get calendar source list from GConf!");
		return;
	}

	group = e_source_list_peek_group_by_name (slist, _("Google"));

	if (group == NULL)
		 g_message ("\n Google Group Not found ");

	if (group == NULL) {
		gboolean res;
		group = e_source_group_new (_("Google"), "Google://");
		res = e_source_list_add_group (slist, group, -1);

		if (res == FALSE) {
			g_warning ("Could not add Google source group!");
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
		d(printf ("\n Google Eplugin starting up ...\n"));
		ensure_google_source_group ();
	}

	return 0;
}


/********************************************************************************************************************/
/* the location field for google sources */
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

#if 0

FIXME: Not sure why this function is declared but called no where. This needs fixing.

static void
ssl_changed (GtkToggleButton *button, ESource *source)
{
	e_source_set_property(source, "ssl",
			      gtk_toggle_button_get_active(button) ? "1" : "0");
}

#endif


static gboolean
is_email (const char *address)
{
	/* This is supposed to check if the address's domain could be
           an FQDN but alas, it's not worth the pain and suffering. */
	const char *at;

	at = strchr (address, '@');
	/* make sure we have an '@' and that it's not the first or last char */
	if (!at || at == address || *(at + 1) == '\0')
		return FALSE;

	return TRUE;
}

static void
user_changed (GtkEntry *editable, ESource *source)
{
	EUri       *euri;
	char       *uri;
	char       *ruri;
	const char *user;
	char *projection;

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

	projection = g_strdup ("/private/full");

	if (!is_email (user)) {
		user = g_strconcat (user, "@gmail.com", NULL);
	}

	e_source_set_relative_uri (source, g_strconcat (CALENDAR_LOCATION, g_strdup(user), g_strdup (projection), NULL));
	e_source_set_property (source, "username", euri->user);
	e_source_set_property (source, "protocol", "google");
	e_source_set_property (source, "auth-domain", "google");

	ruri = print_uri_noproto (euri);
	g_free (ruri);
	e_uri_free (euri);
}

static char *
get_refresh_minutes (GtkWidget *spin, GtkWidget *option)
{
	int setting = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
	switch (gtk_option_menu_get_history (GTK_OPTION_MENU (option))) {
	case 0:
		/* minutes */
		break;
	case 1:
		/* hours */
		setting *= 60;
		break;
	case 2:
		/* days */
		setting *= 1440;
		break;
	case 3:
		/* weeks - is this *really* necessary? */
		setting *= 10080;
		break;
	default:
		g_warning ("Time unit out of range");
		break;
	}

	return g_strdup_printf ("%d", setting);
}

static void
spin_changed (GtkSpinButton *spin, ECalConfigTargetSource *t)
{
	gchar *refresh_str;
	GtkWidget *option;

	option = g_object_get_data (G_OBJECT(spin), "option");

	refresh_str = get_refresh_minutes ((GtkWidget *)spin, option);
	e_source_set_property (t->source, "refresh", refresh_str);
	g_free (refresh_str);
}

static void
option_changed (GtkSpinButton *option, ECalConfigTargetSource *t)
{
	gchar *refresh_str;
	GtkWidget *spin;

	spin = g_object_get_data (G_OBJECT(option), "spin");

	refresh_str = get_refresh_minutes (spin, (GtkWidget *)option);
	e_source_set_property (t->source, "refresh", refresh_str);
	g_free (refresh_str);
}

static void
set_refresh_time (ESource *source, GtkWidget *spin, GtkWidget *option)
{
	int time;
	int item_num = 0;
	const char *refresh_str = e_source_get_property (source, "refresh");
	time = refresh_str ? atoi (refresh_str) : 30;

	if (time  && !(time % 10080)) {
		/* weeks */
		item_num = 3;
		time /= 10080;
	} else if (time && !(time % 1440)) {
		/* days */
		item_num = 2;
		time /= 1440;
	} else if (time && !(time % 60)) {
		/* hours */
		item_num = 1;
		time /= 60;
	}
	gtk_option_menu_set_history (GTK_OPTION_MENU (option), item_num);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), time);
}

GtkWidget *
plugin_google  (EPlugin                    *epl,
	     EConfigHookItemFactoryData *data)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource      *source;
	ESourceGroup *group;
	EUri         *euri;
	GtkWidget    *parent;
	GtkWidget    *cssl;
	GtkWidget    *widget;
	GtkWidget    *luser;
	GtkWidget    *user;
	GtkWidget    *label;
	char         *uri;
	char         *username;
	const char   *ssl_prop;
	gboolean      ssl_enabled;
	int           row;

	GtkWidget *option, *spin, *menu, *hbox;
	GtkWidget *times [4];
	int i;

	source = t->source;
	group = e_source_peek_group (source);

	widget = NULL;
	if (!g_str_has_prefix (e_source_group_peek_base_uri (group),
			       "Google")) {
		return NULL;
	}

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

	e_source_set_property (source, "ssl", "1");

	/* Build up the UI */
	parent = data->parent;
	row = GTK_TABLE (parent)->nrows;

	cssl = gtk_check_button_new_with_mnemonic (_("Use _SSL"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cssl), ssl_enabled);
	gtk_widget_show (cssl);
	gtk_table_attach (GTK_TABLE (parent),
			  cssl, 1, 2,
			  row + 3, row + 4,
			  GTK_FILL, 0, 0, 0);
	luser = gtk_label_new_with_mnemonic (_("User_name:"));
	gtk_widget_show (luser);
	gtk_misc_set_alignment (GTK_MISC (luser), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent),
			  luser, 0, 1,
			  row + 1, row + 2,
			  GTK_FILL, 0, 0, 0);

	user = gtk_entry_new ();
	gtk_widget_show (user);
	gtk_entry_set_text (GTK_ENTRY (user), username ? username : "");
	gtk_table_attach (GTK_TABLE (parent), user,
			  1, 2, row + 1, row + 2,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);

	gtk_label_set_mnemonic_widget (GTK_LABEL (luser), user);

	label = gtk_label_new_with_mnemonic (_("Re_fresh:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent),
			  label,
			  0, 1,
			  row + 2, row + 3,
			 GTK_EXPAND | GTK_FILL, 0, 0, 0);


	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);

	spin = gtk_spin_button_new_with_range (1, 100, 1);
 	gtk_label_set_mnemonic_widget (GTK_LABEL(label), spin);
	gtk_widget_show (spin);
	gtk_box_pack_start (GTK_BOX(hbox), spin, FALSE, TRUE, 0);

	option = gtk_option_menu_new ();
	gtk_widget_show (option);
	times[0] = gtk_menu_item_new_with_label (_("minutes"));
	times[1] = gtk_menu_item_new_with_label (_("hours"));
	times[2] = gtk_menu_item_new_with_label (_("days"));
	times[3] = gtk_menu_item_new_with_label (_("weeks"));

	menu = gtk_menu_new ();
	gtk_widget_show (menu);
	for (i = 0; i < 4; i++) {
		gtk_widget_show (times[i]);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), times[i]);
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option), menu);
	set_refresh_time (source, spin, option);
	gtk_box_pack_start (GTK_BOX (hbox), option, FALSE, TRUE, 0);

	e_source_set_property (source, "refresh", "30");

	g_object_set_data (G_OBJECT (option), "spin", spin);
	g_signal_connect (G_OBJECT (option), "changed", G_CALLBACK (option_changed), t);
	g_object_set_data (G_OBJECT (spin), "option", option);
	g_signal_connect (G_OBJECT (spin), "value-changed", G_CALLBACK (spin_changed), t);

	gtk_table_attach (GTK_TABLE (parent), hbox, 1, 2, row + 2, row + 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	g_signal_connect (G_OBJECT (user),
			  "changed",
			  G_CALLBACK (user_changed),
			  source);

        g_free (uri);
	g_free (username);

	return widget;
}


