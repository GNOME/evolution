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
 *		Nat Friedman <nat@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include <libebook/e-book.h>
#include <libedataserverui/e-source-combo-box.h>

#include <e-util/e-config.h>
#include <mail/em-config.h>
#include <mail/em-event.h>
#include <camel/camel-mime-message.h>
#include <composer/e-msg-composer.h>

#include "bbdb.h"

#define d(x)

/* Plugin hooks */
gint e_plugin_lib_enable (EPluginLib *ep, gint enable);
void bbdb_handle_send (EPlugin *ep, EMEventTargetComposer *target);
GtkWidget *bbdb_page_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data);

/* For internal use */
struct bbdb_stuff {
	EMConfigTargetPrefs *target;
	ESourceList *source_list;

	GtkWidget *option_menu;
	GtkWidget *gaim_option_menu;
	GtkWidget *check;
	GtkWidget *check_gaim;
};

/* Static forward declarations */
static gboolean bbdb_timeout (gpointer data);
static void bbdb_do_it (EBook *book, const gchar *name, const gchar *email);
static void add_email_to_contact (EContact *contact, const gchar *email);
static void enable_toggled_cb (GtkWidget *widget, gpointer data);
static void source_changed_cb (ESourceComboBox *source_combo_box, struct bbdb_stuff *stuff);
static GtkWidget *create_addressbook_option_menu (struct bbdb_stuff *stuff, gint type);
static void cleanup_cb (GObject *o, gpointer data);

static ESource *
find_esource_by_uri (ESourceList *source_list, const gchar *target_uri)
{
	GSList *groups;

	/* XXX This would be unnecessary if the plugin had stored
	 *     the addressbook's UID instead of the URI in GConf.
	 *     Too late to change it now, I suppose. */

	if (source_list == NULL || target_uri == NULL)
		return NULL;

	groups = e_source_list_peek_groups (source_list);

	while (groups != NULL) {
		GSList *sources;

		sources = e_source_group_peek_sources (groups->data);

		while (sources != NULL) {
			gchar *uri;
			gboolean match;

			uri = e_source_get_uri (sources->data);
			match = (strcmp (uri, target_uri) == 0);
			g_free (uri);

			if (match)
				return sources->data;

			sources = g_slist_next (sources);
		}

		groups = g_slist_next (groups);
	}

	return NULL;
}

gint
e_plugin_lib_enable (EPluginLib *ep, gint enable)
{
	/* Start up the plugin. */
	if (enable) {
		d(fprintf (stderr, "BBDB spinning up...\n"));

		if (bbdb_check_gaim_enabled ())
			bbdb_sync_buddy_list_check ();

		g_timeout_add_seconds (BBDB_BLIST_CHECK_INTERVAL,
			       (GSourceFunc) bbdb_timeout,
			       NULL);
	}

	return 0;
}

static gboolean
bbdb_timeout (gpointer data)
{
	if (bbdb_check_gaim_enabled ())
		bbdb_sync_buddy_list_check ();

	return TRUE;
}

typedef struct
{
	gchar *name;
	gchar *email;
} todo_struct;

static void
free_todo_struct (todo_struct *td)
{
	if (td) {
		g_free (td->name);
		g_free (td->email);
		g_free (td);
	}
}

static GSList *todo = NULL;
G_LOCK_DEFINE_STATIC (todo);

static gpointer
bbdb_do_in_thread (gpointer data)
{
	EBook *book;

	/* Open the addressbook */
	book = bbdb_open_addressbook (AUTOMATIC_CONTACTS_ADDRESSBOOK);
	if (book == NULL) {
		G_LOCK (todo);

		g_slist_foreach (todo, (GFunc)free_todo_struct, NULL);
		g_slist_free (todo);
		todo = NULL;

		G_UNLOCK (todo);
		return NULL;
	}

	G_LOCK (todo);
	while (todo) {
		todo_struct *td = todo->data;

		todo = g_slist_remove (todo, td);

		G_UNLOCK (todo);

		if (td) {
			bbdb_do_it (book, td->name, td->email);
			free_todo_struct (td);
		}

		G_LOCK (todo);
	}
	G_UNLOCK (todo);

	g_object_unref (book);

	return NULL;
}

