/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *
 * Author:
 *   Christopher James Lahey <clahey@helixcode.com>
 *
 * (C) 1999 Helix Code, Inc.
 */

#include <config.h>

#include "e-minicard-view-model.h"

#include <gal/util/e-i18n.h>
#include <gal/util/e-util.h>

#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-font.h>
#include <gal/widgets/e-popup-menu.h>
#include "e-contact-save-as.h"
#include "addressbook/printing/e-contact-print.h"
#include "addressbook/printing/e-contact-print-envelope.h"

#define PARENT_TYPE e_reflow_model_get_type()
EReflowModelClass *parent_class;

#define d(x)

/*
 * EMinicardViewModel callbacks
 * These are the callbacks that define the behavior of our custom model.
 */
static void e_minicard_view_model_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_minicard_view_model_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);


enum {
	ARG_0,
	ARG_BOOK,
	ARG_QUERY,
	ARG_EDITABLE,
};

enum {
	STATUS_MESSAGE,
	DRAG_BEGIN,
	LAST_SIGNAL
};

static guint e_minicard_view_model_signals [LAST_SIGNAL] = {0, };

static void
disconnect_signals(EMinicardViewModel *model)
{
	if (model->book_view && model->create_card_id)
		gtk_signal_disconnect(GTK_OBJECT (model->book_view),
				      model->create_card_id);
	if (model->book_view && model->remove_card_id)
		gtk_signal_disconnect(GTK_OBJECT (model->book_view),
				      model->remove_card_id);
	if (model->book_view && model->modify_card_id)
		gtk_signal_disconnect(GTK_OBJECT (model->book_view),
				      model->modify_card_id);
	if (model->book_view && model->status_message_id)
		gtk_signal_disconnect(GTK_OBJECT (model->book_view),
				      model->status_message_id);

	model->create_card_id = 0;
	model->remove_card_id = 0;
	model->modify_card_id = 0;
	model->status_message_id = 0;
}

static void
remove_book_view(EMinicardViewModel *model)
{
	disconnect_signals (model);
	if (model->book_view)
		gtk_object_unref(GTK_OBJECT(model->book_view));

	model->book_view = NULL;
}

static int
count_lines (const gchar *text)
{
	int num_lines = 1;
	gunichar unival;

	for (text = e_unicode_get_utf8 (text, &unival); (unival && text); text = e_unicode_get_utf8 (text, &unival)) {
		if (unival == '\n') {
			num_lines ++;
		}
	}

	return num_lines;
}

static int
text_height (GnomeCanvas *canvas, const gchar *text)
{
	EFont *font = e_font_from_gdk_font (((GtkWidget *) canvas)->style->font);
	gint height = e_font_height (font) * count_lines (text) / canvas->pixels_per_unit;

	e_font_unref (font);
	return height;
}

typedef struct {
	EMinicardViewModel *emvm;
	ESelectionModel *selection;
} ModelAndSelection;

static void
model_and_selection_free (ModelAndSelection *mns)
{
	gtk_object_unref(GTK_OBJECT(mns->emvm));
	gtk_object_unref(GTK_OBJECT(mns->selection));
	g_free(mns);
}

static void
add_to_list (int model_row, gpointer closure)
{
	GList **list = closure;
	*list = g_list_prepend (*list, GINT_TO_POINTER (model_row));
}

static GList *
get_card_list (ModelAndSelection *mns)
{
	GList *list;
	GList *iterator;

	list = NULL;
	e_selection_model_foreach (mns->selection, add_to_list, &list);

	for (iterator = list; iterator; iterator = iterator->next) {
		iterator->data = mns->emvm->data [GPOINTER_TO_INT (iterator->data)];
	}
	list = g_list_reverse (list);
	return list;
}

static void
save_as (GtkWidget *widget, ModelAndSelection *mns)
{
	GList *list;

	list = get_card_list (mns);
	if (list)
		e_contact_list_save_as (_("Save as VCard"), list);
	g_list_free (list);
	model_and_selection_free (mns);
}

