/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-addressbook-view.c
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *         Chris Toshok <toshok@ximian.com>
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
#include "addressbook/gui/search/e-addressbook-search-dialog.h"
#include "addressbook/gui/widgets/eab-popup.h"
#include "addressbook/gui/widgets/eab-menu.h"

#include "e-util/e-categories-master-list-wombat.h"
#include "libedataserver/e-sexp.h"

#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
#include <gal/widgets/e-treeview-selection-model.h>
#include "gal-view-factory-treeview.h"
#include "gal-view-treeview.h"
#endif
#include "gal-view-minicard.h"
#include "gal-view-factory-minicard.h"

#include "eab-marshal.h"
#include "e-addressbook-view.h"
#include "e-addressbook-model.h"
#include "eab-gui-util.h"
#include "util/eab-book-util.h"
#include "e-addressbook-table-adapter.h"
#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
#include "e-addressbook-treeview-adapter.h"
#endif
#include "eab-contact-merging.h"

#include "widgets/misc/e-error.h"

#include "e-contact-editor.h"
#include <gdk/gdkkeysyms.h>
#include <ctype.h>
#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#define SHOW_ALL_SEARCH "(contains \"x-evolution-any-field\" \"\")"

#define d(x)

static void eab_view_init		(EABView		 *card);
static void eab_view_class_init	(EABViewClass	 *klass);

static void eab_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void eab_view_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static void eab_view_dispose (GObject *object);
static void change_view_type (EABView *view, EABViewType view_type);

static void status_message     (GtkObject *object, const gchar *status, EABView *eav);
static void search_result      (GtkObject *object, EBookViewStatus status, EABView *eav);
static void folder_bar_message (GtkObject *object, const gchar *status, EABView *eav);
static void stop_state_changed (GtkObject *object, EABView *eav);
static void writable_status    (GtkObject *object, gboolean writable, EABView *eav);
static void backend_died       (GtkObject *object, EABView *eav);
static void contact_changed    (EABModel *model, gint index, EABView *eav);
static void contact_removed    (EABModel *model, gint index, EABView *eav);
static GList *get_selected_contacts (EABView *view);

static void command_state_change (EABView *eav);

static void selection_clear_event (GtkWidget *invisible, GdkEventSelection *event,
				   EABView *view);
static void selection_received (GtkWidget *invisible, GtkSelectionData *selection_data,
				guint time, EABView *view);
static void selection_get (GtkWidget *invisible, GtkSelectionData *selection_data,
			   guint info, guint time_stamp, EABView *view);
static void invisible_destroyed (gpointer data, GObject *where_object_was);

static void make_suboptions             (EABView *view);
static void query_changed               (ESearchBar *esb, EABView *view);
static void search_activated            (ESearchBar *esb, EABView *view);
static void search_menu_activated       (ESearchBar *esb, int id, EABView *view);
static void connect_master_list_changed (EABView *view);
static ECategoriesMasterList *get_master_list (void);

#define PARENT_TYPE GTK_TYPE_VBOX
static GtkVBoxClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_BOOK,
	PROP_SOURCE,
	PROP_QUERY,
	PROP_TYPE,
};

enum {
	STATUS_MESSAGE,
	SEARCH_RESULT,
	FOLDER_BAR_MESSAGE,
	COMMAND_STATE_CHANGE,
	LAST_SIGNAL
};

enum DndTargetType {
	DND_TARGET_TYPE_SOURCE_VCARD,
	DND_TARGET_TYPE_VCARD
};
#define VCARD_TYPE "text/x-vcard"
#define SOURCE_VCARD_TYPE "text/x-source-vcard"
static GtkTargetEntry drag_types[] = {
	{ SOURCE_VCARD_TYPE, 0, DND_TARGET_TYPE_SOURCE_VCARD },
	{ VCARD_TYPE, 0, DND_TARGET_TYPE_VCARD }
};
static const int num_drag_types = sizeof (drag_types) / sizeof (drag_types[0]);

static guint eab_view_signals [LAST_SIGNAL] = {0, };

static GdkAtom clipboard_atom = GDK_NONE;

static GalViewCollection *collection = NULL;

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

static ESearchBarItem addressbook_search_items[] = {
	{ N_("Advanced..."), ESB_ADVANCED, NULL },
	{ NULL, -1, NULL },
};

GType
eab_view_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (EABViewClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) eab_view_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EABView),
			0,             /* n_preallocs */
			(GInstanceInitFunc) eab_view_init,
		};

		type = g_type_register_static (PARENT_TYPE, "EABView", &info, 0);
	}

	return type;
}

