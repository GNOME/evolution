/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-field-chooser.c
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <gtk/gtk.h>

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <gtk/gtkscrolledwindow.h>
#include <gal/e-table/e-table-scrolled.h>
#include <gal/e-table/e-table-model.h>
#include <gal/widgets/e-popup-menu.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/menus/gal-view-factory-etable.h>
#include <gal/menus/gal-view-etable.h>
#include <gal/util/e-xml-utils.h>
#include <libgnomeui/gnome-dialog-util.h>

#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-job.h>
#include <libgnomeprintui/gnome-print-dialog.h>
#include <libgnomeprintui/gnome-print-job-preview.h>

#include "addressbook/printing/e-contact-print.h"
#include "addressbook/printing/e-contact-print-envelope.h"

#include "gal-view-factory-minicard.h"
#include "gal-view-minicard.h"
#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
#include <gal/widgets/e-treeview-selection-model.h>
#include "gal-view-factory-treeview.h"
#include "gal-view-treeview.h"
#endif

#include "e-addressbook-marshal.h"
#include "e-addressbook-view.h"
#include "e-addressbook-model.h"
#include "e-addressbook-util.h"
#include "e-addressbook-table-adapter.h"
#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
#include "e-addressbook-treeview-adapter.h"
#endif
#include "e-addressbook-reflow-adapter.h"
#include "e-minicard-view-widget.h"
#include "e-contact-save-as.h"
#include "e-card-merging.h"

#include "e-contact-editor.h"
#include <gdk/gdkkeysyms.h>
#include <ctype.h>
#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#define SHOW_ALL_SEARCH "(contains \"x-evolution-any-field\" \"\")"

#define d(x)

static void e_addressbook_view_init		(EAddressbookView		 *card);
static void e_addressbook_view_class_init	(EAddressbookViewClass	 *klass);

static void e_addressbook_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_addressbook_view_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static void e_addressbook_view_dispose (GObject *object);
static void change_view_type (EAddressbookView *view, EAddressbookViewType view_type);

static void status_message     (GtkObject *object, const gchar *status, EAddressbookView *eav);
static void search_result      (GtkObject *object, EBookViewStatus status, EAddressbookView *eav);
static void folder_bar_message (GtkObject *object, const gchar *status, EAddressbookView *eav);
static void stop_state_changed (GtkObject *object, EAddressbookView *eav);
static void writable_status (GtkObject *object, gboolean writable, EAddressbookView *eav);
static void backend_died (GtkObject *object, EAddressbookView *eav);
static void command_state_change (EAddressbookView *eav);
static void alphabet_state_change (EAddressbookView *eav, gunichar letter);

static void selection_clear_event (GtkWidget *invisible, GdkEventSelection *event,
				   EAddressbookView *view);
static void selection_received (GtkWidget *invisible, GtkSelectionData *selection_data,
				guint time, EAddressbookView *view);
static void selection_get (GtkWidget *invisible, GtkSelectionData *selection_data,
			   guint info, guint time_stamp, EAddressbookView *view);
static void invisible_destroyed (gpointer data, GObject *where_object_was);

static GtkTableClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_BOOK,
	PROP_QUERY,
	PROP_TYPE,
};

enum {
	STATUS_MESSAGE,
	SEARCH_RESULT,
	FOLDER_BAR_MESSAGE,
	COMMAND_STATE_CHANGE,
	ALPHABET_STATE_CHANGE,
	LAST_SIGNAL
};

enum DndTargetType {
	DND_TARGET_TYPE_VCARD,
};
#define VCARD_TYPE "text/x-vcard"
static GtkTargetEntry drag_types[] = {
	{ VCARD_TYPE, 0, DND_TARGET_TYPE_VCARD },
};
static const int num_drag_types = sizeof (drag_types) / sizeof (drag_types[0]);

static guint e_addressbook_view_signals [LAST_SIGNAL] = {0, };

static GdkAtom clipboard_atom = GDK_NONE;

static GalViewCollection *collection = NULL;

GType
e_addressbook_view_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (EAddressbookViewClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_addressbook_view_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EAddressbookView),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_addressbook_view_init,
		};

		type = g_type_register_static (GTK_TYPE_TABLE, "EAddressbookView", &info, 0);
	}

	return type;
}

