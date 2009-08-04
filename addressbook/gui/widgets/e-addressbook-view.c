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
 *		Chris Lahey <clahey@ximian.com>
 *      Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include <table/e-table-scrolled.h>
#include <table/e-table-model.h>
#include <table/e-cell-date.h>
#include <misc/e-gui-utils.h>
#include <widgets/menus/gal-view-factory-etable.h>
#include <filter/rule-editor.h>
#include <widgets/menus/gal-view-etable.h>
#include <e-util/e-xml-utils.h>

#include "addressbook/printing/e-contact-print.h"
#include "addressbook/gui/widgets/eab-popup.h"
#include "addressbook/gui/widgets/eab-menu.h"
#include "a11y/addressbook/ea-addressbook.h"

#include "e-util/e-print.h"
#include "e-util/e-util.h"
#include "libedataserver/e-sexp.h"
#include <libedataserver/e-categories.h>

#include "gal-view-minicard.h"
#include "gal-view-factory-minicard.h"

#include "e-addressbook-view.h"
#include "e-addressbook-model.h"
#include "eab-gui-util.h"
#include "util/eab-book-util.h"
#include "e-addressbook-table-adapter.h"
#include "eab-contact-merging.h"

#include "e-util/e-error.h"
#include "e-util/e-util-private.h"

#include "e-contact-editor.h"
#include <gdk/gdkkeysyms.h>
#include <ctype.h>
#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

#define SHOW_ALL_SEARCH "(contains \"x-evolution-any-field\" \"\")"

#define d(x)

static void eab_view_init		(EABView		 *card);
static void eab_view_class_init	(EABViewClass	 *class);

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
static void contacts_removed    (EABModel *model, gpointer data, EABView *eav);
static GList *get_selected_contacts (EABView *view);

static void command_state_change (EABView *eav);

static void selection_clear_event (GtkWidget *invisible, GdkEventSelection *event,
				   EABView *view);
static void selection_received (GtkWidget *invisible, GtkSelectionData *selection_data,
				guint time, EABView *view);
static void selection_get (GtkWidget *invisible, GtkSelectionData *selection_data,
			   guint info, guint time_stamp, EABView *view);
static void invisible_destroyed (gpointer data, GObject *where_object_was);

static void categories_changed_cb (gpointer object, gpointer user_data);
static void make_suboptions             (EABView *view);
static void query_changed               (ESearchBar *esb, EABView *view);
static void search_activated            (ESearchBar *esb, EABView *view);
static void search_menu_activated       (ESearchBar *esb, gint id, EABView *view);
static GList *get_master_list (gboolean force_rebuild);

static gpointer parent_class;

/* The arguments we take */
enum {
	PROP_0,
	PROP_BOOK,
	PROP_SOURCE,
	PROP_QUERY,
	PROP_TYPE
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

typedef struct EABSearchBarItem {
	ESearchBarItem search;
	gchar *image;
}EABSearchBarItem;

static GtkTargetEntry drag_types[] = {
	{ (gchar *) SOURCE_VCARD_TYPE, 0, DND_TARGET_TYPE_SOURCE_VCARD },
	{ (gchar *) VCARD_TYPE, 0, DND_TARGET_TYPE_VCARD }
};
static const gint num_drag_types = sizeof (drag_types) / sizeof (drag_types[0]);

static guint eab_view_signals [LAST_SIGNAL] = {0, };

static GdkAtom clipboard_atom = GDK_NONE;

static GalViewCollection *collection = NULL;

enum {
	ESB_FULL_NAME,
	ESB_EMAIL,
	ESB_ANY
};

#if 0
static ESearchBarItem addressbook_search_option_items[] = {
	{ N_("Name begins with"), ESB_FULL_NAME, ESB_ITEMTYPE_RADIO },
	{ N_("Email begins with"), ESB_EMAIL, ESB_ITEMTYPE_RADIO },
	{ N_("Any field contains"), ESB_ANY, ESB_ITEMTYPE_RADIO },
	{ NULL, -1, 0 }
};
#endif

static ESearchBarItem addressbook_search_items[] = {
	E_FILTERBAR_ADVANCED,
	{NULL, 0, 0},
	E_FILTERBAR_SAVE,
	E_FILTERBAR_EDIT,
	{NULL, -1, 0}
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

		type = g_type_register_static (GTK_TYPE_VBOX, "EABView", &info, 0);
	}

	return type;
}

