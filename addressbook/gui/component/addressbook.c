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
#include "e-contact-editor.h"
#include "e-ldap-server-dialog.h"

#ifdef USING_OAF
#define CONTROL_FACTORY_ID "OAFIID:control-factory:addressbook:3e10597b-0591-4d45-b082-d781b7aa6e17"
#else
#define CONTROL_FACTORY_ID "control-factory:addressbook"
#endif

#define PROPERTY_FOLDER_URI          "folder_uri"

#define PROPERTY_FOLDER_URI_IDX      1

static void
control_deactivate (BonoboControl *control, BonoboUIHandler *uih)
{
	/* how to remove a menu item */
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
	GtkWidget* contact_editor =
		e_contact_editor_new(e_card_new(""));
	EMinicardView *minicard_view = E_MINICARD_VIEW (user_data);
	EBook *book;

	GtkWidget* dlg = gnome_dialog_new ("Contact Editor", "Save", "Cancel", NULL);

	gtk_object_get(GTK_OBJECT(minicard_view), "book", &book, NULL);

	g_assert (E_IS_BOOK (book));

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dlg)->vbox),
			    contact_editor, TRUE, TRUE, 0);

	gtk_widget_show_all (dlg);

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

#ifdef HAVE_LDAP
static void
null_cb (EBook *book, EBookStatus status, gpointer closure)
{
}

static void
new_server_cb (BonoboUIHandler *uih, void *user_data, const char *path)
{
	EMinicardView *minicard_view = E_MINICARD_VIEW (user_data);
	ELDAPServer server;
	char *uri;
	EBook *book;

	/* fill in the defaults */
	server.host = g_strdup("");
	server.port = 389;
	server.description = g_strdup("");
	server.rootdn = g_strdup("");

	e_ldap_server_editor_show (&server);

	gtk_object_get(GTK_OBJECT(minicard_view), "book", &book, NULL);
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

static void
find_contact_cb (BonoboUIHandler *uih, void *user_data, const char *path)
{
	gint result;
	GtkWidget* search_entry = gtk_entry_new();
	EMinicardView *minicard_view = E_MINICARD_VIEW (user_data);
	gchar* search_text;

	GtkWidget* dlg = gnome_dialog_new ("Search Contacts", "Find", "Cancel", NULL);

	gtk_object_get (GTK_OBJECT(minicard_view), "query", &search_text, NULL);
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
		gtk_object_set (GTK_OBJECT(minicard_view), "query", search_text, NULL);
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
	EMinicardView *minicard_view = E_MINICARD_VIEW (user_data);

	e_minicard_view_remove_selection (minicard_view, card_deleted_cb, NULL);
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
search_entry_activated (GtkWidget* widget, EMinicardView* minicard_view)
{
	char* search_word = gtk_entry_get_text(GTK_ENTRY(widget));
	char* search_query;

	if (search_word && strlen (search_word))
		search_query = g_strdup_printf (
			"(contains \"full_name\" \"%s\")",
			search_word);
	else
		search_query = g_strdup (
			"(contains \"full_name\" \"\")");		
	
	gtk_object_set (GTK_OBJECT(minicard_view), "query",
			search_query, NULL);
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
		  EMinicardView *minicard_view)
{
	Bonobo_UIHandler  remote_uih;
	GtkWidget *toolbar;
	BonoboControl *toolbar_control;
	GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
	GtkWidget *quick_search_widget;

	remote_uih = bonobo_control_get_remote_ui_handler (control);
	bonobo_ui_handler_set_container (uih, remote_uih);		

	bonobo_ui_handler_menu_new_item (uih, "/Actions/New Contact",
					 N_("_New Contact"),       
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, new_contact_cb,
					 (gpointer)minicard_view);

#ifdef HAVE_LDAP
	bonobo_ui_handler_menu_new_item (uih, "/Actions/New Directory Server",
					 N_("N_ew Directory Server"),       
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, new_server_cb,
					 (gpointer)minicard_view);
#endif

	toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL,
				   GTK_TOOLBAR_BOTH);

	gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (toolbar),
					  gnome_toolbar, 
					  NULL, minicard_view);
	
	gtk_box_pack_start (GTK_BOX (hbox), toolbar, FALSE, TRUE, 0);


	/* add the search_vbox to the hbox which will be our toolbar */
	quick_search_widget = make_quick_search_widget (
		search_entry_activated, minicard_view);
	
	gtk_box_pack_start (GTK_BOX (hbox),
			    quick_search_widget,
			    FALSE, TRUE, 0);

	gtk_widget_show_all (hbox);

	toolbar_control = bonobo_control_new (hbox);
	bonobo_ui_handler_dock_add (
		uih, "/Toolbar",
		bonobo_object_corba_objref (BONOBO_OBJECT (toolbar_control)),
		GNOME_DOCK_ITEM_BEH_LOCKED |
		GNOME_DOCK_ITEM_BEH_EXCLUSIVE,
		GNOME_DOCK_TOP,
		1, 1, 0);
}