static void
e_addressbook_view_class_init (EAddressbookViewClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);

	parent_class = gtk_type_class (gtk_table_get_type ());

	object_class->set_property = e_addressbook_view_set_property;
	object_class->get_property = e_addressbook_view_get_property;
	object_class->dispose = e_addressbook_view_dispose;

	g_object_class_install_property (object_class, PROP_BOOK, 
					 g_param_spec_object ("book",
							      _("Book"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_BOOK,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_QUERY, 
					 g_param_spec_string ("query",
							      _("Query"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_TYPE, 
					 g_param_spec_int ("type",
							   _("Type"),
							   /*_( */"XXX blurb" /*)*/,
							   E_ADDRESSBOOK_VIEW_NONE, 
#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
							   E_ADDRESSBOOK_VIEW_TREEVIEW,
#else
							   E_ADDRESSBOOK_VIEW_MINICARD,
#endif
							   E_ADDRESSBOOK_VIEW_NONE,
							   G_PARAM_READWRITE));

	e_addressbook_view_signals [STATUS_MESSAGE] =
		g_signal_new ("status_message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookViewClass, status_message),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	e_addressbook_view_signals [SEARCH_RESULT] =
		g_signal_new ("search_result",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookViewClass, search_result),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	e_addressbook_view_signals [FOLDER_BAR_MESSAGE] =
		g_signal_new ("folder_bar_message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookViewClass, folder_bar_message),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	e_addressbook_view_signals [COMMAND_STATE_CHANGE] =
		g_signal_new ("command_state_change",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookViewClass, command_state_change),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_addressbook_view_signals [ALPHABET_STATE_CHANGE] =
		g_signal_new ("alphabet_state_change",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookViewClass, alphabet_state_change),
			      NULL, NULL,
			      e_addressbook_marshal_NONE__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);


	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);
}

static void
e_addressbook_view_init (EAddressbookView *eav)
{
	eav->view_type = E_ADDRESSBOOK_VIEW_NONE;

	eav->model = e_addressbook_model_new ();

	g_signal_connect (eav->model,
			  "status_message",
			  G_CALLBACK (status_message),
			  eav);

	g_signal_connect (eav->model,
			  "search_result",
			  G_CALLBACK (search_result),
			  eav);

	g_signal_connect (eav->model,
			  "folder_bar_message",
			  G_CALLBACK (folder_bar_message),
			  eav);

	g_signal_connect (eav->model,
			  "stop_state_changed",
			  G_CALLBACK (stop_state_changed),
			  eav);

	g_signal_connect (eav->model,
			  "writable_status",
			  G_CALLBACK (writable_status),
			  eav);

	g_signal_connect (eav->model,
			  "backend_died",
			  G_CALLBACK (backend_died),
			  eav);

	eav->editable = FALSE;
	eav->book = NULL;
	eav->query = g_strdup (SHOW_ALL_SEARCH);

	eav->object = NULL;
	eav->widget = NULL;

	eav->view_instance = NULL;
	eav->view_menus = NULL;
	eav->uic = NULL;
	eav->current_alphabet_widget = NULL;

	eav->invisible = gtk_invisible_new ();

	gtk_selection_add_target (eav->invisible,
				  clipboard_atom,
				  GDK_SELECTION_TYPE_STRING,
				  0);
		
	g_signal_connect (eav->invisible, "selection_get",
			  G_CALLBACK (selection_get), 
			  eav);
	g_signal_connect (eav->invisible, "selection_clear_event",
			  G_CALLBACK (selection_clear_event),
			  eav);
	g_signal_connect (eav->invisible, "selection_received",
			  G_CALLBACK (selection_received),
			  eav);
	g_object_weak_ref (G_OBJECT (eav->invisible), invisible_destroyed, eav);
}

static void
e_addressbook_view_dispose (GObject *object)
{
	EAddressbookView *eav = E_ADDRESSBOOK_VIEW(object);

	if (eav->model) {
		g_object_unref (eav->model);
		eav->model = NULL;
	}

	if (eav->book) {
		g_object_unref (eav->book);
		eav->book = NULL;
	}

	if (eav->query) {
		g_free(eav->query);
		eav->query = NULL;
	}

	eav->uic = NULL;

	if (eav->view_instance) {
		g_object_unref (eav->view_instance);
		eav->view_instance = NULL;
	}

	if (eav->view_menus) {
		g_object_unref (eav->view_menus);
		eav->view_menus = NULL;
	}

	if (eav->clipboard_cards) {
		g_list_foreach (eav->clipboard_cards, (GFunc)g_object_unref, NULL);
		g_list_free (eav->clipboard_cards);
		eav->clipboard_cards = NULL;
	}
		
	if (eav->invisible) {
		gtk_widget_destroy (eav->invisible);
		eav->invisible = NULL;
	}

	if (G_OBJECT_CLASS(parent_class)->dispose)
		G_OBJECT_CLASS(parent_class)->dispose(object);
}

GtkWidget*
e_addressbook_view_new (void)
{
	GtkWidget *widget = GTK_WIDGET (g_object_new (E_TYPE_ADDRESSBOOK_VIEW, NULL));
	return widget;
}

static void
writable_status (GtkObject *object, gboolean writable, EAddressbookView *eav)
{
	eav->editable = writable;
	command_state_change (eav);
}

static void
init_collection (void)
{
	GalViewFactory *factory;
	ETableSpecification *spec;
	char *galview;

	if (collection == NULL) {
		collection = gal_view_collection_new();

		gal_view_collection_set_title (collection, _("Addressbook"));

		galview = gnome_util_prepend_user_home("/evolution/views/addressbook/");
		gal_view_collection_set_storage_directories
			(collection,
			 EVOLUTION_GALVIEWSDIR "/addressbook/",
			 galview);
		g_free(galview);

		spec = e_table_specification_new();
		e_table_specification_load_from_file (spec, EVOLUTION_ETSPECDIR "/e-addressbook-view.etspec");

		factory = gal_view_factory_etable_new (spec);
		g_object_unref (spec);
		gal_view_collection_add_factory (collection, factory);
		g_object_unref (factory);

		factory = gal_view_factory_minicard_new ();
		gal_view_collection_add_factory (collection, factory);
		g_object_unref (factory);

#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
		factory = gal_view_factory_treeview_new ();
		gal_view_collection_add_factory (collection, factory);
		g_object_unref (factory);
#endif

		gal_view_collection_load(collection);
	}
}

static void
display_view(GalViewInstance *instance,
	     GalView *view,
	     gpointer data)
{
	EAddressbookView *address_view = data;
	if (GAL_IS_VIEW_ETABLE(view)) {
		change_view_type (address_view, E_ADDRESSBOOK_VIEW_TABLE);
		gal_view_etable_attach_table (GAL_VIEW_ETABLE(view), e_table_scrolled_get_table(E_TABLE_SCROLLED(address_view->widget)));
	} else if (GAL_IS_VIEW_MINICARD(view)) {
		change_view_type (address_view, E_ADDRESSBOOK_VIEW_MINICARD);
		gal_view_minicard_attach (GAL_VIEW_MINICARD(view), E_MINICARD_VIEW_WIDGET (address_view->object));
	}
#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
	else if (GAL_IS_VIEW_TREEVIEW (view)) {
		change_view_type (address_view, E_ADDRESSBOOK_VIEW_TREEVIEW);
		gal_view_treeview_attach (GAL_VIEW_TREEVIEW(view), GTK_TREE_VIEW (address_view->object));
	}
#endif
	address_view->current_view = view;
}

static void
setup_menus (EAddressbookView *view)
{
	if (view->book && view->view_instance == NULL) {
		init_collection ();
		view->view_instance = gal_view_instance_new (collection, e_book_get_uri (view->book));
	}

	if (view->view_instance && view->uic) {
		view->view_menus = gal_view_menus_new(view->view_instance);
		gal_view_menus_apply(view->view_menus, view->uic, NULL);

		display_view (view->view_instance, gal_view_instance_get_current_view (view->view_instance), view);

		g_signal_connect(view->view_instance, "display_view",
				 G_CALLBACK (display_view), view);
	}
}

static void
e_addressbook_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	EAddressbookView *eav = E_ADDRESSBOOK_VIEW(object);

	switch (prop_id){
	case PROP_BOOK:
		if (eav->book) {
			g_object_unref (eav->book);
		}
		if (g_value_get_object (value)) {
			eav->book = E_BOOK(g_value_get_object (value));
			g_object_ref (eav->book);
		}
		else
			eav->book = NULL;

		if (eav->view_instance) {
			g_object_unref (eav->view_instance);
			eav->view_instance = NULL;
		}

		g_object_set(eav->model,
			     "book", eav->book,
			     NULL);

		setup_menus (eav);

		break;
	case PROP_QUERY:
#if 0 /* This code will mess up ldap a bit.  We need to think about the ramifications of this more. */
		if ((g_value_get_string (value) == NULL && !strcmp (eav->query, SHOW_ALL_SEARCH)) ||
		    (g_value_get_string (value) != NULL && !strcmp (eav->query, g_value_get_string (value))))
			break;
#endif
		g_free(eav->query);
		eav->query = g_strdup(g_value_get_string (value));
		if (!eav->query)
			eav->query = g_strdup (SHOW_ALL_SEARCH);
		g_object_set(eav->model,
			     "query", eav->query,
			     NULL);
		if (eav->current_alphabet_widget != NULL) {
			GtkWidget *current = eav->current_alphabet_widget;

			eav->current_alphabet_widget = NULL;
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (current), FALSE);
		}
		break;
	case PROP_TYPE:
		change_view_type(eav, g_value_get_int (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_addressbook_view_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	EAddressbookView *eav = E_ADDRESSBOOK_VIEW(object);

	switch (prop_id) {
	case PROP_BOOK:
		if (eav->book)
			g_value_set_object (value, eav->book);
		else
			g_value_set_object (value, NULL);
		break;
	case PROP_QUERY:
		g_value_set_string (value, eav->query);
		break;
	case PROP_TYPE:
		g_value_set_int (value, eav->view_type);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static ESelectionModel*
get_selection_model (EAddressbookView *view)
{
	if (view->view_type == E_ADDRESSBOOK_VIEW_MINICARD)
		return e_minicard_view_widget_get_selection_model (E_MINICARD_VIEW_WIDGET(view->object));
	else if (view->view_type == E_ADDRESSBOOK_VIEW_TABLE)
		return e_table_get_selection_model (e_table_scrolled_get_table (E_TABLE_SCROLLED(view->widget)));
#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
	else if (view->view_type == E_ADDRESSBOOK_VIEW_TREEVIEW) {
		return e_treeview_get_selection_model (GTK_TREE_VIEW (view->object));
	}
#endif
	g_return_val_if_reached (NULL);
}

/* Popup menu stuff */
typedef struct {
	EAddressbookView *view;
	EPopupMenu *submenu;
	gpointer closure;
} CardAndBook;

static ESelectionModel*
card_and_book_get_selection_model (CardAndBook *card_and_book)
{
	return get_selection_model (card_and_book->view);
}

static void
card_and_book_free (CardAndBook *card_and_book)
{
	EAddressbookView *view = card_and_book->view;
	ESelectionModel *selection;

	if (card_and_book->submenu)
		gal_view_instance_free_popup_menu (view->view_instance,
						   card_and_book->submenu);

	selection = card_and_book_get_selection_model (card_and_book);
	if (selection)
		e_selection_model_right_click_up(selection);

	g_object_unref (view);
}

static void
get_card_list_1(gint model_row,
		      gpointer closure)
{
	CardAndBook *card_and_book;
	GList **list;
	EAddressbookView *view;
	ECard *card;

	card_and_book = closure;
	list = card_and_book->closure;
	view = card_and_book->view;

	card = e_addressbook_model_get_card(view->model, model_row);
	*list = g_list_prepend(*list, card);
}

static GList *
get_card_list (CardAndBook *card_and_book)
{
	GList *list = NULL;
	ESelectionModel *selection;

	selection = card_and_book_get_selection_model (card_and_book);

	if (selection) {
		card_and_book->closure = &list;
		e_selection_model_foreach (selection, get_card_list_1, card_and_book);
	}

	return list;
}

static void
has_email_address_1(gint model_row,
			  gpointer closure)
{
	CardAndBook *card_and_book;
	gboolean *has_email;
	EAddressbookView *view;
	const ECard *card;
	EList *email;

	card_and_book = closure;
	has_email = card_and_book->closure;
	view = card_and_book->view;

	if (*has_email)
		return;

	card = e_addressbook_model_peek_card(view->model, model_row);

	g_object_get (G_OBJECT (card),
		      "email", &email,
		      NULL);

	if (e_list_length (email) > 0)
		*has_email = TRUE;
}

static gboolean
get_has_email_address (CardAndBook *card_and_book)
{
	ESelectionModel *selection;
	gboolean has_email = FALSE;

	selection = card_and_book_get_selection_model (card_and_book);

	if (selection) {
		card_and_book->closure = &has_email;
		e_selection_model_foreach (selection, has_email_address_1, card_and_book);
	}

	return has_email;
}

static void
save_as (GtkWidget *widget, CardAndBook *card_and_book)
{
	GList *cards = get_card_list (card_and_book);
	if (cards) {
		e_contact_list_save_as(_("Save as VCard"), cards, NULL);
		e_free_object_list(cards);
	}
}

static void
send_as (GtkWidget *widget, CardAndBook *card_and_book)
{
	GList *cards = get_card_list (card_and_book);
	if (cards) {
		e_addressbook_send_card_list(cards, E_ADDRESSBOOK_DISPOSITION_AS_ATTACHMENT);
		e_free_object_list(cards);
	}
}

static void
send_to (GtkWidget *widget, CardAndBook *card_and_book)

{
	GList *cards = get_card_list (card_and_book);

	if (cards) {
		e_addressbook_send_card_list(cards, E_ADDRESSBOOK_DISPOSITION_AS_TO);
		e_free_object_list(cards);
	}
}

static void
print (GtkWidget *widget, CardAndBook *card_and_book)
{
	GList *cards = get_card_list (card_and_book);
	if (cards) {
		if (cards->next)
			gtk_widget_show(e_contact_print_card_list_dialog_new(cards));
		else
			gtk_widget_show(e_contact_print_card_dialog_new(cards->data));
		e_free_object_list(cards);
	}
}

#if 0 /* Envelope printing is disabled for Evolution 1.0. */
static void
print_envelope (GtkWidget *widget, CardAndBook *card_and_book)
{
	GList *cards = get_card_list (card_and_book);
	if (cards) {
		gtk_widget_show(e_contact_list_print_envelope_dialog_new(card_and_book->card));
		e_free_object_list(cards);
	}
}
#endif

static void
copy (GtkWidget *widget, CardAndBook *card_and_book)
{
	e_addressbook_view_copy (card_and_book->view);
}

static void
paste (GtkWidget *widget, CardAndBook *card_and_book)
{
	e_addressbook_view_paste (card_and_book->view);
}

static void
cut (GtkWidget *widget, CardAndBook *card_and_book)
{
	e_addressbook_view_cut (card_and_book->view);
}

static void
delete (GtkWidget *widget, CardAndBook *card_and_book)
{
	if (e_contact_editor_confirm_delete(GTK_WINDOW(gtk_widget_get_toplevel(card_and_book->view->widget)))) {
		EBook *book;
		GList *list = get_card_list(card_and_book);
		GList *iterator;
		gboolean bulk_remove = FALSE;

		bulk_remove = e_book_check_static_capability (card_and_book->view->model->book,
							      "bulk-remove");

		g_object_get(card_and_book->view->model,
			     "book", &book,
			     NULL);

		if (bulk_remove) {
			GList *ids = NULL;

			for (iterator = list; iterator; iterator = iterator->next) {
				ECard *card = iterator->data;
				ids = g_list_prepend (ids, (char*)e_card_get_id (card));
			}

			/* Remove the cards all at once. */
			e_book_remove_cards (book,
					     ids,
					     NULL,
					     NULL);
			
			g_list_free (ids);
		}
		else {
			for (iterator = list; iterator; iterator = iterator->next) {
				ECard *card = iterator->data;
				/* Remove the card. */
				e_book_remove_card (book,
						    card,
						    NULL,
						    NULL);
			}
		}
		e_free_object_list(list);
	}
}

static void
copy_to_folder (GtkWidget *widget, CardAndBook *card_and_book)
{
	e_addressbook_view_copy_to_folder (card_and_book->view);
}

static void
move_to_folder (GtkWidget *widget, CardAndBook *card_and_book)
{
	e_addressbook_view_move_to_folder (card_and_book->view);
}

static void
free_popup_info (GtkWidget *w, CardAndBook *card_and_book)
{
	card_and_book_free (card_and_book);
}

static void
new_card (GtkWidget *widget, CardAndBook *card_and_book)
{
	EBook *book;

	g_object_get(card_and_book->view->model,
		     "book", &book,
		     NULL);
	e_addressbook_show_contact_editor (book, e_card_new(""), TRUE, TRUE);
}

static void
new_list (GtkWidget *widget, CardAndBook *card_and_book)
{
	EBook *book;

	g_object_get(card_and_book->view->model,
		     "book", &book,
		     NULL);
	e_addressbook_show_contact_list_editor (book, e_card_new(""), TRUE, TRUE);
}

#if 0
static void
sources (GtkWidget *widget, CardAndBook *card_and_book)
{
	BonoboControl *control;
	GNOME_Evolution_ShellView shell_view;
	CORBA_Environment ev;

	control = g_object_get_data (G_OBJECT (gcal), "control");
	if (control == NULL)
		return;

	shell_view = get_shell_view_interface (control);
	if (shell_view == CORBA_OBJECT_NIL)
		return;

	CORBA_exception_init (&ev);
	
	GNOME_Evolution_ShellView_showSettings (shell_view, &ev);
	
	if (BONOBO_EX (&ev))
		g_message ("control_util_show_settings(): Could not show settings");

	CORBA_exception_free (&ev);
}
#endif

#define POPUP_READONLY_MASK 0x1
#define POPUP_NOSELECTION_MASK 0x2
#define POPUP_NOEMAIL_MASK 0x4

static void
do_popup_menu(EAddressbookView *view, GdkEvent *event)
{
	CardAndBook *card_and_book;
	GtkMenu *popup;
	EPopupMenu *submenu = NULL;
	ESelectionModel *selection_model;
	gboolean selection = FALSE;

	EPopupMenu menu[] = {
		E_POPUP_ITEM (N_("New Contact..."), G_CALLBACK(new_card), POPUP_READONLY_MASK),
		E_POPUP_ITEM (N_("New Contact List..."), G_CALLBACK(new_list), POPUP_READONLY_MASK),
		E_POPUP_SEPARATOR,
#if 0
		E_POPUP_ITEM (N_("Go to Folder..."), G_CALLBACK (goto_folder), 0),
		E_POPUP_ITEM (N_("Import..."), G_CALLBACK (import), POPUP_READONLY_MASK),
		E_POPUP_SEPARATOR,
		E_POPUP_ITEM (N_("Search for Contacts..."), G_CALLBACK (search), 0),
		E_POPUP_ITEM (N_("Addressbook Sources..."), G_CALLBACK (sources), 0),
		E_POPUP_SEPARATOR,
		E_POPUP_ITEM (N_("Pilot Settings..."), G_CALLBACK (pilot_settings), 0),
#endif
		E_POPUP_SEPARATOR,
		E_POPUP_ITEM (N_("Save as VCard"), G_CALLBACK(save_as), POPUP_NOSELECTION_MASK),
		E_POPUP_ITEM (N_("Forward Contact"), G_CALLBACK(send_as), POPUP_NOSELECTION_MASK),
		E_POPUP_ITEM (N_("Send Message to Contact"), G_CALLBACK(send_to), POPUP_NOSELECTION_MASK | POPUP_NOEMAIL_MASK),
		E_POPUP_ITEM (N_("Print"), G_CALLBACK(print), POPUP_NOSELECTION_MASK),
#if 0 /* Envelope printing is disabled for Evolution 1.0. */
		E_POPUP_ITEM (N_("Print Envelope"), G_CALLBACK(print_envelope), POPUP_NOSELECTION_MASK),
#endif
		E_POPUP_SEPARATOR,

		E_POPUP_ITEM (N_("Copy to folder..."), G_CALLBACK(copy_to_folder), POPUP_NOSELECTION_MASK), 
		E_POPUP_ITEM (N_("Move to folder..."), G_CALLBACK(move_to_folder), POPUP_READONLY_MASK | POPUP_NOSELECTION_MASK),
		E_POPUP_SEPARATOR,

		E_POPUP_ITEM (N_("Cut"), G_CALLBACK (cut), POPUP_READONLY_MASK | POPUP_NOSELECTION_MASK),
		E_POPUP_ITEM (N_("Copy"), G_CALLBACK (copy), POPUP_NOSELECTION_MASK),
		E_POPUP_ITEM (N_("Paste"), G_CALLBACK (paste), POPUP_READONLY_MASK),
		E_POPUP_ITEM (N_("Delete"), G_CALLBACK(delete), POPUP_READONLY_MASK | POPUP_NOSELECTION_MASK),
		E_POPUP_SEPARATOR,

#if 0
		E_POPUP_SUBMENU (N_("Current View"), submenu = gal_view_instance_get_popup_menu (view->view_instance), 0),
#endif
		E_POPUP_TERMINATOR
	};

	card_and_book = g_new(CardAndBook, 1);
	card_and_book->view = view;
	card_and_book->submenu = submenu;

	g_object_ref (card_and_book->view);

	selection_model = card_and_book_get_selection_model (card_and_book);
	if (selection_model)
		selection = e_selection_model_selected_count (selection_model) > 0;

	popup = e_popup_menu_create (menu,
				     0,
				     (e_addressbook_model_editable (view->model) ? 0 : POPUP_READONLY_MASK) +
				     (selection ? 0 : POPUP_NOSELECTION_MASK) +
				     (get_has_email_address (card_and_book) ? 0 : POPUP_NOEMAIL_MASK),
				     card_and_book);

	g_signal_connect (popup, "selection-done",
			  G_CALLBACK (free_popup_info), card_and_book);
	e_popup_menu (popup, event);

}


/* Minicard view stuff */

/* Translators: put here a list of labels you want to see on buttons in
   addressbook. You may use any character to separate labels but it must
   also be placed at the begining ot the string */
const char *button_labels = N_(",123,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z");
/* Translators: put here a list of characters that correspond to buttons
   in addressbook. You may use any character to separate labels but it
   must also be placed at the begining ot the string.
   Use lower case letters if possible. */
const char *button_letters = N_(",0,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z");

typedef struct {
	EAddressbookView *view;
	GtkWidget *button;
	GtkWidget *vbox;
	gchar *letters;
} LetterClosure;

static char **
e_utf8_split (const char *utf8_str, gunichar delim)
{
	GSList *str_list = NULL, *sl;
	int n = 0;
	const char *str, *s;
	char **str_array;

	g_return_val_if_fail (utf8_str != NULL, NULL);

	str = utf8_str;
	while (*str != '\0') {
		int len;
		char *new_str;

		for (s = str; *s != '\0' && g_utf8_get_char (s) != delim; s = g_utf8_next_char (s))
			;
		len = s - str;
		new_str = g_new (char, len + 1);
		if (len > 0) {
			memcpy (new_str, str, len);
		}
		new_str[len] = '\0';
		str_list = g_slist_prepend (str_list, new_str);
		n++;
		if (*s != '\0') {
			str = g_utf8_next_char (s);
		} else {
			str = s;
		}		
	}

	str_array = g_new (char *, n + 1);
	str_array[n--] = NULL;
	for (sl = str_list; sl != NULL; sl = sl->next) {
		str_array[n--] = sl->data;
	}
	g_slist_free (str_list);

	return str_array;
}

static void
jump_to_letters (EAddressbookView *view, gchar* l)
{
	char *query;
	char *s;
	char buf[6 + 1];

	if (g_unichar_isdigit (g_utf8_get_char(l))) {
		const char *letters = _(button_letters);
		char **letter_v;
		GString *gstr;
		char **p;

		letter_v = e_utf8_split (g_utf8_next_char (letters),
		                         g_utf8_get_char (letters));
		g_assert (letter_v != NULL && letter_v[0] != NULL);
		gstr = g_string_new ("(not (or ");
		for (p = letter_v + 1; *p != NULL; p++) {
			for (s = *p; *s != '\0'; s = g_utf8_next_char (s)) {
				buf [g_unichar_to_utf8 (g_utf8_get_char(s), buf)] = '\0';
				g_string_append_printf (gstr, "(beginswith \"file_as\" \"%s\")", buf);
			}
		}
		g_string_append (gstr, "))");
		query = gstr->str;
		g_strfreev (letter_v);
		g_string_free (gstr, FALSE);
	} else {
		GString *gstr;

		gstr = g_string_new ("(or ");

		for (s = l; *s != '\0'; s = g_utf8_next_char (s)) {
			buf [g_unichar_to_utf8 (g_utf8_get_char(s), buf)] = '\0';
			g_string_append_printf (gstr, "(beginswith \"file_as\" \"%s\")", buf);
		}

		g_string_append (gstr, ")");
		query = gstr->str;
		g_string_free (gstr, FALSE);
	}
	g_object_set (view,
		      "query", query,
		      NULL);
	g_free (query);
}

static void
button_toggled(GtkWidget *button, LetterClosure *closure)
{
	EAddressbookView *view = closure->view;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
		GtkWidget *current = view->current_alphabet_widget;

		view->current_alphabet_widget = NULL;
		if (current && current != button)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (current), FALSE);
		jump_to_letters (view, closure->letters);
		view->current_alphabet_widget = button;
		alphabet_state_change (view, g_utf8_get_char(closure->letters));
	} else {
		if (view->current_alphabet_widget != NULL &&
		    view->current_alphabet_widget == button) {
			view->current_alphabet_widget = NULL;
			g_object_set (view,
				      "query", NULL,
				      NULL);
			alphabet_state_change (view, 0);
		}
	}
}

static void
free_closure(gpointer data, GObject *where_object_was)
{
	GtkWidget *button = GTK_WIDGET (where_object_was);
	LetterClosure *closure = data;
	if (button != NULL &&
	    button == closure->view->current_alphabet_widget) {
		closure->view->current_alphabet_widget = NULL;
	}
	g_free (closure->letters);
	g_free (closure);
}

static GtkWidget *
create_alphabet (EAddressbookView *view)
{
	GtkWidget *widget, *viewport, *vbox;
	const char *labels, *letters;
	char **label_v, **letter_v;
	char **pl, **pc;
	gunichar sep;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
	                                GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (widget), viewport);
	gtk_container_set_border_width (GTK_CONTAINER (viewport), 4);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);

	vbox = gtk_vbox_new (FALSE, 4);
	gtk_container_add (GTK_CONTAINER (viewport), vbox);

	labels = _(button_labels);
	sep = g_utf8_get_char (labels);
	label_v = e_utf8_split (g_utf8_next_char (labels), sep);
	letters = _(button_letters);
	sep = g_utf8_get_char (letters);
	letter_v = e_utf8_split (g_utf8_next_char (letters), sep);
	g_assert (label_v != NULL && letter_v != NULL);
	for (pl = label_v, pc = letter_v; *pl != NULL && *pc != NULL; pl++, pc++) {
		GtkWidget *button;
		LetterClosure *closure;
		char *label;

		label = *pl;
		button = gtk_toggle_button_new_with_label (label);
		gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

		closure = g_new (LetterClosure, 1);
		closure->view = view;
		closure->letters = g_strdup (*pc);
		closure->button = button;
		closure->vbox = vbox;
		g_signal_connect(button, "toggled",
				 G_CALLBACK (button_toggled), closure);
		g_object_weak_ref (G_OBJECT (button), free_closure, closure);

	}
	g_strfreev (label_v);
	g_strfreev (letter_v);

	gtk_widget_show_all (widget);

	return widget;
}

static void
selection_changed (GObject *o, EAddressbookView *view)
{
	command_state_change (view);
}

static void
minicard_right_click (EMinicardView *minicard_view_item, GdkEvent *event, EAddressbookView *view)
{
	do_popup_menu(view, event);
}

static void
create_minicard_view (EAddressbookView *view)
{
	GtkWidget *scrolled_window;
	GtkWidget *alphabet;
	GtkWidget *minicard_view;
	GtkWidget *minicard_hbox;
	EAddressbookReflowAdapter *adapter;

	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	minicard_hbox = gtk_hbox_new(FALSE, 0);

	adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(e_addressbook_reflow_adapter_new (view->model));
	minicard_view = e_minicard_view_widget_new(adapter);

	/* A hack */
	g_object_set_data (G_OBJECT (adapter), "view", view);

	g_signal_connect(minicard_view, "selection_change",
			 G_CALLBACK(selection_changed), view);

	g_signal_connect(minicard_view, "right_click",
			 G_CALLBACK(minicard_right_click), view);


	view->object = G_OBJECT(minicard_view);
	view->widget = minicard_hbox;

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (scrolled_window), minicard_view);


	gtk_box_pack_start(GTK_BOX(minicard_hbox), scrolled_window, TRUE, TRUE, 0);

	alphabet = create_alphabet(view);
	if (alphabet)
		gtk_box_pack_start(GTK_BOX(minicard_hbox), alphabet, FALSE, FALSE, 0);

	gtk_table_attach(GTK_TABLE(view), minicard_hbox,
			 0, 1,
			 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
			 0, 0);

	gtk_widget_show_all( GTK_WIDGET(minicard_hbox) );

	gtk_widget_pop_colormap ();

	e_reflow_model_changed (E_REFLOW_MODEL (adapter));

	g_object_unref (adapter);
}

static void
table_double_click(ETableScrolled *table, gint row, gint col, GdkEvent *event, EAddressbookView *view)
{
	if (E_IS_ADDRESSBOOK_TABLE_ADAPTER(view->object)) {
		EAddressbookModel *model = view->model;
		ECard *card = e_addressbook_model_get_card(model, row);
		EBook *book;

		g_object_get(model,
			     "book", &book,
			     NULL);
		
		g_assert (E_IS_BOOK (book));

		if (e_card_evolution_list (card))
			e_addressbook_show_contact_list_editor (book, card, FALSE, view->editable);
		else
			e_addressbook_show_contact_editor (book, card, FALSE, view->editable);
	}
}

static gint
table_right_click(ETableScrolled *table, gint row, gint col, GdkEvent *event, EAddressbookView *view)
{
	do_popup_menu(view, event);
	return TRUE;
}

static gint
table_white_space_event(ETableScrolled *table, GdkEvent *event, EAddressbookView *view)
{
	if (event->type == GDK_BUTTON_PRESS && ((GdkEventButton *)event)->button == 3) {
		do_popup_menu(view, event);
		return TRUE;
	} else {
		return FALSE;
	}
}

static void
table_drag_data_get (ETable             *table,
		     int                 row,
		     int                 col,
		     GdkDragContext     *context,
		     GtkSelectionData   *selection_data,
		     guint               info,
		     guint               time,
		     gpointer            user_data)
{
	EAddressbookView *view = user_data;

	if (!E_IS_ADDRESSBOOK_TABLE_ADAPTER(view->object))
		return;

	switch (info) {
	case DND_TARGET_TYPE_VCARD: {
		char *value;

		value = e_card_get_vcard(view->model->data[row]);

		gtk_selection_data_set (selection_data,
					selection_data->target,
					8,
					value, strlen (value));
		break;
	}
	}
}

static void
emit_status_message (EAddressbookView *eav, const gchar *status)
{
	g_signal_emit (eav,
		       e_addressbook_view_signals [STATUS_MESSAGE], 0,
		       status);
}

static void
emit_search_result (EAddressbookView *eav, EBookViewStatus status)
{
	g_signal_emit (eav,
		       e_addressbook_view_signals [SEARCH_RESULT], 0,
		       status);
}

static void
emit_folder_bar_message (EAddressbookView *eav, const gchar *message)
{
	g_signal_emit (eav,
		       e_addressbook_view_signals [FOLDER_BAR_MESSAGE], 0,
		       message);
}

static void
status_message (GtkObject *object, const gchar *status, EAddressbookView *eav)
{
	emit_status_message (eav, status);
}

static void
search_result (GtkObject *object, EBookViewStatus status, EAddressbookView *eav)
{
	emit_search_result (eav, status);
}

static void
folder_bar_message (GtkObject *object, const gchar *status, EAddressbookView *eav)
{
	emit_folder_bar_message (eav, status);
}

static void
stop_state_changed (GtkObject *object, EAddressbookView *eav)
{
	command_state_change (eav);
}

static void
command_state_change (EAddressbookView *eav)
{
	/* Reffing during emission is unnecessary.  Gtk automatically refs during an emission. */
	g_signal_emit (eav, e_addressbook_view_signals [COMMAND_STATE_CHANGE], 0);
}

static void
alphabet_state_change (EAddressbookView *eav, gunichar letter)
{
	g_signal_emit (eav, e_addressbook_view_signals [ALPHABET_STATE_CHANGE], 0, letter);
}

static void
backend_died (GtkObject *object, EAddressbookView *eav)
{
	char *message = g_strdup_printf (_("The addressbook backend for\n%s\nhas crashed. "
					   "You will have to restart Evolution in order "
					   "to use it again"),
					 e_book_get_uri (eav->book));
        gnome_error_dialog_parented (message, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (eav))));
        g_free (message);
}

