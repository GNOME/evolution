/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* addressbook.c
 *
 * Copyright (C) 2000, 2001, 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Chris Lahey (clahey@ximian.com)
 */

#include <config.h>

#include <string.h>
#include <glib.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-href.h>
#include <libgnomeui/gnome-uidefs.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-property-bag.h>
#include <gal/util/e-util.h>

#include "e-util/e-passwords.h"

#include "evolution-shell-component-utils.h"
#include "e-activity-handler.h"
#include "e-contact-editor.h"
#include "addressbook-config.h"
#include "addressbook.h"
#include "addressbook-component.h"
#include "addressbook/gui/search/e-addressbook-search-dialog.h"
#include "addressbook/gui/widgets/e-addressbook-view.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "addressbook/printing/e-contact-print.h"
#include "addressbook/util/eab-book-util.h"

#include <libebook/e-book-async.h>

/* This is used for the addressbook status bar */
#define EVOLUTION_CONTACTS_PROGRESS_IMAGE "evolution-contacts-mini.png"
static GdkPixbuf *progress_icon = NULL;

#define d(x)

#define PROPERTY_SOURCE_UID          "source_uid"

#define PROPERTY_SOURCE_UID_IDX      1

typedef struct {
	gint refs;
	GHashTable *uid_to_view;
	GtkWidget *notebook;
	EBook *book;
	guint activity_id;
	BonoboControl *control;
	BonoboPropertyBag *properties;
	ESourceList *source_list;
	char *passwd;
} AddressbookView;

static void addressbook_view_ref (AddressbookView *);
static void addressbook_view_unref (AddressbookView *);

static void addressbook_authenticate (EBook *book, gboolean previous_failure,
				      ESource *source, EBookCallback cb, gpointer closure);

static void book_open_cb (EBook *book, EBookStatus status, gpointer closure);
static void set_status_message (EABView *eav, const char *message, AddressbookView *view);
static void search_result (EABView *eav, EBookViewStatus status, AddressbookView *view);

static EABView *
get_current_view (AddressbookView *view)
{
	return EAB_VIEW (gtk_notebook_get_nth_page (GTK_NOTEBOOK (view->notebook),
						    gtk_notebook_get_current_page (GTK_NOTEBOOK (view->notebook))));
}

static void
save_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_save_as(v);
}

static void
view_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_view(v);
}

static void
search_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		gtk_widget_show(eab_search_dialog_new(v));
}

static void
delete_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_delete_selection(v);
}

static void
print_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_print(v);
}

static void
print_preview_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_print_preview(v);
}

static void
stop_loading_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_stop(v);
}

static void
cut_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_cut(v);
}

static void
copy_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_copy(v);
}

static void
paste_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_paste(v);
}

static void
select_all_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_select_all (v);
}

static void
send_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_send (v);
}

static void
send_contact_to_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_send_to (v);
}

static void
copy_contact_to_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_copy_to_folder (v);
}

static void
move_contact_to_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_move_to_folder (v);
}

static void
forget_passwords_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	e_passwords_forget_passwords();
}

static void
set_status_message (EABView *eav, const char *message, AddressbookView *view)
{
	EActivityHandler *activity_handler = addressbook_component_peek_activity_handler (addressbook_component_peek ());

	if (!message || !*message) {
		if (view->activity_id != 0) {
			e_activity_handler_operation_finished (activity_handler, view->activity_id);
			view->activity_id = 0;
		}
	} else if (view->activity_id == 0) {
		char *clientid = g_strdup_printf ("%p", view);

		if (progress_icon == NULL)
			progress_icon = gdk_pixbuf_new_from_file (EVOLUTION_IMAGESDIR "/" EVOLUTION_CONTACTS_PROGRESS_IMAGE, NULL);

		view->activity_id = e_activity_handler_operation_started (activity_handler, clientid,
									  progress_icon, message, TRUE);

		g_free (clientid);
	} else {
		e_activity_handler_operation_progressing (activity_handler, view->activity_id, message, -1.0);
	}

}


