/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * addressbook.c: 
 *
 * Author:
 *   Chris Lahey (clahey@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */

#include <config.h>

#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo.h>

#include "addressbook.h"

#include <ebook/e-book.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>
#include "addressbook/gui/search/e-addressbook-search-dialog.h"

#include "addressbook/gui/widgets/e-addressbook-view.h"
#include "filter/e-search-bar.h"

#include <select-names/e-select-names.h>
#include <select-names/e-select-names-manager.h>

#include "e-contact-editor.h"
#include "e-contact-save-as.h"
#include "addressbook-config.h"

#include <addressbook/printing/e-contact-print.h>

#define PROPERTY_FOLDER_URI          "folder_uri"

#define PROPERTY_FOLDER_URI_IDX      1

typedef struct {
	EAddressbookView *view;
	ESearchBar *search;
	GtkWidget *vbox;
	BonoboControl *control;
	BonoboPropertyBag *properties;
	char *uri;
} AddressbookView;

static void
card_added_cb (EBook* book, EBookStatus status, const char *id,
	    gpointer user_data)
{
	g_print ("%s: %s(): a card was added\n", __FILE__, __FUNCTION__);
}

static void
card_modified_cb (EBook* book, EBookStatus status,
		  gpointer user_data)
{
	g_print ("%s: %s(): a card was modified\n", __FILE__, __FUNCTION__);
}

/* Callback for the add_card signal from the contact editor */
static void
add_card_cb (EContactEditor *ce, ECard *card, gpointer data)
{
	EBook *book;

	book = E_BOOK (data);
	e_book_add_card (book, card, card_added_cb, NULL);
}

/* Callback for the commit_card signal from the contact editor */
static void
commit_card_cb (EContactEditor *ce, ECard *card, gpointer data)
{
	EBook *book;

	book = E_BOOK (data);
	e_book_commit_card (book, card, card_modified_cb, NULL);
}

/* Callback for the delete_card signal from the contact editor */
static void
delete_card_cb (EContactEditor *ce, ECard *card, gpointer data)
{
	EBook *book;

	book = E_BOOK (data);
	e_book_remove_card (book, card, card_modified_cb, NULL);
}

/* Callback used when the contact editor is closed */
static void
editor_closed_cb (EContactEditor *ce, gpointer data)
{
	gtk_object_unref (GTK_OBJECT (ce));
}

static void
new_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	ECard *card;
	EBook *book;
	EContactEditor *ce;
	AddressbookView *view = (AddressbookView *) user_data;

	card = e_card_new("");

	gtk_object_get(GTK_OBJECT(view->view),
		       "book", &book,
		       NULL);

	g_assert (E_IS_BOOK (book));

	ce = e_contact_editor_new (card, TRUE);

	gtk_signal_connect (GTK_OBJECT (ce), "add_card",
			    GTK_SIGNAL_FUNC (add_card_cb), book);
	gtk_signal_connect (GTK_OBJECT (ce), "commit_card",
			    GTK_SIGNAL_FUNC (commit_card_cb), book);
	gtk_signal_connect (GTK_OBJECT (ce), "delete_card",
			    GTK_SIGNAL_FUNC (delete_card_cb), book);
	gtk_signal_connect (GTK_OBJECT (ce), "editor_closed",
			    GTK_SIGNAL_FUNC (editor_closed_cb), NULL);

	gtk_object_sink(GTK_OBJECT(card));
}

static void
config_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	addressbook_config (NULL /* XXX */);
}

static void
search_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	EBook *book;
	AddressbookView *view = (AddressbookView *) user_data;

	gtk_object_get(GTK_OBJECT(view->view),
		       "book", &book,
		       NULL);
	g_assert (E_IS_BOOK (book));

	gtk_widget_show(e_addressbook_search_dialog_new(book));
}

#if 0
static void
find_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	gint result;
	GtkWidget* search_entry = gtk_entry_new();
	gchar* search_text;
	AddressbookView *view = (AddressbookView *) user_data;

	GtkWidget* dlg = gnome_dialog_new ("Search Contacts", "Find",
					   GNOME_STOCK_BUTTON_CANCEL, NULL);

	gtk_object_get (view->view,
			"query", &search_text,
			NULL);
	e_utf8_gtk_entry_set_text(GTK_ENTRY(search_entry), search_text);
	g_free (search_text);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dlg)->vbox),
			    search_entry, TRUE, TRUE, 0);

	gtk_widget_show_all (dlg);

	gnome_dialog_close_hides (GNOME_DIALOG (dlg), TRUE);
	result = gnome_dialog_run_and_close (GNOME_DIALOG (dlg));

	/* If the user clicks "okay"...*/
	if (result == 0) {
		search_text = e_utf8_gtk_entry_get_text(GTK_ENTRY(search_entry));
		gtk_object_set (view->view, 
				"query", query, 
				NULL);
		g_free (search_text);
	}
}
#endif

static void
delete_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	e_addressbook_view_delete_selection(view->view);
}