static void
create_table_view (EAddressbookView *view)
{
	ETableModel *adapter;
	GtkWidget *table;
	
	adapter = e_addressbook_table_adapter_new(view->model);

	/* Here we create the table.  We give it the three pieces of
	   the table we've created, the header, the model, and the
	   initial layout.  It does the rest.  */
	table = e_table_scrolled_new_from_spec_file (adapter, NULL, EVOLUTION_ETSPECDIR "/e-addressbook-view.etspec", NULL);

	view->object = G_OBJECT(adapter);
	view->widget = table;

	g_signal_connect(e_table_scrolled_get_table(E_TABLE_SCROLLED(table)), "double_click",
			 G_CALLBACK(table_double_click), view);
	g_signal_connect(e_table_scrolled_get_table(E_TABLE_SCROLLED(table)), "right_click",
			 G_CALLBACK(table_right_click), view);
	g_signal_connect(e_table_scrolled_get_table(E_TABLE_SCROLLED(table)), "white_space_event",
			 G_CALLBACK(table_white_space_event), view);
	g_signal_connect(e_table_scrolled_get_table(E_TABLE_SCROLLED(table)), "selection_change",
			 G_CALLBACK(selection_changed), view);

	/* drag & drop signals */
	e_table_drag_source_set (E_TABLE(E_TABLE_SCROLLED(table)->table), GDK_BUTTON1_MASK,
				 drag_types, num_drag_types, GDK_ACTION_MOVE);
	
	g_signal_connect (E_TABLE_SCROLLED(table)->table,
			  "table_drag_data_get",
			  G_CALLBACK (table_drag_data_get),
			  view);

	gtk_table_attach(GTK_TABLE(view), table,
			 0, 1,
			 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
			 0, 0);

	gtk_widget_show( GTK_WIDGET(table) );
}

