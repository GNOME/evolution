/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-field-chooser.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include <gnome.h>

#include "e-addressbook-view.h"

#include <gal/e-table/e-table-scrolled.h>
#include <gal/e-table/e-table-model.h>
#include <gal/widgets/e-scroll-frame.h>
#include <gal/widgets/e-popup-menu.h>
#include "widgets/menus/gal-view-menus.h"
#include <gal/menus/gal-view-factory-etable.h>

#include "e-addressbook-model.h"

#include "e-minicard-view-widget.h"

#include "e-contact-editor.h"
#include "e-contact-save-as.h"
#include "addressbook/printing/e-contact-print.h"
#include "addressbook/printing/e-contact-print-envelope.h"
#include "e-card-simple.h"
#include "e-card.h"
#include "e-book.h"

#include "glade/glade-xml.h"

#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-dialog.h>
#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print-master-preview.h>

static void e_addressbook_view_init		(EAddressbookView		 *card);
static void e_addressbook_view_class_init	(EAddressbookViewClass	 *klass);
static void e_addressbook_view_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_addressbook_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_addressbook_view_destroy (GtkObject *object);
static void change_view_type (EAddressbookView *view, EAddressbookViewType view_type);

static GtkTableClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_BOOK,
	ARG_QUERY,
	ARG_TYPE,
};

GtkType
e_addressbook_view_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		static const GtkTypeInfo info =
		{
			"EAddressbookView",
			sizeof (EAddressbookView),
			sizeof (EAddressbookViewClass),
			(GtkClassInitFunc) e_addressbook_view_class_init,
			(GtkObjectInitFunc) e_addressbook_view_init,
			/* reserved_1 */ NULL,
		       	/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (gtk_table_get_type (), &info);
	}

	return type;
}

static void
e_addressbook_view_class_init (EAddressbookViewClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS(klass);

	parent_class = gtk_type_class (gtk_table_get_type ());

	object_class->set_arg = e_addressbook_view_set_arg;
	object_class->get_arg = e_addressbook_view_get_arg;
	object_class->destroy = e_addressbook_view_destroy;

	gtk_object_add_arg_type ("EAddressbookView::book", GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE, ARG_BOOK);
	gtk_object_add_arg_type ("EAddressbookView::query", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_QUERY);
	gtk_object_add_arg_type ("EAddressbookView::type", GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE, ARG_TYPE);
}

static void
e_addressbook_view_init (EAddressbookView *eav)
{
	eav->view_type = E_ADDRESSBOOK_VIEW_NONE;

	eav->book = NULL;
	eav->query = g_strdup("(contains \"x-evolution-any-field\" \"\")");

	eav->object = NULL;
	eav->widget = NULL;
}

static void
e_addressbook_view_destroy (GtkObject *object)
{
	EAddressbookView *eav = E_ADDRESSBOOK_VIEW(object);

	if (eav->book)
		gtk_object_unref(GTK_OBJECT(eav->book));
	g_free(eav->query);

	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

GtkWidget*
e_addressbook_view_new (void)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_addressbook_view_get_type ()));
	return widget;
}

static void
e_addressbook_view_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EAddressbookView *eav = E_ADDRESSBOOK_VIEW(object);

	switch (arg_id){
	case ARG_BOOK:
		if (eav->book)
			gtk_object_unref(GTK_OBJECT(eav->book));
		if (GTK_VALUE_OBJECT(*arg)) {
			eav->book = E_BOOK(GTK_VALUE_OBJECT(*arg));
			gtk_object_ref(GTK_OBJECT(eav->book));
		}
		else
			eav->book = NULL;
		if (eav->object)
			gtk_object_set(GTK_OBJECT(eav->object),
				       "book", eav->book,
				       NULL);
		break;
	case ARG_QUERY:
		g_free(eav->query);
		eav->query = g_strdup(GTK_VALUE_STRING(*arg));
		if (!eav->query)
			eav->query = g_strdup("(contains \"x-evolution-any-field\" \"\")");
		if (eav->object)
			gtk_object_set(GTK_OBJECT(eav->object),
				       "query", eav->query,
				       NULL);
		break;
	case ARG_TYPE:
		change_view_type(eav, GTK_VALUE_ENUM(*arg));
		break;
	default:
		break;
	}
}

