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
#include <calendar/gui/e-cal-config.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-account-list.h>
#include <libecal/e-cal.h>

#include <string.h>

#define d(x)

/*****************************************************************************/
/* prototypes */
gint              e_plugin_lib_enable      (EPluginLib                 *ep,
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
e_plugin_lib_enable (EPluginLib *ep, gint enable)
{

	if (enable) {
		d(g_print ("CalDAV Eplugin starting up ...\n"));
		ensure_caldav_source_group (E_CAL_SOURCE_TYPE_EVENT);
		ensure_caldav_source_group (E_CAL_SOURCE_TYPE_TODO);
		ensure_caldav_source_group (E_CAL_SOURCE_TYPE_JOURNAL);
	}

	return 0;
}

/* replaces all '@' with '%40' in str; returns newly allocated string */
static gchar *
replace_at_sign (const gchar *str)
{
	gchar *res, *at;

	if (!str)
		return NULL;

	res = g_strdup (str);
	while (at = strchr (res, '@'), at) {
		gchar *tmp = g_malloc0 (sizeof (gchar) * (1 + strlen (res) + 2));

		strncpy (tmp, res, at - res);
		strcat (tmp, "%40");
		strcat (tmp, at + 1);

		g_free (res);
		res = tmp;
	}

	return res;
}

/*****************************************************************************/
/* the location field for caldav sources */

/* stolen from calendar-weather eplugin */
static gchar *
print_uri_noproto (EUri *uri)
{
	gchar *uri_noproto, *user, *pass;

	if (uri->user)
		user = replace_at_sign (uri->user);
	else
		user = NULL;

	if (uri->passwd)
		pass = replace_at_sign (uri->passwd);
	else
		pass = NULL;

	if (uri->port != 0)
		uri_noproto = g_strdup_printf (
			"%s%s%s%s%s%s%s:%d%s%s%s",
			user ? user : "",
			uri->authmech ? ";auth=" : "",
			uri->authmech ? uri->authmech : "",
			pass ? ":" : "",
			pass ? pass : "",
			user ? "@" : "",
			uri->host ? uri->host : "",
			uri->port,
			uri->path ? uri->path : "",
			uri->query ? "?" : "",
			uri->query ? uri->query : "");
	else
		uri_noproto = g_strdup_printf (
			"%s%s%s%s%s%s%s%s%s%s",
			user ? user : "",
			uri->authmech ? ";auth=" : "",
			uri->authmech ? uri->authmech : "",
			pass ? ":" : "",
			pass ? pass : "",
			user ? "@" : "",
			uri->host ? uri->host : "",
			uri->path ? uri->path : "",
			uri->query ? "?" : "",
			uri->query ? uri->query : "");

	g_free (user);
	g_free (pass);

	return uri_noproto;
}

static void
location_changed (GtkEntry *editable, ESource *source)
{
	EUri       *euri;
	gchar       *ruri;
	const gchar *uri, *username;

	uri = gtk_entry_get_text (GTK_ENTRY (editable));

	euri = e_uri_new (uri);
	g_return_if_fail (euri != NULL);

	username = e_source_get_property (source, "username");
	if (username && !*username)
		username = NULL;

	if ((!euri->user && username) || (euri->user && username && !g_str_equal (euri->user, username))) {
		g_free (euri->user);
		euri->user = g_strdup (username);
	}

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
	gchar       *uri;
	gchar       *ruri;
	const gchar *user;

	uri = e_source_get_uri (source);
	user = gtk_entry_get_text (GTK_ENTRY (editable));

	if (uri == NULL) {
		g_free (uri);
		return;
	}

	euri = e_uri_new (uri);
	g_free (euri->user);
	euri->user = NULL;

	if (user != NULL && *user) {
		euri->user = g_strdup (user);
		e_source_set_property (source, "auth", "1");
	} else {
		e_source_set_property (source, "auth", NULL);
	}

	e_source_set_property (source, "username", user);
	ruri = print_uri_noproto (euri);
	e_source_set_relative_uri (source, ruri);
	g_free (ruri);
	e_uri_free (euri);
}

static void
set_refresh_time (ESource *source, GtkWidget *spin, GtkWidget *combobox)
{
	gint time;
	gint item_num = 0;
	const gchar *refresh_str = e_source_get_property (source, "refresh");
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
	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), item_num);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), time);
}

