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

#include <gtk/gtkinvisible.h>

#include <libgnome/gnome-paper.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <gal/e-table/e-table-scrolled.h>
#include <gal/e-table/e-table-model.h>
#include <gal/widgets/e-scroll-frame.h>
#include <gal/widgets/e-popup-menu.h>
#include <gal/widgets/e-unicode.h>
#include <gal/menus/gal-view-factory-etable.h>
#include <gal/menus/gal-view-etable.h>
#include <gal/util/e-unicode-i18n.h>
#include <gal/unicode/gunicode.h>
#include <libgnomeui/gnome-dialog-util.h>

#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-dialog.h>
#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print-master-preview.h>

#include "addressbook/printing/e-contact-print.h"
#include "addressbook/printing/e-contact-print-envelope.h"

#include "gal-view-factory-minicard.h"
#include "gal-view-minicard.h"

#include "e-addressbook-view.h"
#include "e-addressbook-model.h"
#include "e-addressbook-util.h"
#include "e-addressbook-table-adapter.h"
#include "e-addressbook-reflow-adapter.h"
#include "e-minicard-view-widget.h"
#include "e-contact-save-as.h"
#include "e-card-merging.h"

#include "e-contact-editor.h"
#include <gdk/gdkkeysyms.h>
#include <ctype.h>

static void e_addressbook_view_init		(EAddressbookView		 *card);
static void e_addressbook_view_class_init	(EAddressbookViewClass	 *klass);
static void e_addressbook_view_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_addressbook_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_addressbook_view_destroy (GtkObject *object);
static void change_view_type (EAddressbookView *view, EAddressbookViewType view_type);

static void status_message     (GtkObject *object, const gchar *status, EAddressbookView *eav);
static void folder_bar_message (GtkObject *object, const gchar *status, EAddressbookView *eav);
static void stop_state_changed (GtkObject *object, EAddressbookView *eav);
static void writable_status (GtkObject *object, gboolean writable, EAddressbookView *eav);
static void command_state_change (EAddressbookView *eav);

static void selection_clear_event (GtkWidget *invisible, GdkEventSelection *event,
				   EAddressbookView *view);
static void selection_received (GtkWidget *invisible, GtkSelectionData *selection_data,
				guint time, EAddressbookView *view);
static void selection_get (GtkWidget *invisible, GtkSelectionData *selection_data,
			   guint info, guint time_stamp, EAddressbookView *view);
static void invisible_destroyed (GtkWidget *invisible, EAddressbookView *view);

static GtkTableClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_BOOK,
	ARG_QUERY,
	ARG_TYPE,
};

enum {
	STATUS_MESSAGE,
	FOLDER_BAR_MESSAGE,
	COMMAND_STATE_CHANGE,
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
	GtkWidgetClass *widget_class;

	object_class = GTK_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);

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

	e_addressbook_view_signals [STATUS_MESSAGE] =
		gtk_signal_new ("status_message",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EAddressbookViewClass, status_message),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	e_addressbook_view_signals [FOLDER_BAR_MESSAGE] =
		gtk_signal_new ("folder_bar_message",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EAddressbookViewClass, folder_bar_message),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	e_addressbook_view_signals [COMMAND_STATE_CHANGE] =
		gtk_signal_new ("command_state_change",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EAddressbookViewClass, command_state_change),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, e_addressbook_view_signals, LAST_SIGNAL);

	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);
}