static void
bbdb_do_thread (const gchar *name, const gchar *email)
{
	todo_struct *td;

	if (!name && !email)
		return;

	td = g_new (todo_struct, 1);
	td->name = g_strdup (name);
	td->email = g_strdup (email);

	G_LOCK (todo);
	if (todo) {
		/* the list isn't empty, which means there is a thread taking
		   care of that, thus just add it to the queue */
		todo = g_slist_append (todo, td);
	} else {
		GError *error = NULL;

		/* list was empty, add item and create a thread */
		todo = g_slist_append (todo, td);
		g_thread_create (bbdb_do_in_thread, NULL, FALSE, &error);

		if (error) {
			g_warning ("%s: Creation of the thread failed with error: %s", G_STRFUNC, error->message);
			g_error_free (error);

			G_UNLOCK (todo);
			bbdb_do_in_thread (NULL);
			G_LOCK (todo);
		}
	}
	G_UNLOCK (todo);
}

static void
walk_destinations_and_free (EDestination **dests)
{
	const gchar *name, *addr;
	gint i;

	if (!dests)
		return;

	for (i = 0; dests[i] != NULL; i++) {
		if (e_destination_is_evolution_list (dests[i])) {
			const GList *members;

			for (members = e_destination_list_get_dests (dests[i]); members; members = members->next) {
				const EDestination *member = members->data;

				if (!member)
					continue;

				name = e_destination_get_name (member);
				addr = e_destination_get_email (member);

				if (name || addr)
					bbdb_do_thread (name, addr);
			}
		} else {
			name = e_destination_get_name (dests[i]);
			addr = e_destination_get_email (dests[i]);

			if (name || addr)
				bbdb_do_thread (name, addr);
		}
	}

	e_destination_freev (dests);
}

void
bbdb_handle_send (EPlugin *ep, EMEventTargetComposer *target)
{
	EComposerHeaderTable *table;
	GConfClient *gconf;
	gboolean enable;

	gconf = gconf_client_get_default ();
	enable = gconf_client_get_bool (gconf, GCONF_KEY_ENABLE, NULL);
	g_object_unref (gconf);

	if (!enable)
		return;

	table = e_msg_composer_get_header_table (target->composer);
	g_return_if_fail (table);

	/* read information from the composer, not from a generated message */
	walk_destinations_and_free (e_composer_header_table_get_destinations_to (table));
	walk_destinations_and_free (e_composer_header_table_get_destinations_cc (table));
}

static void
bbdb_do_it (EBook *book, const gchar *name, const gchar *email)
{
	gchar *query_string, *delim, *temp_name = NULL;
	EBookQuery *query;
	GList *contacts = NULL, *l;
	EContact *contact;
	gboolean status;
	GError *error = NULL;

	g_return_if_fail (book != NULL);

	if (email == NULL || !strcmp (email, ""))
		return;

	if ((delim = strchr (email, '@')) == NULL)
		return;

	/* don't miss the entry if the mail has only e-mail id and no name */
	if (name == NULL || ! strcmp (name, "")) {
		temp_name = g_strndup (email, delim - email);
		name = temp_name;
	}

	/* If any contacts exists with this email address, don't do anything */
	query_string = g_strdup_printf ("(contains \"email\" \"%s\")", email);
	query = e_book_query_from_string (query_string);
	g_free (query_string);

	status = e_book_get_contacts (book, query, &contacts, NULL);
	if (query)
		e_book_query_unref (query);
	if (contacts != NULL || !status) {
		for (l = contacts; l != NULL; l = l->next)
			g_object_unref ((GObject *)l->data);
		g_list_free (contacts);
		g_free (temp_name);

		return;
	}

	if (g_utf8_strchr (name, -1, '\"')) {
		GString *tmp = g_string_new (name);
		gchar *p;

		while (p = g_utf8_strchr (tmp->str, tmp->len, '\"'), p)
			tmp = g_string_erase (tmp, p - tmp->str, 1);

		g_free (temp_name);
		temp_name = g_string_free (tmp, FALSE);
		name = temp_name;
	}

	/* If a contact exists with this name, add the email address to it. */
	query_string = g_strdup_printf ("(is \"full_name\" \"%s\")", name);
	query = e_book_query_from_string (query_string);
	g_free (query_string);

	status = e_book_get_contacts (book, query, &contacts, NULL);
	if (query)
		e_book_query_unref (query);
	if (contacts != NULL || !status) {

		/* FIXME: If there's more than one contact with this
		   name, just give up; we're not smart enough for
		   this. */
		if (!status || contacts->next != NULL) {
			for (l = contacts; l != NULL; l = l->next)
				g_object_unref ((GObject *)l->data);
			g_list_free (contacts);
			g_free (temp_name);
			return;
		}

		contact = (EContact *) contacts->data;
		add_email_to_contact (contact, email);
		if (! e_book_commit_contact (book, contact, &error)) {
			g_warning ("bbdb: Could not modify contact: %s\n", error->message);
			g_error_free (error);
		}

		for (l = contacts; l != NULL; l = l->next)
			g_object_unref ((GObject *)l->data);
		g_list_free (contacts);

		g_free (temp_name);
		return;
	}

	/* Otherwise, create a new contact. */
	contact = e_contact_new ();
	e_contact_set (contact, E_CONTACT_FULL_NAME, (gpointer) name);
	add_email_to_contact (contact, email);
	g_free (temp_name);

	if (! e_book_add_contact (book, contact, &error)) {
		g_warning ("bbdb: Failed to add new contact: %s\n", error->message);
		g_error_free (error);
		return;
	}

	g_object_unref (G_OBJECT (contact));
}