static gchar *
get_refresh_minutes (GtkWidget *spin, GtkWidget *combobox)
{
	gint setting = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
	switch (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox))) {
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
spin_changed (GtkSpinButton *spin, ESource *source)
{
	gchar *refresh_str;
	GtkWidget *combobox;

	combobox = g_object_get_data (G_OBJECT (spin), "combobox");

	refresh_str = get_refresh_minutes ((GtkWidget *) spin, combobox);
	e_source_set_property (source, "refresh", refresh_str);
	g_free (refresh_str);
}

static void
combobox_changed (GtkComboBox *combobox, ESource *source)
{
	gchar *refresh_str;
	GtkWidget *spin;

	spin = g_object_get_data (G_OBJECT (combobox), "spin");

	refresh_str = get_refresh_minutes (spin, (GtkWidget *) combobox);
	e_source_set_property (source, "refresh", refresh_str);
	g_free (refresh_str);
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
	GtkWidget    *label, *hbox, *spin, *combobox;
	gchar         *uri;
	gchar         *username;
	const gchar   *ssl_prop;
	gboolean      ssl_enabled;
	gint           row;

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

	g_free (euri->user);
	euri->user = NULL;
	uri = e_uri_to_string (euri, FALSE);

	username = e_source_get_duped_property (source, "username");

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

	row++;

	cssl = gtk_check_button_new_with_mnemonic (_("Use _SSL"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cssl), ssl_enabled);
	gtk_widget_show (cssl);
	gtk_table_attach (GTK_TABLE (parent),
			  cssl, 1, 2,
			  row , row + 1,
			  GTK_FILL, 0, 0, 0);

	g_signal_connect (G_OBJECT (cssl),
			  "toggled",
			  G_CALLBACK (ssl_changed),
			  source);

	row++;

	luser = gtk_label_new_with_mnemonic (_("User_name:"));
	gtk_widget_show (luser);
	gtk_misc_set_alignment (GTK_MISC (luser), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent),
			  luser, 0, 1,
			  row, row + 1,
			  GTK_FILL, 0, 0, 0);

	user = gtk_entry_new ();
	gtk_widget_show (user);
	gtk_entry_set_text (GTK_ENTRY (user), username ? username : "");
	gtk_table_attach (GTK_TABLE (parent), user,
			  1, 2, row, row + 1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);

	gtk_label_set_mnemonic_widget (GTK_LABEL (luser), user);

	g_signal_connect (G_OBJECT (user),
			  "changed",
			  G_CALLBACK (user_changed),
			  source);

	row++;

        g_free (uri);
	g_free (username);

	/* add refresh option */
	label = gtk_label_new_with_mnemonic (_("Re_fresh:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent), label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);

	spin = gtk_spin_button_new_with_range (0, 100, 1);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin);
	gtk_widget_show (spin);
	gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, TRUE, 0);

	combobox = gtk_combo_box_new_text ();
	gtk_widget_show (combobox);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _("minutes"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _("hours"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _("days"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _("weeks"));
	set_refresh_time (source, spin, combobox);
	gtk_box_pack_start (GTK_BOX (hbox), combobox, FALSE, TRUE, 0);

	g_object_set_data (G_OBJECT (combobox), "spin", spin);
	g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (combobox_changed), source);
	g_object_set_data (G_OBJECT (spin), "combobox", combobox);
	g_signal_connect (G_OBJECT (spin), "value-changed", G_CALLBACK (spin_changed), source);

	gtk_table_attach (GTK_TABLE (parent), hbox, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	return widget;
}