static void
search_result (EABView *eav, EBookViewStatus status, AddressbookView *view)
{
	char *str = NULL;

	switch (status) {
	case E_BOOK_VIEW_STATUS_OK:
		return;
	case E_BOOK_VIEW_STATUS_SIZE_LIMIT_EXCEEDED:
		str = _("More cards matched this query than either the server is \n"
			"configured to return or Evolution is configured to display.\n"
			"Please make your search more specific or raise the result limit in\n"
			"the directory server preferences for this addressbook.");
		break;
	case E_BOOK_VIEW_STATUS_TIME_LIMIT_EXCEEDED:
		str = _("The time to execute this query exceeded the server limit or the limit\n"
			"you have configured for this addressbook.  Please make your search\n"
			"more specific or raise the time limit in the directory server\n"
			"preferences for this addressbook.");
		break;
	case E_BOOK_VIEW_ERROR_INVALID_QUERY:
		str = _("The backend for this addressbook was unable to parse this query.");
		break;
	case E_BOOK_VIEW_ERROR_QUERY_REFUSED:
		str = _("The backend for this addressbook refused to perform this query.");
		break;
	case E_BOOK_VIEW_ERROR_OTHER_ERROR:
		str = _("This query did not complete successfully.");
		break;
	}

	if (str) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new (NULL,
						 0,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_OK,
						 str);
		g_signal_connect (dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
		gtk_widget_show (dialog);
	}
}

static void
update_command_state (EABView *eav, AddressbookView *view)
{
	BonoboUIComponent *uic;

	if (eav != get_current_view (view))
		return;

	addressbook_view_ref (view);

	uic = bonobo_control_get_ui_component (view->control);

	if (bonobo_ui_component_get_container (uic) != CORBA_OBJECT_NIL) {
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSaveAsVCard",
					      "sensitive",
					      eab_view_can_save_as (eav) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsView",
					      "sensitive",
					      eab_view_can_view (eav) ? "1" : "0", NULL);

		/* Print Contact */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsPrint",
					      "sensitive",
					      eab_view_can_print (eav) ? "1" : "0", NULL);

		/* Print Contact */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsPrintPreview",
					      "sensitive",
					      eab_view_can_print (eav) ? "1" : "0", NULL);

		/* Delete Contact */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactDelete",
					      "sensitive",
					      eab_view_can_delete (eav) ? "1" : "0", NULL);

		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsCut",
					      "sensitive",
					      eab_view_can_cut (eav) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsCopy",
					      "sensitive",
					      eab_view_can_copy (eav) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsPaste",
					      "sensitive",
					      eab_view_can_paste (eav) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSelectAll",
					      "sensitive",
					      eab_view_can_select_all (eav) ? "1" : "0", NULL);

		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSendContactToOther",
					      "sensitive",
					      eab_view_can_send (eav) ? "1" : "0", NULL);

		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSendMessageToContact",
					      "sensitive",
					      eab_view_can_send_to (eav) ? "1" : "0", NULL);

		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsMoveToFolder",
					      "sensitive",
					      eab_view_can_move_to_folder (eav) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsCopyToFolder",
					      "sensitive",
					      eab_view_can_copy_to_folder (eav) ? "1" : "0", NULL);

		/* Stop */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactStop",
					      "sensitive",
					      eab_view_can_stop (eav) ? "1" : "0", NULL);
	}

	addressbook_view_unref (view);
}

static BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("ContactsPrint", print_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsPrintPreview", print_preview_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsSaveAsVCard", save_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsView", view_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ToolSearch", search_cb),

	BONOBO_UI_UNSAFE_VERB ("ContactDelete", delete_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactStop", stop_loading_cb),

	BONOBO_UI_UNSAFE_VERB ("ContactsCut", cut_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsCopy", copy_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsPaste", paste_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsSelectAll", select_all_contacts_cb),

	BONOBO_UI_UNSAFE_VERB ("ContactsSendContactToOther", send_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsSendMessageToContact", send_contact_to_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsMoveToFolder", move_contact_to_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsCopyToFolder", copy_contact_to_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsForgetPasswords", forget_passwords_cb),
	/* ContactsViewPreview is a toggle */

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] = {
	E_PIXMAP ("/menu/File/FileOps/ContactsSaveAsVCard", "save-as-16.png"),
	E_PIXMAP ("/menu/File/Print/ContactsPrint", "print.xpm"),
	E_PIXMAP ("/menu/File/Print/ContactsPrintPreview", "print-preview.xpm"),

	E_PIXMAP ("/menu/EditPlaceholder/Edit/ContactsCut", "16_cut.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/ContactsCopy", "16_copy.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/ContactsPaste", "16_paste.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/ContactDelete", "evolution-trash-mini.png"),

	E_PIXMAP ("/menu/Tools/ComponentPlaceholder/ToolSearch", "search-16.png"),

	E_PIXMAP ("/Toolbar/ContactsPrint", "buttons/print.png"),
	E_PIXMAP ("/Toolbar/ContactDelete", "buttons/delete-message.png"),

	E_PIXMAP_END
};

static void
control_activate (BonoboControl     *control,
		  BonoboUIComponent *uic,
		  AddressbookView   *view)
{
	Bonobo_UIContainer remote_ui_container;
	EABView *v = get_current_view (view);

	remote_ui_container = bonobo_control_get_remote_ui_container (control, NULL);
	bonobo_ui_component_set_container (uic, remote_ui_container, NULL);
	bonobo_object_release_unref (remote_ui_container, NULL);

	bonobo_ui_component_add_verb_list_with_data (
		uic, verbs, view);
	
	bonobo_ui_component_freeze (uic, NULL);

	bonobo_ui_util_set_ui (uic, PREFIX,
			       EVOLUTION_UIDIR "/evolution-addressbook.xml",
			       "evolution-addressbook", NULL);

	if (v)
		eab_view_setup_menus (v, uic);

	e_pixmaps_update (uic, pixmaps);

	bonobo_ui_component_thaw (uic, NULL);

	if (v)
		update_command_state (v, view);
}

static void
control_activate_cb (BonoboControl *control, 
		     gboolean activate, 
		     AddressbookView *view)
{
	BonoboUIComponent *uic;
	EABView *v = get_current_view (view);

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	if (activate) {
		control_activate (control, uic, view);
		if (activate && v && v->model)
			eab_model_force_folder_bar_message (v->model);
	} else {
		bonobo_ui_component_unset_container (uic, NULL);
		eab_view_discard_menus (v);
	}
}

static void
gather_uids_foreach (char *key,
		     gpointer value,
		     GList **list)
{
	(*list) = g_list_prepend (*list, key);
}

static void
source_list_changed_cb (ESourceList *source_list, AddressbookView *view)
{
	GList *uids, *l;
	EABView *v;

	uids = NULL;
	g_hash_table_foreach (view->uid_to_view, (GHFunc)gather_uids_foreach, &uids);

	for (l = uids; l; l = l->next) {
		char *uid = l->data;
		if (e_source_list_peek_source_by_uid (source_list, uid)) {
			/* the source still exists, do nothing */
		}
		else {
			/* the source no longer exists, remove the
			   view and remove it from our hash table. */
			v = g_hash_table_lookup (view->uid_to_view,
						 uid);
			g_hash_table_remove (view->uid_to_view, uid);
			gtk_notebook_remove_page (GTK_NOTEBOOK (view->notebook),
						  gtk_notebook_page_num (GTK_NOTEBOOK (view->notebook),
									 GTK_WIDGET (v)));
			g_object_unref (v);
		}
	}

	/* make sure we've got the current view selected and updated
	   properly */
	v = get_current_view (view);
	if (v) {
		eab_view_setup_menus (v, bonobo_control_get_ui_component (view->control));
		update_command_state (v, view);
	}
}

static void
addressbook_view_clear (AddressbookView *view)
{
	if (view->book) {
		g_object_unref (view->book);
		view->book = NULL;
	}
	
	if (view->properties) {
		bonobo_object_unref (BONOBO_OBJECT(view->properties));
		view->properties = NULL;
	}
		
	g_free(view->passwd);
	view->passwd = NULL;

	if (view->source_list) {
		g_object_unref (view->source_list);
		view->source_list = NULL;
	}

	if (view->uid_to_view) {
		g_hash_table_destroy (view->uid_to_view);
		view->uid_to_view = NULL;
	}
}

static void
addressbook_view_ref (AddressbookView *view)
{
	g_assert (view->refs > 0);
	++view->refs;
}

static void
addressbook_view_unref (AddressbookView *view)
{
	g_assert (view->refs > 0);
	--view->refs;
	if (view->refs == 0) {
		addressbook_view_clear (view);
		g_free (view);
	}
}

void
addressbook_show_load_error_dialog (GtkWidget *parent, ESource *source, EBookStatus status)
{
	char *label_string;
	GtkWidget *warning_dialog;
	GtkWidget *href = NULL;
	gchar *uri;

	g_return_if_fail (source != NULL);

	uri = e_source_get_uri (source);

	if (!strncmp (uri, "file:", 5)) {
		label_string = 
			_("We were unable to open this addressbook.  Please check that the\n"
			  "path exists and that you have permission to access it.");
	}
	else if (!strncmp (uri, "ldap:", 5)) {
		/* special case for ldap: contact folders so we can tell the user about openldap */
#if HAVE_LDAP
		label_string = 
			_("We were unable to open this addressbook.  This either\n"
			  "means you have entered an incorrect URI, or the LDAP server\n"
			  "is unreachable.");
#else
		label_string =
			_("This version of Evolution does not have LDAP support\n"
			  "compiled in to it.  If you want to use LDAP in Evolution\n"
			  "you must compile the program from the CVS sources after\n"
			  "retrieving OpenLDAP from the link below.\n");
		href = gnome_href_new ("http://www.openldap.org/", "OpenLDAP at http://www.openldap.org/");
#endif
	} else {
		/* other network folders */
		label_string =
			_("We were unable to open this addressbook.  This either\n"
			  "means you have entered an incorrect URI, or the server\n"
			  "is unreachable.");
	}

	warning_dialog = gtk_message_dialog_new (parent ? GTK_WINDOW (parent) : NULL,
						 0,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_CLOSE, 
						 label_string,
						 NULL);

	g_signal_connect (warning_dialog, 
			  "response", 
			  G_CALLBACK (gtk_widget_destroy),
			  warning_dialog);

	gtk_window_set_title (GTK_WINDOW (warning_dialog), _("Unable to open addressbook"));

	if (href)
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (warning_dialog)->vbox), 
				    href, FALSE, FALSE, 0);

	gtk_widget_show_all (warning_dialog);

	g_free (uri);
}

