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
#include "e-minicard-view.h"
#include "e-contact-editor.h"

static void
control_deactivate (BonoboControl *control, BonoboUIHandler *uih)
{
	/* how to remove a menu item */
	bonobo_ui_handler_menu_remove (uih, "/Actions/New Contact"); 

	/* remove our toolbar */
	bonobo_ui_handler_dock_remove (uih, "/Toolbar");
}

static void
do_nothing_cb (BonoboUIHandler *uih, void *user_data, const char *path)
{
	printf ("Yow! I am called back!\n");
}
 

#define BLANK_VCARD        \
"BEGIN:VCARD
"            \
"FN:
"                    \
"N:
"                     \
"BDAY:
"                  \
"TEL;WORK:
"              \
"TEL;CELL:
"              \
"EMAIL;INTERNET:
"        \
"EMAIL;INTERNET:
"        \
"ADR;WORK;POSTAL:
"       \
"ADR;HOME;POSTAL;INTL:
"  \
"END:VCARD
"              \
"
"


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
} AddressbookView;

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
	g_free(view);
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

	addressbook_control_factory =
		bonobo_generic_factory_new (
			"control-factory:addressbook",
			addressbook_factory, NULL);

	if (addressbook_control_factory == NULL) {
		g_error ("I could not register a Addressbook factory.");
	}
}
