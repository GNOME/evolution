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
#include <e-util/e-canvas.h>
#include <e-util/e-util.h>
#include "e-minicard-view.h"

#include <e-table.h>
#include <e-cell-text.h>

#include <e-addressbook-model.h>
#include "e-contact-editor.h"
#include "e-ldap-server-dialog.h"

#ifdef USING_OAF
#define CONTROL_FACTORY_ID "OAFIID:control-factory:addressbook:3e10597b-0591-4d45-b082-d781b7aa6e17"
#else
#define CONTROL_FACTORY_ID "control-factory:addressbook"
#endif

#define PROPERTY_FOLDER_URI          "folder_uri"

#define PROPERTY_FOLDER_URI_IDX      1

typedef enum {
	ADDRESSBOOK_VIEW_NONE, /* initialized to this */
	ADDRESSBOOK_VIEW_TABLE,
	ADDRESSBOOK_VIEW_MINICARD
} AddressbookViewType;

typedef struct {
	AddressbookViewType view_type;
	EBook *book;
	GtkWidget *vbox;
	GtkWidget *minicard_vbox;
	GtkWidget *canvas;
	GnomeCanvasItem *view;
	GnomeCanvasItem *rect;
	GtkWidget *table;
	ETableModel *model;
	ECardSimple *simple;
	GtkAllocation last_alloc;
	BonoboControl *control;
	BonoboPropertyBag *properties;
	char *uri;
} AddressbookView;

static void change_view_type (AddressbookView *view, AddressbookViewType view_type);

static void
control_deactivate (BonoboControl *control, BonoboUIHandler *uih)
{
	/* how to remove a menu item */
	bonobo_ui_handler_menu_remove (uih, "/View/<sep>");
	bonobo_ui_handler_menu_remove (uih, "/View/Toggle View"); 
	bonobo_ui_handler_menu_remove (uih, "/Actions/New Contact"); 
#ifdef HAVE_LDAP
	bonobo_ui_handler_menu_remove (uih, "/Actions/New Directory Server");
#endif
	/* remove our toolbar */
	bonobo_ui_handler_dock_remove (uih, "/Toolbar");
}

static void
do_nothing_cb (BonoboUIHandler *uih, void *user_data, const char *path)
{
	printf ("Yow! I am called back!\n");
}

static void
card_added_cb (EBook* book, EBookStatus status, const char *id,
	    gpointer user_data)
{
	g_print ("%s: %s(): a card was added\n", __FILE__, __FUNCTION__);
}

static void
new_contact_cb (BonoboUIHandler *uih, void *user_data, const char *path)
{
	gint result;
	ECard *card;
	GtkWidget* contact_editor;
	EBook *book;
	AddressbookView *view = (AddressbookView *) user_data;
	GtkObject *object;
	GtkWidget* dlg = gnome_dialog_new ("Contact Editor", "Save", "Cancel", NULL);

	card = e_card_new("");
	contact_editor = e_contact_editor_new(card);
	gtk_object_sink(GTK_OBJECT(card));

	gtk_window_set_policy(GTK_WINDOW(dlg), FALSE, TRUE, FALSE);

	if (view->view)
		object = GTK_OBJECT(view->view);
	else
		object = GTK_OBJECT(view->model);
	gtk_object_get(object, "book", &book, NULL);


	g_assert (E_IS_BOOK (book));

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dlg)->vbox),
			    contact_editor, TRUE, TRUE, 0);

	gtk_widget_show (contact_editor);
	gtk_widget_show (dlg);

	gnome_dialog_close_hides (GNOME_DIALOG (dlg), TRUE);
	result = gnome_dialog_run_and_close (GNOME_DIALOG (dlg));

	
	/* If the user clicks "okay"...*/
	if (result == 0) {
		ECard *card;
		g_assert (contact_editor);
		g_assert (GTK_IS_OBJECT (contact_editor));
		gtk_object_get(GTK_OBJECT(contact_editor),
			       "card", &card,
			       NULL);
		
		/* Add the card in the contact editor to our ebook */
		e_book_add_card (
			book,
			card,
			card_added_cb,
			NULL);
	}
	
}

