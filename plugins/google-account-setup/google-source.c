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
 *		Ebby Wiselyn <ebbywiselyn@gmail.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
#include <calendar/gui/e-cal-event.h>
#include <calendar/gui/calendar-component.h>

#include <libedataserver/e-url.h>
#include <libedataserver/e-account-list.h>
#include <libedataserver/e-proxy.h>
#include <libecal/e-cal.h>
#include <libedataserverui/e-cell-renderer-color.h>
#include <libedataserverui/e-passwords.h>

#include <google/libgdata/gdata-service-iface.h>
#include <google/libgdata/gdata-feed.h>
#include <google/libgdata-google/gdata-google-service.h>

#include "google-contacts-source.h"

#define GOOGLE_BASE_URI "google://"
#define CALENDAR_LOCATION "://www.google.com/calendar/feeds/"
#define CALENDAR_DEFAULT_PATH "/private/full"
#define URL_GET_SUBSCRIBED_CALENDARS "://www.google.com/calendar/feeds/default/allcalendars/full"
#define CALENDAR_CALDAV_URI "caldav://%s@www.google.com/calendar/dav/%s/events"

#define d(x)

/*****************************************************************************/
/* prototypes */
gint e_plugin_lib_enable (EPluginLib *ep, gint enable);
GtkWidget *plugin_google (EPlugin *epl, EConfigHookItemFactoryData *data);
void e_calendar_google_migrate (EPlugin *epl, ECalEventTargetComponent *data);

/*****************************************************************************/
/* plugin intialization */

static void
ensure_google_source_group (void)
{
	ESourceList  *slist;

	if (!e_cal_get_sources (&slist, E_CAL_SOURCE_TYPE_EVENT, NULL)) {
		g_warning ("Could not get calendar source list from GConf!");
		return;
	}

	e_source_list_ensure_group (slist, _("Google"), GOOGLE_BASE_URI, FALSE);
	g_object_unref (slist);
}

gint
e_plugin_lib_enable (EPluginLib *ep, gint enable)
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

static gchar *
decode_at_back (const gchar *user)
{
	gchar *res, *at;

	g_return_val_if_fail (user != NULL, NULL);

	res = g_strdup (user);
	while (at = strstr (res, "%40"), at != NULL) {
		*at = '@';
		memmove (at + 1, at + 3, strlen (at + 3) + 1);
	}

	return res;
}

static gboolean
is_email (const gchar *address)
{
	/* This is supposed to check if the address's domain could be
           an FQDN but alas, it's not worth the pain and suffering. */
	const gchar *at;

	at = strchr (address, '@');
	/* make sure we have an '@' and that it's not the first or last gchar */
	if (!at || at == address || *(at + 1) == '\0')
		return FALSE;

	return TRUE;
}

static gchar *
sanitize_user_mail (const gchar *user)
{
	if (!user)
		return NULL;

	if (strstr (user, "%40") != NULL) {
		return g_strdup (user);
	} else if (!is_email (user)) {
		return g_strconcat (user, "%40gmail.com", NULL);
	} else {
		gchar *tmp = g_malloc0 (sizeof (gchar) * (1 + strlen (user) + 2));
		gchar *at = strchr (user, '@');

		strncpy (tmp, user, at - user);
		strcat (tmp, "%40");
		strcat (tmp, at + 1);

		return tmp;
	}
}

static gchar *
construct_default_uri (const gchar *username, gboolean is_ssl)
{
	gchar *user, *uri;

	user = sanitize_user_mail (username);
	uri = g_strconcat (is_ssl ? "https" : "http", CALENDAR_LOCATION, user, CALENDAR_DEFAULT_PATH, NULL);
	g_free (user);

	return uri;
}