static void
print_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	e_addressbook_view_print(view->view);
}

static void
show_all_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	e_addressbook_view_show_all(view->view);
}

static void
stop_loading_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	e_addressbook_view_stop(view->view);
}

static void
update_view_type (AddressbookView *view)
{
	BonoboUIComponent *uic = bonobo_control_get_ui_component (view->control);
	EAddressbookViewType view_type;

	if (!uic || bonobo_ui_component_get_container (uic) == CORBA_OBJECT_NIL)
		return;

	gtk_object_get (GTK_OBJECT (view->view), "type", &view_type, NULL);

	switch (view_type) {
	case E_ADDRESSBOOK_VIEW_TABLE:
		if (uic)
			bonobo_ui_component_set_prop (uic, "/menu/View/AsTable",
						      "label", _("As _Minicards"), NULL);

		break;
	case E_ADDRESSBOOK_VIEW_MINICARD:
		if (uic)
			bonobo_ui_component_set_prop (uic, "/menu/View/AsTable",
						      "label", _("As _Table"), NULL);
		break;
	default:
		g_warning ("view_type must be either TABLE or MINICARD\n");
		return;
	}	
}

static void
change_view_type (AddressbookView *view, EAddressbookViewType view_type)
{
	gtk_object_set (GTK_OBJECT (view->view), "type", view_type, NULL);

	update_view_type (view);
}

static void
toggle_view_as_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = user_data;
	EAddressbookViewType view_type;

	gtk_object_get (GTK_OBJECT (view->view), "type", &view_type, NULL);

	if (view_type == E_ADDRESSBOOK_VIEW_TABLE)
		change_view_type (view, E_ADDRESSBOOK_VIEW_MINICARD);
	else
		change_view_type (view, E_ADDRESSBOOK_VIEW_TABLE);
}

BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("ContactsPrint", print_cb),
	BONOBO_UI_UNSAFE_VERB ("ViewAsTable", toggle_view_as_cb),
	BONOBO_UI_UNSAFE_VERB ("ViewNewContact", new_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ToolSearch", search_cb),

	BONOBO_UI_UNSAFE_VERB ("AddressbookConfig", config_cb),

	BONOBO_UI_UNSAFE_VERB ("ContactNew", new_contact_cb),
/*	BONOBO_UI_UNSAFE_VERB ("ContactFind", find_contact_cb),*/
	BONOBO_UI_UNSAFE_VERB ("ContactDelete", delete_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactViewAll", show_all_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactStop", stop_loading_cb),
	
	BONOBO_UI_VERB_END
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

	update_view_type (view);

	bonobo_ui_component_thaw (uic, NULL);
}

static void
control_activate_cb (BonoboControl *control, 
		     gboolean activate, 
		     AddressbookView *view)
{
	BonoboUIComponent *uic;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);
	
	if (activate)
		control_activate (control, uic, view);
	else
		bonobo_ui_component_unset_container (uic);
}

static void
addressbook_view_free(AddressbookView *view)
{
	if (view->properties)
		bonobo_object_unref(BONOBO_OBJECT(view->properties));
	g_free(view->uri);
	g_free(view);
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	if (status == E_BOOK_STATUS_SUCCESS) {
		AddressbookView *view = closure;

		gtk_object_set(GTK_OBJECT(view->view),
			       "book", book,
			       NULL);
	} else {
		GtkWidget *warning_dialog, *label, *href;
        	warning_dialog = gnome_dialog_new (
        		_("Unable to open addressbook"),
			GNOME_STOCK_BUTTON_CLOSE,
        		NULL);
        
        	label = gtk_label_new (
        		_("We were unable to open this addressbook.  This either\n"
			  "means you have entered an incorrect URI, or have tried\n"
			  "to access an LDAP server and don't have LDAP support\n"
			  "compiled in.  If you've entered a URI, check the URI for\n"
			  "correctness and reenter.  If not, you probably have\n"
			  "attempted to access an LDAP server.  If you wish to be\n"
			  "able to use LDAP, you'll need to download and install\n"
			  "OpenLDAP and recompile and install Evolution.\n"));
		gtk_misc_set_alignment(GTK_MISC(label),
				       0, .5);
		gtk_label_set_justify(GTK_LABEL(label),
				      GTK_JUSTIFY_LEFT);
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (warning_dialog)->vbox), 
				    label, TRUE, TRUE, 0);
        	gtk_widget_show (label);

		href = gnome_href_new ("http://www.openldap.org/", "OpenLDAP at http://www.openldap.org/");
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (warning_dialog)->vbox), 
				    href, FALSE, FALSE, 0);
        	gtk_widget_show (href);

		gnome_dialog_run (GNOME_DIALOG (warning_dialog));
		
		gtk_object_destroy (GTK_OBJECT (warning_dialog));
	}
}

static void destroy_callback(GtkWidget *widget, gpointer data)
{
	AddressbookView *view = data;
	addressbook_view_free(view);
}

