/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors :
 *  Ebby Wiselyn <ebbywiselyn@gmail.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
#include <libedataserverui/e-cell-renderer-color.h>
#include <libedataserverui/e-passwords.h>

#include <google/libgdata/gdata-service-iface.h>
#include <google/libgdata/gdata-feed.h>
#include <google/libgdata-google/gdata-google-service.h>

#include "google-contacts-source.h"

#define CALENDAR_LOCATION "http://www.google.com/calendar/feeds/"
#define CALENDAR_DEFAULT_PATH "/private/full"
#define URL_GET_SUBSCRIBED_CALENDARS "http://www.google.com/calendar/feeds/default/allcalendars/full"

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
		ensure_google_contacts_source_group ();
	} else {
		remove_google_contacts_source_group ();
	}

	return 0;
}


/********************************************************************************************************************/
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

static char *
sanitize_user_mail (const char *user)
{
	if (!user)
		return NULL;

	if (!is_email (user)) {
		return g_strconcat (user, "%40gmail.com", NULL);
	} else {
		char *tmp = g_malloc0 (sizeof (char) * (1 + strlen (user) + 2));
		char *at = strchr (user, '@');

		strncpy (tmp, user, at - user);
		strcat (tmp, "%40");
		strcat (tmp, at + 1);

		return tmp;
	}
}

static char *
construct_default_uri (const char *username)
{
	char *user, *uri;

	user = sanitize_user_mail (username);
	uri = g_strconcat (CALENDAR_LOCATION, user, CALENDAR_DEFAULT_PATH, NULL);
	g_free (user);

	return uri;
}

/* checks whether the given_uri is pointing to the default user's calendar or not */
static gboolean
is_default_uri (const char *given_uri, const char *username)
{
	char *uri, *at;
	int ats;
	gboolean res;

	if (!given_uri)
		return TRUE;

	uri = construct_default_uri (username);

	/* count number of '@' in given_uri to know how much memory will be required */
	ats = 0;
	for (at = strchr (given_uri, '@'); at; at = strchr (at + 1, '@')) {
		ats++;
	}

	if (!ats)
		res = g_ascii_strcasecmp (given_uri, uri) == 0;
	else {
		const char *last;
		char *tmp = g_malloc0 (sizeof (char) * (1 + strlen (given_uri) + (2 * ats)));

		last = given_uri;
		for (at = strchr (last, '@'); at; at = strchr (at + 1, '@')) {
			strncat (tmp, last, at - last);
			strcat (tmp, "%40");
			last = at + 1;
		}
		strcat (tmp, last);

		res = g_ascii_strcasecmp (tmp, uri) == 0;

		g_free (tmp);
	}

	g_free (uri);

	return res;
}

static void init_combo_values (GtkComboBox *combo, const char *deftitle, const char *defuri);

static void
user_changed (GtkEntry *editable, ESource *source)
{
	char       *uri;
	const char *user;

	/* two reasons why set readonly to FALSE:
	   a) the e_source_set_relative_uri does nothing for readonly sources
	   b) we are going to set default uri, which should be always writeable */
	e_source_set_readonly (source, FALSE);

	user = gtk_entry_get_text (GTK_ENTRY (editable));
	uri = construct_default_uri (user);
	e_source_set_relative_uri (source, uri);
	g_free (uri);

	e_source_set_property (source, "username", user);
	e_source_set_property (source, "protocol", "google");
	e_source_set_property (source, "auth-domain", "google");
	e_source_set_property (source, "auth", (user && *user) ? "1" : NULL);
	e_source_set_property (source, "googlename", NULL);

	/* we changed user, thus reset the chosen calendar combo too, because
	   other user means other calendars subscribed */
	init_combo_values (GTK_COMBO_BOX (g_object_get_data (G_OBJECT (editable), "CalendarCombo")), _("Default"), NULL);
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

enum {
	COL_COLOR = 0, /* GDK_TYPE_COLOR */
	COL_TITLE,     /* G_TYPE_STRING */
	COL_URL_PATH,  /* G_TYPE_STRING */
	COL_READ_ONLY, /* G_TYPE_BOOLEAN */
	NUM_COLUMNS
};

static void
init_combo_values (GtkComboBox *combo, const char *deftitle, const char *defuri)
{
	GtkTreeIter iter;
	GtkListStore *store;

	if (!combo)
		return;

	store = GTK_LIST_STORE (gtk_combo_box_get_model (combo));

	gtk_list_store_clear (store);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
		COL_COLOR, NULL,
		COL_TITLE, deftitle,
		COL_URL_PATH, defuri,
		COL_READ_ONLY, FALSE,
		-1);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
}

