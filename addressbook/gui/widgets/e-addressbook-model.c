/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *
 * Author:
 *   Christopher James Lahey <clahey@helixcode.com>
 *
 * (C) 1999 Helix Code, Inc.
 */

#include <config.h>
#include "e-addressbook-model.h"
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <gnome.h>

#define PARENT_TYPE gtk_object_get_type()
GtkObjectClass *parent_class;

/*
 * EAddressbookModel callbacks
 * These are the callbacks that define the behavior of our custom model.
 */
static void e_addressbook_model_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_addressbook_model_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);


enum {
	ARG_0,
	ARG_BOOK,
	ARG_QUERY,
	ARG_EDITABLE,
};

enum {
	WRITABLE_STATUS,
	STATUS_MESSAGE,
	CARD_ADDED,
	CARD_REMOVED,
	CARD_CHANGED,
	MODEL_CHANGED,
	STOP_STATE_CHANGED,
	LAST_SIGNAL
};

#define COLS (E_CARD_SIMPLE_FIELD_LAST)

static guint e_addressbook_model_signals [LAST_SIGNAL] = {0, };

static void
remove_book_view(EAddressbookModel *model)
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
	if (model->book_view && model->sequence_complete_id)
		gtk_signal_disconnect(GTK_OBJECT (model->book_view),
				      model->sequence_complete_id);

	model->create_card_id = 0;
	model->remove_card_id = 0;
	model->modify_card_id = 0;
	model->status_message_id = 0;
	model->sequence_complete_id = 0;

	model->search_in_progress = FALSE;

	if (model->book_view)
		gtk_object_unref(GTK_OBJECT(model->book_view));

	model->book_view = NULL;
}

static void
addressbook_destroy(GtkObject *object)
{
	EAddressbookModel *model = E_ADDRESSBOOK_MODEL(object);
	int i;

	if (model->get_view_idle)
		g_source_remove(model->get_view_idle);

	remove_book_view(model);

	if (model->book) {
		if (model->writable_status_id)
			gtk_signal_disconnect(GTK_OBJECT (model->book),
					      model->writable_status_id);

		model->writable_status_id = 0;

		gtk_object_unref(GTK_OBJECT(model->book));
	}

	for ( i = 0; i < model->data_count; i++ ) {
		gtk_object_unref(GTK_OBJECT(model->data[i]));
	}
	g_free(model->data);
}

static void
create_card(EBookView *book_view,
	    const GList *cards,
	    EAddressbookModel *model)
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

	gtk_signal_emit (GTK_OBJECT (model),
			 e_addressbook_model_signals [CARD_ADDED],
			 old_count, model->data_count - old_count);
}

static void
remove_card(EBookView *book_view,
	    const char *id,
	    EAddressbookModel *model)
{
	int i;

	for ( i = 0; i < model->data_count; i++) {
		if ( !strcmp(e_card_get_id(model->data[i]), id) ) {
			gtk_object_unref(GTK_OBJECT(model->data[i]));
			memmove(model->data + i, model->data + i + 1, (model->data_count - i - 1) * sizeof (ECard *));
			model->data_count--;

			gtk_signal_emit (GTK_OBJECT (model),
					 e_addressbook_model_signals [CARD_REMOVED],
					 i);
			break;
		}
	}
}

static void
modify_card(EBookView *book_view,
	    const GList *cards,
	    EAddressbookModel *model)
{
	for ( ; cards; cards = cards->next) {
		int i;
		for ( i = 0; i < model->data_count; i++) {
			if ( !strcmp(e_card_get_id(model->data[i]), e_card_get_id(E_CARD(cards->data))) ) {
				gtk_object_unref(GTK_OBJECT(model->data[i]));
				model->data[i] = e_card_duplicate(E_CARD(cards->data));
				gtk_object_ref(GTK_OBJECT(model->data[i]));
				gtk_signal_emit (GTK_OBJECT (model),
						 e_addressbook_model_signals [CARD_CHANGED],
						 i);
				break;
			}
		}
	}
}

static void
status_message (EBookView *book_view,
		char* status,
		EAddressbookModel *model)
{
	gtk_signal_emit (GTK_OBJECT (model),
			 e_addressbook_model_signals [STATUS_MESSAGE],
			 status);
}

static void
sequence_complete (EBookView *book_view,
		   EAddressbookModel *model)
{
	model->search_in_progress = FALSE;
	gtk_signal_emit (GTK_OBJECT (model),
			 e_addressbook_model_signals [STOP_STATE_CHANGED]);
}

static void
writable_status (EBook *book,
		 gboolean writable,
		 EAddressbookModel *model)
{
	if (!model->editable_set) {
		model->editable = writable;

		gtk_signal_emit (GTK_OBJECT (model),
				 e_addressbook_model_signals [WRITABLE_STATUS],
				 writable);
	}
}