static void
e_addressbook_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EAddressbookView *eav = E_ADDRESSBOOK_VIEW(object);

	switch (arg_id) {
	case ARG_BOOK:
		if (eav->book)
			GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(eav->book);
		else
			GTK_VALUE_OBJECT (*arg) = NULL;
		break;
	case ARG_QUERY:
		GTK_VALUE_STRING (*arg) = eav->query;
		break;
	case ARG_TYPE:
		GTK_VALUE_ENUM (*arg) = eav->view_type;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}



typedef struct {
	EAddressbookView *view;
	char letter;
} LetterClosure;

static void
jump_to_letter(GtkWidget *button, LetterClosure *closure)
{
	if (closure->view->widget && E_IS_MINICARD_VIEW_WIDGET(closure->view->widget))
		e_minicard_view_widget_jump_to_letter(E_MINICARD_VIEW_WIDGET(closure->view->widget), closure->letter);
}

static void
free_closure(GtkWidget *button, LetterClosure *closure)
{
	g_free(closure);
}

static void
connect_button (EAddressbookView *view, GladeXML *gui, char letter)
{
	char *name;
	GtkWidget *button;
	LetterClosure *closure;
	name = g_strdup_printf("button-%c", letter);
	button = glade_xml_get_widget(gui, name);
	g_free(name);
	if (!button)
		return;
	closure = g_new(LetterClosure, 1);
	closure->view = view;
	closure->letter = letter;
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(jump_to_letter), closure);
	gtk_signal_connect(GTK_OBJECT(button), "destroy",
			   GTK_SIGNAL_FUNC(free_closure), closure);
}

static GtkWidget *
create_alphabet (EAddressbookView *view)
{
	GtkWidget *widget;
	char letter;
	GladeXML *gui = glade_xml_new (EVOLUTION_GLADEDIR "/alphabet.glade", NULL);

	widget = glade_xml_get_widget(gui, "scrolledwindow-top");
	if (!widget) {
		return NULL;
	}
	
	connect_button(view, gui, '1');
	for (letter = 'a'; letter <= 'z'; letter ++) {
		connect_button(view, gui, letter);
	}
	
	gtk_object_unref(GTK_OBJECT(gui));
	return widget;
}

static void
create_minicard_view (EAddressbookView *view)
{
	GtkWidget *scrollframe;
	GtkWidget *alphabet;
	GtkWidget *minicard_view;
	GtkWidget *minicard_hbox;

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	minicard_hbox = gtk_hbox_new(FALSE, 0);

	minicard_view = e_minicard_view_widget_new();

	view->object = GTK_OBJECT(minicard_view);
	view->widget = minicard_hbox;

	scrollframe = e_scroll_frame_new (NULL, NULL);
	e_scroll_frame_set_policy (E_SCROLL_FRAME (scrollframe),
				   GTK_POLICY_AUTOMATIC,
				   GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (scrollframe), minicard_view);


	gtk_box_pack_start(GTK_BOX(minicard_hbox), scrollframe, TRUE, TRUE, 0);

	alphabet = create_alphabet(view);
	if (alphabet) {
		gtk_object_ref(GTK_OBJECT(alphabet));
		gtk_widget_unparent(alphabet);
		gtk_box_pack_start(GTK_BOX(minicard_hbox), alphabet, FALSE, FALSE, 0);
		gtk_object_unref(GTK_OBJECT(alphabet));
	}

	gtk_table_attach(GTK_TABLE(view), minicard_hbox,
			 0, 1,
			 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
			 0, 0);

	gtk_widget_show_all( GTK_WIDGET(minicard_hbox) );

	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();
}


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
table_double_click(ETableScrolled *table, gint row, EAddressbookView *view)
{
	if (E_IS_ADDRESSBOOK_MODEL(view->object)) {
		EAddressbookModel *model = E_ADDRESSBOOK_MODEL(view->object);
		ECard *card = e_addressbook_model_get_card(model, row);
		EBook *book;
		EContactEditor *ce;
		
		gtk_object_get(GTK_OBJECT(model),
			       "book", &book,
			       NULL);
		
		g_assert (E_IS_BOOK (book));

		ce = e_contact_editor_new (card, FALSE);

		gtk_signal_connect (GTK_OBJECT (ce), "add_card",
				    GTK_SIGNAL_FUNC (add_card_cb), book);
		gtk_signal_connect (GTK_OBJECT (ce), "commit_card",
				    GTK_SIGNAL_FUNC (commit_card_cb), book);
		gtk_signal_connect (GTK_OBJECT (ce), "delete_card",
				    GTK_SIGNAL_FUNC (delete_card_cb), book);
		gtk_signal_connect (GTK_OBJECT (ce), "editor_closed",
				    GTK_SIGNAL_FUNC (editor_closed_cb), NULL);

		gtk_object_unref(GTK_OBJECT(card));
	}
}