static void
toggle_view_as_cb (BonoboUIHandler *uih, void *user_data, const char *path)
{
	AddressbookView *view = user_data;
	
	if (view->view_type == ADDRESSBOOK_VIEW_TABLE)
		change_view_type (view, ADDRESSBOOK_VIEW_MINICARD);
	else
		change_view_type (view, ADDRESSBOOK_VIEW_TABLE);
}

#ifdef HAVE_LDAP
static void
null_cb (EBook *book, EBookStatus status, gpointer closure)
{
}

static void
new_server_cb (BonoboUIHandler *uih, void *user_data, const char *path)
{
	ELDAPServer server;
	char *uri;
	EBook *book;
	AddressbookView *view = (AddressbookView *) user_data;
	GtkObject *object;

	/* fill in the defaults */
	server.host = g_strdup("");
	server.port = 389;
	server.description = g_strdup("");
	server.rootdn = g_strdup("");

	e_ldap_server_editor_show (&server);

	if (view->view)
		object = GTK_OBJECT(view->view);
	else
		object = GTK_OBJECT(view->model);
	gtk_object_get(object, "book", &book, NULL);
	g_assert (E_IS_BOOK (book));
	
	/* XXX write out the new server info */

	/* now update the view */
	uri = g_strdup_printf ("ldap://%s:%d/%s", server.host, server.port, server.rootdn);

	e_book_unload_uri (book);

	if (! e_book_load_uri (book, uri, null_cb, NULL)) {
		g_warning ("error calling load_uri!\n");
	}
}
#endif

static char *
get_query (AddressbookView *view)
{
	GtkObject *object;
	char *query = NULL;

	if (view->view)
		object = GTK_OBJECT(view->view);
	else
		object = GTK_OBJECT(view->model);

	if (object)
		gtk_object_get (object, "query", &query, NULL);

	return query;
}

static void
set_query (AddressbookView *view, char *query)
{
	GtkObject *object;

	if (view->view)
		object = GTK_OBJECT(view->view);
	else
		object = GTK_OBJECT(view->model);

	gtk_object_set (object, 
			"query", query, 
			NULL);
}

static void
set_book(AddressbookView *view)
{
	if (view->book)
		gtk_object_set(view->view ? GTK_OBJECT(view->view) : GTK_OBJECT(view->model),
			       "book", view->book,
			       NULL);
}

static void
find_contact_cb (BonoboUIHandler *uih, void *user_data, const char *path)
{
	gint result;
	GtkWidget* search_entry = gtk_entry_new();
	gchar* search_text;
	AddressbookView *view = (AddressbookView *) user_data;
	GtkWidget* dlg = gnome_dialog_new ("Search Contacts", "Find", "Cancel", NULL);

	search_text = get_query (view);
	gtk_entry_set_text(GTK_ENTRY(search_entry), search_text);
	g_free (search_text);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dlg)->vbox),
			    search_entry, TRUE, TRUE, 0);

	gtk_widget_show_all (dlg);

	gnome_dialog_close_hides (GNOME_DIALOG (dlg), TRUE);
	result = gnome_dialog_run_and_close (GNOME_DIALOG (dlg));

	/* If the user clicks "okay"...*/
	if (result == 0) {
		search_text = gtk_entry_get_text(GTK_ENTRY(search_entry));
		set_query (view, search_text);
	}
	
}

static void
card_deleted_cb (EBook* book, EBookStatus status, gpointer user_data)
{
	g_print ("%s: %s(): a card was deleted\n", __FILE__, __FUNCTION__);
}

static void
delete_contact_cb (BonoboUIHandler *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view->view)
		e_minicard_view_remove_selection (E_MINICARD_VIEW(view->view), card_deleted_cb, NULL);
}

