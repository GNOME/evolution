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

#include "e-util/e-categories-master-list-wombat.h"
#include "e-util/e-sexp.h"
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
#include <widgets/misc/e-search-bar.h>
#include <widgets/misc/e-filter-bar.h>

/* This is used for the addressbook status bar */
#define EVOLUTION_CONTACTS_PROGRESS_IMAGE "evolution-contacts-mini.png"
static GdkPixbuf *progress_icon = NULL;

#define d(x)

#define PROPERTY_SOURCE_UID          "source_uid"

#define PROPERTY_SOURCE_UID_IDX      1

typedef struct {
	gint refs;
	EABView *view;
	ESearchBar *search;
	gint        ecml_changed_id;
	GtkWidget *vbox;
	EBook *book;
	guint activity_id;
	BonoboControl *control;
	BonoboPropertyBag *properties;
	GConfClient *gconf_client;
	ESourceList *source_list;
	ESource *source;
	char *passwd;
	gboolean ignore_search_changes;
	gboolean failed_to_load;
} AddressbookView;

static void addressbook_view_ref (AddressbookView *);
static void addressbook_view_unref (AddressbookView *);

static void addressbook_authenticate (EBook *book, gboolean previous_failure,
				      ESource *source, EBookCallback cb, gpointer closure);

static void book_open_cb (EBook *book, EBookStatus status, gpointer closure);

static void
save_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		eab_view_save_as(view->view);
}

static void
view_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		eab_view_view(view->view);
}

static void
search_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;

	if (view->view)
		gtk_widget_show(eab_search_dialog_new(view->view));
}

static void
delete_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view) {
		eab_view_delete_selection(view->view);
	}
}

static void
print_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		eab_view_print(view->view);
}

static void
print_preview_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		eab_view_print_preview(view->view);
}

static void
stop_loading_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		eab_view_stop(view->view);
}

static void
cut_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		eab_view_cut(view->view);
}

static void
copy_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		eab_view_copy(view->view);
}

static void
paste_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		eab_view_paste(view->view);
}

static void
select_all_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		eab_view_select_all (view->view);
}

static void
send_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		eab_view_send (view->view);
}

static void
send_contact_to_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		eab_view_send_to (view->view);
}

static void
copy_contact_to_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		eab_view_copy_to_folder (view->view);
}

static void
move_contact_to_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		eab_view_move_to_folder (view->view);
}

static void
forget_passwords_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	e_passwords_forget_passwords();
}

static void
update_command_state (EABView *eav, AddressbookView *view)
{
	BonoboUIComponent *uic;

	if (view->view == NULL)
		return;

	addressbook_view_ref (view);

	uic = bonobo_control_get_ui_component (view->control);

	if (bonobo_ui_component_get_container (uic) != CORBA_OBJECT_NIL) {
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSaveAsVCard",
					      "sensitive",
					      eab_view_can_save_as (view->view) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsView",
					      "sensitive",
					      eab_view_can_view (view->view) ? "1" : "0", NULL);

		/* Print Contact */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsPrint",
					      "sensitive",
					      eab_view_can_print (view->view) ? "1" : "0", NULL);

		/* Print Contact */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsPrintPreview",
					      "sensitive",
					      eab_view_can_print (view->view) ? "1" : "0", NULL);

		/* Delete Contact */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactDelete",
					      "sensitive",
					      eab_view_can_delete (view->view) ? "1" : "0", NULL);

		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsCut",
					      "sensitive",
					      eab_view_can_cut (view->view) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsCopy",
					      "sensitive",
					      eab_view_can_copy (view->view) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsPaste",
					      "sensitive",
					      eab_view_can_paste (view->view) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSelectAll",
					      "sensitive",
					      eab_view_can_select_all (view->view) ? "1" : "0", NULL);

		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSendContactToOther",
					      "sensitive",
					      eab_view_can_send (view->view) ? "1" : "0", NULL);

		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSendMessageToContact",
					      "sensitive",
					      eab_view_can_send_to (view->view) ? "1" : "0", NULL);

		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsMoveToFolder",
					      "sensitive",
					      eab_view_can_move_to_folder (view->view) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsCopyToFolder",
					      "sensitive",
					      eab_view_can_copy_to_folder (view->view) ? "1" : "0", NULL);

		/* Stop */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactStop",
					      "sensitive",
					      eab_view_can_stop (view->view) ? "1" : "0", NULL);
	}

	addressbook_view_unref (view);
}

