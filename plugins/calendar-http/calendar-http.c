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
#include <calendar/gui/e-cal-config.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-url.h>
#include <glib/gi18n.h>
#include <string.h>

GtkWidget *e_calendar_http_url (EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *e_calendar_http_refresh (EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean   e_calendar_http_check (EPlugin *epl, EConfigHookPageCheckData *data);
GtkWidget * e_calendar_http_secure (EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *e_calendar_http_auth (EPlugin *epl, EConfigHookItemFactoryData *data);

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
url_changed (GtkEntry *entry, ESource *source)
{
	EUri *uri;
	gchar *relative_uri;

	uri = e_uri_new (gtk_entry_get_text (GTK_ENTRY (entry)));

	if (strncmp (uri->protocol, "https", sizeof ("https") - 1) == 0) {
		gpointer secure_checkbox;

		secure_checkbox = g_object_get_data (G_OBJECT (gtk_widget_get_parent (GTK_WIDGET (entry))),
						     "secure_checkbox");

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (secure_checkbox), TRUE);
	}

	g_free (uri->user);
	uri->user = g_strdup (e_source_get_property (source, "username"));
	relative_uri = print_uri_noproto (uri);
	e_source_set_relative_uri (source, relative_uri);
	g_free (relative_uri);
	e_uri_free (uri);
}

GtkWidget *
e_calendar_http_url (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	static GtkWidget *label;
	GtkWidget *entry, *parent;
	gint row;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	EUri *uri;
	gchar *uri_text;
	static GtkWidget *hidden = NULL;

	if (!hidden)
		hidden = gtk_label_new ("");

	if (data->old)
		gtk_widget_destroy (label);

	uri_text = e_source_get_uri (t->source);
	uri = e_uri_new (uri_text);
	if ((strcmp (uri->protocol, "http") &&
	     strcmp (uri->protocol, "https") &&
	     strcmp (uri->protocol, "webcal"))) {
		e_uri_free (uri);
		g_free (uri_text);
		return hidden;
	}
	g_free (uri->user);
	uri->user = NULL;

	g_free (uri_text);
	uri_text = e_uri_to_string (uri, FALSE);
	e_uri_free (uri);

	parent = data->parent;

	row = ((GtkTable*)parent)->nrows;

	label = gtk_label_new_with_mnemonic (_("_URL:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent), label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

	entry = gtk_entry_new ();
	gtk_widget_show (entry);
	gtk_entry_set_text (GTK_ENTRY (entry), uri_text);
	gtk_table_attach (GTK_TABLE (parent), entry, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (url_changed), t->source);

	g_free (uri_text);
	return entry;
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
spin_changed (GtkSpinButton *spin, ECalConfigTargetSource *t)
{
	gchar *refresh_str;
	GtkWidget *combobox;

	combobox = g_object_get_data (G_OBJECT (spin), "combobox");

	refresh_str = get_refresh_minutes ((GtkWidget *) spin, combobox);
	e_source_set_property (t->source, "refresh", refresh_str);
	g_free (refresh_str);
}

static void
combobox_changed (GtkComboBox *combobox, ECalConfigTargetSource *t)
{
	gchar *refresh_str;
	GtkWidget *spin;

	spin = g_object_get_data (G_OBJECT (combobox), "spin");

	refresh_str = get_refresh_minutes (spin, (GtkWidget *) combobox);
	e_source_set_property (t->source, "refresh", refresh_str);
	g_free (refresh_str);
}

static void
secure_setting_changed (GtkWidget *widget, ESource *source)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		e_source_set_property (source, "use_ssl", "1");
	else
		e_source_set_property (source, "use_ssl", "0");
}

GtkWidget *
e_calendar_http_refresh (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	static GtkWidget *label;
	GtkWidget *combobox, *spin, *hbox, *parent;
	gint row;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource *source = t->source;
	EUri *uri;
	gchar * uri_text;
	static GtkWidget *hidden = NULL;

	if (!hidden)
		hidden = gtk_label_new ("");

	if (data->old)
		gtk_widget_destroy (label);

	uri_text = e_source_get_uri (t->source);
	uri = e_uri_new (uri_text);
	g_free (uri_text);
	if ((strcmp (uri->protocol, "http") &&
	     strcmp (uri->protocol, "https") &&
	     strcmp (uri->protocol, "webcal"))) {
		e_uri_free (uri);
		return hidden;
	}
	e_uri_free (uri);

	parent = data->parent;

	row = ((GtkTable*)parent)->nrows;

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
	g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (combobox_changed), t);
	g_object_set_data (G_OBJECT (spin), "combobox", combobox);
	g_signal_connect (G_OBJECT (spin), "value-changed", G_CALLBACK (spin_changed), t);

	gtk_table_attach (GTK_TABLE (parent), hbox, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	return hbox;
}

GtkWidget *
e_calendar_http_secure (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	GtkWidget *secure_setting, *parent;
	const gchar *secure_prop;
	gint row;
	EUri *uri;
	gchar * uri_text;
	static GtkWidget *hidden = NULL;

	if (!hidden)
		hidden = gtk_label_new ("");

	uri_text = e_source_get_uri (t->source);
	uri = e_uri_new (uri_text);
	g_free (uri_text);
	if ((strcmp (uri->protocol, "http") &&
	     strcmp (uri->protocol, "https") &&
	     strcmp (uri->protocol, "webcal"))) {
		e_uri_free (uri);
		return hidden;
	}
	e_uri_free (uri);

	parent = data->parent;

	row = ((GtkTable*)parent)->nrows;

	secure_setting = gtk_check_button_new_with_mnemonic (_("_Secure connection"));

	secure_prop = e_source_get_property (t->source, "use_ssl");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (secure_setting), (secure_prop && g_str_equal (secure_prop, "1"))  ? TRUE : FALSE);

	g_signal_connect (secure_setting, "toggled", G_CALLBACK (secure_setting_changed), t->source);

	gtk_widget_show (secure_setting);
	gtk_table_attach (GTK_TABLE (parent), secure_setting, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	/* Store pointer to secure checkbox so we can retrieve it in url_changed() */
	g_object_set_data (G_OBJECT (parent), "secure_checkbox", (gpointer)secure_setting);

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
	}

	uri = e_source_get_uri (source);
	if (uri != NULL) {
		EUri *euri;
		gchar *ruri;

		if (username && !*username)
			username = NULL;

		euri = e_uri_new (uri);

		g_free (euri->user);
		euri->user = g_strdup (username);

		ruri = print_uri_noproto (euri);
		e_source_set_relative_uri (source, ruri);
		e_uri_free (euri);
		g_free (ruri);
		g_free (uri);
	}
}

GtkWidget *
e_calendar_http_auth (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	static GtkWidget *label;
	GtkWidget *entry, *parent;
	gint row;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	EUri *uri;
	gchar *uri_text;
	const gchar *username;
	static GtkWidget *hidden = NULL;

	if (!hidden)
		hidden = gtk_label_new ("");

	if (data->old)
		gtk_widget_destroy (label);

	uri_text = e_source_get_uri (t->source);
	uri = e_uri_new (uri_text);
	g_free (uri_text);
	if ((strcmp (uri->protocol, "http") &&
	     strcmp (uri->protocol, "https") &&
	     strcmp (uri->protocol, "webcal"))) {
		e_uri_free (uri);
		return hidden;
	}
	e_uri_free (uri);

	parent = data->parent;

	row = ((GtkTable*)parent)->nrows;

	label = gtk_label_new_with_mnemonic (_("Userna_me:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (parent), label, 0, 1, row, row+1, GTK_FILL, GTK_SHRINK, 0, 0);

	username = e_source_get_property (t->source, "username");

	entry = gtk_entry_new ();
	gtk_widget_show (entry);
	gtk_entry_set_text (GTK_ENTRY (entry), username ? username : "");
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (username_changed), t->source);

	gtk_table_attach (GTK_TABLE (parent), entry, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	return entry;
}

gboolean
e_calendar_http_check (EPlugin *epl, EConfigHookPageCheckData *data)
{
	/* FIXME - check pageid */
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	EUri *uri;
	gboolean ok = FALSE;
	ESourceGroup *group = e_source_peek_group (t->source);
	gchar *uri_text;

	if (strncmp (e_source_group_peek_base_uri (group), "webcal", 6))
		return TRUE;

	uri_text = e_source_get_uri (t->source);
	if (!strncmp (uri_text, "file:", 5)) {
		g_free (uri_text);
		return FALSE;
	}

	uri = e_uri_new (uri_text);
	ok = ((!strcmp (uri->protocol, "webcal")) ||
	      (!strcmp (uri->protocol, "http")) ||
	      (!strcmp (uri->protocol, "https")) ||
	      (!strcmp (uri->protocol, "file")) );
	e_uri_free (uri);
	g_free (uri_text);

	return ok;
}