typedef struct {
	EBook *book;
	ECard *card;
	GtkWidget *widget;
} CardAndBook;

static void
card_and_book_free (CardAndBook *card_and_book)
{
	gtk_object_unref(GTK_OBJECT(card_and_book->card));
	gtk_object_unref(GTK_OBJECT(card_and_book->book));
}

static void
save_as (GtkWidget *widget, CardAndBook *card_and_book)
{
	e_contact_save_as(_("Save as VCard"), card_and_book->card);
	card_and_book_free(card_and_book);
}

static void
send_as (GtkWidget *widget, CardAndBook *card_and_book)
{
	e_card_send(card_and_book->card, E_CARD_DISPOSITION_AS_ATTACHMENT);
	card_and_book_free(card_and_book);
}

static void
send_to (GtkWidget *widget, CardAndBook *card_and_book)
{
	e_card_send(card_and_book->card, E_CARD_DISPOSITION_AS_TO);
	card_and_book_free(card_and_book);
}

static void
print (GtkWidget *widget, CardAndBook *card_and_book)
{
	gtk_widget_show(e_contact_print_card_dialog_new(card_and_book->card));
	card_and_book_free(card_and_book);
}

static void
print_envelope (GtkWidget *widget, CardAndBook *card_and_book)
{
	gtk_widget_show(e_contact_print_envelope_dialog_new(card_and_book->card));
	card_and_book_free(card_and_book);
}

static void
delete (GtkWidget *widget, CardAndBook *card_and_book)
{
	if (e_contact_editor_confirm_delete(GTK_WINDOW(gtk_widget_get_toplevel(card_and_book->widget)))) {
		/* Add the card in the contact editor to our ebook */
		e_book_remove_card (card_and_book->book,
				    card_and_book->card,
				    NULL,
				    NULL);
	}
	card_and_book_free(card_and_book);
}

static gint
table_right_click(ETableScrolled *table, gint row, gint col, GdkEvent *event, EAddressbookView *view)
{
	if (E_IS_ADDRESSBOOK_MODEL(view->object)) {
		EAddressbookModel *model = E_ADDRESSBOOK_MODEL(view->object);
		CardAndBook *card_and_book;
		
		EPopupMenu menu[] = {
			{"Save as VCard", NULL, GTK_SIGNAL_FUNC(save_as), NULL, 0}, 
			{"Send contact to other", NULL, GTK_SIGNAL_FUNC(send_as), NULL, 0},
			{"Send message to contact", NULL, GTK_SIGNAL_FUNC(send_to), NULL, 0},
			{"Print", NULL, GTK_SIGNAL_FUNC(print), NULL, 0},
			{"Print Envelope", NULL, GTK_SIGNAL_FUNC(print_envelope), NULL, 0},
			{"Delete", NULL, GTK_SIGNAL_FUNC(delete), NULL, 0},
			{NULL, NULL, NULL, NULL, 0}
		};
		
		card_and_book = g_new(CardAndBook, 1);
		card_and_book->card = e_addressbook_model_get_card(model, row);
		card_and_book->widget = GTK_WIDGET(table);
		gtk_object_get(GTK_OBJECT(model),
			       "book", &(card_and_book->book),
			       NULL);
		
		gtk_object_ref(GTK_OBJECT(card_and_book->book));
		
		e_popup_menu_run (menu, (GdkEventButton *)event, 0, 0, card_and_book);
		return TRUE;
	} else
		return FALSE;
}
#define SPEC "<?xml version=\"1.0\"?>      \
<ETableSpecification click-to-add=\"true\" draw-grid=\"true\" _click-to-add-message=\"* Click here to add a contact *\">   \
  <ETableColumn model_col= \"0\" _title=\"Name\"          expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col= \"2\" _title=\"Email\"         expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col= \"3\" _title=\"Primary\"       expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col= \"4\" _title=\"Business\"      expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col= \"5\" _title=\"Home\"          expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col= \"6\" _title=\"Organization\"  expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col= \"7\" _title=\"Business\"      expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col= \"8\" _title=\"Home\"          expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col= \"9\" _title=\"Mobile\"        expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"10\" _title=\"Car\"           expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"11\" _title=\"Business Fax\"  expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"12\" _title=\"Home Fax\"      expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"13\" _title=\"Business 2\"    expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"14\" _title=\"Home 2\"        expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"15\" _title=\"ISDN\"          expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"16\" _title=\"Other\"         expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"17\" _title=\"Pager\"         expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"18\" _title=\"Other\"         expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"19\" _title=\"Email 2\"       expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"20\" _title=\"Email 3\"       expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"21\" _title=\"Web Site\"      expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"22\" _title=\"Department\"    expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"23\" _title=\"Office\"        expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"24\" _title=\"Title\"         expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"25\" _title=\"Profession\"    expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"26\" _title=\"Manager\"       expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"27\" _title=\"Assistant\"     expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"28\" _title=\"Nickname\"      expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"29\" _title=\"Spouse\"        expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"30\" _title=\"Note\"          expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableColumn model_col=\"31\" _title=\"Free-busy URL\" expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
  <ETableState>                            \
    <column source=\"0\"/>                 \
    <column source=\"1\"/>                 \
    <column source=\"5\"/>                 \
    <column source=\"3\"/>                 \
    <column source=\"4\"/>                 \
    <grouping>                             \
      <leaf column=\"0\" ascending=\"true\"/> \
    </grouping>                            \
  </ETableState>                           \
</ETableSpecification>"