static GnomeUIInfo gnome_toolbar [] = {
	GNOMEUIINFO_ITEM_STOCK (N_("New"), N_("Create a new contact"), new_contact_cb, GNOME_STOCK_PIXMAP_NEW),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_STOCK (N_("Find"), N_("Find a contact"), find_contact_cb, GNOME_STOCK_PIXMAP_SEARCH),
	GNOMEUIINFO_ITEM_STOCK (N_("Print"), N_("Print contacts"), do_nothing_cb, GNOME_STOCK_PIXMAP_PRINT),
	GNOMEUIINFO_ITEM_STOCK (N_("Delete"), N_("Delete a contact"), delete_contact_cb, GNOME_STOCK_PIXMAP_TRASH),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_END
};


static void
search_entry_activated (GtkWidget* widget, gpointer user_data)
{
	char* search_word = gtk_entry_get_text(GTK_ENTRY(widget));
	char* search_query;
	AddressbookView *view = (AddressbookView *) user_data;

	if (search_word && strlen (search_word))
		search_query = g_strdup_printf (
			"(contains \"full_name\" \"%s\")",
			search_word);
	else
		search_query = g_strdup (
			"(contains \"full_name\" \"\")");

	set_query(view, search_query);

	g_free (search_query);
}

static GtkWidget*
make_quick_search_widget (GtkSignalFunc start_search_func,
			  gpointer user_data_for_search)
{
	GtkWidget *search_vbox = gtk_vbox_new (FALSE, 0);
	GtkWidget *search_entry = gtk_entry_new ();

	if (start_search_func)
	{
		gtk_signal_connect (GTK_OBJECT (search_entry), "activate",
				    (GtkSignalFunc) search_entry_activated,
				    user_data_for_search);
	}
	
	/* add the search entry to the our search_vbox */
	gtk_box_pack_start (GTK_BOX (search_vbox), search_entry,
			    FALSE, TRUE, 3);
	gtk_box_pack_start (GTK_BOX (search_vbox),
			    gtk_label_new("Quick Search"),
			    FALSE, TRUE, 0);

	return search_vbox;
}

static void
control_activate (BonoboControl *control, BonoboUIHandler *uih,
		  AddressbookView *view)
{
	Bonobo_UIHandler  remote_uih;
	GtkWidget *toolbar;
	BonoboControl *toolbar_control;
	GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
	GtkWidget *quick_search_widget;

	remote_uih = bonobo_control_get_remote_ui_handler (control);
	bonobo_ui_handler_set_container (uih, remote_uih);		

	bonobo_ui_handler_menu_new_separator (uih, "/View/<sep>", -1);

	bonobo_ui_handler_menu_new_item (uih, "/View/Toggle View",
					 N_("As _Table"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, toggle_view_as_cb,
					 (gpointer)view);

	bonobo_ui_handler_menu_new_item (uih, "/Actions/New Contact",
					 N_("_New Contact"),       
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, new_contact_cb,
					 (gpointer)view);

#ifdef HAVE_LDAP
	bonobo_ui_handler_menu_new_item (uih, "/Actions/New Directory Server",
					 N_("N_ew Directory Server"),       
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, new_server_cb,
					 (gpointer)view);
#endif

	toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL,
				   GTK_TOOLBAR_BOTH);

	gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (toolbar),
					  gnome_toolbar, 
					  NULL, view);
	
	gtk_box_pack_start (GTK_BOX (hbox), toolbar, FALSE, TRUE, 0);


	/* add the search_vbox to the hbox which will be our toolbar */
	quick_search_widget = make_quick_search_widget (
		search_entry_activated, view);
	
	gtk_box_pack_start (GTK_BOX (hbox),
			    quick_search_widget,
			    FALSE, TRUE, 0);

	gtk_widget_show_all (hbox);

	toolbar_control = bonobo_control_new (hbox);
	bonobo_ui_handler_dock_add (
		uih, "/Toolbar",
		bonobo_object_corba_objref (BONOBO_OBJECT (toolbar_control)),
		GNOME_DOCK_ITEM_BEH_EXCLUSIVE,
		GNOME_DOCK_TOP,
		1, 1, 0);
}