/* checks whether the given_uri is pointing to the default user's calendar or not */
static gboolean
is_default_uri (const gchar *given_uri, const gchar *username)
{
	gchar *uri, *at;
	gint ats, i;
	gboolean res = FALSE;

	if (!given_uri)
		return TRUE;

	for (i = 0; !res && i < 2; i++) {
		/* try both versions here, with and without ssl */
		uri = construct_default_uri (username, i == 0);

		/* count number of '@' in given_uri to know how much memory will be required */
		ats = 0;
		for (at = strchr (given_uri, '@'); at; at = strchr (at + 1, '@')) {
			ats++;
		}

		if (!ats)
			res = g_ascii_strcasecmp (given_uri, uri) == 0;
		else {
			const gchar *last;
			gchar *tmp = g_malloc0 (sizeof (gchar) * (1 + strlen (given_uri) + (2 * ats)));

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
	}

	return res;
}

static void
update_source_uris (ESource *source, const gchar *uri)
{
	gchar *abs_uri, *tmp, *user_sanitized, *slash;
	const gchar *user, *feeds;

	g_return_if_fail (source != NULL);
	g_return_if_fail (uri != NULL);

	/* this also changes an absolute uri */
	e_source_set_relative_uri (source, uri);

	user = e_source_get_property (source, "username");
	g_return_if_fail (user != NULL);

	feeds = strstr (uri, "/feeds/");
	g_return_if_fail (feeds != NULL);
	feeds += 7;

	user_sanitized = sanitize_user_mail (user);
	/* no "%40" in the URL path for caldav, really */
	tmp = decode_at_back (feeds);

	slash = strchr (tmp, '/');
	if (slash)
		*slash = '\0';

	abs_uri = g_strdup_printf (CALENDAR_CALDAV_URI, user_sanitized, tmp);
	e_source_set_absolute_uri (source, abs_uri);

	g_free (abs_uri);
	g_free (tmp);
	g_free (user_sanitized);
}

static void init_combo_values (GtkComboBox *combo, const gchar *deftitle, const gchar *defuri);

static void
update_user_in_source (ESource *source, const gchar *new_user)
{
	gchar       *uri, *eml, *user;
	const gchar *ssl;

	/* to ensure it will not be freed before the work with it is done */
	user = g_strdup (new_user);

	/* two reasons why set readonly to FALSE:
	   a) the e_source_set_relative_uri does nothing for readonly sources
	   b) we are going to set default uri, which should be always writeable */
	e_source_set_readonly (source, FALSE);

	if (user && *user) {
		/* store the non-encoded email in the "username" property */
		if (strstr (user, "@") == NULL && strstr (user, "%40") == NULL)
			eml = g_strconcat (user, "@gmail.com", NULL);
		else
			eml = decode_at_back (user);
	} else {
		eml = NULL;
	}

	/* set username first, as it's used in update_source_uris */
	e_source_set_property (source, "username", eml);

	ssl = e_source_get_property (source, "ssl");
	uri = construct_default_uri (user, !ssl || g_str_equal (ssl, "1"));
	update_source_uris (source, uri);
	g_free (uri);

	/* "setup-username" is used to this plugin only, to keep what user wrote,
	   not what uses the backend */
	e_source_set_property (source, "setup-username", user);
	e_source_set_property (source, "auth", (user && *user) ? "1" : NULL);
	e_source_set_property (source, "googlename", NULL);

	/* delete obsolete properties */
	e_source_set_property (source, "protocol", NULL);
	e_source_set_property (source, "auth-domain", NULL);

	g_free (eml);
	g_free (user);
}

static void
user_changed (GtkEntry *editable, ESource *source)
{
	update_user_in_source (source, gtk_entry_get_text (GTK_ENTRY (editable)));

	/* we changed user, thus reset the chosen calendar combo too, because
	   other user means other calendars subscribed */
	init_combo_values (GTK_COMBO_BOX (g_object_get_data (G_OBJECT (editable), "CalendarCombo")), _("Default"), NULL);
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

	combobox = g_object_get_data (G_OBJECT(spin), "combobox");

	refresh_str = get_refresh_minutes ((GtkWidget *)spin, combobox);
	e_source_set_property (t->source, "refresh", refresh_str);
	g_free (refresh_str);
}

static void
combobox_changed (GtkComboBox *combobox, ECalConfigTargetSource *t)
{
	gchar *refresh_str;
	GtkWidget *spin;

	spin = g_object_get_data (G_OBJECT(combobox), "spin");

	refresh_str = get_refresh_minutes (spin, (GtkWidget *)combobox);
	e_source_set_property (t->source, "refresh", refresh_str);
	g_free (refresh_str);
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

enum {
	COL_COLOR = 0, /* GDK_TYPE_COLOR */
	COL_TITLE,     /* G_TYPE_STRING */
	COL_URL_PATH,  /* G_TYPE_STRING */
	COL_READ_ONLY, /* G_TYPE_BOOLEAN */
	NUM_COLUMNS
};

static void
init_combo_values (GtkComboBox *combo, const gchar *deftitle, const gchar *defuri)
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
		gchar *uri = NULL, *title = NULL;
		gboolean readonly = FALSE;

		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COL_TITLE, &title, COL_URL_PATH, &uri, COL_READ_ONLY, &readonly, -1);

		if (!uri) {
			const gchar *ssl = e_source_get_property (source, "ssl");
			uri = construct_default_uri (e_source_get_property (source, "username"), !ssl || g_str_equal (ssl, "1"));
		}

		if (is_default_uri (uri, e_source_get_property (source, "username"))) {
			/* do not store title when we use default uri */
			g_free (title);
			title = NULL;
		}

		/* first set readonly to FALSE, otherwise if TRUE, then e_source_set_readonly does nothing */
		e_source_set_readonly (source, FALSE);
		update_source_uris (source, uri);
		e_source_set_readonly (source, readonly);
		e_source_set_property (source, "googlename", title);

		/* delete obsolete properties */
		e_source_set_property (source, "protocol", NULL);
		e_source_set_property (source, "auth-domain", NULL);

		g_free (title);
		g_free (uri);
	}
}

