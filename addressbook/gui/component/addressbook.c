/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * addressbook.c: 
 *
 * Author:
 *   Chris Lahey (clahey@ximian.com)
 *
 * (C) 2000 Ximian, Inc.
 */

#include <config.h>

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-exception.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>

#include "e-util/e-categories-master-list-wombat.h"
#include "e-util/e-sexp.h"
#include "e-util/e-passwords.h"
#include "select-names/e-select-names.h"
#include "select-names/e-select-names-manager.h"

#include "evolution-shell-component-utils.h"
#include "evolution-activity-client.h"
#include "e-contact-editor.h"
#include "e-contact-save-as.h"
#include "addressbook-config.h"
#include "addressbook.h"
#include "addressbook-component.h"
#include "addressbook/gui/search/e-addressbook-search-dialog.h"
#include "addressbook/gui/widgets/e-addressbook-view.h"
#include "addressbook/gui/widgets/e-addressbook-util.h"
#include "addressbook/printing/e-contact-print.h"

#include <ebook/e-book.h>
#include <ebook/e-book-util.h>
#include <widgets/misc/e-search-bar.h>
#include <widgets/misc/e-filter-bar.h>

/* This is used for the addressbook status bar */
#define EVOLUTION_CONTACTS_PROGRESS_IMAGE "evolution-contacts-mini.png"
static GdkPixbuf *progress_icon[2] = { NULL, NULL };

#define d(x)

#define PROPERTY_FOLDER_URI          "folder_uri"

#define PROPERTY_FOLDER_URI_IDX      1

typedef struct {
	gint refs;
	EAddressbookView *view;
	ESearchBar *search;
	gint        ecml_changed_id;
	GtkWidget *vbox;
	EvolutionActivityClient *activity;
	BonoboControl *control;
	BonoboPropertyBag *properties;
	char *uri;
	char *passwd;
} AddressbookView;

static void addressbook_view_ref (AddressbookView *);
static void addressbook_view_unref (AddressbookView *);

static void
new_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	EBook *book;
	AddressbookView *view = (AddressbookView *) user_data;

	if (view->view) {
		gtk_object_get(GTK_OBJECT(view->view),
			       "book", &book,
			       NULL);

		g_assert (E_IS_BOOK (book));

		e_addressbook_show_contact_editor (book, e_card_new(""), TRUE, e_addressbook_view_can_create(view->view));
	}
}

static void
new_contact_list_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	EBook *book;
	AddressbookView *view = (AddressbookView *) user_data;

	if (view->view) {
		gtk_object_get(GTK_OBJECT(view->view),
			       "book", &book,
			       NULL);

		g_assert (E_IS_BOOK (book));

		e_addressbook_show_contact_list_editor (book, e_card_new(""), TRUE, e_addressbook_view_can_create(view->view));
	}
}

static void
save_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		e_addressbook_view_save_as(view->view);
}

static void
config_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	addressbook_config (NULL /* XXX */);
}

static void
search_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;

	if (view->view)
		gtk_widget_show(e_addressbook_search_dialog_new(view->view));
}

static void
delete_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		e_addressbook_view_delete_selection(view->view);
}

static void
print_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		e_addressbook_view_print(view->view);
}

static void
print_preview_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		e_addressbook_view_print_preview(view->view);
}

static void
stop_loading_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		e_addressbook_view_stop(view->view);
}

static void
cut_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		e_addressbook_view_cut(view->view);
}

static void
copy_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		e_addressbook_view_copy(view->view);
}

static void
paste_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		e_addressbook_view_paste(view->view);
}

static void
select_all_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		e_addressbook_view_select_all (view->view);
}

static void
send_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		e_addressbook_view_send (view->view);
}

static void
send_contact_to_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		e_addressbook_view_send_to (view->view);
}

static void
forget_passwords_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	e_passwords_forget_passwords();
}