static void
create_table_view (EAddressbookView *view)
{
	ETableModel *model;
	ECardSimple *simple;
	GtkWidget *table;
	
	simple = e_card_simple_new(NULL);

	model = e_addressbook_model_new();

	/* Here we create the table.  We give it the three pieces of
	   the table we've created, the header, the model, and the
	   initial layout.  It does the rest.  */
	table = e_table_scrolled_new (model, NULL, SPEC, NULL);

	view->object = GTK_OBJECT(model);
	view->widget = table;

	gtk_signal_connect(GTK_OBJECT(table), "double_click",
			   GTK_SIGNAL_FUNC(table_double_click), view);
	gtk_signal_connect(GTK_OBJECT(table), "right_click",
			   GTK_SIGNAL_FUNC(table_right_click), view);

	gtk_table_attach(GTK_TABLE(view), table,
			 0, 1,
			 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
			 0, 0);

	gtk_widget_show( GTK_WIDGET(table) );

	gtk_object_unref(GTK_OBJECT(simple));
}


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
	default:
		g_warning ("view_type must be either TABLE or MINICARD\n");
		return;
	}

	view->view_type = view_type;

	gtk_object_set(view->object,
		       "query", view->query,
		       "book", view->book,
		       NULL);
}

static void
e_contact_print_destroy(GnomeDialog *dialog, gpointer data)
{
	ETableScrolled *table = gtk_object_get_data(GTK_OBJECT(dialog), "table");
	EPrintable *printable = gtk_object_get_data(GTK_OBJECT(dialog), "printable");
	gtk_object_unref(GTK_OBJECT(printable));
	gtk_object_unref(GTK_OBJECT(table));
}

static void
e_contact_print_button(GnomeDialog *dialog, gint button, gpointer data)
{
	GnomePrintMaster *master;
	GnomePrintContext *pc;
	EPrintable *printable = gtk_object_get_data(GTK_OBJECT(dialog), "printable");
	GtkWidget *preview;
	switch( button ) {
	case GNOME_PRINT_PRINT:
		master = gnome_print_master_new_from_dialog( GNOME_PRINT_DIALOG(dialog) );
		pc = gnome_print_master_get_context( master );
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
		gnome_print_master_close(master);
		gnome_print_master_print(master);
		gtk_object_unref(GTK_OBJECT(master));
		gnome_dialog_close(dialog);
		break;
	case GNOME_PRINT_PREVIEW:
		master = gnome_print_master_new_from_dialog( GNOME_PRINT_DIALOG(dialog) );
		pc = gnome_print_master_get_context( master );
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
		gnome_print_master_close(master);
		preview = GTK_WIDGET(gnome_print_master_preview_new(master, "Print Preview"));
		gtk_widget_show_all(preview);
		gtk_object_unref(GTK_OBJECT(master));
		break;
	case GNOME_PRINT_CANCEL:
		gnome_dialog_close(dialog);
		break;
	}
}