static void
e_addressbook_model_class_init (GtkObjectClass *object_class)
{
	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = addressbook_destroy;
	object_class->set_arg   = e_addressbook_model_set_arg;
	object_class->get_arg   = e_addressbook_model_get_arg;

	gtk_object_add_arg_type ("EAddressbookModel::book", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_BOOK);
	gtk_object_add_arg_type ("EAddressbookModel::query", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_QUERY);
	gtk_object_add_arg_type ("EAddressbookModel::editable", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_EDITABLE);

	e_addressbook_model_signals [WRITABLE_STATUS] =
		gtk_signal_new ("writable_status",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EAddressbookModelClass, writable_status),
				gtk_marshal_NONE__BOOL,
				GTK_TYPE_NONE, 1, GTK_TYPE_BOOL);

	e_addressbook_model_signals [STATUS_MESSAGE] =
		gtk_signal_new ("status_message",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EAddressbookModelClass, status_message),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	e_addressbook_model_signals [CARD_ADDED] =
		gtk_signal_new ("card_added",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EAddressbookModelClass, card_added),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);

	e_addressbook_model_signals [CARD_REMOVED] =
		gtk_signal_new ("card_removed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EAddressbookModelClass, card_removed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	e_addressbook_model_signals [CARD_CHANGED] =
		gtk_signal_new ("card_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EAddressbookModelClass, card_changed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	e_addressbook_model_signals [MODEL_CHANGED] =
		gtk_signal_new ("model_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EAddressbookModelClass, model_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_addressbook_model_signals [STOP_STATE_CHANGED] =
		gtk_signal_new ("stop_state_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EAddressbookModelClass, stop_state_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, e_addressbook_model_signals, LAST_SIGNAL);
}

static void
e_addressbook_model_init (GtkObject *object)
{
	EAddressbookModel *model = E_ADDRESSBOOK_MODEL(object);
	model->book = NULL;
	model->query = g_strdup("(contains \"x-evolution-any-field\" \"\")");
	model->book_view = NULL;
	model->get_view_idle = 0;
	model->create_card_id = 0;
	model->remove_card_id = 0;
	model->modify_card_id = 0;
	model->status_message_id = 0;
	model->writable_status_id = 0;
	model->sequence_complete_id = 0;
	model->data = NULL;
	model->data_count = 0;
	model->allocated_count = 0;
	model->search_in_progress = FALSE;
	model->editable = FALSE;
	model->editable_set = FALSE;
	model->first_get_view = TRUE;
}

static void
book_view_loaded (EBook *book, EBookStatus status, EBookView *book_view, gpointer closure)
{
	EAddressbookModel *model = closure;
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
	model->sequence_complete_id = gtk_signal_connect(GTK_OBJECT(model->book_view),
							 "sequence_complete",
							 GTK_SIGNAL_FUNC(sequence_complete),
							 model);

	for ( i = 0; i < model->data_count; i++ ) {
		gtk_object_unref(GTK_OBJECT(model->data[i]));
	}

	g_free(model->data);
	model->data = NULL;
	model->data_count = 0;
	model->allocated_count = 0;
	model->search_in_progress = TRUE;
	gtk_signal_emit (GTK_OBJECT (model),
			 e_addressbook_model_signals [MODEL_CHANGED]);
	gtk_signal_emit (GTK_OBJECT (model),
			 e_addressbook_model_signals [STOP_STATE_CHANGED]);
}

static gboolean
get_view (EAddressbookModel *model)
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
e_addressbook_model_get_card(EAddressbookModel *model,
			     int                row)
{
	if (model->data && row < model->data_count) {
		ECard *card;
		card = e_card_duplicate (model->data[row]);
		gtk_object_ref (GTK_OBJECT (card));
		return card;
	}
	return NULL;
}

static void
e_addressbook_model_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EAddressbookModel *model;

	model = E_ADDRESSBOOK_MODEL (o);
	
	switch (arg_id){
	case ARG_BOOK:
		if (model->book) {
			if (model->writable_status_id)
				gtk_signal_disconnect(GTK_OBJECT (model->book),
						      model->writable_status_id);

			model->writable_status_id = 0;

			gtk_object_unref(GTK_OBJECT(model->book));
		}
		model->book = E_BOOK(GTK_VALUE_OBJECT (*arg));
		if (model->book) {
			gtk_object_ref(GTK_OBJECT(model->book));
			if (model->get_view_idle == 0)
				model->get_view_idle = g_idle_add((GSourceFunc)get_view, model);
			gtk_signal_connect (GTK_OBJECT(model->book),
					    "writable_status",
					    writable_status, model);
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
		model->editable_set = TRUE;
		break;
	}
}

static void
e_addressbook_model_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EAddressbookModel *e_addressbook_model;

	e_addressbook_model = E_ADDRESSBOOK_MODEL (object);

	switch (arg_id) {
	case ARG_BOOK:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(e_addressbook_model->book);
		break;
	case ARG_QUERY:
		GTK_VALUE_STRING (*arg) = g_strdup(e_addressbook_model->query);
		break;
	case ARG_EDITABLE:
		GTK_VALUE_BOOL (*arg) = e_addressbook_model->editable;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

GtkType
e_addressbook_model_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"EAddressbookModel",
			sizeof (EAddressbookModel),
			sizeof (EAddressbookModelClass),
			(GtkClassInitFunc) e_addressbook_model_class_init,
			(GtkObjectInitFunc) e_addressbook_model_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

EAddressbookModel*
e_addressbook_model_new (void)
{
	EAddressbookModel *et;

	et = gtk_type_new (e_addressbook_model_get_type ());
	
	return et;
}

void   e_addressbook_model_stop    (EAddressbookModel *model)
{
	remove_book_view(model);
	model->search_in_progress = FALSE;
	gtk_signal_emit (GTK_OBJECT (model),
			 e_addressbook_model_signals [STOP_STATE_CHANGED]);
	gtk_signal_emit (GTK_OBJECT (model),
			 e_addressbook_model_signals [STATUS_MESSAGE],
			 "Search Interrupted.");
}

gboolean
e_addressbook_model_can_stop (EAddressbookModel *model)
{
	return model->search_in_progress;
}

int
e_addressbook_model_card_count (EAddressbookModel *model)
{
	return model->data_count;
}

ECard *
e_addressbook_model_card_at (EAddressbookModel *model, int index)
{
	return model->data[index];
}

gboolean
e_addressbook_model_editable (EAddressbookModel *model)
{
	return model->editable;
}

EBook *
e_addressbook_model_get_ebook (EAddressbookModel *model)
{
	return model->book;
}