static void
claim_error (GtkWindow *parent, const gchar *error)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (parent,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"%s",
			error);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
update_proxy_settings (GDataService *service, const gchar *uri)
{
	EProxy *proxy;
	SoupURI *proxy_uri = NULL;

	proxy = e_proxy_new ();
	e_proxy_setup_proxy (proxy);

	/* use proxy if necessary */
	if (e_proxy_require_proxy_for_uri (proxy, uri)) {
		proxy_uri = e_proxy_peek_uri_for (proxy, uri);
	}

	gdata_service_set_proxy (service, proxy_uri);
	g_object_unref (proxy);
}

static void
retrieve_list_clicked (GtkButton *button, GtkComboBox *combo)
{
	ESource *source;
	GDataGoogleService *service;
	GDataFeed *feed;
	gchar *user, *password, *tmp;
	const gchar *username, *ssl;
	gchar *get_subscribed_url;
	GError *error = NULL;
	GtkWindow *parent;

	g_return_if_fail (button != NULL);
	g_return_if_fail (combo != NULL);

	parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button)));

	source = g_object_get_data (G_OBJECT (button), "ESource");
	g_return_if_fail (source != NULL);

	username = e_source_get_property (source, "username");
	g_return_if_fail (username != NULL && *username != '\0');

	user = decode_at_back (username);
	tmp = g_strdup_printf (_("Enter password for user %s to access list of subscribed calendars."), user);
	password = e_passwords_ask_password (_("Enter password"), "Calendar", "", tmp,
			E_PASSWORDS_REMEMBER_NEVER | E_PASSWORDS_REPROMPT | E_PASSWORDS_SECRET | E_PASSWORDS_DISABLE_REMEMBER,
			NULL, parent);
	g_free (tmp);

	if (!password) {
		g_free (user);
		return;
	}

	service = gdata_google_service_new ("cl", "evolution-client-0.0.1");
	gdata_service_set_credentials (GDATA_SERVICE (service), user, password);
	/* privacy... maybe... */
	memset (password, 0, strlen (password));
	g_free (password);

	ssl = e_source_get_property (source, "ssl");
	get_subscribed_url = g_strconcat ((!ssl || g_str_equal (ssl, "1")) ? "https" : "http", URL_GET_SUBSCRIBED_CALENDARS, NULL);
	update_proxy_settings (GDATA_SERVICE (service), get_subscribed_url);
	feed = gdata_service_get_feed (GDATA_SERVICE (service), get_subscribed_url, &error);
	g_free (get_subscribed_url);

	if (feed) {
		GSList *l;
		gchar *old_selected = NULL;
		gint idx, active = -1, default_idx = -1;
		GtkListStore *store = GTK_LIST_STORE (gtk_combo_box_get_model (combo));
		GtkTreeIter iter;

		if (gtk_combo_box_get_active_iter (combo, &iter))
			gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COL_URL_PATH, &old_selected, -1);

		gtk_list_store_clear (store);

		for (l = gdata_feed_get_entries (feed), idx = 1; l != NULL; l = l->next) {
			const gchar *uri, *title, *color, *access;
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

				if (default_idx == -1 && is_default_uri (uri, user)) {
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
	g_free (user);
}

static void
retrieve_list_sensitize (GtkEntry *username_entry,
                         GtkWidget *button)
{
	const gchar *text;
	gboolean sensitive;

	text = gtk_entry_get_text (username_entry);
	sensitive = (text != NULL && *text != '\0');
	gtk_widget_set_sensitive (button, sensitive);
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
	GtkWidget    *widget;
	GtkWidget    *luser;
	GtkWidget    *user;
	GtkWidget    *label;
	GtkWidget    *combo;
	gchar         *uri;
	const gchar   *username;
	gint           row;
	GtkCellRenderer *renderer;
	GtkListStore *store;

	GtkWidget *combobox, *spin, *hbox;

	source = t->source;
	group = e_source_peek_group (source);

	widget = NULL;
	if (g_ascii_strncasecmp (GOOGLE_BASE_URI, e_source_group_peek_base_uri (group), strlen (GOOGLE_BASE_URI)) != 0) {
		return NULL;
	}

	uri = e_source_get_uri (source);
	euri = e_uri_new (uri);
	g_free (uri);

	if (euri == NULL) {
		return NULL;
	}

	e_uri_free (euri);

	username = e_source_get_property (source, "setup-username");
	if (!username)
		username = e_source_get_property (source, "username");

	/* google's CalDAV requires SSL, thus forcing it here, and no setup for it */
	e_source_set_property (source, "ssl", "1");

	/* Build up the UI */
	parent = data->parent;
	row = GTK_TABLE (parent)->nrows;

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

	if (!e_source_get_property (source, "refresh"))
		e_source_set_property (source, "refresh", "30");

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

	gtk_table_attach (GTK_TABLE (parent), hbox, 1, 2, row + 2, row + 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	g_signal_connect (G_OBJECT (user),
			  "changed",
			  G_CALLBACK (user_changed),
			  source);

	label = gtk_label_new_with_mnemonic (_("Cal_endar:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (parent), label, 0, 1, row + 3, row + 4, GTK_FILL | GTK_EXPAND, 0, 0, 0);

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
	g_signal_connect (user, "changed", G_CALLBACK (retrieve_list_sensitize), label);
	g_object_set_data (G_OBJECT (label), "ESource", source);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_set_sensitive (label, username && *username);

	gtk_widget_show_all (hbox);
	gtk_table_attach (GTK_TABLE (parent), hbox, 1, 2, row + 3, row + 4, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	return widget;
}

void
e_calendar_google_migrate (EPlugin *epl, ECalEventTargetComponent *data)
{
	CalendarComponent *component;
	ESourceList *source_list;
	ESourceGroup *google = NULL;
	gboolean changed = FALSE;

	component = data->component;
	source_list = calendar_component_peek_source_list (component);

	google = e_source_list_peek_group_by_base_uri (source_list, GOOGLE_BASE_URI);
	if (google) {
		GSList *s;

		for (s = e_source_group_peek_sources (google); s; s = s->next) {
			ESource *source = E_SOURCE (s->data);

			if (!source)
				continue;

			/* new source through CalDAV uses absolute uri, thus it should be migrated if not set */
			if (!e_source_peek_absolute_uri (source)) {
				update_user_in_source (source, e_source_get_property (source, "username"));
				changed = TRUE;
			}
		}
	}

	if (changed)
		e_source_list_sync (source_list, NULL);
}