static void
e_addressbook_view_init (EAddressbookView *eav)
{
	eav->view_type = E_ADDRESSBOOK_VIEW_NONE;

	eav->model = e_addressbook_model_new ();

	gtk_signal_connect (GTK_OBJECT(eav->model),
			    "status_message",
			    GTK_SIGNAL_FUNC (status_message),
			    eav);

	gtk_signal_connect (GTK_OBJECT(eav->model),
			    "folder_bar_message",
			    GTK_SIGNAL_FUNC (folder_bar_message),
			    eav);

	gtk_signal_connect (GTK_OBJECT(eav->model),
			    "stop_state_changed",
			    GTK_SIGNAL_FUNC (stop_state_changed),
			    eav);

	gtk_signal_connect (GTK_OBJECT(eav->model),
			    "writable_status",
			    GTK_SIGNAL_FUNC (writable_status),
			    eav);

	eav->editable = FALSE;
	eav->book = NULL;
	eav->query = g_strdup("(contains \"x-evolution-any-field\" \"\")");

	eav->object = NULL;
	eav->widget = NULL;

	eav->view_collection = NULL;
	eav->view_menus = NULL;

	eav->invisible = gtk_invisible_new ();

	gtk_selection_add_target (eav->invisible,
				  clipboard_atom,
				  GDK_SELECTION_TYPE_STRING,
				  0);
		
	gtk_signal_connect (GTK_OBJECT(eav->invisible), "selection_get",
			    GTK_SIGNAL_FUNC (selection_get), 
			    eav);
	gtk_signal_connect (GTK_OBJECT(eav->invisible), "selection_clear_event",
			    GTK_SIGNAL_FUNC (selection_clear_event),
			    eav);
	gtk_signal_connect (GTK_OBJECT(eav->invisible), "selection_received",
			    GTK_SIGNAL_FUNC (selection_received),
			    eav);
	gtk_signal_connect (GTK_OBJECT(eav->invisible), "destroy",
			    GTK_SIGNAL_FUNC (invisible_destroyed),
			    eav);
}