static void
change_view_type (AddressbookView *view, EABViewType view_type)
{
	g_object_set (view->view, "type", view_type, NULL);
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

	remote_ui_container = bonobo_control_get_remote_ui_container (control, NULL);
	bonobo_ui_component_set_container (uic, remote_ui_container, NULL);
	bonobo_object_release_unref (remote_ui_container, NULL);

	e_search_bar_set_ui_component (view->search, uic);

	bonobo_ui_component_add_verb_list_with_data (
		uic, verbs, view);
	
	bonobo_ui_component_freeze (uic, NULL);

	bonobo_ui_util_set_ui (uic, PREFIX,
			       EVOLUTION_UIDIR "/evolution-addressbook.xml",
			       "evolution-addressbook", NULL);

	eab_view_setup_menus (view->view, uic);

	e_pixmaps_update (uic, pixmaps);

	bonobo_ui_component_thaw (uic, NULL);

	update_command_state (view->view, view);
}

static void
control_activate_cb (BonoboControl *control, 
		     gboolean activate, 
		     AddressbookView *view)
{
	BonoboUIComponent *uic;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	if (activate) {
		control_activate (control, uic, view);
		if (activate && view->view && view->view->model)
			eab_model_force_folder_bar_message (view->view->model);

		/* if the book failed to load, we kick off another
		   load here */

		if (view->failed_to_load && view->source) {
			EBook *book;

			book = e_book_new ();

			addressbook_load_source (book, view->source, book_open_cb, view);
		}
	} else {
		bonobo_ui_component_unset_container (uic, NULL);
		eab_view_discard_menus (view->view);
	}
}