#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
static void
treeview_row_activated(GtkTreeView *treeview,
		       GtkTreePath *path, GtkTreeViewColumn *column,
		       EAddressbookView *view)
{
	EAddressbookModel *model = view->model;
	int row = gtk_tree_path_get_indices (path)[0];
	ECard *card = e_addressbook_model_get_card(model, row);
	EBook *book;

	g_object_get(model,
		     "book", &book,
		     NULL);
		
	g_assert (E_IS_BOOK (book));

	if (e_card_evolution_list (card))
		e_addressbook_show_contact_list_editor (book, card, FALSE, view->editable);
	else
		e_addressbook_show_contact_editor (book, card, FALSE, view->editable);
}

static void
create_treeview_view (EAddressbookView *view)
{
	GtkTreeModel *adapter;
	ECardSimple *simple;
	GtkWidget *treeview;
	GtkWidget *scrolled;
	int i;

	simple = e_card_simple_new(NULL);

	adapter = e_addressbook_treeview_adapter_new(view->model);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	treeview = gtk_tree_view_new_with_model (adapter);

	gtk_widget_show (treeview);

	gtk_container_add (GTK_CONTAINER (scrolled), treeview);

	for (i = 0; i < 15; i ++) {
		GtkTreeViewColumn *column =
			gtk_tree_view_column_new_with_attributes (e_card_simple_get_name (simple, i),
								  gtk_cell_renderer_text_new (),
								  "text", i,
								  NULL);

		gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
	}

	view->object = G_OBJECT(treeview);
	view->widget = scrolled;

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)), GTK_SELECTION_MULTIPLE);
	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (treeview), 
						GDK_BUTTON1_MASK,
						drag_types,
						num_drag_types,
						GDK_ACTION_MOVE);

	g_signal_connect(treeview, "row_activated",
			 G_CALLBACK (treeview_row_activated), view);