static void
send_as (GtkWidget *widget, ModelAndSelection *mns)
{
	GList *list;

	list = get_card_list (mns);
	if (list)
		e_card_list_send (list, E_CARD_DISPOSITION_AS_ATTACHMENT);
	g_list_free (list);
	model_and_selection_free (mns);
}

static void
send_to (GtkWidget *widget, ModelAndSelection *mns)
{
	GList *list;

	list = get_card_list (mns);
	if (list)
		e_card_list_send (list, E_CARD_DISPOSITION_AS_TO);
	g_list_free (list);
	model_and_selection_free (mns);
}

static void
print (GtkWidget *widget, ModelAndSelection *mns)
{
	GList *list;

	list = get_card_list (mns);
	if (list)
		gtk_widget_show (e_contact_print_card_list_dialog_new (list));
	g_list_free (list);
	model_and_selection_free (mns);
}

static void
print_envelope (GtkWidget *widget, ModelAndSelection *mns)
{
	GList *list;

	list = get_card_list (mns);
	if (list)
		gtk_widget_show (e_contact_print_envelope_list_dialog_new (list));
	g_list_free (list);
	model_and_selection_free (mns);
}

static void
card_changed_cb (EBook* book, EBookStatus status, gpointer user_data)
{
	d(g_print ("%s: %s(): a card was changed with status %d\n", __FILE__, __FUNCTION__, status));
}

static void
delete (GtkWidget *widget, ModelAndSelection *mns)
{
	GList *list;

	list = get_card_list (mns);
	if (list) {

		if (e_contact_editor_confirm_delete(NULL)) { /*FIXME: Give a GtkWindow here. */
			GList *iterator;
			EBook *book;

			book = mns->emvm->book;

			for (iterator = list; iterator; iterator = iterator->next) {
				ECard *card = iterator->data;

				gtk_object_ref(GTK_OBJECT(card));

				e_book_remove_card (book,
						    card,
						    card_changed_cb,
						    NULL);

				gtk_object_unref(GTK_OBJECT(card));
			}
		}
	}

	g_list_free (list);
	model_and_selection_free (mns);
}

gint
e_minicard_view_model_right_click (EMinicardViewModel *emvm, GdkEvent *event, ESelectionModel *selection)
{
	ModelAndSelection *mns = g_new(ModelAndSelection, 1);
	EPopupMenu menu[] = { {N_("Save as VCard"), NULL, GTK_SIGNAL_FUNC(save_as), NULL, 0},
			      {N_("Send contact to other"), NULL, GTK_SIGNAL_FUNC(send_as), NULL, 0},
			      {N_("Send message to contact"), NULL, GTK_SIGNAL_FUNC(send_to), NULL, 0},
			      {N_("Print"), NULL, GTK_SIGNAL_FUNC(print), NULL, 0},
			      {N_("Print Envelope"), NULL, GTK_SIGNAL_FUNC(print_envelope), NULL, 0},
			      {N_("Delete"), NULL, GTK_SIGNAL_FUNC(delete), NULL, 0},
			      {NULL, NULL, NULL, 0}};

	mns->emvm = emvm;
	mns->selection = selection;
	gtk_object_ref(GTK_OBJECT(mns->emvm));
	gtk_object_ref(GTK_OBJECT(mns->selection));
	e_popup_menu_run (menu, event, 0, 0, mns);
	return TRUE;
}

static void
addressbook_destroy(GtkObject *object)
{
	EMinicardViewModel *model = E_MINICARD_VIEW_MODEL(object);
	int i;

	if (model->get_view_idle)
		g_source_remove(model->get_view_idle);

	remove_book_view (model);

	g_free(model->query);
	if (model->book)
		gtk_object_unref(GTK_OBJECT(model->book));

	for ( i = 0; i < model->data_count; i++ ) {
		gtk_object_unref(GTK_OBJECT(model->data[i]));
	}
	g_free(model->data);
}

static void
addressbook_set_width (EReflowModel *erm, int width)
{
}

/* This function returns the number of items in our EReflowModel. */
static int
addressbook_count (EReflowModel *erm)
{
	EMinicardViewModel *addressbook = E_MINICARD_VIEW_MODEL(erm);
	return addressbook->data_count;
}