static void
update_command_state (EAddressbookView *eav, AddressbookView *view)
{
	BonoboUIComponent *uic;

	if (view->view == NULL)
		return;

	addressbook_view_ref (view);

	uic = bonobo_control_get_ui_component (view->control);
	
	if (bonobo_ui_component_get_container (uic) != CORBA_OBJECT_NIL) {

		/* New Contact */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactNew",
					      "sensitive",
					      e_addressbook_view_can_create (view->view) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactNewList",
					      "sensitive",
					      e_addressbook_view_can_create (view->view) ? "1" : "0", NULL);
		
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSaveAsVCard",
					      "sensitive",
					      e_addressbook_view_can_save_as (view->view) ? "1" : "0", NULL);
		
		/* Print Contact */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsPrint",
					      "sensitive",
					      e_addressbook_view_can_print (view->view) ? "1" : "0", NULL);
		
		/* Print Contact */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsPrintPreview",
					      "sensitive",
					      e_addressbook_view_can_print (view->view) ? "1" : "0", NULL);
		
		/* Delete Contact */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactDelete",
					      "sensitive",
					      e_addressbook_view_can_delete (view->view) ? "1" : "0", NULL);
		
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsCut",
					      "sensitive",
					      e_addressbook_view_can_cut (view->view) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsCopy",
					      "sensitive",
					      e_addressbook_view_can_copy (view->view) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsPaste",
					      "sensitive",
					      e_addressbook_view_can_paste (view->view) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSelectAll",
					      "sensitive",
					      e_addressbook_view_can_select_all (view->view) ? "1" : "0", NULL);
		
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSendContactToOther",
					      "sensitive",
					      e_addressbook_view_can_send (view->view) ? "1" : "0", NULL);
		
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSendMessageToContact",
					      "sensitive",
					      e_addressbook_view_can_send_to (view->view) ? "1" : "0", NULL);
		
		
		/* Stop */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactStop",
					      "sensitive",
					      e_addressbook_view_can_stop (view->view) ? "1" : "0", NULL);
		
	}

	addressbook_view_unref (view);
}

static void
change_view_type (AddressbookView *view, EAddressbookViewType view_type)
{
	gtk_object_set (GTK_OBJECT (view->view), "type", view_type, NULL);
}

static BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("ContactsPrint", print_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsPrintPreview", print_preview_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsSaveAsVCard", save_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ToolSearch", search_cb),

	BONOBO_UI_UNSAFE_VERB ("AddressbookConfig", config_cb),

	BONOBO_UI_UNSAFE_VERB ("ContactNew", new_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactNewList", new_contact_list_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactDelete", delete_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactStop", stop_loading_cb),

	BONOBO_UI_UNSAFE_VERB ("ContactsCut", cut_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsCopy", copy_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsPaste", paste_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsSelectAll", select_all_contacts_cb),

	BONOBO_UI_UNSAFE_VERB ("ContactsSendContactToOther", send_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsSendMessageToContact", send_contact_to_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsForgetPasswords", forget_passwords_cb),

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] = {
	E_PIXMAP ("/menu/File/New/NewFirstItem/ContactNew", "evolution-contacts-mini.png"),
	E_PIXMAP ("/menu/File/FileOps/ContactsSaveAsVCard", "save-as-16.png"),
	E_PIXMAP ("/menu/File/Print/ContactsPrint", "print.xpm"),
	E_PIXMAP ("/menu/File/Print/ContactsPrintPreview", "print-preview.xpm"),

	E_PIXMAP ("/menu/EditPlaceholder/Edit/ContactsCut", "16_cut.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/ContactsCopy", "16_copy.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/ContactsPaste", "16_paste.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/ContactDelete", "evolution-trash-mini.png"),

	E_PIXMAP ("/menu/Tools/ComponentPlaceholder/ToolSearch", "search-16.png"),
	E_PIXMAP ("/menu/Tools/ComponentPlaceholder/AddressbookConfig", "configure_16_addressbook.xpm"),

	E_PIXMAP ("/Toolbar/ContactNew", "new_contact.xpm"),
	E_PIXMAP ("/Toolbar/ContactNewList", "all_contacts.xpm"),
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

	remote_ui_container = bonobo_control_get_remote_ui_container (control);
	bonobo_ui_component_set_container (uic, remote_ui_container);
	bonobo_object_release_unref (remote_ui_container, NULL);

	bonobo_ui_component_add_verb_list_with_data (
		uic, verbs, view);
	
	bonobo_ui_component_freeze (uic, NULL);

	bonobo_ui_util_set_ui (uic, EVOLUTION_DATADIR,
			       "evolution-addressbook.xml",
			       "evolution-addressbook");

	e_addressbook_view_setup_menus (view->view, uic);

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
			e_addressbook_model_force_folder_bar_message (view->view->model);

	} else {
		bonobo_ui_component_unset_container (uic);
		e_addressbook_view_discard_menus (view->view);
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
	if (view->refs == 0)
		g_free (view);
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
	EBook *book;

	if (view->uri && view->view) {
		gtk_object_get(GTK_OBJECT(view->view),
			       "book", &book,
			       NULL);
		gtk_object_unref (GTK_OBJECT (book));
	}
	
	if (view->properties) {
		bonobo_object_unref (BONOBO_OBJECT(view->properties));
		view->properties = NULL;
	}

	if (view->view) {
		gtk_widget_destroy (GTK_WIDGET (view->view));
		view->view = NULL;
	}
		
	g_free(view->passwd);
	view->passwd = NULL;

	g_free(view->uri);
	view->uri = NULL;

	if (view->refs == 0)
		g_free(view);

	if (view->ecml_changed_id != 0) {
		gtk_signal_disconnect (GTK_OBJECT(get_master_list()),
				       view->ecml_changed_id);
		view->ecml_changed_id = 0;
	}
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	AddressbookView *view = closure;

	if (status == E_BOOK_STATUS_SUCCESS) {
		gtk_object_set(GTK_OBJECT(view->view),
			       "book", book,
			       NULL);
	} else {
		AddressbookSource *source;
		GtkWidget *warning_dialog, *label;

        	warning_dialog = gnome_dialog_new (
        		_("Unable to open addressbook"),
			GNOME_STOCK_BUTTON_CLOSE,
        		NULL);

		source = addressbook_storage_get_source_by_uri (view->uri);

		if (source && source->type == ADDRESSBOOK_SOURCE_LDAP) {
#if HAVE_LDAP
			label = gtk_label_new (
					       _("We were unable to open this addressbook.  This either\n"
						 "means you have entered an incorrect URI, or the LDAP server\n"
						 "is down"));
#else
			label = gtk_label_new (
					       _("This version of Evolution does not have LDAP support\n"
						 "compiled in to it.  If you want to use LDAP in Evolution\n"
						 "you must compile the program from the CVS sources after\n"
						 "retrieving OpenLDAP from the link below.\n"));
#endif
		}
		else {
			label = gtk_label_new (
					       _("We were unable to open this addressbook.  Please check that the\n"
						 "path exists and that you have permission to access it."));
		}

		gtk_misc_set_alignment(GTK_MISC(label),
				       0, .5);
		gtk_label_set_justify(GTK_LABEL(label),
				      GTK_JUSTIFY_LEFT);

		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (warning_dialog)->vbox), 
				    label, TRUE, TRUE, 0);
		gtk_widget_show (label);

#ifndef HAVE_LDAP
		if (source && source->type == ADDRESSBOOK_SOURCE_LDAP) {
			GtkWidget *href;
			href = gnome_href_new ("http://www.openldap.org/", "OpenLDAP at http://www.openldap.org/");
			gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (warning_dialog)->vbox), 
					    href, FALSE, FALSE, 0);
			gtk_widget_show (href);
		}
#endif
		gnome_dialog_run (GNOME_DIALOG (warning_dialog));
		
		gtk_object_destroy (GTK_OBJECT (warning_dialog));
	}
}

static void
destroy_callback(GtkWidget *widget, gpointer data)
{
	AddressbookView *view = data;
	if (view->view && view->view->model && view->view->model->book_view)
		e_book_view_stop (view->view->model->book_view);
	addressbook_view_clear (view);
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

	case PROPERTY_FOLDER_URI_IDX:
		if (view && view->uri)
			BONOBO_ARG_SET_STRING (arg, view->uri);
		else
			BONOBO_ARG_SET_STRING (arg, "");
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
	}
}