#if 0
	g_signal_connect(e_table_scrolled_get_table(E_TABLE_SCROLLED(table)), "right_click",
			 G_CALLBACK(table_right_click), view);

	/* drag & drop signals */
	e_table_drag_source_set (E_TABLE(E_TABLE_SCROLLED(table)->table), GDK_BUTTON1_MASK,
				 drag_types, num_drag_types, GDK_ACTION_MOVE);
	
	g_signal_connect (E_TABLE_SCROLLED(table)->table,
			  "table_drag_data_get",
			  G_CALLBACK (table_drag_data_get),
			  view);
#endif


	g_signal_connect(e_treeview_get_selection_model (GTK_TREE_VIEW (treeview)), "selection_changed",
			 G_CALLBACK(selection_changed), view);

	gtk_table_attach(GTK_TABLE(view), scrolled,
			 0, 1,
			 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
			 0, 0);

	gtk_widget_show( GTK_WIDGET(scrolled) );

	g_object_unref (simple);
}
#endif

static void
change_view_type (EAddressbookView *view, EAddressbookViewType view_type)
{
	if (view_type == view->view_type)
		return;

	if (view->widget) {
		gtk_widget_destroy (view->widget);
		view->widget = NULL;
	}
	view->object = NULL;

	switch (view_type) {
	case E_ADDRESSBOOK_VIEW_MINICARD:
		create_minicard_view (view);
		break;
	case E_ADDRESSBOOK_VIEW_TABLE:
		create_table_view (view);
		break;
#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
	case E_ADDRESSBOOK_VIEW_TREEVIEW:
		create_treeview_view (view);
		break;
#endif
	default:
		g_warning ("view_type not recognized.");
		return;
	}

	view->view_type = view_type;

	command_state_change (view);
}