/* This function returns the number of items in our EReflowModel. */
static int
addressbook_height (EReflowModel *erm, int i, GnomeCanvasGroup *parent)
{
	/* FIXME */
	ECardSimpleField field;
	int count = 0;
	int height;
	char *string;
	EMinicardViewModel *emvm = E_MINICARD_VIEW_MODEL(erm);
	ECardSimple *simple = e_card_simple_new (emvm->data[i]);

	string = e_card_simple_get(simple, E_CARD_SIMPLE_FIELD_FILE_AS);
	height = text_height (GNOME_CANVAS_ITEM (parent)->canvas, string ? string : "") + 10.0;
	g_free(string);

	for(field = E_CARD_SIMPLE_FIELD_FULL_NAME; field != E_CARD_SIMPLE_FIELD_LAST - 2 && count < 5; field++) {
		if (field == E_CARD_SIMPLE_FIELD_FAMILY_NAME)
			field ++;
		string = e_card_simple_get(simple, field);
		if (string && *string) {
			int this_height;
			int field_text_height;

			this_height = text_height (GNOME_CANVAS_ITEM (parent)->canvas, e_card_simple_get_name(simple, field));

			field_text_height = text_height (GNOME_CANVAS_ITEM (parent)->canvas, string);
			if (this_height < field_text_height)
				this_height = field_text_height;

			this_height += 3;

			height += this_height;
			count ++;
		}
		g_free (string);
	}
	height += 2;

	gtk_object_unref (GTK_OBJECT (simple));

	return height;
}

static int
addressbook_compare (EReflowModel *erm, int n1, int n2)
{
	ECard *card1, *card2;
	EMinicardViewModel *emvm = E_MINICARD_VIEW_MODEL(erm);
	
	card1 = emvm->data[n1];
	card2 = emvm->data[n2];

	if (card1 && card2) {
		char *file_as1, *file_as2;
		file_as1 = card1->file_as;
		file_as2 = card2->file_as;
		if (file_as1 && file_as2)
			return strcasecmp(file_as1, file_as2);
		if (file_as1)
			return -1;
		if (file_as2)
			return 1;
		return strcmp(e_card_get_id(card1), e_card_get_id(card2));
	}
	if (card1)
		return -1;
	if (card2)
		return 1;
	return 0;
}

static int
minicard_drag_begin (EMinicard *card, GdkEvent *event, EMinicardViewModel *model)
{
	gint ret_val = 0;

	gtk_signal_emit (GTK_OBJECT(model),
			 e_minicard_view_model_signals[DRAG_BEGIN],
			 event, &ret_val);

	return ret_val;
}

static GnomeCanvasItem *
addressbook_incarnate (EReflowModel *erm, int i, GnomeCanvasGroup *parent)
{
	EMinicardViewModel *emvm = E_MINICARD_VIEW_MODEL (erm);
	GnomeCanvasItem *item;

	item = gnome_canvas_item_new(parent,
				     e_minicard_get_type(),
				     "card", emvm->data[i],
				     "editable", emvm->editable,
				     NULL);

#if 0
	gtk_signal_connect (GTK_OBJECT (item), "selected",
			    GTK_SIGNAL_FUNC(card_selected), emvm);
#endif
	gtk_signal_connect (GTK_OBJECT (item), "drag_begin",
			    GTK_SIGNAL_FUNC(minicard_drag_begin), emvm);
	return item;
}

static void
addressbook_reincarnate (EReflowModel *erm, int i, GnomeCanvasItem *item)
{
	EMinicardViewModel *emvm = E_MINICARD_VIEW_MODEL (erm);

	gnome_canvas_item_set(item,
			      "card", emvm->data[i],
			      NULL);
}



static void
create_card(EBookView *book_view,
	    const GList *cards,
	    EMinicardViewModel *model)
{
	int old_count = model->data_count;
	int length = g_list_length ((GList *)cards);

	if (model->data_count + length > model->allocated_count) {
		while (model->data_count + length > model->allocated_count)
			model->allocated_count += 256;
		model->data = g_renew(ECard *, model->data, model->allocated_count);
	}

	for ( ; cards; cards = cards->next) {
		model->data[model->data_count++] = cards->data;
		gtk_object_ref (cards->data);
	}
	e_reflow_model_items_inserted (E_REFLOW_MODEL (model),
				       old_count,
				       model->data_count - old_count);
}