char *
addressbook_expand_uri (const char *uri)
{
	char *new_uri;

	if (!strncmp (uri, "file:", 5)) {
		if (strlen (uri + 7) > 3
		    && !strcmp (uri + strlen(uri) - 3, ".db")) {
			/* it's a .db file */
			new_uri = g_strdup (uri);
		}
		else {
			char *file_name;
			/* we assume it's a dir and glom addressbook.db onto the end. */
			file_name = g_concat_dir_and_file(uri + 7, "addressbook.db");
			new_uri = g_strdup_printf("file://%s", file_name);
			g_free(file_name);
		}
	}
	else {
		new_uri = g_strdup (uri);
	}

	return new_uri;
}

typedef struct {
	char *uri;
	EBookCallback cb;
	gpointer closure;
} LoadUriData;

static void
load_uri_auth_cb (EBook *book, EBookStatus status, gpointer closure)
{
	LoadUriData *data = closure;

	if (status != E_BOOK_STATUS_SUCCESS) {
		/* pop up a nice dialog, or redo the authentication
                   bit some number of times. */
	}

	data->cb (book, status, data->closure);

	g_free (data->uri);
	g_free (data);
}


static void
load_uri_cb (EBook *book, EBookStatus status, gpointer closure)
{
	AddressbookSource *source;
	LoadUriData *load_uri_data = closure;

	source = addressbook_storage_get_source_by_uri (load_uri_data->uri);

	if (status == E_BOOK_STATUS_SUCCESS) {
		/* check if the addressbook needs authentication */

		if (source &&
		    source->type == ADDRESSBOOK_SOURCE_LDAP &&
		    source->auth != ADDRESSBOOK_LDAP_AUTH_NONE) {
			const char *password;
			char *pass_dup = NULL;

			password = e_passwords_get_password(load_uri_data->uri);

			if (!password) {
				char *prompt;
				gboolean remember;

				if (source->auth == ADDRESSBOOK_LDAP_AUTH_SIMPLE_BINDDN)
					prompt = g_strdup_printf (_("Enter password for %s (user %s)"),
								  source->name, source->binddn);
				else
					prompt = g_strdup_printf (_("Enter password for %s (user %s)"),
								  source->name, source->email_addr);
				remember = source->remember_passwd;
				pass_dup = e_passwords_ask_password (
								     prompt, load_uri_data->uri, prompt, TRUE,
								     E_PASSWORDS_REMEMBER_FOREVER, &remember,
								     NULL);
				if (remember != source->remember_passwd) {
					source->remember_passwd = remember;
					addressbook_storage_write_sources ();
				}
				g_free (prompt);
			}

			if (password || pass_dup) {
				char *user;

				if (source->auth == ADDRESSBOOK_LDAP_AUTH_SIMPLE_BINDDN)
					user = source->binddn;
				else
					user = source->email_addr;
				if (!user)
					user = "";
				e_book_authenticate_user (book, user, password ? password : pass_dup,
							  addressbook_storage_auth_type_to_string (source->auth),
							  load_uri_auth_cb, closure);
				g_free (pass_dup);
				return;
			}
		}
	}

	load_uri_data->cb (book, status, load_uri_data->closure);
	g_free (load_uri_data->uri);
	g_free (load_uri_data);
}

gboolean
addressbook_load_uri (EBook *book, const char *uri,
		      EBookCallback cb, gpointer closure)
{
	LoadUriData *load_uri_data = g_new (LoadUriData, 1);
	gboolean rv;

	load_uri_data->uri = g_strdup (uri);
	load_uri_data->cb = cb;
	load_uri_data->closure = closure;

	rv = e_book_load_uri (book, uri, load_uri_cb, load_uri_data);

	if (!rv) {
		g_free (load_uri_data->uri);
		g_free (load_uri_data);
	}

	return rv;
}

typedef struct {
	gpointer closure;
	EBookCallback open_response;
} DefaultBookClosure;