static void
get_prop (BonoboPropertyBag *bag,
	  BonoboArg         *arg,
	  guint              arg_id,
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

static void
set_prop (BonoboPropertyBag *bag,
	  const BonoboArg   *arg,
	  guint              arg_id,
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
		
		if (!strncmp (view->uri, "file:", 5)) {
			char *file_name = g_concat_dir_and_file(view->uri + 7, "addressbook.db");
			uri_data = g_strdup_printf("file://%s", file_name);
			g_free(file_name);
		}
		else {
			uri_data = g_strdup (view->uri);
		}

		if (! e_book_load_uri (book, uri_data, book_open_cb, view))
			printf ("error calling load_uri!\n");

		g_free(uri_data);

		break;
		
	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}

enum {
	ESB_SHOW_ALL,
	ESB_ADVANCED,
};

static ESearchBarItem addressbook_search_menu_items[] = {
	{ N_("Show All"), ESB_SHOW_ALL },
	{ NULL, 0 },
	{ N_("Advanced..."), ESB_ADVANCED},
	{ NULL, -1 }
};

static void
addressbook_menu_activated (ESearchBar *esb, int id, AddressbookView *view)
{
	EBook *book;
	switch (id) {
	case ESB_SHOW_ALL:
		e_addressbook_view_show_all(view->view);
		break;
	case ESB_ADVANCED:
		gtk_object_get(GTK_OBJECT(view->view),
			       "book", &book,
			       NULL);
		g_assert (E_IS_BOOK (book));

		gtk_widget_show(e_addressbook_search_dialog_new(book));
		break;
	}
}

enum {
	ESB_ANY,
	ESB_FULL_NAME,
	ESB_EMAIL,
};

static ESearchBarItem addressbook_search_option_items[] = {
	{ N_("Any field contains"), ESB_ANY },
	{ N_("Name contains"), ESB_FULL_NAME },
	{ N_("Email contains"), ESB_EMAIL },
	{ NULL, -1 }
};

static void
addressbook_query_changed (ESearchBar *esb, AddressbookView *view)
{
	char *search_word, *search_query;
	int search_type;

	gtk_object_get(GTK_OBJECT(esb),
		       "text", &search_word,
		       "option_choice", &search_type,
		       NULL);

	if (search_word && strlen (search_word)) {
		switch (search_type) {
		case ESB_ANY:
			search_query = g_strdup_printf ("(contains \"x-evolution-any-field\" \"%s\")",
							search_word);
			break;
		case ESB_FULL_NAME:
			search_query = g_strdup_printf ("(contains \"full_name\" \"%s\")",
							search_word);
			break;
		case ESB_EMAIL:
			search_query = g_strdup_printf ("(contains \"email\" \"%s\")",
							search_word);
			break;
		default:
			search_query = g_strdup ("(contains \"full_name\" \"\")");
			break;
		}
	} else
		search_query = g_strdup ("(contains \"full_name\" \"\")");

	gtk_object_set (GTK_OBJECT(view->view),
			"query", search_query,
			NULL);

	g_free (search_query);
	g_free (search_word);
}

BonoboControl *
addressbook_factory_new_control (void)
{
	AddressbookView *view;

	view = g_new0 (AddressbookView, 1);

	view->vbox = gtk_vbox_new(FALSE, GNOME_PAD);

	gtk_container_set_border_width(GTK_CONTAINER(view->vbox), GNOME_PAD_SMALL);

	gtk_signal_connect( GTK_OBJECT( view->vbox ), "destroy",
			    GTK_SIGNAL_FUNC( destroy_callback ),
			    ( gpointer ) view );

	/* Create the control. */
	view->control = bonobo_control_new(view->vbox);

	view->search = E_SEARCH_BAR(e_search_bar_new(addressbook_search_menu_items,
						     addressbook_search_option_items));
	gtk_box_pack_start (GTK_BOX (view->vbox), GTK_WIDGET (view->search),
			    FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (view->search), "query_changed",
			    GTK_SIGNAL_FUNC (addressbook_query_changed), view);
	gtk_signal_connect (GTK_OBJECT (view->search), "menu_activated",
			    GTK_SIGNAL_FUNC (addressbook_menu_activated), view);

	view->view = E_ADDRESSBOOK_VIEW(e_addressbook_view_new());
	gtk_box_pack_start(GTK_BOX(view->vbox), GTK_WIDGET(view->view),
			   TRUE, TRUE, 0);

	/* create the initial view */
	change_view_type (view, E_ADDRESSBOOK_VIEW_MINICARD);

	gtk_widget_show( view->vbox );
	gtk_widget_show( GTK_WIDGET(view->view) );
	gtk_widget_show( GTK_WIDGET(view->search) );

	view->properties = bonobo_property_bag_new (get_prop, set_prop, view);

	bonobo_property_bag_add (
		view->properties, PROPERTY_FOLDER_URI, PROPERTY_FOLDER_URI_IDX,
		BONOBO_ARG_STRING, NULL, _("The URI that the Folder Browser will display"), 0);

	bonobo_control_set_properties (view->control,
					 view->properties);

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