static void
remove_card(EBookView *book_view,
	    const char *id,
	    EMinicardViewModel *model)
{
	int i;
	gboolean found = FALSE;
	for ( i = 0; i < model->data_count; i++) {
		if (!strcmp(e_card_get_id(model->data[i]), id) ) {
			gtk_object_unref(GTK_OBJECT(model->data[i]));
			memmove(model->data + i, model->data + i + 1, (model->data_count - i - 1) * sizeof (ECard *));
			model->data_count --;
			found = TRUE;
		}
	}
	if (found)
		e_reflow_model_changed (E_REFLOW_MODEL (model));
}

static void
modify_card(EBookView *book_view,
	    const GList *cards,
	    EMinicardViewModel *model)
{
	for ( ; cards; cards = cards->next) {
		int i;
		for ( i = 0; i < model->data_count; i++) {
			if ( !strcmp(e_card_get_id(model->data[i]), e_card_get_id(cards->data)) ) {
				gtk_object_unref (GTK_OBJECT (model->data[i]));
				model->data[i] = cards->data;
				gtk_object_ref (GTK_OBJECT (model->data[i]));
				e_reflow_model_item_changed (E_REFLOW_MODEL (model), i);
				break;
			}
		}
	}
}

static void
status_message (EBookView *book_view,
		char* status,
		EMinicardViewModel *model)
{
	gtk_signal_emit (GTK_OBJECT (model),
			 e_minicard_view_model_signals [STATUS_MESSAGE],
			 status);
}

static void
e_minicard_view_model_class_init (GtkObjectClass *object_class)
{
	EReflowModelClass *model_class = (EReflowModelClass *) object_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = addressbook_destroy;
	object_class->set_arg   = e_minicard_view_model_set_arg;
	object_class->get_arg   = e_minicard_view_model_get_arg;

	gtk_object_add_arg_type ("EMinicardViewModel::book", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_BOOK);
	gtk_object_add_arg_type ("EMinicardViewModel::query", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_QUERY);
	gtk_object_add_arg_type ("EMinicardViewModel::editable", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_EDITABLE);

	e_minicard_view_model_signals [STATUS_MESSAGE] =
		gtk_signal_new ("status_message",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMinicardViewModelClass, status_message),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	e_minicard_view_model_signals [DRAG_BEGIN] =
		gtk_signal_new ("drag_begin",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMinicardViewModelClass, drag_begin),
				gtk_marshal_INT__POINTER,
				GTK_TYPE_INT, 2, GTK_TYPE_POINTER, GTK_TYPE_POINTER);


	gtk_object_class_add_signals (object_class, e_minicard_view_model_signals, LAST_SIGNAL);
	
	model_class->set_width = addressbook_set_width;
	model_class->count = addressbook_count;
	model_class->height = addressbook_height;
	model_class->compare = addressbook_compare;
	model_class->incarnate = addressbook_incarnate;
	model_class->reincarnate = addressbook_reincarnate;
}

static void
e_minicard_view_model_init (GtkObject *object)
{
	EMinicardViewModel *model = E_MINICARD_VIEW_MODEL(object);
	model->book = NULL;
	model->query = g_strdup("(contains \"x-evolution-any-field\" \"\")");
	model->book_view = NULL;
	model->get_view_idle = 0;
	model->create_card_id = 0;
	model->remove_card_id = 0;
	model->modify_card_id = 0;
	model->status_message_id = 0;
	model->data = NULL;
	model->data_count = 0;
	model->allocated_count = 0;
	model->editable = FALSE;
	model->first_get_view = TRUE;
}