static void
addressbook_default_book_open (EBook *book, EBookStatus status, gpointer closure)
{
	DefaultBookClosure *default_book_closure = closure;
	gpointer user_closure = default_book_closure->closure;
	EBookCallback user_response = default_book_closure->open_response;

	g_free (default_book_closure);

	/* special case the protocol not supported error, since we
	   really only want to failover to the local book in the case
	   where there's no installed backend for that protocol.  all
	   other errors (failure to connect, etc.) should get reported
	   to the caller as normal. */
	if (status == E_BOOK_STATUS_PROTOCOL_NOT_SUPPORTED) {
		e_book_load_local_address_book (book, user_response, user_closure);
	}
	else {
		user_response (book, status, user_closure);
	}
}

gboolean
addressbook_load_default_book (EBook *book, EBookCallback open_response, gpointer closure)
{
	char *val;
	gboolean rv;
	CORBA_Environment ev;
	Bonobo_ConfigDatabase config_db;

	g_return_val_if_fail (book != NULL,          FALSE);
	g_return_val_if_fail (E_IS_BOOK (book),      FALSE);
	g_return_val_if_fail (open_response != NULL, FALSE);

	CORBA_exception_init (&ev);
	config_db = addressbook_config_database (&ev);
	val = bonobo_config_get_string (config_db, "/Addressbook/default_book_uri", &ev);
	CORBA_exception_free (&ev);

	if (val) {
		DefaultBookClosure *default_book_closure = g_new (DefaultBookClosure, 1);
		default_book_closure->closure = closure;
		default_book_closure->open_response = open_response;
		rv = addressbook_load_uri (book, val,
					   addressbook_default_book_open, default_book_closure);
		g_free (val);
	}
	else {
		rv = e_book_load_local_address_book (book, open_response, closure);
	}

	if (!rv) {
		g_warning ("Couldn't load default addressbook");
	}

	return rv;
}