typedef struct {
	EABView *view;
	ESource *source;
} BookOpenData;

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	BookOpenData *data = closure;
	EABView *view = data->view;
	ESource *source = data->source;

	g_free (data);

	/* we always set the "source" property on the EABView, since
	   we use it to reload a previously failed book. */
	g_object_set(view,
		     "source", source,
		     NULL);

	if (status == E_BOOK_ERROR_OK) {
		g_object_set(view,
			     "book", book,
			     NULL);
	}
	else {
		addressbook_show_load_error_dialog (NULL /* XXX */, source, status);
	}


	g_object_unref (source);
}

static void
destroy_callback(gpointer data, GObject *where_object_was)
{
	AddressbookView *view = data;
	addressbook_view_unref (view);
}

typedef struct {
	EBookCallback cb;
	ESource *source;
	gpointer closure;
} LoadSourceData;

static void
load_source_auth_cb (EBook *book, EBookStatus status, gpointer closure)
{
	LoadSourceData *data = closure;

	if (status != E_BOOK_ERROR_OK) {
		if (status == E_BOOK_ERROR_CANCELLED) {
			/* the user clicked cancel in the password dialog */
			GtkWidget *dialog;
			dialog = gtk_message_dialog_new (NULL,
							 0,
							 GTK_MESSAGE_WARNING,
							 GTK_BUTTONS_OK,
							 _("Accessing LDAP Server anonymously"));
			g_signal_connect (dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
			gtk_widget_show (dialog);
			data->cb (book, E_BOOK_ERROR_OK, data->closure);
			g_free (data);
			return;
		}
		else {
			gchar *uri = e_source_get_uri (data->source);

			e_passwords_forget_password ("Addressbook", uri);
			addressbook_authenticate (book, TRUE, data->source, load_source_auth_cb, closure);

			g_free (uri);
			return;
		}
	}

	data->cb (book, status, data->closure);

	g_object_unref (data->source);
	g_free (data);
}

static gboolean
get_remember_password (ESource *source)
{
	const gchar *value;

	value = e_source_get_property (source, "remember_password");
	if (value && !strcasecmp (value, "true"))
		return TRUE;

	return FALSE;
}

static void
set_remember_password (ESource *source, gboolean value)
{
	e_source_set_property (source, "remember_password",
			       value ? "true" : "false");
}

static void
addressbook_authenticate (EBook *book, gboolean previous_failure, ESource *source,
			  EBookCallback cb, gpointer closure)
{
	const char *password = NULL;
	char *pass_dup = NULL;
	const gchar *auth;
	const gchar *user;
	gchar *uri = e_source_get_uri (source);

	password = e_passwords_get_password ("Addressbook", uri);

	auth = e_source_get_property (source, "auth");

	if (auth && !strcmp ("ldap/simple-binddn", auth))
		user = e_source_get_property (source, "binddn");
	else
		user = e_source_get_property (source, "email_addr");
	if (!user)
		user = "";

	if (!password) {
		char *prompt;
		gboolean remember;
		char *failed_auth;

		if (previous_failure) {
			failed_auth = _("Failed to authenticate.\n");
		}
		else {
			failed_auth = "";
		}

		prompt = g_strdup_printf (_("%sEnter password for %s (user %s)"),
					  failed_auth, e_source_peek_name (source), user);

		remember = get_remember_password (source);
		pass_dup = e_passwords_ask_password (prompt, "Addressbook", uri, prompt, TRUE,
						     E_PASSWORDS_REMEMBER_FOREVER, &remember,
						     NULL);
		if (remember != get_remember_password (source))
			set_remember_password (source, remember);

		g_free (prompt);
	}

	if (password || pass_dup) {
		e_book_async_authenticate_user (book, user, password ? password : pass_dup,
						e_source_get_property (source, "auth"),
						cb, closure);
		g_free (pass_dup);
	}
	else {
		/* they hit cancel */
		cb (book, E_BOOK_ERROR_CANCELLED, closure);
	}

	g_free (uri);
}

static void
load_source_cb (EBook *book, EBookStatus status, gpointer closure)
{
	LoadSourceData *load_source_data = closure;

	if (status == E_BOOK_ERROR_OK && book != NULL) {
		const gchar *auth;

		auth = e_source_get_property (load_source_data->source, "auth");

		/* check if the addressbook needs authentication */

		if (auth && strcmp (auth, "none")) {
			addressbook_authenticate (book, FALSE, load_source_data->source,
						  load_source_auth_cb, closure);

			return;
		}
	}
	
	load_source_data->cb (book, status, load_source_data->closure);
	g_object_unref (load_source_data->source);
	g_free (load_source_data);
}

void
addressbook_load_source (EBook *book, ESource *source,
			 EBookCallback cb, gpointer closure)
{
	LoadSourceData *load_source_data = g_new0 (LoadSourceData, 1);

	load_source_data->cb = cb;
	load_source_data->closure = closure;
	load_source_data->source = g_object_ref (source);

	e_book_async_load_source (book, source, load_source_cb, load_source_data);
}

void
addressbook_load_default_book (EBookCallback cb, gpointer closure)
{
	LoadSourceData *load_source_data = g_new (LoadSourceData, 1);

	/* FIXME: We need to get the source for the default book */

	load_source_data->cb = cb;
	load_source_data->closure = closure;

	e_book_async_get_default_addressbook (load_source_cb, load_source_data);
}

static void
set_prop (BonoboPropertyBag *bag,
	  const BonoboArg   *arg,
	  guint              arg_id,
	  CORBA_Environment *ev,
	  gpointer           user_data)
{
	AddressbookView *view = user_data;

	switch (arg_id) {

	case PROPERTY_SOURCE_UID_IDX: {
		ESource *source;
		const gchar *uid;

		uid = BONOBO_ARG_GET_STRING (arg);

		source = e_source_list_peek_source_by_uid (view->source_list, uid);

		if (source) {
			GtkWidget *uid_view;
			EBook *book;
			BookOpenData *data;

			uid_view = g_hash_table_lookup (view->uid_to_view, uid);

			if (uid_view) {
				/* there is a view for this uid.  make
				   sure that the view actually
				   contains an EBook (if it doesn't
				   contain an EBook a previous load
				   failed.  try to load it again */
				g_object_get (uid_view,
					      "book", &book,
					      NULL);

				if (book) {
					g_object_unref (book);
				}
				else {
					book = e_book_new ();

					g_object_get (uid_view,
						      "source", &source,
						      NULL);

					/* source can be NULL here, if
					   a previous load hasn't
					   actually made it to
					   book_open_cb yet. */
					if (source) {
						data = g_new (BookOpenData, 1);
						data->view = g_object_ref (uid_view);
						data->source = source; /* transfer the ref we get back from g_object_get */

						addressbook_load_source (book, source, book_open_cb, data);
					}
				}
			}
			else {
				/* we don't have a view for this uid already
				   set up. */
				GtkWidget *label = gtk_label_new (uid);

				uid_view = eab_view_new ();

				gtk_widget_show (uid_view);
				gtk_widget_show (label);

				g_object_set (uid_view, "type", EAB_VIEW_TABLE, NULL);

				gtk_notebook_append_page (GTK_NOTEBOOK (view->notebook),
							  uid_view,
							  label);

				g_hash_table_insert (view->uid_to_view, g_strdup (uid), uid_view);

				g_signal_connect (uid_view, "status_message",
						  G_CALLBACK(set_status_message), view);

				g_signal_connect (uid_view, "search_result",
						  G_CALLBACK(search_result), view);

				g_signal_connect (uid_view, "command_state_change",
						  G_CALLBACK(update_command_state), view);

				book = e_book_new ();

				data = g_new (BookOpenData, 1);
				data->view = g_object_ref (uid_view);
				data->source = g_object_ref (source);

				addressbook_load_source (book, source, book_open_cb, data);
			}

			gtk_notebook_set_current_page (GTK_NOTEBOOK (view->notebook),
						       gtk_notebook_page_num (GTK_NOTEBOOK (view->notebook),
									      uid_view));

			/* change menus/toolbars to reflect the new view */
			eab_view_setup_menus (EAB_VIEW (uid_view), bonobo_control_get_ui_component (view->control));
			update_command_state (EAB_VIEW (uid_view), view);
		}
		else {
			g_warning ("Could not find source by UID '%s'!", uid);
		}

		break;
	}
	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}

static void
get_prop (BonoboPropertyBag *bag,
	  BonoboArg         *arg,
	  guint              arg_id,
	  CORBA_Environment *ev,
	  gpointer           user_data)
{
	AddressbookView *view = user_data;
	EABView *v = get_current_view (view);
	ESource *source = NULL;

	switch (arg_id) {

	case PROPERTY_SOURCE_UID_IDX:
		if (v) {
			g_object_get (v,
				      "source", &source,
				      NULL);
		}

		if (source) {
			BONOBO_ARG_SET_STRING (arg, e_source_peek_uid (source));

			g_object_unref (source);
		}
		else {
			BONOBO_ARG_SET_STRING (arg, "");
		}

		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
	}
}

BonoboControl *
addressbook_new_control (void)
{
	AddressbookView *view;

	view = g_new0 (AddressbookView, 1);
	view->refs = 1;

	view->uid_to_view = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify)g_free, NULL);

	view->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (view->notebook), FALSE);

	g_object_weak_ref (G_OBJECT (view->notebook), destroy_callback, view);

	/* Create the control. */
	view->control = bonobo_control_new (view->notebook);

	gtk_widget_show (view->notebook);

	view->properties = bonobo_property_bag_new (get_prop, set_prop, view);

	bonobo_property_bag_add (view->properties,
				 PROPERTY_SOURCE_UID, PROPERTY_SOURCE_UID_IDX,
				 BONOBO_ARG_STRING, NULL,
				 _("UID of the contacts source that the view will display"), 0);

	bonobo_control_set_properties (view->control,
				       bonobo_object_corba_objref (BONOBO_OBJECT (view->properties)),
				       NULL);

	view->source_list = e_source_list_new_for_gconf_default ("/apps/evolution/addressbook/sources");
	g_signal_connect (view->source_list,
			  "changed",
			  G_CALLBACK (source_list_changed_cb), view);

	g_signal_connect (view->control, "activate",
			  G_CALLBACK (control_activate_cb), view);

	return view->control;
}
