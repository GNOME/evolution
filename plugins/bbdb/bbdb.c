/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  An Evolution EPlugin that automatically populates your addressbook
 *  as you reply to messages.  Inspired by an Emacs contact management
 *  tool called The Insidious Big Brother Database, a jwz joint.
 *
 *  Nat Friedman
 *  22 October 2004
 *  Boston
 *
 *  Copyright (C) 2004 Novell, Inc.
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

#include <libebook/e-book.h>
#include <libedataserverui/e-source-option-menu.h>

#include <e-util/e-config.h>
#include <mail/em-config.h>
#include <mail/em-event.h>
#include <camel/camel-mime-message.h>

#include "bbdb.h"

/* Plugin hooks */
int e_plugin_lib_enable (EPluginLib *ep, int enable);
void bbdb_handle_reply (EPlugin *ep, EMEventTargetMessage *target);
GtkWidget *bbdb_page_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
GtkWidget *bbdb_page_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data);

/* For internal use */
struct bbdb_stuff {
	EMConfigTargetPrefs *target;
	ESourceList *source_list;

	GtkWidget *option_menu;
	GtkWidget *check;
	GtkWidget *check_gaim;
};

/* Static forward declarations */
static gboolean bbdb_timeout (gpointer data);
static void bbdb_do_it (EBook *book, const char *name, const char *email);
static void add_email_to_contact (EContact *contact, const char *email);
static void enable_toggled_cb (GtkWidget *widget, gpointer data);
static void source_changed_cb (GtkWidget *widget, ESource *source, gpointer data);
static GtkWidget *create_addressbook_option_menu (struct bbdb_stuff *stuff);
static void cleanup_cb (GObject *o, gpointer data);

int
e_plugin_lib_enable (EPluginLib *ep, int enable)
{
 	/* Start up the plugin. */
	if (enable) {
		fprintf (stderr, "BBDB spinning up...\n");

		if (bbdb_check_gaim_enabled ())
			bbdb_sync_buddy_list_check ();

		g_timeout_add (BBDB_BLIST_CHECK_INTERVAL,
			       (GSourceFunc) bbdb_timeout,
			       NULL);
	}

	return 0;
}

static gboolean
bbdb_timeout (gpointer data)
{
	bbdb_sync_buddy_list_check ();

	return TRUE;
}

/* Code to populate addressbook when you reply to a mail follows */

void
bbdb_handle_reply (EPlugin *ep, EMEventTargetMessage *target)
{
	const CamelInternetAddress *cia;
	const char *name;
	const char *email;
	EBook      *book = NULL;
	int         i;

	/* Open the addressbook */
	book = bbdb_open_addressbook ();

	cia = camel_mime_message_get_from (target->message);
	for (i = 0; i < camel_address_length CAMEL_ADDRESS (cia); i ++) {
		camel_internet_address_get (cia, i, &name, &email);
		bbdb_do_it (book, name, email);
	}

	/* If this is a reply-all event, process To: and Cc: also. */
	if (((EEventTarget *) target)->mask & EM_EVENT_MESSAGE_REPLY_ALL) {
		g_object_unref (G_OBJECT (book));
		return;
	}

	cia = camel_mime_message_get_recipients (target->message, CAMEL_RECIPIENT_TYPE_TO);
	for (i = 0; i < camel_address_length CAMEL_ADDRESS (cia); i ++) {
		camel_internet_address_get (cia, i, &name, &email);
		bbdb_do_it (book, name, email);
	}

	cia = camel_mime_message_get_recipients (target->message, CAMEL_RECIPIENT_TYPE_CC);
	for (i = 0; i < camel_address_length CAMEL_ADDRESS (cia); i ++) {
		camel_internet_address_get (cia, i, &name, &email);
		bbdb_do_it (book, name, email);
	}

	g_object_unref (G_OBJECT (book));
}

static void
bbdb_do_it (EBook *book, const char *name, const char *email)
{
	char *query_string;
	EBookQuery *query;
	GList *contacts, *l;
	EContact *contact;

	gboolean status;
	GError *error = NULL;

	g_return_if_fail (book != NULL);

	if (name == NULL || email == NULL)
		return;

	if (! strcmp (name, "") || ! strcmp (email, ""))
		return;

	if (strchr (email, '@') == NULL)
		return;

	/* If any contacts exists with this email address, don't do anything */
	query_string = g_strdup_printf ("(contains \"email\" \"%s\")", email);
	query = e_book_query_from_string (query_string);
	g_free (query_string);

	status = e_book_get_contacts (book, query, &contacts, NULL);
	e_book_query_unref (query);
	if (contacts != NULL) {
		GList *l;
		for (l = contacts; l != NULL; l = l->next)
			g_object_unref ((GObject *)l->data);
		g_list_free (contacts);

		return;
	}

	/* If a contact exists with this name, add the email address to it. */
	query_string = g_strdup_printf ("(is \"full_name\" \"%s\")", name);
	query = e_book_query_from_string (query_string);
	g_free (query_string);

	status = e_book_get_contacts (book, query, &contacts, NULL);
	e_book_query_unref (query);
	if (contacts != NULL) {

		/* FIXME: If there's more than one contact with this
		   name, just give up; we're not smart enough for
		   this. */
		if (contacts->next != NULL)
			return;
		
		contact = (EContact *) contacts->data;
		add_email_to_contact (contact, email);
		if (! e_book_commit_contact (book, contact, &error)) {
			g_warning ("bbdb: Could not modify contact: %s\n", error->message);
			g_error_free (error);
		}
		
		for (l = contacts; l != NULL; l = l->next)
			g_object_unref ((GObject *)l->data);
		g_list_free (contacts);

		return;
	} 

	/* Otherwise, create a new contact. */
	contact = e_contact_new ();
	e_contact_set (contact, E_CONTACT_FULL_NAME, (gpointer) name);
	add_email_to_contact (contact, email);

	if (! e_book_add_contact (book, contact, &error)) {
		g_warning ("bbdb: Failed to add new contact: %s\n", error->message);
		g_error_free (error);
		return;
	}

	g_object_unref (G_OBJECT (contact));
}