typedef struct {
	GtkWidget *table;
	GObject *printable;
} EContactPrintDialogWeakData;

static void
e_contact_print_destroy(gpointer data, GObject *where_object_was)
{
	EContactPrintDialogWeakData *weak_data = data;
	g_object_unref (weak_data->printable);
	g_object_unref (weak_data->table);
	g_free (weak_data);
}

static void
e_contact_print_button(GtkDialog *dialog, gint response, gpointer data)
{
	GnomePrintJob *master;
	GnomePrintContext *pc;
	EPrintable *printable = g_object_get_data(G_OBJECT(dialog), "printable");
	GtkWidget *preview;
	switch( response ) {
	case GNOME_PRINT_DIALOG_RESPONSE_PRINT:
		master = gnome_print_job_new(gnome_print_dialog_get_config ( GNOME_PRINT_DIALOG(dialog) ));
		pc = gnome_print_job_get_context( master );
		e_printable_reset(printable);
		while (e_printable_data_left(printable)) {
			if (gnome_print_gsave(pc) == -1)
				/* FIXME */;
			if (gnome_print_translate(pc, 72, 72) == -1)
				/* FIXME */;
			e_printable_print_page(printable,
					       pc,
					       6.5 * 72,
					       5 * 72,
					       TRUE);
			if (gnome_print_grestore(pc) == -1)
				/* FIXME */;
			if (gnome_print_showpage(pc) == -1)
				/* FIXME */;
		}
		gnome_print_job_close(master);
		gnome_print_job_print(master);
		g_object_unref (master);
		gtk_widget_destroy((GtkWidget *)dialog);
		break;
	case GNOME_PRINT_DIALOG_RESPONSE_PREVIEW:
		master = gnome_print_job_new (gnome_print_dialog_get_config ( GNOME_PRINT_DIALOG(dialog) ));
		pc = gnome_print_job_get_context( master );
		e_printable_reset(printable);
		while (e_printable_data_left(printable)) {
			if (gnome_print_gsave(pc) == -1)
				/* FIXME */;
			if (gnome_print_translate(pc, 72, 72) == -1)
				/* FIXME */;
			e_printable_print_page(printable,
					       pc,
					       6.5 * 72,
					       9 * 72,
					       TRUE);
			if (gnome_print_grestore(pc) == -1)
				/* FIXME */;
			if (gnome_print_showpage(pc) == -1)
				/* FIXME */;
		}
		gnome_print_job_close(master);
		preview = GTK_WIDGET(gnome_print_job_preview_new(master, "Print Preview"));
		gtk_widget_show_all(preview);
		g_object_unref (master);
		break;
	case GNOME_PRINT_DIALOG_RESPONSE_CANCEL:
	default:
		gtk_widget_destroy((GtkWidget *)dialog);
		break;
	}
}