void
e_addressbook_view_setup_menus (EAddressbookView *view,
				BonoboUIComponent *uic)
{
	GalViewCollection *collection;
	GalViewMenus *views;
	GalViewFactory *factory;
	ETableSpecification *spec;

	collection = gal_view_collection_new();

	spec = e_table_specification_new();
	e_table_specification_load_from_string(spec, SPEC);

	factory = gal_view_factory_etable_new(spec);
	gal_view_collection_add_factory(collection, factory);
	gtk_object_sink(GTK_OBJECT(factory));

#if 0
	factory = e_minicard_view_factory_new();
	gal_view_collection_add_factory(collection, factory);
	gtk_object_sink(GTK_OBJECT(factory));
#endif

	views = gal_view_menus_new(collection);
	gal_view_menus_apply(views, uic, NULL);
	gtk_object_sink(GTK_OBJECT(views));

	gtk_object_sink(GTK_OBJECT(collection));
}

void
e_addressbook_view_print(EAddressbookView *view)
{
	if (view->view_type == E_ADDRESSBOOK_VIEW_MINICARD) {
		char *query;
		EBook *book;
		GtkWidget *print;

		gtk_object_get (view->object,
				"query", &query,
				"book", &book,
				NULL);
		print = e_contact_print_dialog_new(book, query);
		g_free(query);
		gtk_widget_show_all(print);
	} else if (view->view_type == E_ADDRESSBOOK_VIEW_TABLE) {
		GtkWidget *dialog;
		EPrintable *printable;

		dialog = gnome_print_dialog_new("Print cards", GNOME_PRINT_DIALOG_RANGE | GNOME_PRINT_DIALOG_COPIES);
		gnome_print_dialog_construct_range_any(GNOME_PRINT_DIALOG(dialog), GNOME_PRINT_RANGE_ALL | GNOME_PRINT_RANGE_SELECTION,
						       NULL, NULL, NULL);
		
		printable = e_table_scrolled_get_printable(E_TABLE_SCROLLED(view->widget));

		gtk_object_ref(GTK_OBJECT(view->widget));

		gtk_object_set_data(GTK_OBJECT(dialog), "table", view->widget);
		gtk_object_set_data(GTK_OBJECT(dialog), "printable", printable);
		
		gtk_signal_connect(GTK_OBJECT(dialog),
				   "clicked", GTK_SIGNAL_FUNC(e_contact_print_button), NULL);
		gtk_signal_connect(GTK_OBJECT(dialog),
				   "destroy", GTK_SIGNAL_FUNC(e_contact_print_destroy), NULL);
		gtk_widget_show(dialog);
	}
}

static void
card_deleted_cb (EBook* book, EBookStatus status, gpointer user_data)
{
	g_print ("%s: %s(): a card was deleted\n", __FILE__, __FUNCTION__);
}

void
e_addressbook_view_delete_selection(EAddressbookView *view)
{
	if (view->view_type == E_ADDRESSBOOK_VIEW_MINICARD)
		e_minicard_view_widget_remove_selection (E_MINICARD_VIEW_WIDGET(view->object), card_deleted_cb, NULL);
}

void
e_addressbook_view_show_all(EAddressbookView *view)
{
	gtk_object_set(GTK_OBJECT(view),
		       "query", NULL,
		       NULL);
}

void
e_addressbook_view_stop(EAddressbookView *view)
{
	switch(view->view_type) {
 	case E_ADDRESSBOOK_VIEW_MINICARD:
		e_minicard_view_widget_stop(E_MINICARD_VIEW_WIDGET (view->object));
		break;
	case E_ADDRESSBOOK_VIEW_TABLE:
		e_addressbook_model_stop(E_ADDRESSBOOK_MODEL (view->object));
		break;
	case E_ADDRESSBOOK_VIEW_NONE:
		break;
	}
}