static void
set_prop (BonoboPropertyBag *bag,
	  const BonoboArg   *arg,
	  guint              arg_id,
	  CORBA_Environment *ev,
	  gpointer           user_data)
{
	AddressbookView *view = user_data;

	char *uri_data;
	EBook *book;
	
	switch (arg_id) {

	case PROPERTY_FOLDER_URI_IDX:
		gtk_object_get(GTK_OBJECT(view->view),
			       "book", &book,
			       NULL);
		if (view->uri) {
			/* we've already had a uri set on this view, so unload it */
			e_book_unload_uri (book);
			g_free (view->uri);
		} else {
			book = e_book_new ();
		}

		view->uri = g_strdup(BONOBO_ARG_GET_STRING (arg));
		
		uri_data = addressbook_expand_uri (view->uri);

		if (! addressbook_load_uri (book, uri_data, book_open_cb, view))
			printf ("error calling load_uri!\n");

		g_free(uri_data);

		break;
		
	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}

static ESearchBarItem addressbook_search_menu_items[] = {
	E_FILTERBAR_RESET,
	{ NULL, -1, NULL },
};

enum {
	ESB_ANY,
	ESB_FULL_NAME,
	ESB_EMAIL,
	ESB_CATEGORY,
	ESB_ADVANCED
};

static ESearchBarItem addressbook_search_option_items[] = {
	{ N_("Any field contains"), ESB_ANY, NULL },
	{ N_("Name contains"), ESB_FULL_NAME, NULL },
	{ N_("Email contains"), ESB_EMAIL, NULL },
	{ N_("Category is"), ESB_CATEGORY, NULL }, /* We attach subitems below */
	{ N_("Advanced..."), ESB_ADVANCED, NULL },
	{ NULL, -1, NULL }
};

static void
addressbook_menu_activated (ESearchBar *esb, int id, AddressbookView *view)
{
	switch (id) {
	case E_FILTERBAR_RESET_ID:
		/* e_addressbook_view_show_all(view->view); */

		/* Fix option menu if we are using "Category is" */
		if (e_search_bar_get_item_id (esb) == ESB_CATEGORY) {

			e_search_bar_set_subitem_id (esb, G_MAXINT);

		} else {

			e_search_bar_set_text (esb, "");

		}

		break;
	}
}

static void
addressbook_query_changed (ESearchBar *esb, AddressbookView *view)
{
	ECategoriesMasterList *master_list;
	char *search_word, *search_query;
	const char *category_name;
	int search_type, subid;

	gtk_object_get(GTK_OBJECT(esb),
		       "text", &search_word,
		       "item_id", &search_type,
		       NULL);

	if (search_type == ESB_ADVANCED) {
		gtk_widget_show(e_addressbook_search_dialog_new(view->view));
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
				search_query = g_strdup_printf ("(contains \"full_name\" %s)",
								s->str);
				break;
			case ESB_EMAIL:
				search_query = g_strdup_printf ("(contains \"email\" %s)",
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
					search_query = g_strdup_printf ("(contains \"category\" \"%s\")", category_name);
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
			gtk_object_set (GTK_OBJECT(view->view),
					"query", search_query,
					NULL);

		g_free (search_query);
		g_free (search_word);
	}
}

static GNOME_Evolution_ShellView
retrieve_shell_view_interface_from_control (BonoboControl *control)
{
	Bonobo_ControlFrame control_frame;
	GNOME_Evolution_ShellView shell_view_interface;
	CORBA_Environment ev;

	shell_view_interface = gtk_object_get_data (GTK_OBJECT (control),
						    "shell_view_interface");

	if (shell_view_interface)
		return shell_view_interface;

	control_frame = bonobo_control_get_control_frame (control);

	if (control_frame == NULL)
		return CORBA_OBJECT_NIL;

	CORBA_exception_init (&ev);
	shell_view_interface = Bonobo_Unknown_queryInterface (control_frame,
							       "IDL:GNOME/Evolution/ShellView:1.0",
							       &ev);
	CORBA_exception_free (&ev);

	if (shell_view_interface != CORBA_OBJECT_NIL)
		gtk_object_set_data (GTK_OBJECT (control),
				     "shell_view_interface",
				     shell_view_interface);
	else
		g_warning ("Control frame doesn't have Evolution/ShellView.");

	return shell_view_interface;
}

static void
set_status_message (EAddressbookView *eav, const char *message, AddressbookView *view)
{

	if (!message || !*message) {
		if (view->activity) {
			gtk_object_unref (GTK_OBJECT (view->activity));
			view->activity = NULL;
		}
	}
	else if (!view->activity) {
		int display;
		char *clientid = g_strdup_printf ("%p", view);

		if (progress_icon[0] == NULL)
			progress_icon[0] = gdk_pixbuf_new_from_file (EVOLUTION_IMAGESDIR "/" EVOLUTION_CONTACTS_PROGRESS_IMAGE);

		view->activity = evolution_activity_client_new (addressbook_component_get_shell_client(), clientid,
								progress_icon, message, TRUE, &display);

		g_free (clientid);
	}
	else {
		evolution_activity_client_update (view->activity, message, -1.0);
	}

}

static void
set_folder_bar_label (EAddressbookView *eav, const char *message, AddressbookView *view)
{
	CORBA_Environment ev;
	GNOME_Evolution_ShellView shell_view_interface;

	CORBA_exception_init (&ev);

	shell_view_interface = retrieve_shell_view_interface_from_control (view->control);
	if (!shell_view_interface) {
		CORBA_exception_free (&ev);
		return;
	}

	d(g_message("Updating via ShellView"));

	if (message == NULL || message[0] == 0) {
		GNOME_Evolution_ShellView_setFolderBarLabel (shell_view_interface,
							     "",
							     &ev);
	}
	else {
		GNOME_Evolution_ShellView_setFolderBarLabel (shell_view_interface,
							     message,
							     &ev);
	}

	if (BONOBO_EX (&ev))
		g_warning ("Exception in label update: %s",
			   bonobo_exception_get_text (&ev));

	CORBA_exception_free (&ev);
}

/* Our global singleton config database */
static Bonobo_ConfigDatabase config_db = NULL;

Bonobo_ConfigDatabase
addressbook_config_database (CORBA_Environment *ev)
{
	if (config_db == NULL)
		config_db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", ev);

	return config_db;
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
	ESearchBarSubitem *subitems;
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
		subitems[i+1].text = e_utf8_to_locale_string (category);
		subitems[i+1].translate = FALSE;
	}
	subitems[N+1].id = -1;
	subitems[N+1].text = NULL;

	qsort (subitems + 1, N, sizeof (subitems[0]), compare_subitems);

	e_search_bar_set_suboption (view->search, ESB_CATEGORY, subitems);
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
		gtk_signal_connect (GTK_OBJECT (get_master_list()), "changed",
				    GTK_SIGNAL_FUNC (ecml_changed), view);
}

BonoboControl *
addressbook_factory_new_control (void)
{
	AddressbookView *view;
	GtkWidget *frame;

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);

	view = g_new0 (AddressbookView, 1);
	view->refs = 1;

	view->vbox = gtk_vbox_new (FALSE, 0);

	gtk_signal_connect (GTK_OBJECT (view->vbox), "destroy",
			    GTK_SIGNAL_FUNC (destroy_callback),
			    (gpointer) view);

	/* Create the control. */
	view->control = bonobo_control_new (view->vbox);

	view->search = E_SEARCH_BAR(e_search_bar_new(addressbook_search_menu_items,
						     addressbook_search_option_items));
	make_suboptions (view);
	connect_master_list_changed (view);

	gtk_box_pack_start (GTK_BOX (view->vbox), GTK_WIDGET (view->search),
			    FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (view->search), "query_changed",
			    GTK_SIGNAL_FUNC (addressbook_query_changed), view);
	gtk_signal_connect (GTK_OBJECT (view->search), "menu_activated",
			    GTK_SIGNAL_FUNC (addressbook_menu_activated), view);

	view->view = E_ADDRESSBOOK_VIEW(e_addressbook_view_new());
	gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (view->view));
	gtk_box_pack_start (GTK_BOX (view->vbox), frame,
			    TRUE, TRUE, 0);

	/* create the initial view */
	change_view_type (view, E_ADDRESSBOOK_VIEW_MINICARD);

	gtk_widget_show (frame);
	gtk_widget_show (view->vbox);
	gtk_widget_show (GTK_WIDGET(view->view));
	gtk_widget_show (GTK_WIDGET(view->search));

	view->properties = bonobo_property_bag_new (get_prop, set_prop, view);

	bonobo_property_bag_add (view->properties,
				 PROPERTY_FOLDER_URI, PROPERTY_FOLDER_URI_IDX,
				 BONOBO_ARG_STRING, NULL, _("The URI that the Folder Browser will display"), 0);

	bonobo_control_set_properties (view->control,
				       view->properties);

	gtk_signal_connect (GTK_OBJECT (view->view),
			    "status_message",
			    GTK_SIGNAL_FUNC(set_status_message),
			    view);

	gtk_signal_connect (GTK_OBJECT (view->view),
			    "folder_bar_message",
			    GTK_SIGNAL_FUNC(set_folder_bar_label),
			    view);

	gtk_signal_connect (GTK_OBJECT (view->view),
			    "command_state_change",
			    GTK_SIGNAL_FUNC(update_command_state),
			    view);
	
	view->uri = NULL;

	gtk_signal_connect (GTK_OBJECT (view->control), "activate",
			    control_activate_cb, view);

	return view->control;
}

static BonoboObject *
addressbook_factory (BonoboGenericFactory *Factory, void *closure)
{
	return BONOBO_OBJECT (addressbook_factory_new_control ());
}

void
addressbook_factory_init (void)
{
	static BonoboGenericFactory *addressbook_control_factory = NULL;

	if (addressbook_control_factory != NULL)
		return;

	addressbook_control_factory = bonobo_generic_factory_new (
		"OAFIID:GNOME_Evolution_Addressbook_ControlFactory",
		addressbook_factory, NULL);

	if (addressbook_control_factory == NULL) {
		g_error ("I could not register a Addressbook factory.");
	}
}