EBook *
bbdb_open_addressbook (gint type)
{
	GConfClient *gconf;
	gchar        *uri;
	EBook       *book = NULL;

	gboolean     status;
	GError      *error = NULL;
	gboolean     enable = TRUE;
	gconf = gconf_client_get_default ();

	/* Check to see if we're supposed to be running */
	if (type == AUTOMATIC_CONTACTS_ADDRESSBOOK)
		enable = gconf_client_get_bool (gconf, GCONF_KEY_ENABLE, NULL);
	if (!enable) {
		g_object_unref (G_OBJECT (gconf));
		return NULL;
	}

	/* Open the appropriate addresbook. */
	if (type == GAIM_ADDRESSBOOK)
		uri = gconf_client_get_string (gconf, GCONF_KEY_WHICH_ADDRESSBOOK_GAIM, NULL);
	else
		uri = gconf_client_get_string (gconf, GCONF_KEY_WHICH_ADDRESSBOOK, NULL);
	g_object_unref (G_OBJECT (gconf));

	if (uri == NULL)
		book = e_book_new_system_addressbook (&error);
	else {
		book = e_book_new_from_uri (uri, &error);
		g_free (uri);
	}

	if (book == NULL) {
		g_warning ("bbdb: failed to get addressbook: %s\n", error->message);
		g_error_free (error);
		return NULL;
	}

	status = e_book_open (book, FALSE, &error);
	if (! status) {
		g_warning ("bbdb: failed to open addressbook: %s\n", error->message);
		g_error_free (error);
		return NULL;
	}

	return book;
}

gboolean
bbdb_check_gaim_enabled (void)
{
	GConfClient *gconf;
	gboolean     gaim_enabled;

	gconf = gconf_client_get_default ();
	gaim_enabled = gconf_client_get_bool (gconf, GCONF_KEY_ENABLE_GAIM, NULL);

	g_object_unref (G_OBJECT (gconf));

	return gaim_enabled;
}

static void
add_email_to_contact (EContact *contact, const gchar *email)
{
	GList *emails;

	emails = e_contact_get (contact, E_CONTACT_EMAIL);
	emails = g_list_append (emails, (gpointer) email);
	e_contact_set (contact, E_CONTACT_EMAIL, (gpointer) emails);
}

/* Code to implement the configuration user interface follows */

static void
enable_toggled_cb (GtkWidget *widget, gpointer data)
{
	struct bbdb_stuff *stuff = (struct bbdb_stuff *) data;
	gboolean active;
	ESource *selected_source;
	gchar *addressbook;

	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	/* Save the new setting to gconf */
	gconf_client_set_bool (stuff->target->gconf, GCONF_KEY_ENABLE, active, NULL);

	gtk_widget_set_sensitive (stuff->option_menu, active);

	addressbook = gconf_client_get_string (stuff->target->gconf, GCONF_KEY_WHICH_ADDRESSBOOK, NULL);

	if (active && !addressbook) {
		const gchar *uri = NULL;
		GError *error = NULL;

		selected_source = e_source_combo_box_get_active (
			E_SOURCE_COMBO_BOX (stuff->option_menu));
		if (selected_source != NULL)
			uri = e_source_get_uri (selected_source);

		gconf_client_set_string (
			stuff->target->gconf,
			GCONF_KEY_WHICH_ADDRESSBOOK,
			uri ? uri : "", &error);

		if (error != NULL) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
	}
	g_free (addressbook);
}