static void
eab_view_class_init (EABViewClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->set_property = eab_view_set_property;
	object_class->get_property = eab_view_get_property;
	object_class->dispose = eab_view_dispose;

	g_object_class_install_property (object_class, PROP_BOOK, 
					 g_param_spec_object ("book",
							      _("Book"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_BOOK,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_SOURCE, 
					 g_param_spec_object ("source",
							      _("Source"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_SOURCE,
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
							   EAB_VIEW_NONE, 
							   EAB_VIEW_TABLE,
							   EAB_VIEW_NONE,
							   G_PARAM_READWRITE));

	eab_view_signals [STATUS_MESSAGE] =
		g_signal_new ("status_message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABViewClass, status_message),
			      NULL, NULL,
			      eab_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	eab_view_signals [SEARCH_RESULT] =
		g_signal_new ("search_result",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABViewClass, search_result),
			      NULL, NULL,
			      eab_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	eab_view_signals [FOLDER_BAR_MESSAGE] =
		g_signal_new ("folder_bar_message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABViewClass, folder_bar_message),
			      NULL, NULL,
			      eab_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	eab_view_signals [COMMAND_STATE_CHANGE] =
		g_signal_new ("command_state_change",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABViewClass, command_state_change),
			      NULL, NULL,
			      eab_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);

	/* init the accessibility support for e_addressbook_view */
	eab_view_a11y_init();
}

static void
eab_view_init (EABView *eav)
{
	eav->view_type = EAB_VIEW_NONE;

	eav->model = NULL;
	eav->object = NULL;
	eav->widget = NULL;
	eav->contact_display_window = NULL;
	eav->contact_display = NULL;
	eav->displayed_contact = -1;

	eav->view_instance = NULL;
	eav->view_menus = NULL;
	eav->current_view = NULL;
	eav->uic = NULL;

	eav->book = NULL;
	eav->source = NULL;
	eav->query = NULL;

	eav->invisible = NULL;
	eav->clipboard_contacts = NULL;
}

static void
eab_view_dispose (GObject *object)
{
	EABView *eav = EAB_VIEW(object);

	if (eav->model) {
		g_signal_handlers_disconnect_matched (eav->model,
						      G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL,
						      object);
		g_object_unref (eav->model);
		eav->model = NULL;
	}

	if (eav->book) {
		g_object_unref (eav->book);
		eav->book = NULL;
	}

	if (eav->source) {
		g_object_unref (eav->source);
		eav->source = NULL;
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

	if (eav->clipboard_contacts) {
		g_list_foreach (eav->clipboard_contacts, (GFunc)g_object_unref, NULL);
		g_list_free (eav->clipboard_contacts);
		eav->clipboard_contacts = NULL;
	}

	if (eav->invisible) {
		gtk_widget_destroy (eav->invisible);
		eav->invisible = NULL;
	}

	if (eav->ecml_changed_id != 0) {
		g_signal_handler_disconnect (get_master_list(),
					     eav->ecml_changed_id);
		eav->ecml_changed_id = 0;
	}

	if (G_OBJECT_CLASS(parent_class)->dispose)
		G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
set_paned_position (EABView *eav)
{
	GConfClient *gconf_client;
	gint         pos;

	/* XXX this should use the addressbook's global gconf client */
	gconf_client = gconf_client_get_default ();
	pos = gconf_client_get_int (gconf_client, "/apps/evolution/addressbook/display/vpane_position", NULL);
	if (pos < 1)
		pos = 144;

	gtk_paned_set_position (GTK_PANED (eav->paned), pos);

	g_object_unref (gconf_client);
}

static gboolean
get_paned_position (EABView *eav)
{
	GConfClient *gconf_client;
	gint pos;

	/* XXX this should use the addressbook's global gconf client */
	gconf_client = gconf_client_get_default ();

	pos = gtk_paned_get_position (GTK_PANED (eav->paned));
	gconf_client_set_int (gconf_client, "/apps/evolution/addressbook/display/vpane_position", pos, NULL);

	g_object_unref (gconf_client);

	return FALSE;
}

GtkWidget*
eab_view_new (void)
{
	GtkWidget *widget = GTK_WIDGET (g_object_new (E_TYPE_AB_VIEW, NULL));
	EABView *eav = EAB_VIEW (widget);
	FilterPart *part;

	/* create our model */
	eav->model = eab_model_new ();

	g_signal_connect (eav->model, "status_message",
			  G_CALLBACK (status_message), eav);
	g_signal_connect (eav->model, "search_result",
			  G_CALLBACK (search_result), eav);
	g_signal_connect (eav->model, "folder_bar_message",
			  G_CALLBACK (folder_bar_message), eav);
	g_signal_connect (eav->model, "stop_state_changed",
			  G_CALLBACK (stop_state_changed), eav);
	g_signal_connect (eav->model, "writable_status",
			  G_CALLBACK (writable_status), eav);
	g_signal_connect (eav->model, "backend_died",
			  G_CALLBACK (backend_died), eav);
	g_signal_connect (eav->model, "contact_changed",
			  G_CALLBACK (contact_changed), eav);
	g_signal_connect (eav->model, "contact_removed",
			  G_CALLBACK (contact_removed), eav);

	eav->editable = FALSE;
	eav->query = g_strdup (SHOW_ALL_SEARCH);

	/* create our search bar */
	eav->search = E_SEARCH_BAR (e_search_bar_new (NULL, addressbook_search_option_items));
	e_search_bar_set_menu (eav->search, addressbook_search_items);
	make_suboptions (eav);
	connect_master_list_changed (eav);
	g_signal_connect (eav->search, "query_changed",
			  G_CALLBACK (query_changed), eav);
	g_signal_connect (eav->search, "search_activated",
			  G_CALLBACK (search_activated), eav);
	g_signal_connect (eav->search, "menu_activated",
			  G_CALLBACK (search_menu_activated), eav);
	gtk_box_pack_start (GTK_BOX (eav), GTK_WIDGET (eav->search), FALSE, FALSE, 0);
	gtk_widget_show (GTK_WIDGET (eav->search));
	gtk_widget_set_sensitive (GTK_WIDGET (eav->search), FALSE);

	/* create the search context */
	eav->search_context = rule_context_new ();
	rule_context_add_part_set (eav->search_context, "partset", filter_part_get_type (),
				   rule_context_add_part, rule_context_next_part);
	rule_context_load (eav->search_context, SEARCH_RULE_DIR "/addresstypes.xml", "");

	eav->search_rule = filter_rule_new ();
	part = rule_context_next_part (eav->search_context, NULL);

	if (part == NULL)
		g_warning ("Could not load addressbook search; no parts.");
	else
		filter_rule_add_part (eav->search_rule, filter_part_clone (part));

	/* create the paned window and contact display */
	eav->paned = gtk_vpaned_new ();
	gtk_box_pack_start (GTK_BOX (eav), eav->paned, TRUE, TRUE, 0);
	g_signal_connect_swapped (eav->paned, "button_release_event",
				  G_CALLBACK (get_paned_position), eav);

	eav->contact_display = eab_contact_display_new ();
	eav->contact_display_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (eav->contact_display_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (eav->contact_display_window), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (eav->contact_display_window), eav->contact_display);
	gtk_paned_add2 (GTK_PANED (eav->paned), eav->contact_display_window);
	gtk_widget_show (eav->contact_display);
	gtk_widget_show (eav->contact_display_window);
	gtk_widget_show (eav->paned);

	/* gtk selection crap */
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

	return widget;
}

RuleContext *
eab_view_peek_search_context (EABView *view)
{
	return view->search_context;
}

FilterRule *
eab_view_peek_search_rule (EABView *view)
{
	return view->search_rule;
}

static void
writable_status (GtkObject *object, gboolean writable, EABView *eav)
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

		gal_view_collection_set_title (collection, _("Address Book"));

		galview = gnome_util_prepend_user_home("/.evolution/addressbook/views");
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

		factory = gal_view_factory_minicard_new();
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
set_view_preview (EABView *view)
{
	/* XXX this should use the addressbook's global gconf client */
	GConfClient *gconf_client;
	gboolean state;

	gconf_client = gconf_client_get_default();
	state = gconf_client_get_bool(gconf_client, "/apps/evolution/addressbook/display/show_preview", NULL);
	bonobo_ui_component_set_prop (view->uic,
				      "/commands/ContactsViewPreview",
				      "state",
				      state ? "1" : "0", NULL);

	eab_view_show_contact_preview (view, state);
	
	g_object_unref (gconf_client);
}

static void
display_view(GalViewInstance *instance,
	     GalView *view,
	     gpointer data)
{
	EABView *address_view = data;
	if (GAL_IS_VIEW_ETABLE(view)) {
		change_view_type (address_view, EAB_VIEW_TABLE);
		gal_view_etable_attach_table (GAL_VIEW_ETABLE(view), e_table_scrolled_get_table(E_TABLE_SCROLLED(address_view->widget)));
	}
	else if (GAL_IS_VIEW_MINICARD(view)) {
		change_view_type (address_view, EAB_VIEW_MINICARD);
		gal_view_minicard_attach (GAL_VIEW_MINICARD (view), E_MINICARD_VIEW_WIDGET (address_view->object));
	}
#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
	else if (GAL_IS_VIEW_TREEVIEW (view)) {
		change_view_type (address_view, EAB_VIEW_TREEVIEW);
		gal_view_treeview_attach (GAL_VIEW_TREEVIEW(view), GTK_TREE_VIEW (address_view->object));
	}
#endif
	address_view->current_view = view;

	set_paned_position (address_view);
	set_view_preview (address_view);
}

static void
view_preview(BonoboUIComponent *uic, const char *path, Bonobo_UIComponent_EventType type, const char *state, void *data)
{
	/* XXX this should use the addressbook's global gconf client */
	GConfClient *gconf_client;
	EABView *view = EAB_VIEW (data);

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	gconf_client = gconf_client_get_default();
	gconf_client_set_bool(gconf_client, "/apps/evolution/addressbook/display/show_preview", state[0] != '0', NULL);

	eab_view_show_contact_preview(view, state[0] != '0');

	g_object_unref (gconf_client);
}

static void
setup_menus (EABView *view)
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

	bonobo_ui_component_add_listener(view->uic, "ContactsViewPreview", view_preview, view);

	set_view_preview (view);
}

static void
eab_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	EABView *eav = EAB_VIEW(object);

	switch (prop_id){
	case PROP_BOOK:
		if (eav->book) {
			g_object_unref (eav->book);
		}
		if (g_value_get_object (value)) {
			eav->book = E_BOOK(g_value_get_object (value));
			g_object_ref (eav->book);
			gtk_widget_set_sensitive (GTK_WIDGET (eav->search), TRUE);
		}
		else {
			eav->book = NULL;
			gtk_widget_set_sensitive (GTK_WIDGET (eav->search), FALSE);
		}

		if (eav->view_instance) {
			g_object_unref (eav->view_instance);
			eav->view_instance = NULL;
		}

		g_object_set(eav->model,
			     "book", eav->book,
			     NULL);

		setup_menus (eav);

		break;
	case PROP_SOURCE:
		if (eav->source) {
			g_warning ("EABView at present does not support multiple writes on the \"source\" property.");
			break;
		}
		else {
			if (g_value_get_object (value)) {
				eav->source = E_SOURCE(g_value_get_object (value));
				g_object_ref (eav->source);
			}
			else {
				eav->source = NULL;
			}
		}
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
eab_view_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	EABView *eav = EAB_VIEW(object);

	switch (prop_id) {
	case PROP_BOOK:
		if (eav->book)
			g_value_set_object (value, eav->book);
		else
			g_value_set_object (value, NULL);
		break;
	case PROP_SOURCE:
		if (eav->source)
			g_value_set_object (value, eav->source);
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
get_selection_model (EABView *view)
{
	if (view->view_type == EAB_VIEW_TABLE)
		return e_table_get_selection_model (e_table_scrolled_get_table (E_TABLE_SCROLLED(view->widget)));
	else if (view->view_type == EAB_VIEW_MINICARD)
		return e_minicard_view_widget_get_selection_model (E_MINICARD_VIEW_WIDGET(view->object));
#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
	else if (view->view_type == EAB_VIEW_TREEVIEW)
		return e_treeview_get_selection_model (GTK_TREE_VIEW (view->object));
#endif
	g_return_val_if_reached (NULL);
}

/* Popup menu stuff */
typedef struct {
	EABView *view;
	gpointer closure;
} ContactAndBook;

static ESelectionModel*
contact_and_book_get_selection_model (ContactAndBook *contact_and_book)
{
	return get_selection_model (contact_and_book->view);
}

static GList *
get_contact_list (EABPopupTargetSelect *t)
{
	GList *list = NULL;
	int i;

	for (i=0;i<t->cards->len;i++)
		list = g_list_prepend(list, t->cards->pdata[i]);

	return list;
}

static void
save_as (EPopup *ep, EPopupItem *pitem, void *data)
{
	/*ContactAndBook *contact_and_book = data;*/
	GList *contacts = get_contact_list ((EABPopupTargetSelect *)ep->target);

	if (contacts) {
		eab_contact_list_save(_("Save as VCard..."), contacts, NULL);
		g_list_free(contacts);
	}
}

static void
send_as (EPopup *ep, EPopupItem *pitem, void *data)
{
	/*ContactAndBook *contact_and_book = data;*/
	GList *contacts = get_contact_list ((EABPopupTargetSelect *)ep->target);

	if (contacts) {
		eab_send_contact_list(contacts, EAB_DISPOSITION_AS_ATTACHMENT);
		g_list_free(contacts);
	}
}

static void
send_to (EPopup *ep, EPopupItem *pitem, void *data)
{
	/*ContactAndBook *contact_and_book = data;*/
	GList *contacts = get_contact_list ((EABPopupTargetSelect *)ep->target);

	if (contacts) {
		eab_send_contact_list(contacts, EAB_DISPOSITION_AS_TO);
		g_list_free(contacts);
	}
}

static void
print (EPopup *ep, EPopupItem *pitem, void *data)
{
	/*ContactAndBook *contact_and_book = data;*/
	EABPopupTargetSelect *t = (EABPopupTargetSelect *)ep->target;

	if (t->cards->len == 1) {
		gtk_widget_show(e_contact_print_contact_dialog_new(t->cards->pdata[0]));
	} else {
		GList *contacts = get_contact_list(t);

		gtk_widget_show(e_contact_print_contact_list_dialog_new(contacts));
		g_list_free(contacts);
	}
}

static void
copy (EPopup *ep, EPopupItem *pitem, void *data)
{
	ContactAndBook *contact_and_book = data;

	eab_view_copy (contact_and_book->view);
}

static void
paste (EPopup *ep, EPopupItem *pitem, void *data)
{
	ContactAndBook *contact_and_book = data;

	eab_view_paste (contact_and_book->view);
}

static void
cut (EPopup *ep, EPopupItem *pitem, void *data)
{
	ContactAndBook *contact_and_book = data;

	eab_view_cut (contact_and_book->view);
}

static void
delete (EPopup *ep, EPopupItem *pitem, void *data)
{
	ContactAndBook *contact_and_book = data;

	eab_view_delete_selection(contact_and_book->view);
}

static void
copy_to_folder (EPopup *ep, EPopupItem *pitem, void *data)
{
	ContactAndBook *contact_and_book = data;

	eab_view_copy_to_folder (contact_and_book->view);
}

static void
move_to_folder (EPopup *ep, EPopupItem *pitem, void *data)
{
	ContactAndBook *contact_and_book = data;

	eab_view_move_to_folder (contact_and_book->view);
}

static void
new_card (EPopup *ep, EPopupItem *pitem, void *data)
{
	/*ContactAndBook *contact_and_book = data;*/
	EContact *contact = e_contact_new();

	eab_show_contact_editor (((EABPopupTargetSelect *)ep->target)->book, contact, TRUE, TRUE);
	g_object_unref (contact);
}

static void
new_list (EPopup *ep, EPopupItem *pitem, void *data)
{
	/*ContactAndBook *contact_and_book = data;*/
	EContact *contact = e_contact_new ();

	eab_show_contact_list_editor (((EABPopupTargetSelect *)ep->target)->book, contact, TRUE, TRUE);
	g_object_unref(contact);
}

static EPopupItem eabv_popup_items[] = {
	{ E_POPUP_ITEM, "10.new",  N_("New Contact..."), new_card, NULL, "stock_contact", 0, EAB_POPUP_SELECT_EDITABLE},
	{ E_POPUP_ITEM, "15.newlist", N_("New Contact List..."), new_list, NULL, "stock_contact-list", 0, EAB_POPUP_SELECT_EDITABLE },

	{ E_POPUP_BAR, "20.bar" },
	{ E_POPUP_ITEM, "30.saveas", N_("Save as VCard..."), save_as, NULL, "stock_save-as", 0, EAB_POPUP_SELECT_ANY },
	{ E_POPUP_ITEM, "40.forward", N_("Forward Contact"), send_as, NULL, "stock_mail-forward", 0, EAB_POPUP_SELECT_ANY },
	{ E_POPUP_ITEM, "50.mailto", N_("Send Message to Contact"), send_to, NULL, "stock_mail-send", 0, EAB_POPUP_SELECT_ANY|EAB_POPUP_SELECT_EMAIL },
	{ E_POPUP_ITEM, "60.print", N_("Print"), print, NULL, "stock_print", 0, EAB_POPUP_SELECT_ANY },

	{ E_POPUP_BAR, "70.bar" },
	{ E_POPUP_ITEM, "80.copyto", N_("Copy to Address Book..."), copy_to_folder, NULL, NULL, 0, EAB_POPUP_SELECT_ANY },
	{ E_POPUP_ITEM, "90.moveto", N_("Move to Address Book..."), move_to_folder, NULL, NULL, 0, EAB_POPUP_SELECT_ANY|EAB_POPUP_SELECT_EDITABLE },

	{ E_POPUP_BAR, "a0.bar" },
	{ E_POPUP_BAR, "b0.cut", N_("Cut"), cut, NULL, "stock_cut", 0, EAB_POPUP_SELECT_ANY|EAB_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "c0.copy", N_("Copy"), copy, NULL, "stock_copy", 0, EAB_POPUP_SELECT_ANY },
	{ E_POPUP_ITEM, "d0.paste", N_("Paste"), paste, NULL, "stock_paste", 0, EAB_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "e0.delete", N_("Delete"), delete, NULL, "stock_delete", 0, EAB_POPUP_SELECT_EDITABLE|EAB_POPUP_SELECT_ANY },
};

static void
get_card_1(gint model_row, void *data)
{
	ContactAndBook *contact_and_book = data;
	EContact *contact;

	contact = eab_model_get_contact(contact_and_book->view->model, model_row);
	if (contact)
		g_ptr_array_add((GPtrArray *)contact_and_book->closure, contact);
}

static void
eabv_popup_free(EPopup *ep, GSList *list, void *data)
{
	ContactAndBook *cab = data;
	ESelectionModel *selection;

	/* NB: this looks strange to me */
	selection = contact_and_book_get_selection_model(cab);
	if (selection)
		e_selection_model_right_click_up(selection);

	g_slist_free(list);
	g_object_unref(cab->view);
	g_free(cab);
}

static void
do_popup_menu(EABView *view, GdkEvent *event)
{
	EABPopup *ep;
	EABPopupTargetSelect *t;
	GSList *menus = NULL;
	int i;
	GtkMenu *menu;
	GPtrArray *cards = g_ptr_array_new();
	ContactAndBook *contact_and_book;
	ESelectionModel *selection_model;

	contact_and_book = g_new(ContactAndBook, 1);
	contact_and_book->view = view;
	g_object_ref(contact_and_book->view);

	selection_model = contact_and_book_get_selection_model(contact_and_book);
	if (selection_model) {
		contact_and_book->closure = cards;
		e_selection_model_foreach(selection_model, get_card_1, contact_and_book);
	}

	ep = eab_popup_new("org.gnome.evolution.addressbook.view.popup");
	t = eab_popup_target_new_select(ep, view->book, !eab_model_editable(view->model), cards);
	t->target.widget = (GtkWidget *)view;

	for (i=0;i<sizeof(eabv_popup_items)/sizeof(eabv_popup_items[0]);i++)
		menus = g_slist_prepend(menus, &eabv_popup_items[i]);

	e_popup_add_items((EPopup *)ep, menus, eabv_popup_free, contact_and_book);

	menu = e_popup_create_menu_once((EPopup *)ep, (EPopupTarget *)t, 0);
	gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event?event->button.button:0, event?event->button.time:gtk_get_current_event_time());
}

static void
render_contact (int row, EABView *view)
{
	EContact *contact = eab_model_get_contact (view->model, row);

	view->displayed_contact = row;

	eab_contact_display_render (EAB_CONTACT_DISPLAY (view->contact_display), contact,
				    EAB_CONTACT_DISPLAY_RENDER_NORMAL);
}

static void
selection_changed (GObject *o, EABView *view)
{
	ESelectionModel *selection_model;

	command_state_change (view);

	selection_model = get_selection_model (view);

	if (e_selection_model_selected_count (selection_model) == 1)
		e_selection_model_foreach (selection_model,
					   (EForeachFunc)render_contact, view);
	else {
		view->displayed_contact = -1;
		eab_contact_display_render (EAB_CONTACT_DISPLAY (view->contact_display), NULL,
					    EAB_CONTACT_DISPLAY_RENDER_NORMAL);
	}
					    
}

static void
table_double_click(ETableScrolled *table, gint row, gint col, GdkEvent *event, EABView *view)
{
	if (E_IS_ADDRESSBOOK_TABLE_ADAPTER(view->object)) {
		EABModel *model = view->model;
		EContact *contact = eab_model_get_contact (model, row);
		EBook *book;

		g_object_get(model,
			     "book", &book,
			     NULL);
		
		g_assert (E_IS_BOOK (book));

		if (e_contact_get (contact, E_CONTACT_IS_LIST))
			eab_show_contact_list_editor (book, contact, FALSE, view->editable);
		else
			eab_show_contact_editor (book, contact, FALSE, view->editable);

		g_object_unref (book);
		g_object_unref (contact);
	}
}

static gint
table_right_click(ETableScrolled *table, gint row, gint col, GdkEvent *event, EABView *view)
{
	do_popup_menu(view, event);
	return TRUE;
}

static gint
table_white_space_event(ETableScrolled *table, GdkEvent *event, EABView *view)
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
	EABView *view = user_data;
	GList *contact_list;

	if (!E_IS_ADDRESSBOOK_TABLE_ADAPTER(view->object))
		return;

	contact_list = get_selected_contacts (view);

	switch (info) {
	case DND_TARGET_TYPE_VCARD: {
		char *value;

		value = eab_contact_list_to_string (contact_list);

		gtk_selection_data_set (selection_data,
					selection_data->target,
					8,
					value, strlen (value));
		break;
	}
	case DND_TARGET_TYPE_SOURCE_VCARD: {
		char *value;

		value = eab_book_and_contact_list_to_string (view->book, contact_list);

		gtk_selection_data_set (selection_data,
					selection_data->target,
					8,
					value, strlen (value));
		break;
	}
	}

	g_list_foreach (contact_list, (GFunc) g_object_unref, NULL);
	g_list_free (contact_list);
}

static void
emit_status_message (EABView *eav, const gchar *status)
{
	g_signal_emit (eav,
		       eab_view_signals [STATUS_MESSAGE], 0,
		       status);
}

static void
emit_search_result (EABView *eav, EBookViewStatus status)
{
	g_signal_emit (eav,
		       eab_view_signals [SEARCH_RESULT], 0,
		       status);
}

static void
emit_folder_bar_message (EABView *eav, const gchar *message)
{
	g_signal_emit (eav,
		       eab_view_signals [FOLDER_BAR_MESSAGE], 0,
		       message);
}

static void
status_message (GtkObject *object, const gchar *status, EABView *eav)
{
	emit_status_message (eav, status);
}

static void
search_result (GtkObject *object, EBookViewStatus status, EABView *eav)
{
	emit_search_result (eav, status);
}

static void
folder_bar_message (GtkObject *object, const gchar *status, EABView *eav)
{
	emit_folder_bar_message (eav, status);
}

static void
stop_state_changed (GtkObject *object, EABView *eav)
{
	command_state_change (eav);
}

static void
command_state_change (EABView *eav)
{
	/* Reffing during emission is unnecessary.  Gtk automatically refs during an emission. */
	g_signal_emit (eav, eab_view_signals [COMMAND_STATE_CHANGE], 0);
}

static void
backend_died (GtkObject *object, EABView *eav)
{
	e_error_run (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (eav))),
		     "addressbook:backend-died", e_book_get_uri (eav->book), NULL);
}

static void
contact_changed (EABModel *model, gint index, EABView *eav)
{
	if (eav->displayed_contact == index) {
		/* if the contact that's presently displayed is changed, re-render it */
		render_contact (index, eav);
	}
}

static void
contact_removed (EABModel *model, gint index, EABView *eav)
{
	if (eav->displayed_contact == index) {
		/* if the contact that's presently displayed is changed, clear the display */
		eab_contact_display_render (EAB_CONTACT_DISPLAY (eav->contact_display), NULL,
					    EAB_CONTACT_DISPLAY_RENDER_NORMAL);
		eav->displayed_contact = -1;
	}
}

static void
minicard_right_click (EMinicardView *minicard_view_item, GdkEvent *event, EABView *view)
{
       do_popup_menu(view, event);
}

static void
create_minicard_view (EABView *view)
{
	GtkWidget *scrolled_window;
	GtkWidget *minicard_view;
	EAddressbookReflowAdapter *adapter;

	adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(e_addressbook_reflow_adapter_new (view->model));
	minicard_view = e_minicard_view_widget_new(adapter);

	g_signal_connect(minicard_view, "selection_change",
			 G_CALLBACK(selection_changed), view);

	g_signal_connect(minicard_view, "right_click",
			 G_CALLBACK(minicard_right_click), view);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	view->object = G_OBJECT(minicard_view);
	view->widget = scrolled_window;

	gtk_container_add (GTK_CONTAINER (scrolled_window), minicard_view);
	gtk_widget_show (minicard_view);

	gtk_widget_show_all( GTK_WIDGET(scrolled_window) );

	gtk_paned_add1 (GTK_PANED (view->paned), scrolled_window);

	e_reflow_model_changed (E_REFLOW_MODEL (adapter));
}

static void
create_table_view (EABView *view)
{
	ETableModel *adapter;
	GtkWidget *table;
	
	adapter = eab_table_adapter_new(view->model);

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
				 drag_types, num_drag_types, GDK_ACTION_MOVE | GDK_ACTION_COPY);
	
	g_signal_connect (E_TABLE_SCROLLED(table)->table,
			  "table_drag_data_get",
			  G_CALLBACK (table_drag_data_get),
			  view);

	gtk_paned_add1 (GTK_PANED (view->paned), table);

	gtk_widget_show( GTK_WIDGET(table) );
}

#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
static void
treeview_row_activated(GtkTreeView *treeview,
		       GtkTreePath *path, GtkTreeViewColumn *column,
		       EABView *view)
{
	EABModel *model = view->model;
	int row = gtk_tree_path_get_indices (path)[0];
	ECard *card = eab_model_get_card(model, row);
	EBook *book;

	g_object_get(model,
		     "book", &book,
		     NULL);
		
	g_assert (E_IS_BOOK (book));

	if (e_card_evolution_list (card))
		eab_show_contact_list_editor (book, card, FALSE, view->editable);
	else
		eab_show_contact_editor (book, card, FALSE, view->editable);

	g_object_unref (book);
	g_object_unref (card);
}

static void
create_treeview_view (EABView *view)
{
	GtkTreeModel *adapter;
	ECardSimple *simple;
	GtkWidget *treeview;
	GtkWidget *scrolled;
	int i;

	simple = e_card_simple_new(NULL);

	adapter = eab_treeview_adapter_new(view->model);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_IN);
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

	gtk_paned_add1 (GTK_PANED (view->paned), scrolled);

	gtk_widget_show( GTK_WIDGET(scrolled) );

	g_object_unref (simple);
}
#endif

static void
change_view_type (EABView *view, EABViewType view_type)
{
	if (view_type == view->view_type)
		return;

	if (view->widget) {
		gtk_container_remove (GTK_CONTAINER (view->paned), view->widget);
		view->widget = NULL;
	}
	view->object = NULL;

	switch (view_type) {
	case EAB_VIEW_TABLE:
		create_table_view (view);
		break;
	case EAB_VIEW_MINICARD:
		create_minicard_view (view);
		break;
#ifdef WITH_ADDRESSBOOK_VIEW_TREEVIEW
	case EAB_VIEW_TREEVIEW:
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



static void
search_activated (ESearchBar *esb, EABView *v)
{
	ECategoriesMasterList *master_list;
	char *search_word, *search_query;
	const char *category_name;
	int search_type, subid;

	g_message ("in search_activated");

	g_object_get(esb,
		     "text", &search_word,
		     "item_id", &search_type,
		     NULL);

	if (search_type == ESB_ADVANCED) {
		gtk_widget_show(eab_search_dialog_new(v));
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
					search_query = g_strdup_printf ("(is \"category_list\" \"%s\")", category_name);
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
			g_object_set (v,
				      "query", search_query,
				      NULL);

		g_free (search_query);
	}

	g_free (search_word);
}

static void
search_menu_activated (ESearchBar *esb, int id, EABView *view)
{
	if (id == ESB_ADVANCED)
		gtk_widget_show(eab_search_dialog_new(view));
}

static void
query_changed (ESearchBar *esb, EABView *view)
{
	int search_type;

	search_type = e_search_bar_get_item_id(esb);
	if (search_type == ESB_ADVANCED)
		gtk_widget_show(eab_search_dialog_new(view));
}

static int
compare_subitems (const void *a, const void *b)
{
	const ESearchBarSubitem *subitem_a = a;
	const ESearchBarSubitem *subitem_b = b;
	char *collate_a, *collate_b;
	int ret;

	collate_a = g_utf8_collate_key (subitem_a->text, -1);
	collate_b = g_utf8_collate_key (subitem_b->text, -1);

	ret =  strcmp (collate_a, collate_b);

	g_free (collate_a);
	g_free (collate_b);
	
	return ret;
}

static void
make_suboptions (EABView *view)
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
ecml_changed (ECategoriesMasterList *ecml, EABView *view)
{
	make_suboptions (view);
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
connect_master_list_changed (EABView *view)
{
	view->ecml_changed_id =
		g_signal_connect (get_master_list(), "changed",
				  G_CALLBACK (ecml_changed), view);
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
			gnome_print_beginpage (pc, "Contacts");
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
			gnome_print_beginpage (pc, "Contacts");
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
eab_view_show_contact_preview (EABView *view, gboolean show)
{
	g_return_if_fail (view && E_IS_ADDRESSBOOK_VIEW (view));

	if (show)
		gtk_widget_show (view->contact_display_window);
	else
		gtk_widget_hide (view->contact_display_window);
}

void
eab_view_setup_menus (EABView *view,
		      BonoboUIComponent *uic)
{

	g_return_if_fail (view != NULL);
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));
	g_return_if_fail (uic != NULL);
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (uic));

	init_collection ();

	view->uic = uic;

	setup_menus (view);

	/* XXX toshok - yeah this really doesn't belong here, but it
	   needs to happen at the same time and takes the uic */
	e_search_bar_set_ui_component (view->search, uic);
}

/**
 * eab_view_discard_menus:
 * @view: An addressbook view.
 * 
 * Makes an addressbook view discard its GAL view menus and its views instance
 * objects.  This should be called when the corresponding Bonobo component is
 * deactivated.
 **/
void
eab_view_discard_menus (EABView *view)
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
eab_view_print(EABView *view)
{
	if (view->view_type == EAB_VIEW_MINICARD) {
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
	else if (view->view_type == EAB_VIEW_TABLE) {
		GtkWidget *dialog;
		EPrintable *printable;
		ETable *etable;
		EContactPrintDialogWeakData *weak_data;

		dialog = gnome_print_dialog_new(NULL, "Print cards", GNOME_PRINT_DIALOG_RANGE | GNOME_PRINT_DIALOG_COPIES);
		gnome_print_dialog_construct_range_any(GNOME_PRINT_DIALOG(dialog), GNOME_PRINT_RANGE_ALL | GNOME_PRINT_RANGE_SELECTION,
						       NULL, NULL, NULL);

		g_object_get(view->widget, "table", &etable, NULL);
		printable = e_table_get_printable(etable);
		g_object_ref (printable);
		gtk_object_sink (GTK_OBJECT (printable));
		g_object_unref(etable);
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
	else if (view->view_type == EAB_VIEW_TREEVIEW) {
		/* XXX */
	}
#endif
}

void
eab_view_print_preview(EABView *view)
{
	if (view->view_type == EAB_VIEW_MINICARD) {
		char *query;
		EBook *book;

		g_object_get (view->model,
			      "query", &query,
			      "book", &book,
			      NULL);
		e_contact_print_preview(book, query);
		g_free(query);
	} else if (view->view_type == EAB_VIEW_TABLE) {
		EPrintable *printable;
		ETable *etable;
		GnomePrintJob *master;
		GnomePrintContext *pc;
		GnomePrintConfig *config;
		GtkWidget *preview;

		g_object_get(view->widget, "table", &etable, NULL);
		printable = e_table_get_printable(etable);
		g_object_unref(etable);
		g_object_ref (printable);
		gtk_object_sink (GTK_OBJECT (printable));

		master = gnome_print_job_new(NULL);
		config = gnome_print_job_get_config (master);
		gnome_print_config_set_int (config, GNOME_PRINT_KEY_NUM_COPIES, 1);
		pc = gnome_print_job_get_context( master );
		e_printable_reset(printable);
		while (e_printable_data_left(printable)) {
			gnome_print_beginpage (pc, "Contacts");
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
	else if (view->view_type == EAB_VIEW_TREEVIEW) {
		/* XXX */
	}
#endif
}

void
eab_view_delete_selection(EABView *view)
{
	GList *list, *l;

	if (!eab_editor_confirm_delete(GTK_WINDOW(gtk_widget_get_toplevel(view->widget))))
		return;

	list = get_selected_contacts(view);
	if (e_book_check_static_capability (view->book, "bulk-remove")) {
		GList *ids = NULL;

		for (l=list;l;l=g_list_next(l)) {
			EContact *contact = l->data;

			ids = g_list_prepend (ids, (char*)e_contact_get_const (contact, E_CONTACT_UID));
		}

		/* Remove the cards all at once. */
		/* XXX no callback specified... ugh */
		e_book_async_remove_contacts (view->book,
					      ids,
					      NULL,
					      NULL);
			
		g_list_free (ids);
	}
	else {
		for (l=list;l;l=g_list_next(l)) {
			EContact *contact = l->data;
			/* Remove the card. */
			/* XXX no callback specified... ugh */
			e_book_async_remove_contact (view->book,
						     contact,
						     NULL,
						     NULL);
		}
	}

	e_free_object_list(list);
}

static void
invisible_destroyed (gpointer data, GObject *where_object_was)
{
	EABView *view = data;
	view->invisible = NULL;
}

static void
selection_get (GtkWidget *invisible,
	       GtkSelectionData *selection_data,
	       guint info,
	       guint time_stamp,
	       EABView *view)
{
	char *value;

	value = eab_contact_list_to_string (view->clipboard_contacts);

	gtk_selection_data_set (selection_data, GDK_SELECTION_TYPE_STRING,
				8, value, strlen (value));
				
}

static void
selection_clear_event (GtkWidget *invisible,
		       GdkEventSelection *event,
		       EABView *view)
{
	if (view->clipboard_contacts) {
		g_list_foreach (view->clipboard_contacts, (GFunc)g_object_unref, NULL);
		g_list_free (view->clipboard_contacts);
		view->clipboard_contacts = NULL;
	}
}

static void
selection_received (GtkWidget *invisible,
		    GtkSelectionData *selection_data,
		    guint time,
		    EABView *view)
{
	if (selection_data->length <= 0 || selection_data->type != GDK_SELECTION_TYPE_STRING) {
		return;
	} else {
		GList *contact_list;
		GList *l;
		char *str = NULL;

		if (selection_data->data [selection_data->length - 1] != 0) {
			str = g_malloc0 (selection_data->length + 1);
			memcpy (str, selection_data->data, selection_data->length);
			contact_list = eab_contact_list_from_string (str);
		} else
			contact_list = eab_contact_list_from_string (selection_data->data);
		
		for (l = contact_list; l; l = l->next) {
			EContact *contact = l->data;

			/* XXX NULL for a callback /sigh */
			eab_merging_book_add_contact (view->book, contact, NULL /* XXX */, NULL);
		}

		g_list_foreach (contact_list, (GFunc)g_object_unref, NULL);
		g_list_free (contact_list);
		g_free (str);
	}
}

static void
add_to_list (int model_row, gpointer closure)
{
	GList **list = closure;
	*list = g_list_prepend (*list, GINT_TO_POINTER (model_row));
}

static GList *
get_selected_contacts (EABView *view)
{
	GList *list;
	GList *iterator;
	ESelectionModel *selection = get_selection_model (view);

	list = NULL;
	e_selection_model_foreach (selection, add_to_list, &list);

	for (iterator = list; iterator; iterator = iterator->next) {
		iterator->data = eab_model_get_contact (view->model, GPOINTER_TO_INT (iterator->data));
	}
	list = g_list_reverse (list);
	return list;
}

void
eab_view_save_as (EABView *view)
{
	GList *list = get_selected_contacts (view);
	if (list)
		eab_contact_list_save (_("Save as VCard..."), list, NULL);
	e_free_object_list(list);
}

void
eab_view_view (EABView *view)
{
	GList *list = get_selected_contacts (view);
	eab_show_multiple_contacts (view->book, list, view->editable);
	e_free_object_list(list);
}

void
eab_view_send (EABView *view)
{
	GList *list = get_selected_contacts (view);
	if (list)
		eab_send_contact_list (list, EAB_DISPOSITION_AS_ATTACHMENT);
	e_free_object_list(list);
}

void
eab_view_send_to (EABView *view)
{
	GList *list = get_selected_contacts (view);
	if (list)
		eab_send_contact_list (list, EAB_DISPOSITION_AS_TO);
	e_free_object_list(list);
}

void
eab_view_cut (EABView *view)
{
	eab_view_copy (view);
	eab_view_delete_selection (view);
}

void
eab_view_copy (EABView *view)
{
	view->clipboard_contacts = get_selected_contacts (view);

	gtk_selection_owner_set (view->invisible, clipboard_atom, GDK_CURRENT_TIME);
}

void
eab_view_paste (EABView *view)
{
	gtk_selection_convert (view->invisible, clipboard_atom,
			       GDK_SELECTION_TYPE_STRING,
			       GDK_CURRENT_TIME);
}

void
eab_view_select_all (EABView *view)
{
	ESelectionModel *model = get_selection_model (view);

	g_return_if_fail (model);

	e_selection_model_select_all (model);
}

void
eab_view_show_all(EABView *view)
{
	g_object_set(view,
		     "query", NULL,
		     NULL);
}

void
eab_view_stop(EABView *view)
{
	if (view)
		eab_model_stop (view->model);
}

static void
view_transfer_contacts (EABView *view, gboolean delete_from_source)
{
	EBook *book;
	GList *contacts;
	GtkWindow *parent_window;

	g_object_get(view->model, 
		     "book", &book,
		     NULL);
	contacts = get_selected_contacts (view);
	parent_window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view)));

	eab_transfer_contacts (book, contacts, delete_from_source, parent_window);
	g_object_unref(book);
}

void
eab_view_copy_to_folder (EABView *view)
{
	view_transfer_contacts (view, FALSE);
}

void
eab_view_move_to_folder (EABView *view)
{
	view_transfer_contacts (view, TRUE);
}


static gboolean
eab_view_selection_nonempty (EABView  *view)
{
	ESelectionModel *selection_model;

	selection_model = get_selection_model (view);
	if (selection_model == NULL)
		return FALSE;

	return e_selection_model_selected_count (selection_model) != 0;
}

gboolean
eab_view_can_create (EABView  *view)
{
	return view ? eab_model_editable (view->model) : FALSE;
}

gboolean
eab_view_can_print (EABView  *view)
{
	return view && view->model ? eab_model_contact_count (view->model) : FALSE;
}

gboolean
eab_view_can_save_as (EABView  *view)
{
	return view ? eab_view_selection_nonempty (view) : FALSE;
}

gboolean
eab_view_can_view (EABView  *view)
{
	return view ? eab_view_selection_nonempty (view) : FALSE;
}

gboolean 
eab_view_can_send (EABView  *view)
{
	return view ? eab_view_selection_nonempty (view) : FALSE;
}

gboolean   
eab_view_can_send_to (EABView  *view)
{
	return view ? eab_view_selection_nonempty (view) : FALSE;
}

gboolean
eab_view_can_delete (EABView  *view)
{
	return view ? eab_view_selection_nonempty (view) && eab_model_editable (view->model) : FALSE;
}

gboolean
eab_view_can_cut (EABView *view)
{
	return view ? eab_view_selection_nonempty (view) && eab_model_editable (view->model) : FALSE;
}

gboolean
eab_view_can_copy (EABView *view)
{
	return view ? eab_view_selection_nonempty (view) : FALSE;
}

gboolean
eab_view_can_paste (EABView *view)
{
	return view ? eab_model_editable (view->model) : FALSE;
}

gboolean
eab_view_can_select_all (EABView *view)
{
	return view ? eab_model_contact_count (view->model) != 0 : FALSE;
}

gboolean
eab_view_can_stop (EABView  *view)
{
	return view ? eab_model_can_stop (view->model) : FALSE;
}

gboolean
eab_view_can_copy_to_folder (EABView *view)
{
	return view ? eab_view_selection_nonempty (view) : FALSE;
}

gboolean
eab_view_can_move_to_folder (EABView *view)
{
	return view ? eab_view_selection_nonempty (view) && eab_model_editable (view->model) : FALSE;
}

EABMenuTargetSelect *
eab_view_get_menu_target (EABView *view, EABMenu *menu)
{
	GPtrArray *cards = g_ptr_array_new();
	ESelectionModel *selection_model;
	EABMenuTargetSelect *t;

	selection_model = get_selection_model (view);
	if (selection_model) {
		ContactAndBook cab;

		cab.view = view;
		cab.closure = cards;
		e_selection_model_foreach(selection_model, get_card_1, &cab);
	}

	t = eab_menu_target_new_select(menu, view->book, !eab_model_editable(view->model), cards);
	t->target.widget = (GtkWidget *)view;

	return t;
}