static void
cal_combo_changed (GtkComboBox *combo, ESource *source)
{
	GtkListStore *store;
	GtkTreeIter iter;

	g_return_if_fail (combo != NULL);
	g_return_if_fail (source != NULL);

	store = GTK_LIST_STORE (gtk_combo_box_get_model (combo));

	if (gtk_combo_box_get_active_iter (combo, &iter)) {
		char *uri = NULL, *title = NULL;
		gboolean readonly = FALSE;

		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COL_TITLE, &title, COL_URL_PATH, &uri, COL_READ_ONLY, &readonly, -1);

		if (!uri)
			uri = construct_default_uri (e_source_get_property (source, "username"));

		if (is_default_uri (uri, e_source_get_property (source, "username"))) {
			/* do not store title when we use default uri */
			g_free (title);
			title = NULL;
		}

		/* first set readonly to FALSE, otherwise if TRUE, then e_source_set_readonly does nothing */
		e_source_set_readonly (source, FALSE);
		e_source_set_relative_uri (source, uri);
		e_source_set_readonly (source, readonly);
		e_source_set_property (source, "googlename", title);
		e_source_set_property (source, "protocol", "google");
		e_source_set_property (source, "auth-domain", "google");

		g_free (title);
		g_free (uri);
	}
}

static void
claim_error (GtkWindow *parent, const char *error)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (parent,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			error);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
retrieve_list_clicked (GtkButton *button, GtkComboBox *combo)
{
	ESource *source;
	GDataGoogleService *service;
	GDataFeed *feed;
	char *password, *tmp;
	const char *username;
	GError *error = NULL;
	GtkWindow *parent;

	g_return_if_fail (button != NULL);
	g_return_if_fail (combo != NULL);

	parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button)));

	source = g_object_get_data (G_OBJECT (button), "ESource");
	g_return_if_fail (source != NULL);

	username = e_source_get_property (source, "username");
	if (!username || !*username) {
		claim_error (parent, _("Please enter user name first."));
		return;
	}

	tmp = g_strdup_printf (_("Enter password for user %s to access list of subscribed calendars."), username);
	password = e_passwords_ask_password (_("Enter password"), "Calendar", "", tmp, 
			E_PASSWORDS_REMEMBER_NEVER | E_PASSWORDS_REPROMPT | E_PASSWORDS_SECRET | E_PASSWORDS_DISABLE_REMEMBER,
			NULL, parent);
	g_free (tmp);

	if (!password)
		return;

	service = gdata_google_service_new ("cl", "evolution-client-0.0.1");
	gdata_service_set_credentials (GDATA_SERVICE (service), username, password);
	/* privacy... maybe... */
	memset (password, 0, strlen (password));
	g_free (password);

	feed = gdata_service_get_feed (GDATA_SERVICE (service), URL_GET_SUBSCRIBED_CALENDARS, &error);

	if (feed) {
		GSList *l;
		char *old_selected = NULL;
		int idx, active = -1, default_idx = -1;
		GtkListStore *store = GTK_LIST_STORE (gtk_combo_box_get_model (combo));
		GtkTreeIter iter;

		if (gtk_combo_box_get_active_iter (combo, &iter))
			gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COL_URL_PATH, &old_selected, -1);

		gtk_list_store_clear (store);

		for (l = gdata_feed_get_entries (feed), idx = 1; l != NULL; l = l->next) {
			const char *uri, *title, *color, *access;
			GSList *links;
			GDataEntry *entry = (GDataEntry *) l->data;

			if (!entry || !GDATA_IS_ENTRY (entry))
				continue;

			/* skip hidden entries */
			if (gdata_entry_get_custom (entry, "hidden") && g_ascii_strcasecmp (gdata_entry_get_custom (entry, "hidden"), "true") == 0)
				continue;

			uri = NULL;
			for (links = gdata_entry_get_links (entry); links && !uri; links = links->next) {
				GDataEntryLink *link = (GDataEntryLink *)links->data;

				if (!link || !link->href || !link->rel)
					continue;

				if (g_ascii_strcasecmp (link->rel, "alternate") == 0)
					uri = link->href;
			}

			title = gdata_entry_get_title (entry);
			color = gdata_entry_get_custom (entry, "color");
			access = gdata_entry_get_custom (entry, "accesslevel");

			if (uri && title) {
				GdkColor gdkcolor;

				if (old_selected && g_str_equal (old_selected, uri))
					active = idx;

				if (color)
					gdk_color_parse (color, &gdkcolor);

				if (default_idx == -1 && is_default_uri (uri, username)) {
					/* have the default uri always NULL and first in the combo */
					uri = NULL;
					gtk_list_store_insert (store, &iter, 0);
					default_idx = idx;
				} else {
					gtk_list_store_append (store, &iter);
				}

				gtk_list_store_set (store, &iter,
					COL_COLOR, color ? &gdkcolor : NULL,
					COL_TITLE, title,
					COL_URL_PATH, uri,
					COL_READ_ONLY, access && !g_str_equal (access, "owner") && !g_str_equal (access, "contributor"),
					-1);
				idx++;
			}
		}

		if (default_idx == -1) {
			/* Hey, why we didn't find the default uri? Did something go so wrong or what? */
			gtk_list_store_insert (store, &iter, 0);
			gtk_list_store_set (store, &iter,
				COL_COLOR, NULL,
				COL_TITLE, _("Default"),
				COL_URL_PATH, NULL,
				COL_READ_ONLY, FALSE,
				-1);
		}

		gtk_combo_box_set_active (combo, active == -1 ? 0 : active);

		g_free (old_selected);
		g_object_unref (feed);
	} else {
		tmp = g_strdup_printf (_("Cannot read data from Google server.\n%s"), (error && error->message) ? error->message : _("Unknown error."));
		claim_error (parent, tmp);
		g_free (tmp);

		if (error) {
			g_error_free (error);
			error = NULL;
		}
	}

	g_object_unref (service);
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
	GtkWidget    *combo;
	char         *uri;
	const char   *username;
	const char   *ssl_prop;
	gboolean      ssl_enabled;
	int           row;
	GtkCellRenderer *renderer;
	GtkListStore *store;

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

	username = e_source_get_property (source, "username");
	g_free (euri->user);
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

	label = gtk_label_new_with_mnemonic (_("Cal_endar:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (parent), label, 0, 1, row + 4, row + 5, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	store = gtk_list_store_new (
		NUM_COLUMNS,
		GDK_TYPE_COLOR,		/* COL_COLOR */
		G_TYPE_STRING,		/* COL_TITLE */
		G_TYPE_STRING,		/* COL_URL_PATH */
		G_TYPE_BOOLEAN);	/* COL_READ_ONLY */

	combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

	renderer = e_cell_renderer_color_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "color", COL_COLOR, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "text", COL_TITLE, NULL);

	init_combo_values (GTK_COMBO_BOX (combo),
		e_source_get_property (source, "googlename") ? e_source_get_property (source, "googlename") : _("Default"),
		e_source_get_property (source, "googlename") ? e_source_peek_relative_uri (source) : NULL);

	g_signal_connect (combo, "changed", G_CALLBACK (cal_combo_changed), source);

	g_object_set_data (G_OBJECT (user), "CalendarCombo", combo);

	hbox = gtk_hbox_new (FALSE, 6);

	gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);
	label = gtk_button_new_with_mnemonic (_("Retrieve _list"));
	g_signal_connect (label, "clicked", G_CALLBACK (retrieve_list_clicked), combo);
	g_object_set_data (G_OBJECT (label), "ESource", source);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);
	gtk_table_attach (GTK_TABLE (parent), hbox, 1, 2, row + 4, row + 5, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	return widget;
}