static void
enable_gaim_toggled_cb (GtkWidget *widget, gpointer data)
{
	struct bbdb_stuff *stuff = (struct bbdb_stuff *) data;
	gboolean active;
	ESource *selected_source;
	gchar *addressbook_gaim;

	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	/* Save the new setting to gconf */
	gconf_client_set_bool (stuff->target->gconf, GCONF_KEY_ENABLE_GAIM, active, NULL);

	addressbook_gaim = gconf_client_get_string (stuff->target->gconf, GCONF_KEY_WHICH_ADDRESSBOOK_GAIM, NULL);
	gtk_widget_set_sensitive (stuff->gaim_option_menu, active);
	if (active && !addressbook_gaim) {
		selected_source = e_source_combo_box_get_active (
			E_SOURCE_COMBO_BOX (stuff->gaim_option_menu));
		gconf_client_set_string (stuff->target->gconf, GCONF_KEY_WHICH_ADDRESSBOOK_GAIM, e_source_get_uri (selected_source), NULL);
	}

	g_free (addressbook_gaim);
}

static void
synchronize_button_clicked_cb (GtkWidget *button)
{
	bbdb_sync_buddy_list ();
}

static void
source_changed_cb (ESourceComboBox *source_combo_box,
                   struct bbdb_stuff *stuff)
{
	ESource *source;
	GError *error = NULL;

	source = e_source_combo_box_get_active (source_combo_box);