EBook *
bbdb_open_addressbook (void)
{
	GConfClient *gconf;
	char        *uri;
	EBook       *book = NULL;

	gboolean     enable;

	gboolean     status;
	GError      *error;
	
	gconf = gconf_client_get_default ();

	/* Check to see if we're supposed to be running */
	enable = gconf_client_get_bool (gconf, GCONF_KEY_ENABLE, NULL);
	if (! enable) {
		g_object_unref (G_OBJECT (gconf));
		return NULL;
	}

	/* Open the appropriate addresbook. */
	uri = gconf_client_get_string (gconf, GCONF_KEY_WHICH_ADDRESSBOOK, NULL);
	g_object_unref (G_OBJECT (gconf));
	if (uri == NULL)
		book = e_book_new_system_addressbook (&error);
	else
		book = e_book_new_from_uri (uri, &error);
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
bbdb_check_gaim_enabled ()
{
	GConfClient *gconf;
	gboolean     gaim_enabled;

	gconf = gconf_client_get_default ();
	gaim_enabled = gconf_client_get_bool (gconf, GCONF_KEY_ENABLE_GAIM, NULL);

	g_object_unref (G_OBJECT (gconf));

	return gaim_enabled;
}

static void
add_email_to_contact (EContact *contact, const char *email)
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

	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	/* Save the new setting to gconf */
	gconf_client_set_bool (stuff->target->gconf, GCONF_KEY_ENABLE, active, NULL);
	
	gtk_widget_set_sensitive (stuff->option_menu, active);
}

static void
enable_gaim_toggled_cb (GtkWidget *widget, gpointer data)
{
	struct bbdb_stuff *stuff = (struct bbdb_stuff *) data;
	gboolean active;

	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	/* Save the new setting to gconf */
	gconf_client_set_bool (stuff->target->gconf, GCONF_KEY_ENABLE_GAIM, active, NULL);
}

static void
synchronize_button_clicked_cb (GtkWidget *button)
{
	bbdb_sync_buddy_list ();
}

static void
source_changed_cb (GtkWidget *widget, ESource *source, gpointer data)
{
	struct bbdb_stuff *stuff = (struct bbdb_stuff *) data;
	
	gconf_client_set_string (stuff->target->gconf, GCONF_KEY_WHICH_ADDRESSBOOK, e_source_get_uri (source), NULL);
}

static GtkWidget *
create_addressbook_option_menu (struct bbdb_stuff *stuff)
{
	GtkWidget   *menu;
	ESourceList *source_list;
	char        *selected_source_uri;
	ESource     *selected_source;

	GConfClient *gconf = stuff->target->gconf;

	source_list = e_source_list_new_for_gconf (gconf, "/apps/evolution/addressbook/sources");
	menu = e_source_option_menu_new (source_list);

	selected_source_uri = gconf_client_get_string (gconf, GCONF_KEY_WHICH_ADDRESSBOOK, NULL);
	if (selected_source_uri != NULL) {
		selected_source = e_source_new_with_absolute_uri ("", selected_source_uri);
		e_source_option_menu_select (E_SOURCE_OPTION_MENU (menu), selected_source);
	}

	gtk_widget_show (menu);

	stuff->source_list = source_list;

	return menu;
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
	GtkWidget *check_gaim;
	GtkWidget *button;

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
	gtk_label_set_markup (GTK_LABEL (frame_label), _("<span weight=\"bold\">Automatic Contacts</span>"));
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
	check = gtk_check_button_new_with_mnemonic (_("_Automatically create entries in the addressbook when responding to mail"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gconf_client_get_bool (target->gconf, GCONF_KEY_ENABLE, NULL));
	g_signal_connect (GTK_TOGGLE_BUTTON (check), "toggled", G_CALLBACK (enable_toggled_cb), stuff);
	gtk_box_pack_start (GTK_BOX (inner_vbox), check, FALSE, FALSE, 0);
	stuff->check = check;
	
	/* Source selection open menu */
	option = create_addressbook_option_menu (stuff);
	g_signal_connect (option, "source_selected", G_CALLBACK (source_changed_cb), stuff);
	gtk_widget_set_sensitive (option, gconf_client_get_bool (target->gconf, GCONF_KEY_ENABLE, NULL));
	gtk_box_pack_start (GTK_BOX (inner_vbox), option, FALSE, FALSE, 0);
	stuff->option_menu = option;

	/* "Instant Messaging Contacts" */
	frame = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (page), frame, TRUE, TRUE, 24);

	frame_label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (frame_label), _("<span weight=\"bold\">Instant Messaging Contacts</span>"));
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
	check_gaim = gtk_check_button_new_with_mnemonic (_("Periodically synchronize contact information and images from my _instant messenger"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_gaim), gconf_client_get_bool (target->gconf, GCONF_KEY_ENABLE_GAIM, NULL));
	g_signal_connect (GTK_TOGGLE_BUTTON (check_gaim), "toggled", G_CALLBACK (enable_gaim_toggled_cb), stuff);
	gtk_box_pack_start (GTK_BOX (inner_vbox), check_gaim, FALSE, FALSE, 0);
	stuff->check_gaim = check_gaim;

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