static void
control_activate_cb (BonoboControl *control, 
		     gboolean activate, 
		     AddressbookView *view)
{
	BonoboUIHandler  *uih;

	uih = bonobo_control_get_ui_handler (control);
	g_assert (uih);
	
	if (activate)
		control_activate (control, uih, view);
	else
		control_deactivate (control, uih);
}

static void
addressbook_view_free(AddressbookView *view)
{
	if (view->properties)
		bonobo_object_unref(BONOBO_OBJECT(view->properties));
	if (view->book)
		gtk_object_unref(GTK_OBJECT(view->book));
	g_free(view->uri);
	g_free(view);
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	AddressbookView *view = closure;
	if (status == E_BOOK_STATUS_SUCCESS) {
		set_book (view);
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
			  "OpenLDAP and recompile and install evolution.\n"));
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

static void allocate_callback(GtkWidget *canvas, GtkAllocation *allocation, gpointer data)
{
	double width;
	AddressbookView *view = data;
	view->last_alloc = *allocation;
	gnome_canvas_item_set( view->view,
			       "height", (double) allocation->height,
			       NULL );
	gnome_canvas_item_set( view->view,
			       "minimum_width", (double) allocation->width,
			       NULL );
	gtk_object_get(GTK_OBJECT(view->view),
		       "width", &width,
		       NULL);
	width = MAX(width, allocation->width);
	gnome_canvas_set_scroll_region(GNOME_CANVAS( view->canvas ), 0, 0, width, allocation->height );
	gnome_canvas_item_set( view->rect,
			       "x2", (double) width,
			       "y2", (double) allocation->height,
			       NULL );
}