	gconf_client_set_string (
		stuff->target->gconf,
		GCONF_KEY_WHICH_ADDRESSBOOK,
		source ? e_source_get_uri (source) : "", &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
gaim_source_changed_cb (ESourceComboBox *source_combo_box,
                        struct bbdb_stuff *stuff)
{
	ESource *source;
	GError *error = NULL;

	source = e_source_combo_box_get_active (source_combo_box);

	gconf_client_set_string (
		stuff->target->gconf,
		GCONF_KEY_WHICH_ADDRESSBOOK_GAIM,
		source ? e_source_get_uri (source) : "", &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static GtkWidget *
create_addressbook_option_menu (struct bbdb_stuff *stuff, gint type)
{
	GtkWidget   *combo_box;
	ESourceList *source_list;
	ESource     *selected_source;
	gchar        *selected_source_uri;

	GConfClient *gconf = stuff->target->gconf;

	source_list = e_source_list_new_for_gconf (gconf, "/apps/evolution/addressbook/sources");
	combo_box = e_source_combo_box_new (source_list);

	if (type == GAIM_ADDRESSBOOK)
		selected_source_uri = gconf_client_get_string (gconf, GCONF_KEY_WHICH_ADDRESSBOOK_GAIM, NULL);
	else
		selected_source_uri = gconf_client_get_string (gconf, GCONF_KEY_WHICH_ADDRESSBOOK, NULL);
	selected_source = find_esource_by_uri (
		source_list, selected_source_uri);
	g_free (selected_source_uri);

	if (selected_source != NULL)
		e_source_combo_box_set_active (
			E_SOURCE_COMBO_BOX (combo_box), selected_source);

	gtk_widget_show (combo_box);

	stuff->source_list = source_list;

	return combo_box;
}

GtkWidget *
bbdb_page_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data)
{
	struct bbdb_stuff *stuff;
	EMConfigTargetPrefs *target = (EMConfigTargetPrefs *) hook_data->config->target;
	GtkWidget *page;
	GtkWidget *tab_label;
	GtkWidget *frame;
	GtkWidget *frame_label;
	GtkWidget *padding_label;
	GtkWidget *hbox;
	GtkWidget *inner_vbox;
	GtkWidget *check;
	GtkWidget *option;
	GtkWidget *gaim_option;
	GtkWidget *check_gaim;
	GtkWidget *label;
	GtkWidget *gaim_label;
	GtkWidget *button;
	gchar *str;

	/* A structure to pass some stuff around */
	stuff = g_new0 (struct bbdb_stuff, 1);
	stuff->target = target;

	/* Create a new notebook page */
	page = gtk_vbox_new (FALSE, 0);
	GTK_CONTAINER (page)->border_width = 12;
	tab_label = gtk_label_new (_("Automatic Contacts"));
	gtk_notebook_append_page (GTK_NOTEBOOK (hook_data->parent), page, tab_label);

	/* Frame */
	frame = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (page), frame, FALSE, FALSE, 0);

	/* "Automatic Contacts" */
	frame_label = gtk_label_new ("");
	str = g_strdup_printf ("<span weight=\"bold\">%s</span>", _("Automatic Contacts"));
	gtk_label_set_markup (GTK_LABEL (frame_label), str);
	g_free (str);
	GTK_MISC (frame_label)->xalign = 0.0;
	gtk_box_pack_start (GTK_BOX (frame), frame_label, FALSE, FALSE, 0);

	/* Indent/padding */
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (frame), hbox, FALSE, TRUE, 0);
	padding_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), padding_label, FALSE, FALSE, 0);
	inner_vbox = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (hbox), inner_vbox, FALSE, FALSE, 0);

	/* Enable BBDB checkbox */
	check = gtk_check_button_new_with_mnemonic (_("Create _address book entries when sending mails"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gconf_client_get_bool (target->gconf, GCONF_KEY_ENABLE, NULL));
	g_signal_connect (GTK_TOGGLE_BUTTON (check), "toggled", G_CALLBACK (enable_toggled_cb), stuff);
	gtk_box_pack_start (GTK_BOX (inner_vbox), check, FALSE, FALSE, 0);
	stuff->check = check;

	label = gtk_label_new (_("Select Address book for Automatic Contacts"));
	gtk_box_pack_start (GTK_BOX (inner_vbox), label, FALSE, FALSE, 0);

	/* Source selection option menu */
	option = create_addressbook_option_menu (stuff, AUTOMATIC_CONTACTS_ADDRESSBOOK);
	g_signal_connect (option, "changed", G_CALLBACK (source_changed_cb), stuff);
	gtk_widget_set_sensitive (option, gconf_client_get_bool (target->gconf, GCONF_KEY_ENABLE, NULL));
	gtk_box_pack_start (GTK_BOX (inner_vbox), option, FALSE, FALSE, 0);
	stuff->option_menu = option;

	/* "Instant Messaging Contacts" */
	frame = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (page), frame, TRUE, TRUE, 24);

	frame_label = gtk_label_new ("");
	str = g_strdup_printf ("<span weight=\"bold\">%s</span>", _("Instant Messaging Contacts"));
	gtk_label_set_markup (GTK_LABEL (frame_label), str);
	g_free (str);
	GTK_MISC (frame_label)->xalign = 0.0;
	gtk_box_pack_start (GTK_BOX (frame), frame_label, FALSE, FALSE, 0);

	/* Indent/padding */
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (frame), hbox, FALSE, TRUE, 0);
	padding_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), padding_label, FALSE, FALSE, 0);
	inner_vbox = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (hbox), inner_vbox, FALSE, FALSE, 0);

	/* Enable Gaim Checkbox */
	check_gaim = gtk_check_button_new_with_mnemonic (_("Synchronize contact info and images from Pidgin buddy list"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_gaim), gconf_client_get_bool (target->gconf, GCONF_KEY_ENABLE_GAIM, NULL));
	g_signal_connect (GTK_TOGGLE_BUTTON (check_gaim), "toggled", G_CALLBACK (enable_gaim_toggled_cb), stuff);
	gtk_box_pack_start (GTK_BOX (inner_vbox), check_gaim, FALSE, FALSE, 0);
	stuff->check_gaim = check_gaim;

	gaim_label = gtk_label_new (_("Select Address book for Pidgin buddy list"));
	gtk_box_pack_start (GTK_BOX (inner_vbox), gaim_label, FALSE, FALSE, 0);

	/* Gaim Source Selection Option Menu */
	gaim_option = create_addressbook_option_menu (stuff, GAIM_ADDRESSBOOK);
	g_signal_connect (gaim_option, "changed", G_CALLBACK (gaim_source_changed_cb), stuff);
	gtk_widget_set_sensitive (gaim_option, gconf_client_get_bool (target->gconf, GCONF_KEY_ENABLE_GAIM, NULL));
	gtk_box_pack_start (GTK_BOX (inner_vbox), gaim_option, FALSE, FALSE, 0);
	stuff->gaim_option_menu = gaim_option;

	/* Synchronize now button. */
	button = gtk_button_new_with_mnemonic (_("Synchronize with _buddy list now"));
	g_signal_connect (GTK_BUTTON (button), "clicked", G_CALLBACK (synchronize_button_clicked_cb), stuff);
	gtk_box_pack_start (GTK_BOX (inner_vbox), button, FALSE, FALSE, 0);

	/* Clean up */
	g_signal_connect (page, "destroy", G_CALLBACK (cleanup_cb), stuff);

	gtk_widget_show_all (page);

	return page;
}

static void
cleanup_cb (GObject *o, gpointer data)
{
	struct bbdb_stuff *stuff = data;

	g_object_unref (stuff->source_list);
	g_free (stuff);
}