void
e_addressbook_view_setup_menus (EAddressbookView *view,
				BonoboUIComponent *uic)
{

	g_return_if_fail (view != NULL);
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));
	g_return_if_fail (uic != NULL);
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (uic));

	init_collection ();

	view->uic = uic;

	setup_menus (view);
}

/**
 * e_addressbook_view_discard_menus:
 * @view: An addressbook view.
 * 
 * Makes an addressbook view discard its GAL view menus and its views instance
 * objects.  This should be called when the corresponding Bonobo component is
 * deactivated.
 **/
void
e_addressbook_view_discard_menus (EAddressbookView *view)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));
	g_return_if_fail (view->view_instance);

	if (view->view_menus) {
		gal_view_menus_unmerge (view->view_menus, NULL);

		g_object_unref (view->view_menus);
		view->view_menus = NULL;
	}

	if (view->view_instance) {
		g_object_unref (view->view_instance);
		view->view_instance = NULL;
	}

	view->uic = NULL;
}

void
e_addressbook_view_print(EAddressbookView *view)
{
	if (view->view_type == E_ADDRESSBOOK_VIEW_MINICARD) {
		char *query;
		EBook *book;
		GtkWidget *print;

		g_object_get (view->model,
			      "query", &query,
			      "book", &book,
			      NULL);
		print = e_contact_print_dialog_new(book, query);
		g_free(query);
		gtk_widget_show_all(print);
	}
	else if (view->view_type == E_ADDRESSBOOK_VIEW_TABLE) {
		GtkWidget *dialog;
		EPrintable *printable;
		ETable *etable;
		EContactPrintDialogWeakData *weak_data;

		dialog = gnome_print_dialog_new(NULL, "Print cards", GNOME_PRINT_DIALOG_RANGE | GNOME_PRINT_DIALOG_COPIES);
		gnome_print_dialog_construct_range_any(GNOME_PRINT_DIALOG(dialog), GNOME_PRINT_RANGE_ALL | GNOME_PRINT_RANGE_SELECTION,
						       NULL, NULL, NULL);

		g_object_get(view->widget, "table", &etable, NULL);
		printable = e_table_get_printable(etable);

		g_object_ref (view->widget);

		g_object_set_data (G_OBJECT (dialog), "table", view->widget);
		g_object_set_data (G_OBJECT (dialog), "printable", printable);
		
		g_signal_connect(dialog,
				 "response", G_CALLBACK(e_contact_print_button), NULL);

		weak_data = g_new (EContactPrintDialogWeakData, 1);

		weak_data->table = view->widget;
		weak_data->printable = G_OBJECT (printable);

		g_object_weak_ref (G_OBJECT (dialog), e_contact_print_destroy, weak_data);

		gtk_widget_show(dialog);
	}
#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
	else if (view->view_type == E_ADDRESSBOOK_VIEW_TREEVIEW) {
		/* XXX */
	}
#endif
}

void
e_addressbook_view_print_preview(EAddressbookView *view)
{
	if (view->view_type == E_ADDRESSBOOK_VIEW_MINICARD) {
		char *query;
		EBook *book;

		g_object_get (view->model,
			      "query", &query,
			      "book", &book,
			      NULL);
		e_contact_print_preview(book, query);
		g_free(query);
	}
	else if (view->view_type == E_ADDRESSBOOK_VIEW_TABLE) {
		EPrintable *printable;
		ETable *etable;
		GnomePrintJob *master;
		GnomePrintContext *pc;
		GnomePrintConfig *config;
		GtkWidget *preview;

		g_object_get(view->widget, "table", &etable, NULL);
		printable = e_table_get_printable(etable);

		master = gnome_print_job_new(NULL);
		config = gnome_print_job_get_config (master);
		gnome_print_config_set_int (config, GNOME_PRINT_KEY_NUM_COPIES, 1);
		pc = gnome_print_job_get_context( master );
		e_printable_reset(printable);
		while (e_printable_data_left(printable)) {
			if (gnome_print_gsave(pc) == -1)
				/* FIXME */;
			if (gnome_print_translate(pc, 72, 72) == -1)
				/* FIXME */;
			e_printable_print_page(printable,
					       pc,
					       6.5 * 72,
					       9 * 72,
					       TRUE);
			if (gnome_print_grestore(pc) == -1)
				/* FIXME */;
			if (gnome_print_showpage(pc) == -1)
				/* FIXME */;
		}
		gnome_print_job_close(master);
		preview = GTK_WIDGET(gnome_print_job_preview_new(master, "Print Preview"));
		gtk_widget_show_all(preview);
		g_object_unref (master);
		g_object_unref (printable);
	}
#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
	else if (view->view_type == E_ADDRESSBOOK_VIEW_TREEVIEW) {
		/* XXX */
	}
#endif
}

void
e_addressbook_view_delete_selection(EAddressbookView *view)
{
	CardAndBook card_and_book;

	memset (&card_and_book, 0, sizeof (card_and_book));
	card_and_book.view = view;

	delete (GTK_WIDGET (view), &card_and_book);
}

static void
invisible_destroyed (gpointer data, GObject *where_object_was)
{
	EAddressbookView *view = data;
	view->invisible = NULL;
}

static void
selection_get (GtkWidget *invisible,
	       GtkSelectionData *selection_data,
	       guint info,
	       guint time_stamp,
	       EAddressbookView *view)
{
	char *value;

	value = e_card_list_get_vcard(view->clipboard_cards);

	gtk_selection_data_set (selection_data, GDK_SELECTION_TYPE_STRING,
				8, value, strlen (value));
				
}