static void
eab_view_class_init (EABViewClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS(class);
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
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	eab_view_signals [SEARCH_RESULT] =
		g_signal_new ("search_result",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABViewClass, search_result),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	eab_view_signals [FOLDER_BAR_MESSAGE] =
		g_signal_new ("folder_bar_message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABViewClass, folder_bar_message),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	eab_view_signals [COMMAND_STATE_CHANGE] =
		g_signal_new ("command_state_change",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EABViewClass, command_state_change),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
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

	e_categories_unregister_change_listener (G_CALLBACK (categories_changed_cb), eav);

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

	/*
	if (eav->search_context) {
		g_object_unref (eav->search_context);
		eav->search_context = NULL;
	}
	*/

	if (eav->search_rule) {
		g_object_unref (eav->search_rule);
		eav->search_rule = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
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
	gchar *xmlfile;
	gchar *userfile;

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
	g_signal_connect (eav->model, "contacts_removed",
			  G_CALLBACK (contacts_removed), eav);

	eav->editable = FALSE;
	eav->query = g_strdup (SHOW_ALL_SEARCH);

	/* create the search context */
	eav->search_context = rule_context_new ();
	rule_context_add_part_set (eav->search_context, "partset", filter_part_get_type (),
				   rule_context_add_part, rule_context_next_part);
	rule_context_add_rule_set (eav->search_context, "ruleset", filter_rule_get_type (),
				   rule_context_add_rule, rule_context_next_rule);

	userfile = g_build_filename ( g_get_home_dir (), ".evolution/addressbook/searches.xml", NULL);
	xmlfile = g_build_filename (SEARCH_RULE_DIR, "addresstypes.xml", NULL);

	g_object_set_data_full (G_OBJECT (eav->search_context), "user", userfile, g_free);
	g_object_set_data_full (G_OBJECT (eav->search_context), "system", xmlfile, g_free);

	rule_context_load (eav->search_context, xmlfile, userfile);

	eav->search_rule = filter_rule_new ();
	part = rule_context_next_part (eav->search_context, NULL);

	if (part == NULL)
		g_warning ("Could not load addressbook search; no parts.");
	else
		filter_rule_add_part (eav->search_rule, filter_part_clone (part));

	eav->search = e_filter_bar_new (eav->search_context, xmlfile, userfile, NULL, eav);

	g_free (xmlfile);
	g_free (userfile);

	e_search_bar_set_menu ( (ESearchBar *) eav->search, addressbook_search_items);
	gtk_widget_show (GTK_WIDGET (eav->search));
	make_suboptions (eav);

	e_categories_register_change_listener (G_CALLBACK (categories_changed_cb), eav);

	g_signal_connect (eav->search, "query_changed",
			  G_CALLBACK (query_changed), eav);
	g_signal_connect (eav->search, "search_activated",
			  G_CALLBACK (search_activated), eav);
	g_signal_connect (eav->search, "menu_activated",
			  G_CALLBACK (search_menu_activated), eav);

	gtk_box_pack_start (GTK_BOX (eav), GTK_WIDGET (eav->search), FALSE, FALSE, 0);

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
	gchar *galview;
	gchar *addressbookdir;
	gchar *etspecfile;

	if (collection == NULL) {
		collection = gal_view_collection_new();

		gal_view_collection_set_title (collection, _("Address Book"));

		galview = g_build_filename (
			e_get_user_data_dir (), "addressbook", "views", NULL);
		addressbookdir = g_build_filename (EVOLUTION_GALVIEWSDIR,
						   "addressbook",
						   NULL);
		gal_view_collection_set_storage_directories
			(collection,
			 addressbookdir,
			 galview);
		g_free(addressbookdir);
		g_free(galview);

		spec = e_table_specification_new();
		etspecfile = g_build_filename (EVOLUTION_ETSPECDIR,
					       "e-addressbook-view.etspec",
					       NULL);
		if (!e_table_specification_load_from_file (spec, etspecfile))
			g_error ("Unable to load ETable specification file "
				 "for address book");
		g_free (etspecfile);

		factory = gal_view_factory_etable_new (spec);
		g_object_unref (spec);
		gal_view_collection_add_factory (collection, factory);
		g_object_unref (factory);

		factory = gal_view_factory_minicard_new();
		gal_view_collection_add_factory (collection, factory);
		g_object_unref (factory);

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
		gal_view_minicard_attach (GAL_VIEW_MINICARD (view), address_view);
	}
	address_view->current_view = view;

	set_paned_position (address_view);
	set_view_preview (address_view);
}

static void
view_preview(BonoboUIComponent *uic, const gchar *path, Bonobo_UIComponent_EventType type, const gchar *state, gpointer data)
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

	switch (prop_id) {
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
	gint i;

	for (i=0;i<t->cards->len;i++)
		list = g_list_prepend(list, t->cards->pdata[i]);

	return list;
}

static void
save_as (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	/*ContactAndBook *contact_and_book = data;*/
	GList *contacts = get_contact_list ((EABPopupTargetSelect *)ep->target);

	if (contacts) {
		eab_contact_list_save(_("Save as vCard..."), contacts, NULL);
		g_list_free(contacts);
	}
}

static void
send_as (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	/*ContactAndBook *contact_and_book = data;*/
	GList *contacts = get_contact_list ((EABPopupTargetSelect *)ep->target);

	if (contacts) {
		eab_send_contact_list(contacts, EAB_DISPOSITION_AS_ATTACHMENT);
		g_list_free(contacts);
	}
}

static void
send_to (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	/*ContactAndBook *contact_and_book = data;*/
	GList *contacts = get_contact_list ((EABPopupTargetSelect *)ep->target);

	if (contacts) {
		eab_send_contact_list(contacts, EAB_DISPOSITION_AS_TO);
		g_list_free(contacts);
	}
}

static void
print (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	/*ContactAndBook *contact_and_book = data;*/
	EABPopupTargetSelect *t = (EABPopupTargetSelect *)ep->target;
	GList *contact_list;

	contact_list = get_contact_list (t);
	e_contact_print (
		NULL, NULL, contact_list,
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
	g_list_free (contact_list);
}

static void
copy (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	ContactAndBook *contact_and_book = data;

	eab_view_copy (contact_and_book->view);
}

static void
paste (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	ContactAndBook *contact_and_book = data;

	eab_view_paste (contact_and_book->view);
}

static void
cut (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	ContactAndBook *contact_and_book = data;

	eab_view_cut (contact_and_book->view);
}

static void
delete (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	ContactAndBook *contact_and_book = data;

	eab_view_delete_selection(contact_and_book->view, TRUE);
}

static void
copy_to_folder (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	ContactAndBook *contact_and_book = data;

	eab_view_copy_to_folder (contact_and_book->view, FALSE);
}

static void
move_to_folder (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	ContactAndBook *contact_and_book = data;

	eab_view_move_to_folder (contact_and_book->view, FALSE);
}

static void
open_contact (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	ContactAndBook *contact_and_book = data;

	eab_view_view (contact_and_book->view);
}

static void
new_card (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	/*ContactAndBook *contact_and_book = data;*/
	EContact *contact = e_contact_new();

	eab_show_contact_editor (((EABPopupTargetSelect *)ep->target)->book, contact, TRUE, TRUE);
	g_object_unref (contact);
}

static void
new_list (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	/*ContactAndBook *contact_and_book = data;*/
	EContact *contact = e_contact_new ();

	eab_show_contact_list_editor (((EABPopupTargetSelect *)ep->target)->book, contact, TRUE, TRUE);
	g_object_unref(contact);
}

static EPopupItem eabv_popup_items[] = {
	{ E_POPUP_ITEM, (gchar *) "05.open", (gchar *) N_("_Open"), open_contact, NULL, NULL, EAB_POPUP_SELECT_ANY|EAB_POPUP_SELECT_EDITABLE },
	{ E_POPUP_BAR, (gchar *) "10.bar" },
	{ E_POPUP_ITEM, (gchar *) "10.new",  (gchar *) N_("_New Contact..."), new_card, NULL, (gchar *) "contact-new", 0, EAB_POPUP_SELECT_EDITABLE},
	{ E_POPUP_ITEM, (gchar *) "15.newlist", (gchar *) N_("New Contact _List..."), new_list, NULL, (gchar *) "stock_contact-list", 0, EAB_POPUP_SELECT_EDITABLE },

	{ E_POPUP_BAR, (gchar *) "20.bar" },
	{ E_POPUP_ITEM, (gchar *) "30.saveas", (gchar *) N_("_Save as vCard..."), save_as, NULL, (gchar *) "document-save-as", 0, EAB_POPUP_SELECT_ANY },
	{ E_POPUP_ITEM, (gchar *) "40.forward", (gchar *) N_("_Forward Contact"), send_as, NULL, (gchar *) "mail-forward", EAB_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, (gchar *) "40.forward", (gchar *) N_("_Forward Contacts"), send_as, NULL, (gchar *) "mail-forward", EAB_POPUP_SELECT_MANY },
	{ E_POPUP_ITEM, (gchar *) "50.mailto", (gchar *) N_("Send _Message to Contact"), send_to, NULL, (gchar *) "mail-message-new", EAB_POPUP_SELECT_ONE|EAB_POPUP_SELECT_EMAIL|EAB_POPUP_CONTACT },
	{ E_POPUP_ITEM, (gchar *) "50.mailto", (gchar *) N_("Send _Message to List"), send_to, NULL, (gchar *) "mail-message-new", EAB_POPUP_SELECT_ONE|EAB_POPUP_SELECT_EMAIL|EAB_POPUP_LIST },
	{ E_POPUP_ITEM, (gchar *) "50.mailto", (gchar *) N_("Send _Message to Contacts"), send_to, NULL, (gchar *) "mail-message-new", EAB_POPUP_SELECT_MANY|EAB_POPUP_SELECT_EMAIL },
	{ E_POPUP_ITEM, (gchar *) "60.print", (gchar *) N_("_Print"), print, NULL, (gchar *) "document-print", 0, EAB_POPUP_SELECT_ANY },

	{ E_POPUP_BAR, (gchar *) "70.bar" },
	{ E_POPUP_ITEM, (gchar *) "80.copyto", (gchar *) N_("Cop_y to Address Book..."), copy_to_folder, NULL, NULL, 0, EAB_POPUP_SELECT_ANY },
	{ E_POPUP_ITEM, (gchar *) "90.moveto", (gchar *) N_("Mo_ve to Address Book..."), move_to_folder, NULL, NULL, 0, EAB_POPUP_SELECT_ANY|EAB_POPUP_SELECT_EDITABLE },

	{ E_POPUP_BAR, (gchar *) "a0.bar" },
	{ E_POPUP_ITEM, (gchar *) "b0.cut", (gchar *) N_("Cu_t"), cut, NULL, (gchar *) "edit-cut", 0, EAB_POPUP_SELECT_ANY|EAB_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, (gchar *) "c0.copy", (gchar *) N_("_Copy"), copy, NULL, (gchar *) "edit-copy", 0, EAB_POPUP_SELECT_ANY },
	{ E_POPUP_ITEM, (gchar *) "d0.paste", (gchar *) N_("P_aste"), paste, NULL, (gchar *) "edit-paste", 0, EAB_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, (gchar *) "e0.delete", (gchar *) N_("_Delete"), delete, NULL, (gchar *) "edit-delete", 0, EAB_POPUP_SELECT_EDITABLE|EAB_POPUP_SELECT_ANY },
};

static void
get_card_1(gint model_row, gpointer data)
{
	ContactAndBook *contact_and_book = data;
	EContact *contact;

	contact = eab_model_get_contact(contact_and_book->view->model, model_row);
	if (contact)
		g_ptr_array_add((GPtrArray *)contact_and_book->closure, contact);
}

static void
eabv_popup_free(EPopup *ep, GSList *list, gpointer data)
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
	gint i;
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

	/** @HookPoint-EABPopup:Addressbook view Context Menu
	 * @Id: org.gnome.evolution.addressbook.view.popup
	 * @Class: org.gnome.evolution.addresbook.popup:1.0
	 * @Target: EABPopupTargetSelect
	 *
	 * The context menu on the contacts view.
	 */

	ep = eab_popup_new("org.gnome.evolution.addressbook.view.popup");
	t = eab_popup_target_new_select(ep, view->book, !eab_model_editable(view->model), cards);
	t->target.widget = (GtkWidget *)view;

	for (i=0;i<sizeof(eabv_popup_items)/sizeof(eabv_popup_items[0]);i++)
		menus = g_slist_prepend(menus, &eabv_popup_items[i]);

	e_popup_add_items((EPopup *)ep, menus, NULL, eabv_popup_free, contact_and_book);

	menu = e_popup_create_menu_once((EPopup *)ep, (EPopupTarget *)t, 0);
	gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event?event->button.button:0, event?event->button.time:gtk_get_current_event_time());
}

static void
render_contact (gint row, EABView *view)
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

		g_return_if_fail (E_IS_BOOK (book));

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
		     gint                 row,
		     gint                 col,
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
		gchar *value;

		value = eab_contact_list_to_string (contact_list);

		gtk_selection_data_set (selection_data,
					selection_data->target,
					8,
					(guchar *)value, strlen (value));
		g_free (value);
		break;
	}
	case DND_TARGET_TYPE_SOURCE_VCARD: {
		gchar *value;

		value = eab_book_and_contact_list_to_string (view->book, contact_list);

		gtk_selection_data_set (selection_data,
					selection_data->target,
					8,
					(guchar *)value, strlen (value));
		g_free (value);
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
contacts_removed (EABModel *model, gpointer data, EABView *eav)
{
	GArray *indices = (GArray *) data;
	gint count = indices->len;
	gint i;

	for (i = 0; i < count; i ++) {

		if (eav->displayed_contact == g_array_index (indices, gint, i)) {

			/* if the contact that's presently displayed is changed, clear the display */
			eab_contact_display_render (EAB_CONTACT_DISPLAY (eav->contact_display), NULL,
						    EAB_CONTACT_DISPLAY_RENDER_NORMAL);
			eav->displayed_contact = -1;
			break;
		}
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
	ETableExtras *extras;
	ECell *cell;
	GtkWidget *table;
	gchar *etspecfile;

	adapter = eab_table_adapter_new(view->model);

	extras = e_table_extras_new ();

	/* set proper format component for a default 'date' cell renderer */
	cell = e_table_extras_get_cell (extras, "date");
	e_cell_date_set_format_component (E_CELL_DATE (cell), "addressbook");

	/* Here we create the table.  We give it the three pieces of
	   the table we've created, the header, the model, and the
	   initial layout.  It does the rest.  */
	etspecfile = g_build_filename (EVOLUTION_ETSPECDIR,
				       "e-addressbook-view.etspec",
				       NULL);
	table = e_table_scrolled_new_from_spec_file (adapter, extras, etspecfile, NULL);
	g_free (etspecfile);

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
	GList *master_list;
	gchar *search_word, *search_query, *view_sexp;
	const gchar *category_name;
	gint search_type, subid;

	g_object_get(esb,
		     "text", &search_word,
		     "item_id", &search_type,
		     NULL);

	if (search_type == E_FILTERBAR_ADVANCED_ID) {
		/* rebuild view immediately */
		query_changed (esb, v);
	}
	else {
		if ((search_word && strlen (search_word))) {
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
				search_query = g_strdup_printf ("(beginswith \"email\" %s)",
								s->str);
				break;
			default:
				search_query = g_strdup ("(contains \"x-evolution-any-field\" \"\")");
				break;
			}
			g_string_free (s, TRUE);

		} else
			search_query = g_strdup ("(contains \"x-evolution-any-field\" \"\")");

		/* Merge view and sexp */
		subid = e_search_bar_get_viewitem_id (esb);

		if (subid) {
			master_list = get_master_list (FALSE);
			if (subid < 3) {
				view_sexp = g_strdup ("(not (and (exists \"CATEGORIES\") (not (is \"CATEGORIES\" \"\"))))");
			} else {
				category_name = g_list_nth_data (master_list, subid-3);
				view_sexp = g_strdup_printf ("(is \"category_list\" \"%s\")", category_name);
			}
			search_query = g_strconcat ("(and ", view_sexp, search_query, ")", NULL);
			g_free (view_sexp);
		}

		if (search_query)
			g_object_set (v,
				      "query", search_query,
				      NULL);

		g_free (search_query);
	}

	g_free (search_word);
	v->displayed_contact = -1;
	eab_contact_display_render (EAB_CONTACT_DISPLAY (v->contact_display), NULL,
						    EAB_CONTACT_DISPLAY_RENDER_NORMAL);
}

static void
search_menu_activated (ESearchBar *esb, gint id, EABView *view)
{
	if (id == E_FILTERBAR_ADVANCED_ID)
		e_search_bar_set_item_id (esb, id);
}

static void
query_changed (ESearchBar *esb, EABView *view)
{
	gint search_type;
	gchar *query;

	search_type = e_search_bar_get_item_id(esb);
	if (search_type == E_FILTERBAR_ADVANCED_ID) {
		g_object_get (esb, "query", &query, NULL);
		g_object_set (view, "query", query, NULL);
		g_free (query);
	}
}

static gint
compare_subitems (gconstpointer a, gconstpointer b)
{
	const ESearchBarItem *subitem_a = a;
	const ESearchBarItem *subitem_b = b;
	gchar *collate_a, *collate_b;
	gint ret;

	collate_a = g_utf8_collate_key (subitem_a->text, -1);
	collate_b = g_utf8_collate_key (subitem_b->text, -1);

	ret =  strcmp (collate_a, collate_b);

	g_free (collate_a);
	g_free (collate_b);

	return ret;
}

static GtkWidget *
generate_viewoption_menu (EABSearchBarItem *subitems)
{
	GtkWidget *menu, *menu_item;
	gint i = 0;

	menu = gtk_menu_new ();

	for (i = 0; subitems[i].search.id != -1; ++i) {
		if (subitems[i].search.text) {
			gchar *str = NULL;
			str = e_str_without_underscores (subitems[i].search.text);
			menu_item = gtk_image_menu_item_new_with_label (str);
                        if (subitems[i].image) {
                                GtkWidget *image;

                                image = gtk_image_new_from_file (
                                        subitems[i].image);
                                gtk_image_menu_item_set_image (
                                        GTK_IMAGE_MENU_ITEM (menu_item),
                                        image);
                        }
			g_free (str);
		} else {
			menu_item = gtk_menu_item_new ();
			gtk_widget_set_sensitive (menu_item, FALSE);
		}

		g_object_set_data (G_OBJECT (menu_item), "EsbItemId",
				   GINT_TO_POINTER (subitems[i].search.id));

		gtk_widget_show (menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	}

	return menu;
}

static void
categories_changed_cb (gpointer object, gpointer user_data)
{
	get_master_list (TRUE);
	make_suboptions (user_data);
}

static void
make_suboptions (EABView *view)
{
	EABSearchBarItem *subitems, *s;
	GList *master_list;
	gint i, N;
	GtkWidget *menu;

	master_list = get_master_list (FALSE);
	N = g_list_length (master_list);
	subitems = g_new (EABSearchBarItem, N+4);

	subitems[0].search.id = 0;
	subitems[0].search.text = g_strdup (_("Any Category"));
	subitems[0].image = NULL;

	subitems[1].search.text = g_strdup (_("Unmatched"));
	subitems[1].search.id = 1;
	subitems[1].image = NULL;

	subitems[2].search.text = NULL;
	subitems[2].search.id = 0;
	subitems[2].image = NULL;

	for (i=0; i<N; ++i) {
		const gchar *category = g_list_nth_data (master_list, i);
		subitems[i+3].search.id = i+3;
		subitems[i+3].search.text = g_strdup (category);
		subitems[i+3].image = (gchar *)e_categories_get_icon_file_for (category);
	}

	subitems[N+3].search.id = -1;
	subitems[N+3].search.text = NULL;
	subitems[N+3].image = NULL;

	qsort (subitems + 3, N, sizeof (subitems[0]), compare_subitems);
	menu = generate_viewoption_menu (subitems);
	e_search_bar_set_viewoption_menu ((ESearchBar *)view->search, menu);

	for (s = subitems; ((ESearchBarItem *)s)->id != -1; s++) {
		if (((ESearchBarItem *)s)->text)
			g_free (((ESearchBarItem *)s)->text);
	}
	g_free (subitems);
}

static GList *
get_master_list (gboolean force_rebuild)
{
	static GList *category_list = NULL;

	if (force_rebuild) {
		g_list_free (category_list);
		category_list = NULL;
	}

	if (category_list == NULL) {
		GList *l, *p = e_categories_get_list ();

		for (l = p; l; l = l->next) {
			if (e_categories_is_searchable ((const gchar *) l->data))
				category_list = g_list_prepend (category_list, l->data);
		}

		category_list = g_list_reverse (category_list);

		g_list_free (p);
	}

	return category_list;
}

static void
contact_print_button_draw_page (GtkPrintOperation *operation,
                                GtkPrintContext *context,
                                gint page_nr,
                                EPrintable *printable)
{
	GtkPageSetup *setup;
	gdouble top_margin;
	cairo_t *cr;

	setup = gtk_print_context_get_page_setup (context);
	top_margin = gtk_page_setup_get_top_margin (setup, GTK_UNIT_POINTS);

	cr = gtk_print_context_get_cairo_context (context);

	e_printable_reset (printable);

	while (e_printable_data_left (printable)) {
		cairo_save (cr);
		e_printable_print_page (
			printable, context, 6.5 * 72, top_margin + 10, TRUE);
		cairo_restore (cr);
	}
}

static void
e_contact_print_button (EPrintable *printable, GtkPrintOperationAction action)
{
	GtkPrintOperation *operation;

	operation = e_print_operation_new ();
	gtk_print_operation_set_n_pages (operation, 1);

	g_signal_connect (
		operation, "draw_page",
		G_CALLBACK (contact_print_button_draw_page), printable);

	gtk_print_operation_run (operation, action, NULL, NULL);

	g_object_unref (operation);
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
	e_search_bar_set_ui_component ( (ESearchBar *)view->search, uic);
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
eab_view_print (EABView *view, GtkPrintOperationAction action)
{
	if (view->view_type == EAB_VIEW_MINICARD) {
		EBook *book;
		EBookQuery *query;
		gchar *query_string;
		GList *contact_list;

		g_object_get (
			view->model, "query", &query_string,
			"book", &book, NULL);

		if (query_string != NULL)
			query = e_book_query_from_string (query_string);
		else
			query = NULL;
		g_free (query_string);

		contact_list = get_selected_contacts (view);
		e_contact_print (book, query, contact_list, action);
		g_list_foreach (contact_list, (GFunc) g_object_unref, NULL);
		g_list_free (contact_list);

		if (query != NULL)
			e_book_query_unref (query);

	} else if (view->view_type == EAB_VIEW_TABLE) {
		EPrintable *printable;
		ETable *table;

		g_object_get (view->widget, "table", &table, NULL);
		printable = e_table_get_printable (table);
		g_object_ref_sink (printable);
		g_object_unref (table);

		e_contact_print_button (printable, action);

		g_object_unref (printable);
	}
}

/* callback function to handle removal of contacts for
 * which a user doesnt have write permission
 */
static void delete_contacts_cb (EBook *book,  EBookStatus status,  gpointer closure)
{
	switch (status) {
		case E_BOOK_ERROR_OK :
		case E_BOOK_ERROR_CANCELLED :
			break;
		case E_BOOK_ERROR_PERMISSION_DENIED :
			e_error_run (NULL, "addressbook:contact-delete-error-perm", NULL);
			break;
		default :
			/* Unknown error */
			eab_error_dialog (_("Failed to delete contact"), status);
			break;
	}
}

void
eab_view_delete_selection(EABView *view, gboolean is_delete)
{
	GList *list, *l;
	gboolean plural = FALSE, is_list = FALSE;
	EContact *contact;
	ETable *etable = NULL;
	EMinicardView *card_view;
	ESelectionModel *selection_model = NULL;
	gchar *name = NULL;
	gint row = 0, select;

	list = get_selected_contacts (view);
	contact = list->data;

	if (g_list_next(list))
		plural = TRUE;
	else
		name = e_contact_get (contact, E_CONTACT_FILE_AS);

	if (e_contact_get (contact, E_CONTACT_IS_LIST))
		is_list = TRUE;

	if (view->view_type == EAB_VIEW_MINICARD) {
		card_view = e_minicard_view_widget_get_view (E_MINICARD_VIEW_WIDGET(view->object));
		selection_model = get_selection_model (view);
		row = e_selection_model_cursor_row (selection_model);
	}

	else if (view->view_type == EAB_VIEW_TABLE) {
		etable = e_table_scrolled_get_table(E_TABLE_SCROLLED(view->widget));
		row = e_table_get_cursor_row (E_TABLE (etable));
	}

	/* confirm delete */
	if (is_delete &&
	    !eab_editor_confirm_delete(GTK_WINDOW(gtk_widget_get_toplevel(view->widget)),
				       plural, is_list, name)) {
		g_free (name);
		g_list_foreach (list, (GFunc) g_object_unref, NULL);
		g_list_free (list);
		return;
	}

	if (e_book_check_static_capability (view->book, "bulk-remove")) {
		GList *ids = NULL;

		for (l=list;l;l=g_list_next(l)) {
			contact = l->data;

			ids = g_list_prepend (ids, (gchar *)e_contact_get_const (contact, E_CONTACT_UID));
		}

		/* Remove the cards all at once. */
		e_book_async_remove_contacts (view->book,
					      ids,
					      delete_contacts_cb,
					      NULL);

		g_list_free (ids);
	}
	else {
		for (l=list;l;l=g_list_next(l)) {
			contact = l->data;
			/* Remove the card. */
			e_book_async_remove_contact (view->book,
						     contact,
						     delete_contacts_cb,
						     NULL);
		}
	}

	/* Sets the cursor, at the row after the deleted row */
	if (view->view_type == EAB_VIEW_MINICARD && row!=0) {
		select = e_sorter_model_to_sorted (selection_model->sorter, row);

	/* Sets the cursor, before the deleted row if its the last row */
		if (select == e_selection_model_row_count (selection_model) - 1)
			select = select - 1;
		else
			select = select + 1;

		row = e_sorter_sorted_to_model (selection_model->sorter, select);
		e_selection_model_cursor_changed (selection_model, row, 0);
	}

	/* Sets the cursor, at the row after the deleted row */
	else if (view->view_type == EAB_VIEW_TABLE && row!=0) {
		select = e_table_model_to_view_row (E_TABLE (etable), row);

	/* Sets the cursor, before the deleted row if its the last row */
		if (select == e_table_model_row_count (E_TABLE(etable)->model) - 1)
			select = select - 1;
		else
			select = select + 1;

		row = e_table_view_to_model_row (E_TABLE (etable), select);
		e_table_set_cursor_row (E_TABLE (etable), row);
	}
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
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
	gchar *value;

	value = eab_contact_list_to_string (view->clipboard_contacts);

	gtk_selection_data_set (selection_data, GDK_SELECTION_TYPE_STRING,
				8, (guchar *)value, strlen (value));
	g_free (value);
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
		gchar *str = NULL;

		if (selection_data->data [selection_data->length - 1] != 0) {
			str = g_malloc0 (selection_data->length + 1);
			memcpy (str, selection_data->data, selection_data->length);
			contact_list = eab_contact_list_from_string (str);
		} else
			contact_list = eab_contact_list_from_string ((gchar *)selection_data->data);

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
add_to_list (gint model_row, gpointer closure)
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
eab_view_save_as (EABView *view, gboolean all)
{
	GList *list = NULL;
	EBook *book;

	g_object_get(view->model,
		     "book", &book,
		     NULL);

	if (all) {
		EBookQuery *query = e_book_query_any_field_contains("");
		e_book_get_contacts(book, query, &list, NULL);
		e_book_query_unref(query);
	}
	else {
		list = get_selected_contacts(view);
	}
	if (list)
		eab_contact_list_save (_("Save as vCard..."), list, NULL);
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

void
eab_view_view (EABView *view)
{
	GList *list = get_selected_contacts (view);
	eab_show_multiple_contacts (view->book, list, view->editable);
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

void
eab_view_send (EABView *view)
{
	GList *list = get_selected_contacts (view);
	if (list)
		eab_send_contact_list (list, EAB_DISPOSITION_AS_ATTACHMENT);
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

void
eab_view_send_to (EABView *view)
{
	GList *list = get_selected_contacts (view);
	if (list)
		eab_send_contact_list (list, EAB_DISPOSITION_AS_TO);
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

void
eab_view_cut (EABView *view)
{
	eab_view_copy (view);
	eab_view_delete_selection (view, FALSE);
}

static gboolean
contact_display_has_selection (EABContactDisplay *display)
{
	gchar *string;
	gint selection_length;
	gboolean has_selection;

	string = gtk_html_get_selection_html (GTK_HTML (display), &selection_length);

	has_selection = string ? TRUE : FALSE;

	if (string)
		g_free (string);

	return has_selection;
}

void
eab_view_copy (EABView *view)
{
	if (GTK_WIDGET_HAS_FOCUS (view->contact_display) &&
	    contact_display_has_selection (EAB_CONTACT_DISPLAY (view->contact_display)))
	{
		gtk_html_copy (GTK_HTML (view->contact_display));
	}
	else
	{
		view->clipboard_contacts = get_selected_contacts (view);

		gtk_selection_owner_set (view->invisible, clipboard_atom, GDK_CURRENT_TIME);
	}
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
view_transfer_contacts (EABView *view, gboolean delete_from_source, gboolean all)
{
	EBook *book;
	GList *contacts = NULL;
	GtkWindow *parent_window;

	g_object_get(view->model,
		     "book", &book,
		     NULL);

	if (all) {
		EBookQuery *query = e_book_query_any_field_contains("");
		e_book_get_contacts(book, query, &contacts, NULL);
		e_book_query_unref(query);
	}
	else {
		contacts = get_selected_contacts (view);
	}
	parent_window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view)));

	eab_transfer_contacts (book, contacts, delete_from_source, parent_window);
	g_object_unref(book);
}

void
eab_view_copy_to_folder (EABView *view, gboolean all)
{
	view_transfer_contacts (view, FALSE, all);
}

void
eab_view_move_to_folder (EABView *view, gboolean all)
{
	view_transfer_contacts (view, TRUE, all);
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