static ECategoriesMasterList *
get_master_list (void)
{
	static ECategoriesMasterList *category_list = NULL;

	if (category_list == NULL)
		category_list = e_categories_master_list_wombat_new ();
	return category_list;
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

	if (view->ecml_changed_id != 0) {
		g_signal_handler_disconnect (get_master_list(),
					     view->ecml_changed_id);
		view->ecml_changed_id = 0;
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

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	AddressbookView *view = closure;

	if (status == E_BOOK_ERROR_OK) {
		view->failed_to_load = FALSE;
		g_object_set(view->view,
			     "book", book,
			     NULL);
		view->book = book;
	}
	else {
		char *label_string;
		GtkWidget *warning_dialog;
		GtkWidget *href = NULL;
		gchar *uri;

		view->failed_to_load = TRUE;

		uri = e_source_get_uri (view->source);

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

        	warning_dialog = gtk_message_dialog_new (
			 NULL /* XXX */,
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
}

static void
destroy_callback(gpointer data, GObject *where_object_was)
{
	AddressbookView *view = data;
	addressbook_view_unref (view);
}

static void
get_prop (BonoboPropertyBag *bag,
	  BonoboArg         *arg,
	  guint              arg_id,
	  CORBA_Environment *ev,
	  gpointer           user_data)
{
	AddressbookView *view = user_data;

	switch (arg_id) {

	case PROPERTY_SOURCE_UID_IDX:
		if (view && view->source)
			BONOBO_ARG_SET_STRING (arg, e_source_peek_uid (view->source));
		else
			BONOBO_ARG_SET_STRING (arg, "");
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
	}
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
	const gchar *uid;

	switch (arg_id) {

	case PROPERTY_SOURCE_UID_IDX:
		if (view->book) {
			/* we've already had a uri set on this view, so unload it */
			e_book_async_unload_uri (view->book); 
			view->source = NULL;
		} else {
			view->book = e_book_new ();
		}

		view->failed_to_load = FALSE;

		uid = BONOBO_ARG_GET_STRING (arg);
		view->source = e_source_list_peek_source_by_uid (view->source_list, uid);

		if (view->source)
			addressbook_load_source (view->book, view->source, book_open_cb, view);
		else
			g_warning ("Could not find source by UID '%s'!", uid);

		break;
		
	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}

enum {
	ESB_FULL_NAME,
	ESB_EMAIL,
	ESB_CATEGORY,
	ESB_ANY,
	ESB_ADVANCED
};

static ESearchBarItem addressbook_search_option_items[] = {
	{ N_("Name begins with"), ESB_FULL_NAME, NULL },
	{ N_("Email begins with"), ESB_EMAIL, NULL },
	{ N_("Category is"), ESB_CATEGORY, NULL }, /* We attach subitems below */
	{ N_("Any field contains"), ESB_ANY, NULL },
	{ N_("Advanced..."), ESB_ADVANCED, NULL },
	{ NULL, -1, NULL }
};

static void
addressbook_search_activated (ESearchBar *esb, AddressbookView *view)
{
	ECategoriesMasterList *master_list;
	char *search_word, *search_query;
	const char *category_name;
	int search_type, subid;

	if (view->ignore_search_changes) {
		return;
	}

	g_object_get(esb,
		     "text", &search_word,
		     "item_id", &search_type,
		     NULL);

	if (search_type == ESB_ADVANCED) {
		gtk_widget_show(eab_search_dialog_new(view->view));
	}
	else {
		if ((search_word && strlen (search_word)) || search_type == ESB_CATEGORY) {
			GString *s = g_string_new ("");
			e_sexp_encode_string (s, search_word);
			switch (search_type) {
			case ESB_ANY:
				search_query = g_strdup_printf ("(contains \"x-evolution-any-field\" %s)",
								s->str);
				break;
			case ESB_FULL_NAME:
				search_query = g_strdup_printf ("(beginswith \"full_name\" %s)",
								s->str);
				break;
			case ESB_EMAIL:
				search_query = g_strdup_printf ("(beginswith \"email\" %s)",
								s->str);
				break;
			case ESB_CATEGORY:
				subid = e_search_bar_get_subitem_id (esb);

				if (subid < 0 || subid == G_MAXINT) {
					/* match everything */
					search_query = g_strdup ("(contains \"x-evolution-any-field\" \"\")");
				} else {
					master_list = get_master_list ();
					category_name = e_categories_master_list_nth (master_list, subid);
					search_query = g_strdup_printf ("(is \"category\" \"%s\")", category_name);
				}
				break;
			default:
				search_query = g_strdup ("(contains \"x-evolution-any-field\" \"\")");
				break;
			}
			g_string_free (s, TRUE);
		} else
			search_query = g_strdup ("(contains \"x-evolution-any-field\" \"\")");

		if (search_query)
			g_object_set (view->view,
				      "query", search_query,
				      NULL);

		g_free (search_query);
	}

	g_free (search_word);
}

static void
addressbook_query_changed (ESearchBar *esb, AddressbookView *view)
{
	int search_type;

	g_object_get(esb,
		     "item_id", &search_type,
		     NULL);

	if (search_type == ESB_ADVANCED) {
		gtk_widget_show(eab_search_dialog_new(view->view));
	}
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

static int
compare_subitems (const void *a, const void *b)
{
	const ESearchBarSubitem *subitem_a = a;
	const ESearchBarSubitem *subitem_b = b;

	return strcoll (subitem_a->text, subitem_b->text);
}

static void
make_suboptions (AddressbookView *view)
{
	ESearchBarSubitem *subitems, *s;
	ECategoriesMasterList *master_list;
	gint i, N;

	master_list = get_master_list ();
	N = e_categories_master_list_count (master_list);
	subitems = g_new (ESearchBarSubitem, N+2);

	subitems[0].id = G_MAXINT;
	subitems[0].text = g_strdup (_("Any Category"));
	subitems[0].translate = FALSE;

	for (i=0; i<N; ++i) {
		const char *category = e_categories_master_list_nth (master_list, i);

		subitems[i+1].id = i;
		subitems[i+1].text = g_strdup (category);
		subitems[i+1].translate = FALSE;
	}
	subitems[N+1].id = -1;
	subitems[N+1].text = NULL;

	qsort (subitems + 1, N, sizeof (subitems[0]), compare_subitems);

	e_search_bar_set_suboption (view->search, ESB_CATEGORY, subitems);

	for (s = subitems; s->id != -1; s++) {
		if (s->text)
			g_free (s->text);
	}
	g_free (subitems);
}

static void
ecml_changed (ECategoriesMasterList *ecml, AddressbookView *view)
{
	make_suboptions (view);
}

static void
connect_master_list_changed (AddressbookView *view)
{
	view->ecml_changed_id =
		g_signal_connect (get_master_list(), "changed",
				  G_CALLBACK (ecml_changed), view);
}

BonoboControl *
addressbook_new_control (void)
{
	AddressbookView *view;

	view = g_new0 (AddressbookView, 1);
	view->refs = 1;
	view->ignore_search_changes = FALSE;

	view->vbox = gtk_vbox_new (FALSE, 0);

	g_object_weak_ref (G_OBJECT (view->vbox), destroy_callback, view);

	/* Create the control. */
	view->control = bonobo_control_new (view->vbox);

	view->search = E_SEARCH_BAR (e_search_bar_new (NULL, addressbook_search_option_items));
	make_suboptions (view);
	connect_master_list_changed (view);

	gtk_box_pack_start (GTK_BOX (view->vbox), GTK_WIDGET (view->search),
			    FALSE, FALSE, 0);
	g_signal_connect (view->search, "query_changed",
			  G_CALLBACK (addressbook_query_changed), view);
	g_signal_connect (view->search, "search_activated",
			  G_CALLBACK (addressbook_search_activated), view);

	view->view = EAB_VIEW(eab_view_new());
	gtk_box_pack_start (GTK_BOX (view->vbox), GTK_WIDGET (view->view),
			    TRUE, TRUE, 0);

	/* create the initial view */
	change_view_type (view, EAB_VIEW_TABLE);

	gtk_widget_show (view->vbox);
	gtk_widget_show (GTK_WIDGET(view->view));
	gtk_widget_show (GTK_WIDGET(view->search));

	view->properties = bonobo_property_bag_new (get_prop, set_prop, view);

	bonobo_property_bag_add (view->properties,
				 PROPERTY_SOURCE_UID, PROPERTY_SOURCE_UID_IDX,
				 BONOBO_ARG_STRING, NULL,
				 _("UID of the contacts source that the view will display"), 0);

	bonobo_control_set_properties (view->control,
				       bonobo_object_corba_objref (BONOBO_OBJECT (view->properties)),
				       NULL);

	g_signal_connect (view->view, "status_message",
			  G_CALLBACK(set_status_message), view);

	g_signal_connect (view->view, "search_result",
			  G_CALLBACK(search_result), view);

	g_signal_connect (view->view, "command_state_change",
			  G_CALLBACK(update_command_state), view);

	view->gconf_client = gconf_client_get_default ();
	view->source_list = e_source_list_new_for_gconf (view->gconf_client,
							 "/apps/evolution/addressbook/sources");
	view->source = NULL;

	g_signal_connect (view->control, "activate",
			  G_CALLBACK (control_activate_cb), view);

	return view->control;
}