static void
e_addressbook_view_destroy (GtkObject *object)
{
	EAddressbookView *eav = E_ADDRESSBOOK_VIEW(object);

	if (eav->model) {
		gtk_object_unref(GTK_OBJECT(eav->model));
		eav->model = NULL;
	}

	if (eav->book) {
		gtk_object_unref(GTK_OBJECT(eav->book));
		eav->book = NULL;
	}

	g_free(eav->query);
	eav->query = NULL;

	if (eav->view_collection) {
		gtk_object_unref (GTK_OBJECT (eav->view_collection));
		eav->view_collection = NULL;
	}

	if (eav->view_menus) {
		gtk_object_unref (GTK_OBJECT (eav->view_menus));
		eav->view_menus = NULL;
	}

	if (eav->clipboard_cards) {
		g_list_foreach (eav->clipboard_cards, (GFunc)gtk_object_unref, NULL);
		g_list_free (eav->clipboard_cards);
		eav->clipboard_cards = NULL;
	}
		
	if (eav->invisible) {
		gtk_widget_destroy (eav->invisible);
		eav->invisible = NULL;
	}

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
book_writable_cb (EBook *book, gboolean writable, EAddressbookView *eav)
{
	eav->editable = writable;
	gtk_object_set (GTK_OBJECT (eav->model),
			"editable", eav->editable,
			NULL);
	writable_status (GTK_OBJECT(book), writable, eav);
}

static void
e_addressbook_view_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EAddressbookView *eav = E_ADDRESSBOOK_VIEW(object);

	switch (arg_id){
	case ARG_BOOK:
		if (eav->book) {
			gtk_object_unref(GTK_OBJECT(eav->book));
		}
		if (GTK_VALUE_OBJECT(*arg)) {
			eav->book = E_BOOK(GTK_VALUE_OBJECT(*arg));
			gtk_object_ref(GTK_OBJECT(eav->book));
			gtk_signal_connect (GTK_OBJECT (eav->book),
					    "writable_status",
					    book_writable_cb, eav);
		}
		else
			eav->book = NULL;
		gtk_object_set(GTK_OBJECT(eav->model),
			       "book", eav->book,
			       NULL);

		break;
	case ARG_QUERY:
		g_free(eav->query);
		eav->query = g_strdup(GTK_VALUE_STRING(*arg));
		if (!eav->query)
			eav->query = g_strdup("(contains \"x-evolution-any-field\" \"\")");
		gtk_object_set(GTK_OBJECT(eav->model),
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
	gunichar letter;
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
jump_to_letter(GtkWidget *button, LetterClosure *closure)
{
	char *query;

	if (g_unichar_isdigit (closure->letter)) {
		const char *letters = U_(button_letters);
		char **letter_v;
		GString *gstr;
		char **p;

		letter_v = e_utf8_split (g_utf8_next_char (letters),
		                         g_utf8_get_char (letters));
		g_assert (letter_v != NULL && letter_v[0] != NULL);
		gstr = g_string_new ("(not (or ");
		for (p = letter_v + 1; *p != NULL; p++) {
			g_string_sprintfa (gstr, "(beginswith \"file_as\" \"%s\")", *p);
		}
		g_string_append (gstr, "))");
		query = gstr->str;
		g_strfreev (letter_v);
		g_string_free (gstr, FALSE);
	} else {
		char s[6 + 1];

		s [g_unichar_to_utf8 (closure->letter, s)] = '\0';
		query = g_strdup_printf ("(beginswith \"file_as\" \"%s\")", s);
	}
	gtk_object_set (GTK_OBJECT (closure->view),
			"query", query,
			NULL);
	g_free (query);
}

static void
free_closure(GtkWidget *button, LetterClosure *closure)
{
	g_free(closure);
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
	gtk_widget_set_usize (vbox, 27, 0);

	labels = U_(button_labels);
	sep = g_utf8_get_char (labels);
	label_v = e_utf8_split (g_utf8_next_char (labels), sep);
	letters = U_(button_letters);
	sep = g_utf8_get_char (letters);
	letter_v = e_utf8_split (g_utf8_next_char (letters), sep);
	g_assert (label_v != NULL && letter_v != NULL);
	for (pl = label_v, pc = letter_v; *pl != NULL && *pc != NULL; pl++, pc++) {
		GtkWidget *button;
		LetterClosure *closure;
		char *label;

		label = e_utf8_to_locale_string (*pl);
		button = gtk_button_new_with_label (label);
		g_free (label);
		gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

		closure = g_new (LetterClosure, 1);
		closure->view = view;
		closure->letter = g_utf8_get_char (*pc);
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
		                   GTK_SIGNAL_FUNC (jump_to_letter), closure);
		gtk_signal_connect(GTK_OBJECT(button), "destroy",
		                   GTK_SIGNAL_FUNC (free_closure), closure);

	}
	g_strfreev (label_v);
	g_strfreev (letter_v);

	gtk_widget_show_all (widget);

	return widget;
}

static void
minicard_selection_change (EMinicardViewWidget *widget, EAddressbookView *view)
{
	command_state_change (view);
}

static void
create_minicard_view (EAddressbookView *view)
{
	GtkWidget *scrollframe;
	GtkWidget *alphabet;
	GtkWidget *minicard_view;
	GtkWidget *minicard_hbox;
	EAddressbookReflowAdapter *adapter;

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	minicard_hbox = gtk_hbox_new(FALSE, 0);

	adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(e_addressbook_reflow_adapter_new (view->model));
	minicard_view = e_minicard_view_widget_new(adapter);

	gtk_signal_connect(GTK_OBJECT(minicard_view), "selection_change",
			   GTK_SIGNAL_FUNC(minicard_selection_change), view);


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

	e_reflow_model_changed (E_REFLOW_MODEL (adapter));

	gtk_object_unref (GTK_OBJECT (adapter));
}

static void
table_double_click(ETableScrolled *table, gint row, gint col, GdkEvent *event, EAddressbookView *view)
{
	if (E_IS_ADDRESSBOOK_TABLE_ADAPTER(view->object)) {
		EAddressbookModel *model = view->model;
		ECard *card = e_addressbook_model_get_card(model, row);
		EBook *book;

		gtk_object_get(GTK_OBJECT(model),
			       "book", &book,
			       NULL);
		
		g_assert (E_IS_BOOK (book));

		if (e_card_evolution_list (card))
			e_addressbook_show_contact_list_editor (book, card, FALSE, view->editable);
		else
			e_addressbook_show_contact_editor (book, card, FALSE, view->editable);
	}
}

typedef struct {
	EBook *book;
	ECard *card;
	EAddressbookView *view;
	GtkWidget *widget;
	gpointer closure;
} CardAndBook;

static void
card_and_book_free (CardAndBook *card_and_book)
{
	gtk_object_unref(GTK_OBJECT(card_and_book->card));
	gtk_object_unref(GTK_OBJECT(card_and_book->book));
	gtk_object_unref(GTK_OBJECT(card_and_book->view));
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
	ETable *table;

	table = E_TABLE(card_and_book->widget);
	card_and_book->closure = &list;
	e_table_selected_row_foreach(table,
				     get_card_list_1,
				     card_and_book);
	return list;
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

#if 0 /* Envelope printing is disabled for Evolution 1.0. */
static void
print_envelope (GtkWidget *widget, CardAndBook *card_and_book)
{
	gtk_widget_show(e_contact_print_envelope_dialog_new(card_and_book->card));
	card_and_book_free(card_and_book);
}
#endif

static void
delete (GtkWidget *widget, CardAndBook *card_and_book)
{
	if (e_contact_editor_confirm_delete(GTK_WINDOW(gtk_widget_get_toplevel(card_and_book->widget)))) {
		GList *list = get_card_list(card_and_book);
		GList *iterator;
		for (iterator = list; iterator; iterator = iterator->next) {
			ECard *card = iterator->data;
			/* Add the card in the contact editor to our ebook */
			e_book_remove_card (card_and_book->book,
					    card,
					    NULL,
					    NULL);
		}
		e_free_object_list(list);
	}
	card_and_book_free(card_and_book);
}

static gint
table_right_click(ETableScrolled *table, gint row, gint col, GdkEvent *event, EAddressbookView *view)
{
	if (E_IS_ADDRESSBOOK_TABLE_ADAPTER(view->object)) {
		EAddressbookModel *model = view->model;
		CardAndBook *card_and_book;

		EPopupMenu menu[] = {
			{N_("Save as VCard"), NULL, GTK_SIGNAL_FUNC(save_as), NULL, 0}, 
			{N_("Forward Contact"), NULL, GTK_SIGNAL_FUNC(send_as), NULL, 0},
			{N_("Send Message to Contact"), NULL, GTK_SIGNAL_FUNC(send_to), NULL, 0},
			{N_("Print"), NULL, GTK_SIGNAL_FUNC(print), NULL, 0},
#if 0 /* Envelope printing is disabled for Evolution 1.0. */
			{N_("Print Envelope"), NULL, GTK_SIGNAL_FUNC(print_envelope), NULL, 0},
#endif
			{N_("Delete"), NULL, GTK_SIGNAL_FUNC(delete), NULL, 0},
			{NULL, NULL, NULL, NULL, 0}
		};

		card_and_book = g_new(CardAndBook, 1);
		card_and_book->card = e_addressbook_model_get_card(model, row);
		card_and_book->widget = GTK_WIDGET(table);
		card_and_book->view = view;
		gtk_object_get(GTK_OBJECT(model),
			       "book", &(card_and_book->book),
			       NULL);

		gtk_object_ref(GTK_OBJECT(card_and_book->book));
		gtk_object_ref(GTK_OBJECT(card_and_book->view));

		e_popup_menu_run (menu, event, 0, 0, card_and_book);
		return TRUE;
	} else
		return FALSE;
}

static void
table_selection_change(ETableScrolled *table, EAddressbookView *view)
{
	command_state_change (view);
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

		row = e_table_view_to_model_row (table, row);
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
	gtk_signal_emit (GTK_OBJECT (eav),
			 e_addressbook_view_signals [STATUS_MESSAGE],
			 status);
}

static void
emit_folder_bar_message (EAddressbookView *eav, const gchar *message)
{
	gtk_signal_emit (GTK_OBJECT (eav),
			 e_addressbook_view_signals [FOLDER_BAR_MESSAGE],
			 message);
}

static void
status_message (GtkObject *object, const gchar *status, EAddressbookView *eav)
{
	emit_status_message (eav, status);
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
writable_status (GtkObject *object, gboolean writable, EAddressbookView *eav)
{
	command_state_change (eav);
}

static void
command_state_change (EAddressbookView *eav)
{
	gtk_object_ref (GTK_OBJECT (eav)); /* who knows what might happen during this emission? */
	gtk_signal_emit (GTK_OBJECT (eav), e_addressbook_view_signals [COMMAND_STATE_CHANGE]);
	gtk_object_unref (GTK_OBJECT (eav));
}

#ifdef JUST_FOR_TRANSLATORS
static char *list [] = {
	N_("* Click here to add a contact *"),
	N_("File As"),
	N_("Full Name"),
	N_("Email"),
	N_("Primary Phone"),
	N_("Assistant Phone"),
	N_("Business Phone"),
	N_("Callback Phone"),
	N_("Company Phone"),
	N_("Home Phone"),
	N_("Organization"),
	N_("Business Address"),
	N_("Home Address"),
	N_("Mobile Phone"),
	N_("Car Phone"),
	N_("Business Fax"),
	N_("Home Fax"),
	N_("Business Phone 2"),
	N_("Home Phone 2"),
	N_("ISDN"),
	N_("Other Phone"),
	N_("Other Fax"),
	N_("Pager"),
	N_("Radio"),
	N_("Telex"),
	N_("TTY"),
	N_("Other Address"),
	N_("Email 2"),
	N_("Email 3"),
	N_("Web Site"),
	N_("Department"),
	N_("Office"),
	N_("Title"),
	N_("Profession"),
	N_("Manager"),
	N_("Assistant"),
	N_("Nickname"),
	N_("Spouse"),
	N_("Note"),
	N_("Free-busy URL"),
};
#endif

#define SPEC "<?xml version=\"1.0\"?>      \
<ETableSpecification click-to-add=\"true\" draw-grid=\"true\" _click-to-add-message=\"* Click here to add a contact *\">   \
  <ETableColumn model_col= \"0\" _title=\"File As\"          expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col= \"1\" _title=\"Full Name\"        expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col= \"2\" _title=\"Email\"            expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col= \"3\" _title=\"Primary Phone\"    expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col= \"4\" _title=\"Assistant Phone\"  expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col= \"5\" _title=\"Business Phone\"   expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col= \"6\" _title=\"Callback Phone\"   expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col= \"7\" _title=\"Company Phone\"    expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col= \"8\" _title=\"Home Phone\"       expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col= \"9\" _title=\"Organization\"     expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"10\" _title=\"Business Address\" expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"11\" _title=\"Home Address\"     expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"12\" _title=\"Mobile Phone\"     expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"13\" _title=\"Car Phone\"        expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"14\" _title=\"Business Fax\"     expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"15\" _title=\"Home Fax\"         expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"16\" _title=\"Business Phone 2\" expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"17\" _title=\"Home Phone 2\"     expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"18\" _title=\"ISDN\"             expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"19\" _title=\"Other Phone\"      expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"20\" _title=\"Other Fax\"        expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"21\" _title=\"Pager\"            expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"22\" _title=\"Radio\"            expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"23\" _title=\"Telex\"            expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"24\" _title=\"TTY\"              expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"25\" _title=\"Other Address\"    expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"26\" _title=\"Email 2\"          expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"27\" _title=\"Email 3\"          expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"28\" _title=\"Web Site\"         expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"29\" _title=\"Department\"       expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"30\" _title=\"Office\"           expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"31\" _title=\"Title\"            expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"32\" _title=\"Profession\"       expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"33\" _title=\"Manager\"          expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"34\" _title=\"Assistant\"        expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"35\" _title=\"Nickname\"         expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"36\" _title=\"Spouse\"           expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"37\" _title=\"Note\"             expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col=\"38\" _title=\"Free-busy URL\"    expansion=\"1.0\" minimum_width=\"75\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableState>                            \
    <column source=\"0\"/>                 \
    <column source=\"1\"/>                 \
    <column source=\"5\"/>                 \
    <column source=\"2\"/>                 \
    <column source=\"3\"/>                 \
    <grouping>                             \
      <leaf column=\"0\" ascending=\"true\"/> \
    </grouping>                            \
  </ETableState>                           \
</ETableSpecification>"

static void
create_table_view (EAddressbookView *view)
{
	ETableModel *adapter;
	ECardSimple *simple;
	GtkWidget *table;
	
	simple = e_card_simple_new(NULL);

	adapter = e_addressbook_table_adapter_new(view->model);

	/* Here we create the table.  We give it the three pieces of
	   the table we've created, the header, the model, and the
	   initial layout.  It does the rest.  */
	table = e_table_scrolled_new (adapter, NULL, SPEC, NULL);

	view->object = GTK_OBJECT(adapter);
	view->widget = table;

	gtk_signal_connect(GTK_OBJECT(e_table_scrolled_get_table(E_TABLE_SCROLLED(table))), "double_click",
			   GTK_SIGNAL_FUNC(table_double_click), view);
	gtk_signal_connect(GTK_OBJECT(e_table_scrolled_get_table(E_TABLE_SCROLLED(table))), "right_click",
			   GTK_SIGNAL_FUNC(table_right_click), view);
	gtk_signal_connect(GTK_OBJECT(e_table_scrolled_get_table(E_TABLE_SCROLLED(table))), "selection_change",
			   GTK_SIGNAL_FUNC(table_selection_change), view);

	/* drag & drop signals */
	e_table_drag_source_set (E_TABLE(E_TABLE_SCROLLED(table)->table), GDK_BUTTON1_MASK,
				 drag_types, num_drag_types, GDK_ACTION_MOVE);
	
	gtk_signal_connect (GTK_OBJECT (E_TABLE_SCROLLED(table)->table),
			    "table_drag_data_get",
			    GTK_SIGNAL_FUNC (table_drag_data_get),
			    view);

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

	command_state_change (view);
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

static void
display_view(GalViewCollection *collection,
	     GalView *view,
	     gpointer data)
{
	EAddressbookView *address_view = data;
	if (GAL_IS_VIEW_ETABLE(view)) {
		change_view_type (address_view, E_ADDRESSBOOK_VIEW_TABLE);
		e_table_set_state_object(e_table_scrolled_get_table(E_TABLE_SCROLLED(address_view->widget)), GAL_VIEW_ETABLE(view)->state);
	} else if (GAL_IS_VIEW_MINICARD(view)) {
		change_view_type (address_view, E_ADDRESSBOOK_VIEW_MINICARD);
	}
}

void
e_addressbook_view_setup_menus (EAddressbookView *view,
				BonoboUIComponent *uic)
{
	GalViewFactory *factory;
	ETableSpecification *spec;
	char *galview;

	g_return_if_fail (view != NULL);
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));
	g_return_if_fail (uic != NULL);
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (uic));
	g_return_if_fail (view->view_collection == NULL);

	g_assert (view->view_collection == NULL);
	g_assert (view->view_menus == NULL);

	view->view_collection = gal_view_collection_new();

	galview = gnome_util_prepend_user_home("/evolution/views/addressbook/");
	gal_view_collection_set_storage_directories(view->view_collection,
						    EVOLUTION_DATADIR "/evolution/views/addressbook/",
						    galview);
	g_free(galview);

	spec = e_table_specification_new();
	e_table_specification_load_from_string(spec, SPEC);

	factory = gal_view_factory_etable_new (spec);
	gtk_object_unref (GTK_OBJECT (spec));
	gal_view_collection_add_factory (view->view_collection, factory);
	gtk_object_unref (GTK_OBJECT (factory));

	factory = gal_view_factory_minicard_new ();
	gal_view_collection_add_factory (view->view_collection, factory);
	gtk_object_unref (GTK_OBJECT (factory));

	gal_view_collection_load(view->view_collection);

	view->view_menus = gal_view_menus_new(view->view_collection);
	gal_view_menus_apply(view->view_menus, uic, NULL);
	gtk_signal_connect(GTK_OBJECT(view->view_collection), "display_view",
			   display_view, view);
}

/**
 * e_addressbook_view_discard_menus:
 * @view: An addressbook view.
 * 
 * Makes an addressbook view discard its GAL view menus and its views collection
 * objects.  This should be called when the corresponding Bonobo component is
 * deactivated.
 **/
void
e_addressbook_view_discard_menus (EAddressbookView *view)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));
	g_return_if_fail (view->view_collection);

	g_assert (view->view_collection != NULL);
	g_assert (view->view_menus != NULL);

	gtk_object_unref (GTK_OBJECT (view->view_collection));
	view->view_collection = NULL;

	gtk_object_unref (GTK_OBJECT (view->view_menus));
	view->view_menus = NULL;
}