static void
selection_clear_event (GtkWidget *invisible,
		       GdkEventSelection *event,
		       EAddressbookView *view)
{
	if (view->clipboard_cards) {
		g_list_foreach (view->clipboard_cards, (GFunc)g_object_unref, NULL);
		g_list_free (view->clipboard_cards);
		view->clipboard_cards = NULL;
	}
}

static void
selection_received (GtkWidget *invisible,
		    GtkSelectionData *selection_data,
		    guint time,
		    EAddressbookView *view)
{
	if (selection_data->length < 0 || selection_data->type != GDK_SELECTION_TYPE_STRING) {
		return;
	}
	else {
		/* XXX make sure selection_data->data = \0 terminated */
		GList *card_list = e_card_load_cards_from_string_with_default_charset (selection_data->data, "ISO-8859-1");
		GList *l;
		
		if (!card_list /* it wasn't a vcard list */)
			return;

		for (l = card_list; l; l = l->next) {
			ECard *card = l->data;

			e_card_merging_book_add_card (view->book, card, NULL /* XXX */, NULL);
		}

		g_list_foreach (card_list, (GFunc)g_object_unref, NULL);
		g_list_free (card_list);
	}
}

static void
add_to_list (int model_row, gpointer closure)
{
	GList **list = closure;
	*list = g_list_prepend (*list, GINT_TO_POINTER (model_row));
}

static GList *
get_selected_cards (EAddressbookView *view)
{
	GList *list;
	GList *iterator;
	ESelectionModel *selection = get_selection_model (view);

	list = NULL;
	e_selection_model_foreach (selection, add_to_list, &list);

	for (iterator = list; iterator; iterator = iterator->next) {
		iterator->data = e_addressbook_model_card_at (view->model, GPOINTER_TO_INT (iterator->data));
		if (iterator->data)
			g_object_ref (iterator->data);
	}
	list = g_list_reverse (list);
	return list;
}

void
e_addressbook_view_save_as (EAddressbookView *view)
{
	GList *list = get_selected_cards (view);
	if (list)
		e_contact_list_save_as (_("Save as VCard"), list, NULL);
	e_free_object_list(list);
}

void
e_addressbook_view_view (EAddressbookView *view)
{
	GList *list = get_selected_cards (view);
	e_addressbook_show_multiple_cards (view->book, list, view->editable);
	e_free_object_list(list);
}

void
e_addressbook_view_send (EAddressbookView *view)
{
	GList *list = get_selected_cards (view);
	if (list)
		e_addressbook_send_card_list (list, E_ADDRESSBOOK_DISPOSITION_AS_ATTACHMENT);
	e_free_object_list(list);
}

void
e_addressbook_view_send_to (EAddressbookView *view)
{
	GList *list = get_selected_cards (view);
	if (list)
		e_addressbook_send_card_list (list, E_ADDRESSBOOK_DISPOSITION_AS_TO);
	e_free_object_list(list);
}

void
e_addressbook_view_cut (EAddressbookView *view)
{
	e_addressbook_view_copy (view);
	e_addressbook_view_delete_selection (view);
}

void
e_addressbook_view_copy (EAddressbookView *view)
{
	view->clipboard_cards = get_selected_cards (view);

	gtk_selection_owner_set (view->invisible, clipboard_atom, GDK_CURRENT_TIME);
}

void
e_addressbook_view_paste (EAddressbookView *view)
{
	gtk_selection_convert (view->invisible, clipboard_atom,
			       GDK_SELECTION_TYPE_STRING,
			       GDK_CURRENT_TIME);
}

void
e_addressbook_view_select_all (EAddressbookView *view)
{
	ESelectionModel *model = get_selection_model (view);

	g_return_if_fail (model);

	e_selection_model_select_all (model);
}

void
e_addressbook_view_show_all(EAddressbookView *view)
{
	g_object_set(view,
		     "query", NULL,
		     NULL);
}

void
e_addressbook_view_stop(EAddressbookView *view)
{
	if (view)
		e_addressbook_model_stop (view->model);
}

static void
view_transfer_cards (EAddressbookView *view, gboolean delete_from_source)
{
	EBook *book;
	GList *cards;
	GtkWindow *parent_window;

	g_object_get(view->model, 
		     "book", &book,
		     NULL);
	cards = get_selected_cards (view);
	parent_window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view)));

	e_addressbook_transfer_cards (book, cards, delete_from_source, parent_window);
}

void
e_addressbook_view_copy_to_folder (EAddressbookView *view)
{
	view_transfer_cards (view, FALSE);
}

void
e_addressbook_view_move_to_folder (EAddressbookView *view)
{
	view_transfer_cards (view, TRUE);
}


static gboolean
e_addressbook_view_selection_nonempty (EAddressbookView  *view)
{
	ESelectionModel *selection_model;

	selection_model = get_selection_model (view);
	if (selection_model == NULL)
		return FALSE;

	return e_selection_model_selected_count (selection_model) != 0;
}

gboolean
e_addressbook_view_can_create (EAddressbookView  *view)
{
	return view ? e_addressbook_model_editable (view->model) : FALSE;
}

gboolean
e_addressbook_view_can_print (EAddressbookView  *view)
{
	return view && view->model ? e_addressbook_model_card_count (view->model) : FALSE;
}

gboolean
e_addressbook_view_can_save_as (EAddressbookView  *view)
{
	return view ? e_addressbook_view_selection_nonempty (view) : FALSE;
}

gboolean
e_addressbook_view_can_view (EAddressbookView  *view)
{
	return view ? e_addressbook_view_selection_nonempty (view) : FALSE;
}

gboolean 
e_addressbook_view_can_send (EAddressbookView  *view)
{
	return view ? e_addressbook_view_selection_nonempty (view) : FALSE;
}

gboolean   
e_addressbook_view_can_send_to (EAddressbookView  *view)
{
	return view ? e_addressbook_view_selection_nonempty (view) : FALSE;
}

gboolean
e_addressbook_view_can_delete (EAddressbookView  *view)
{
	return view ? e_addressbook_view_selection_nonempty (view) && e_addressbook_model_editable (view->model) : FALSE;
}

gboolean
e_addressbook_view_can_cut (EAddressbookView *view)
{
	return view ? e_addressbook_view_selection_nonempty (view) && e_addressbook_model_editable (view->model) : FALSE;
}

gboolean
e_addressbook_view_can_copy (EAddressbookView *view)
{
	return view ? e_addressbook_view_selection_nonempty (view) : FALSE;
}

gboolean
e_addressbook_view_can_paste (EAddressbookView *view)
{
	return view ? e_addressbook_model_editable (view->model) : FALSE;
}

gboolean
e_addressbook_view_can_select_all (EAddressbookView *view)
{
	return view ? e_addressbook_model_card_count (view->model) != 0 : FALSE;
}

gboolean
e_addressbook_view_can_stop (EAddressbookView  *view)
{
	return view ? e_addressbook_model_can_stop (view->model) : FALSE;
}

gboolean
e_addressbook_view_can_copy_to_folder (EAddressbookView *view)
{
	return view ? e_addressbook_view_selection_nonempty (view) : FALSE;
}

gboolean
e_addressbook_view_can_move_to_folder (EAddressbookView *view)
{
	return view ? e_addressbook_view_selection_nonempty (view) && e_addressbook_model_editable (view->model) : FALSE;
}