static void resize(GnomeCanvas *canvas, gpointer data)
{
	double width;
	AddressbookView *view = data;
	gtk_object_get(GTK_OBJECT(view->view),
		       "width", &width,
		       NULL);
	width = MAX(width, view->last_alloc.width);
	gnome_canvas_set_scroll_region(GNOME_CANVAS(view->canvas), 0, 0, width, view->last_alloc.height );
	gnome_canvas_item_set( view->rect,
			       "x2", (double) width,
			       "y2", (double) view->last_alloc.height,
			       NULL );	
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

	char *uri_file;
	char *uri_data;
	
	switch (arg_id) {

	case PROPERTY_FOLDER_URI_IDX:
		if (view->uri) {
			/* we've already had a uri set on this view, so unload it */
			e_book_unload_uri (view->book);
			g_free (view->uri);
		}

		view->uri = g_strdup(BONOBO_ARG_GET_STRING (arg));
		
		uri_file = g_concat_dir_and_file(view->uri + 7, "uri");

		uri_data = e_read_file(uri_file);

		if (!uri_data) {
			char *file_name = g_concat_dir_and_file(view->uri + 7, "addressbook.db");
			uri_data = g_strdup_printf("file://%s", file_name);
			g_free(file_name);
		}
		if (! e_book_load_uri (view->book, uri_data, book_open_cb, view))
			printf ("error calling load_uri!\n");

		
		g_free(uri_data);
		g_free(uri_file);

		break;
		
	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}

#define SPEC "<?xml version=\"1.0\"?>    \
<ETableSpecification>                    \
  <columns-shown>                        \
    <column>0</column>                   \
    <column>1</column>                   \
    <column>2</column>                   \
    <column>3</column>                   \
    <column>4</column>                   \
    <column>5</column>                   \
    <column>6</column>                   \
    <column>7</column>                   \
    <column>8</column>                   \
    <column>9</column>                   \
    <column>10</column>                  \
    <column>11</column>                  \
    <column>12</column>                  \
    <column>13</column>                  \
  </columns-shown>                       \
  <grouping>                             \
    <leaf column=\"0\" ascending=\"1\"/> \
  </grouping>                            \
</ETableSpecification>"

static void
teardown_minicard_view (AddressbookView *view)
{
	if (view->view) {
		gtk_object_destroy(GTK_OBJECT(view->view));
		view->view = NULL;
	}
	if (view->minicard_vbox) {
		gtk_widget_destroy(view->minicard_vbox);
		view->minicard_vbox = NULL;
	}

	view->canvas = NULL;
}

static void
create_minicard_view (AddressbookView *view, char *initial_query)
{
	GtkWidget *scrollbar;

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	view->minicard_vbox = gtk_vbox_new(FALSE, 0);

	view->canvas = e_canvas_new();
	view->rect = gnome_canvas_item_new(
		gnome_canvas_root( GNOME_CANVAS( view->canvas ) ),
		gnome_canvas_rect_get_type(),
		"x1", (double) 0,
		"y1", (double) 0,
		"x2", (double) 100,
		"y2", (double) 100,
		"fill_color", "white",
		NULL );

	view->view = gnome_canvas_item_new(
		gnome_canvas_root( GNOME_CANVAS( view->canvas ) ),
		e_minicard_view_get_type(),
		"height", (double) 100,
		"minimum_width", (double) 100,
		NULL );

	gtk_signal_connect( GTK_OBJECT( view->canvas ), "reflow",
			    GTK_SIGNAL_FUNC( resize ),
			    view);

	gnome_canvas_set_scroll_region ( GNOME_CANVAS( view->canvas ),
					 0, 0,
					 100, 100 );

	gtk_box_pack_start(GTK_BOX(view->minicard_vbox), view->canvas, TRUE, TRUE, 0);

	scrollbar = gtk_hscrollbar_new(
		gtk_layout_get_hadjustment(GTK_LAYOUT(view->canvas)));

	gtk_box_pack_start(GTK_BOX(view->minicard_vbox), scrollbar, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(view->vbox), view->minicard_vbox, TRUE, TRUE, 0);

	gtk_widget_show_all( GTK_WIDGET(view->minicard_vbox) );

	/* Connect the signals */
	gtk_signal_connect( GTK_OBJECT( view->canvas ), "size_allocate",
			    GTK_SIGNAL_FUNC( allocate_callback ),
			    ( gpointer ) view );

#if 0
	gdk_window_set_back_pixmap(
		GTK_LAYOUT(view->canvas)->bin_window, NULL, FALSE);
#endif

	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();
}

static void
teardown_table_view (AddressbookView *view)
{
	if (view->table) {
		gtk_widget_destroy (GTK_WIDGET (view->table));
		view->table = NULL;
	}
	if (view->model) {
		gtk_object_unref (GTK_OBJECT (view->model));
		view->model = NULL;
	}
	if (view->simple) {
		gtk_object_destroy (GTK_OBJECT (view->simple));
		view->simple = NULL;
	}
}

static void
create_table_view (AddressbookView *view, char *initial_query)
{
	ECell *cell_left_just;
	ETableHeader *e_table_header;
	int i;
	
	view->simple = e_card_simple_new(NULL);

	view->model = e_addressbook_model_new();

	/*
	  Next we create a header.  The ETableHeader is used in two
	  different way.  The first is the full_header.  This is the
	  list of possible columns in the view.  The second use is
	  completely internal.  Many of the ETableHeader functions are
	  for that purpose.  The only functions we really need are
	  e_table_header_new and e_table_header_add_col.

	  First we create the header.  */
	e_table_header = e_table_header_new ();
	
	/* Next we have to build renderers for all of the columns.
	   Since all our columns are text columns, we can simply use
	   the same renderer over and over again.  If we had different
	   types of columns, we could use a different renderer for
	   each column. */
	cell_left_just = e_cell_text_new (view->model, NULL, GTK_JUSTIFY_LEFT);
		
	/* Next we create a column object for each view column and add
	   them to the header.  We don't create a column object for
	   the importance column since it will not be shown. */
	for (i = 0; i < E_CARD_SIMPLE_FIELD_LAST - 1; i++){
		/* Create the column. */
		ETableCol *ecol = e_table_col_new (
						   i, e_card_simple_get_name(view->simple, i+1),
						   1.0, 20, cell_left_just,
						   g_str_compare, TRUE);
		/* Add it to the header. */
		e_table_header_add_column (e_table_header, ecol, i);
	}

	/* Here we create the table.  We give it the three pieces of
	   the table we've created, the header, the model, and the
	   initial layout.  It does the rest.  */
	view->table = e_table_new (e_table_header, E_TABLE_MODEL(view->model), SPEC);

	gtk_box_pack_start(GTK_BOX(view->vbox), view->table, TRUE, TRUE, 0);

	gtk_widget_show_all( GTK_WIDGET(view->table) );
}

static void
change_view_type (AddressbookView *view, AddressbookViewType view_type)
{
	char *query = NULL;
	BonoboUIHandler *uih = bonobo_control_get_ui_handler (view->control);

	if (view_type == view->view_type)
		return;
	
	if (view->view_type != ADDRESSBOOK_VIEW_NONE)
		query = get_query(view);
	else
		query = g_strdup("(contains \"full_name\" \"\")");


	switch (view_type) {
	case ADDRESSBOOK_VIEW_MINICARD:
		teardown_table_view (view);
		create_minicard_view (view, query);
		if (uih)
			bonobo_ui_handler_menu_set_label (uih, "/View/Toggle View",
							  N_("As _Table"));
		break;
	case ADDRESSBOOK_VIEW_TABLE:
		teardown_minicard_view (view);
		create_table_view (view, query);
		if (uih)
			bonobo_ui_handler_menu_set_label (uih, "/View/Toggle View",
							  N_("As _Minicards"));
		break;
	default:
		g_warning ("view_type must be either TABLE or MINICARD\n");
		g_free (query);
		return;
	}

	view->view_type = view_type;

	/* set the book */
	set_book (view);

	/* and reset the query */
	if (query)
		set_query (view, query);
	g_free (query);
}


static BonoboObject *
addressbook_factory (BonoboGenericFactory *Factory, void *closure)
{
	AddressbookView *view;

	view = g_new0 (AddressbookView, 1);

	view->vbox = gtk_vbox_new(FALSE, 0);

	gtk_signal_connect( GTK_OBJECT( view->vbox ), "destroy",
			    GTK_SIGNAL_FUNC( destroy_callback ),
			    ( gpointer ) view );

	/* Create the control. */
	view->control = bonobo_control_new(view->vbox);

	view->model = NULL;
	view->view = NULL;

	/* create the initial view */
	change_view_type (view, ADDRESSBOOK_VIEW_MINICARD);

	gtk_widget_show_all( view->vbox );

	/* create the view's ebook */
	view->book = e_book_new ();

	view->properties = bonobo_property_bag_new (get_prop, set_prop, view);

	bonobo_property_bag_add (
		view->properties, PROPERTY_FOLDER_URI, PROPERTY_FOLDER_URI_IDX,
		BONOBO_ARG_STRING, NULL, _("The URI that the Folder Browser will display"), 0);

	bonobo_control_set_property_bag (view->control,
					 view->properties);

	view->uri = NULL;

	gtk_signal_connect (GTK_OBJECT (view->control), "activate",
			    control_activate_cb, view);

	return BONOBO_OBJECT (view->control);
}

void
addressbook_factory_init (void)
{
	static BonoboGenericFactory *addressbook_control_factory = NULL;

	if (addressbook_control_factory != NULL)
		return;

	addressbook_control_factory = bonobo_generic_factory_new (CONTROL_FACTORY_ID,
								  addressbook_factory,
								  NULL);

	if (addressbook_control_factory == NULL) {
		g_error ("I could not register a Addressbook factory.");
	}
}