static ESelectionModel*
get_selection_model (EAddressbookView *view)
{
	if (view->view_type == E_ADDRESSBOOK_VIEW_MINICARD)
		return e_minicard_view_widget_get_selection_model (E_MINICARD_VIEW_WIDGET(view->object));
	else
		return E_SELECTION_MODEL(E_TABLE_SCROLLED(view->widget)->table->selection);
}

void
e_addressbook_view_print(EAddressbookView *view)
{
	if (view->view_type == E_ADDRESSBOOK_VIEW_MINICARD) {
		char *query;
		EBook *book;
		GtkWidget *print;

		gtk_object_get (GTK_OBJECT(view->model),
				"query", &query,
				"book", &book,
				NULL);
		print = e_contact_print_dialog_new(book, query);
		g_free(query);
		gtk_widget_show_all(print);
	} else if (view->view_type == E_ADDRESSBOOK_VIEW_TABLE) {
		GtkWidget *dialog;
		EPrintable *printable;
		ETable *etable;

		dialog = gnome_print_dialog_new("Print cards", GNOME_PRINT_DIALOG_RANGE | GNOME_PRINT_DIALOG_COPIES);
		gnome_print_dialog_construct_range_any(GNOME_PRINT_DIALOG(dialog), GNOME_PRINT_RANGE_ALL | GNOME_PRINT_RANGE_SELECTION,
						       NULL, NULL, NULL);

		gtk_object_get(GTK_OBJECT(view->widget), "table", &etable, NULL);
		printable = e_table_get_printable(etable);

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

void
e_addressbook_view_print_preview(EAddressbookView *view)
{
	if (view->view_type == E_ADDRESSBOOK_VIEW_MINICARD) {
		char *query;
		EBook *book;

		gtk_object_get (GTK_OBJECT(view->model),
				"query", &query,
				"book", &book,
				NULL);
		e_contact_print_preview(book, query);
		g_free(query);
	} else if (view->view_type == E_ADDRESSBOOK_VIEW_TABLE) {
		EPrintable *printable;
		ETable *etable;
		GnomePrintMaster *master;
		GnomePrintContext *pc;
		GtkWidget *preview;

		gtk_object_get(GTK_OBJECT(view->widget), "table", &etable, NULL);
		printable = e_table_get_printable(etable);

		master = gnome_print_master_new();
		gnome_print_master_set_copies (master, 1, FALSE);
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
		gtk_object_unref(GTK_OBJECT(printable));
	}
}

static void
card_deleted_cb (EBook* book, EBookStatus status, gpointer user_data)
{
	if (status != E_BOOK_STATUS_SUCCESS) {
		e_addressbook_error_dialog (_("Error removing card"), status);
	}
}

static void
do_remove (int i, gpointer user_data)
{
	EBook *book;
	ECard *card;
	EAddressbookView *view = user_data;

	gtk_object_get (GTK_OBJECT(view->model),
			"book", &book,
			NULL);

	card = e_addressbook_model_get_card (view->model, i);

	e_book_remove_card(book, card, card_deleted_cb, view);

	gtk_object_unref (GTK_OBJECT (card));
}

void
e_addressbook_view_delete_selection(EAddressbookView *view)
{
	ESelectionModel *model = get_selection_model (view);

	g_return_if_fail (model);

	e_selection_model_foreach (model,
				   do_remove,
				   view);
}

static void
invisible_destroyed (GtkWidget *invisible, EAddressbookView *view)
{
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
		g_list_foreach (view->clipboard_cards, (GFunc)gtk_object_unref, NULL);
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

		g_list_foreach (card_list, (GFunc)gtk_object_unref, NULL);
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
			gtk_object_ref (iterator->data);
	}
	list = g_list_reverse (list);
	return list;
}

void
e_addressbook_view_save_as (EAddressbookView *view)
{
	GList *list = get_selected_cards (view);
	if (list)
		e_contact_list_save_as (_("Save as VCard"), list);
	g_list_free (list);
}

void
e_addressbook_view_send (EAddressbookView *view)
{
	GList *list = get_selected_cards (view);
	if (list)
		e_card_list_send (list, E_CARD_DISPOSITION_AS_ATTACHMENT);
	g_list_free (list);
}

void
e_addressbook_view_send_to (EAddressbookView *view)
{
	GList *list = get_selected_cards (view);
	if (list)
		e_card_list_send (list, E_CARD_DISPOSITION_AS_TO);
	g_list_free (list);
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
	gtk_object_set(GTK_OBJECT(view),
		       "query", NULL,
		       NULL);
}

void
e_addressbook_view_stop(EAddressbookView *view)
{
	if (view)
		e_addressbook_model_stop (view->model);
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