static void
control_activate_cb (BonoboControl *control, 
		     gboolean activate, 
		     EMinicardView* minicard_view)
{
	BonoboUIHandler  *uih;

	uih = bonobo_control_get_ui_handler (control);
	g_assert (uih);
	
	if (activate)
		control_activate (control, uih, minicard_view);
	else
		control_deactivate (control, uih);
}

typedef struct {
	GtkWidget *canvas;
	GnomeCanvasItem *view;
	GnomeCanvasItem *rect;
	GtkAllocation last_alloc;
	BonoboPropertyBag *properties;
	char *uri;
} AddressbookView;

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
	AddressbookView *view = closure;
	if (status == E_BOOK_STATUS_SUCCESS)
		gnome_canvas_item_set(view->view,
				      "book", book,
				      NULL);
}

static EBook *
ebook_create (AddressbookView *view)
{
	EBook *book;
	
	book = e_book_new ();

	if (!book) {
		printf ("%s: %s(): Couldn't create EBook, bailing.\n",
			__FILE__,
			__FUNCTION__);
		return NULL;
	}
	


	if (! e_book_load_uri (book, "file:/tmp/test.db", book_open_cb, view))
	{
		printf ("error calling load_uri!\n");
	}


	return book;
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

	EBook *book;
	char *uri_file;
	char *uri_data;
	char *uri;
	
	switch (arg_id) {

	case PROPERTY_FOLDER_URI_IDX:
		view->uri = g_strdup(BONOBO_ARG_GET_STRING (arg));
		
		book = e_book_new ();
		
		if (!book) {
			printf ("%s: %s(): Couldn't create EBook, bailing.\n",
				__FILE__,
				__FUNCTION__);
			return;
		}
		
		uri_file = g_concat_dir_and_file(view->uri + 7, "uri");

		uri_data = e_read_file(uri_file);
		if (uri_data)
			uri = uri_data;
		else
			uri = view->uri;
		
		if (! e_book_load_uri (book, uri, book_open_cb, view))
			{
				printf ("error calling load_uri!\n");
			}
		
		g_free(uri_data);
		g_free(uri_file);

		break;
		
	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}

static BonoboObject *
addressbook_factory (BonoboGenericFactory *Factory, void *closure)
{
	BonoboControl      *control;
	EBook *book;
	GtkWidget *vbox, *scrollbar;
	AddressbookView *view;

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
	
	view = g_new (AddressbookView, 1);
	
	vbox = gtk_vbox_new(FALSE, 0);
	
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

	gtk_box_pack_start(GTK_BOX(vbox), view->canvas, TRUE, TRUE, 0);

	scrollbar = gtk_hscrollbar_new(
		gtk_layout_get_hadjustment(GTK_LAYOUT(view->canvas)));

	gtk_box_pack_start(GTK_BOX(vbox), scrollbar, FALSE, FALSE, 0);

	/* Connect the signals */
	gtk_signal_connect( GTK_OBJECT( vbox ), "destroy",
			    GTK_SIGNAL_FUNC( destroy_callback ),
			    ( gpointer ) view );

	gtk_signal_connect( GTK_OBJECT( view->canvas ), "size_allocate",
			    GTK_SIGNAL_FUNC( allocate_callback ),
			    ( gpointer ) view );

	gtk_widget_show_all( vbox );
#if 0
	gdk_window_set_back_pixmap(
		GTK_LAYOUT(view->canvas)->bin_window, NULL, FALSE);
#endif

	book = ebook_create(view);

	/* Create the control. */
	control = bonobo_control_new(vbox);

	view->properties = bonobo_property_bag_new (get_prop, set_prop, view);

	bonobo_property_bag_add (
		view->properties, PROPERTY_FOLDER_URI, PROPERTY_FOLDER_URI_IDX,
		BONOBO_ARG_STRING, NULL, _("The URI that the Folder Browser will display"), 0);

	bonobo_control_set_property_bag (control,
					 view->properties);

	view->uri = NULL;

	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    control_activate_cb, view->view);

	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

	return BONOBO_OBJECT (control);
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