static void
book_view_loaded (EBook *book, EBookStatus status, EBookView *book_view, gpointer closure)
{
	EMinicardViewModel *model = closure;
	int i;
	remove_book_view(model);
	model->book_view = book_view;
	if (model->book_view)
		gtk_object_ref(GTK_OBJECT(model->book_view));
	model->create_card_id = gtk_signal_connect(GTK_OBJECT(model->book_view),
						  "card_added",
						  GTK_SIGNAL_FUNC(create_card),
						  model);
	model->remove_card_id = gtk_signal_connect(GTK_OBJECT(model->book_view),
						  "card_removed",
						  GTK_SIGNAL_FUNC(remove_card),
						  model);
	model->modify_card_id = gtk_signal_connect(GTK_OBJECT(model->book_view),
						  "card_changed",
						  GTK_SIGNAL_FUNC(modify_card),
						  model);
	model->status_message_id = gtk_signal_connect(GTK_OBJECT(model->book_view),
						      "status_message",
						      GTK_SIGNAL_FUNC(status_message),
						      model);

	for ( i = 0; i < model->data_count; i++ ) {
		gtk_object_unref(GTK_OBJECT(model->data[i]));
	}

	g_free(model->data);
	model->data = NULL;
	model->data_count = 0;
	model->allocated_count = 0;
	e_reflow_model_changed(E_REFLOW_MODEL(model));
}

static gboolean
get_view (EMinicardViewModel *model)
{
	if (model->book && model->query) {
		if (model->first_get_view) {
			char *capabilities;
			capabilities = e_book_get_static_capabilities (model->book);
			if (capabilities && strstr (capabilities, "local")) {
				e_book_get_book_view (model->book, model->query, book_view_loaded, model);
			}
			model->first_get_view = FALSE;
		}
		else
			e_book_get_book_view (model->book, model->query, book_view_loaded, model);
	}

	model->get_view_idle = 0;
	return FALSE;
}

ECard *
e_minicard_view_model_get_card(EMinicardViewModel *model,
			       int                row)
{
	if (model->data && row < model->data_count) {
		gtk_object_ref(GTK_OBJECT(model->data[row]));
		return model->data[row];
	}
	return NULL;
}

static void
e_minicard_view_model_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EMinicardViewModel *model;

	model = E_MINICARD_VIEW_MODEL (o);
	
	switch (arg_id){
	case ARG_BOOK:
		if (model->book)
			gtk_object_unref(GTK_OBJECT(model->book));
		model->book = E_BOOK(GTK_VALUE_OBJECT (*arg));
		if (model->book) {
			gtk_object_ref(GTK_OBJECT(model->book));
			if (model->get_view_idle == 0)
				model->get_view_idle = g_idle_add((GSourceFunc)get_view, model);
		}
		break;
	case ARG_QUERY:
		if (model->query)
			g_free(model->query);
		model->query = g_strdup(GTK_VALUE_STRING (*arg));
		if (model->get_view_idle == 0)
			model->get_view_idle = g_idle_add((GSourceFunc)get_view, model);
		break;
	case ARG_EDITABLE:
		model->editable = GTK_VALUE_BOOL (*arg);
		break;
	}
}

static void
e_minicard_view_model_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EMinicardViewModel *e_minicard_view_model;

	e_minicard_view_model = E_MINICARD_VIEW_MODEL (object);

	switch (arg_id) {
	case ARG_BOOK:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(e_minicard_view_model->book);
		break;
	case ARG_QUERY:
		GTK_VALUE_STRING (*arg) = g_strdup(e_minicard_view_model->query);
		break;
	case ARG_EDITABLE:
		GTK_VALUE_BOOL (*arg) = e_minicard_view_model->editable;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

GtkType
e_minicard_view_model_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"EMinicardViewModel",
			sizeof (EMinicardViewModel),
			sizeof (EMinicardViewModelClass),
			(GtkClassInitFunc) e_minicard_view_model_class_init,
			(GtkObjectInitFunc) e_minicard_view_model_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

EReflowModel *
e_minicard_view_model_new (void)
{
	EMinicardViewModel *et;

	et = gtk_type_new (e_minicard_view_model_get_type ());
	
	return E_REFLOW_MODEL(et);
}

void   e_minicard_view_model_stop    (EMinicardViewModel *model)
{
	remove_book_view(model);
}
