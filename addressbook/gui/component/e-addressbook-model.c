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

#define PARENT_TYPE e_table_model_get_type()
ETableModelClass *parent_class;

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

static void
addressbook_destroy(GtkObject *object)
{
	EAddressbookModel *model = E_ADDRESSBOOK_MODEL(object);
	int i;

	if (model->get_view_idle)
		g_source_remove(model->get_view_idle);
	if (model->book_view && model->create_card_id)
		gtk_signal_disconnect(GTK_OBJECT (model->book_view),
				      model->create_card_id);
	if (model->book_view && model->remove_card_id)
		gtk_signal_disconnect(GTK_OBJECT (model->book_view),
				      model->remove_card_id);
	if (model->book_view && model->modify_card_id)
		gtk_signal_disconnect(GTK_OBJECT (model->book_view),
				      model->modify_card_id);
	if (model->book)
		gtk_object_unref(GTK_OBJECT(model->book));
	if (model->book_view)
		gtk_object_unref(GTK_OBJECT(model->book_view));

	for ( i = 0; i < model->data_count; i++ ) {
		gtk_object_unref(GTK_OBJECT(model->data[i]));
	}
	g_free(model->data);
}

/* This function returns the number of columns in our ETableModel. */
static int
addressbook_col_count (ETableModel *etc)
{
	return E_CARD_SIMPLE_FIELD_LAST;
}

/* This function returns the number of rows in our ETableModel. */
static int
addressbook_row_count (ETableModel *etc)
{
	EAddressbookModel *addressbook = E_ADDRESSBOOK_MODEL(etc);
	return addressbook->data_count;
}

/* This function returns the value at a particular point in our ETableModel. */
static void *
addressbook_value_at (ETableModel *etc, int col, int row)
{
	EAddressbookModel *addressbook = E_ADDRESSBOOK_MODEL(etc);
	const char *value;
	if ( col >= E_CARD_SIMPLE_FIELD_LAST - 1 || row >= addressbook->data_count )
		return NULL;

	value = e_card_simple_get_const(addressbook->data[row], 
					col + 1);
	return (void *)(value ? value : "");
}

/* This function sets the value at a particular point in our ETableModel. */
static void
addressbook_set_value_at (ETableModel *etc, int col, int row, const void *val)
{
	EAddressbookModel *addressbook = E_ADDRESSBOOK_MODEL(etc);
	ECard *card;
	if (addressbook->editable) {
		if ( col >= E_CARD_SIMPLE_FIELD_LAST - 1|| row >= addressbook->data_count )
			return;
		e_card_simple_set(addressbook->data[row],
				  col + 1,
				  val);
		gtk_object_get(GTK_OBJECT(addressbook->data[row]),
			       "card", &card,
			       NULL);
		e_book_commit_card(addressbook->book, card, NULL, NULL);
		
		e_table_model_cell_changed(etc, col, row);
	}
}

/* This function returns whether a particular cell is editable. */
static gboolean
addressbook_is_cell_editable (ETableModel *etc, int col, int row)
{
	return E_ADDRESSBOOK_MODEL(etc)->editable;
}

static void
addressbook_append_row (ETableModel *etm, ETableModel *source, gint row)
{
	ECard *card;
	ECardSimple *simple;
	EAddressbookModel *addressbook = E_ADDRESSBOOK_MODEL(etm);
	int col;

	card = e_card_new("");
	simple = e_card_simple_new(card);

	for (col = 0; col < E_CARD_SIMPLE_FIELD_LAST - 1; col++) {
		const void *val = e_table_model_value_at(source, col, row);
		e_card_simple_set(simple,
				  col + 1,
				  val);
	}
	e_card_simple_sync_card(simple);
	e_book_add_card(addressbook->book, card, NULL, NULL);
	gtk_object_unref(GTK_OBJECT(simple));
	gtk_object_unref(GTK_OBJECT(card));
}

/* This function duplicates the value passed to it. */
static void *
addressbook_duplicate_value (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
}

/* This function frees the value passed to it. */
static void
addressbook_free_value (ETableModel *etc, int col, void *value)
{
	g_free(value);
}

static void *
addressbook_initialize_value (ETableModel *etc, int col)
{
	return g_strdup("");
}

static gboolean
addressbook_value_is_empty (ETableModel *etc, int col, const void *value)
{
	return !(value && *(char *)value);
}

static char *
addressbook_value_to_string (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
}

static void
create_card(EBookView *book_view,
	    const GList *cards,
	    EAddressbookModel *model)
{
	model->data = g_realloc(model->data, (model->data_count + g_list_length((GList *)cards)) * sizeof(ECard *));
	for ( ; cards; cards = cards->next) {
		model->data[model->data_count++] = e_card_simple_new (E_CARD(cards->data));
		e_table_model_row_inserted(E_TABLE_MODEL(model), model->data_count - 1);
	}
}

static void
remove_card(EBookView *book_view,
	    const char *id,
	    EAddressbookModel *model)
{
	int i;
	for ( i = 0; i < model->data_count; i++) {
		if ( !strcmp(e_card_simple_get_id(model->data[i]), id) ) {
			gtk_object_unref(GTK_OBJECT(model->data[i]));
			memmove(model->data + i, model->data + i + 1, (model->data_count - i - 1) * sizeof (ECard *));
			e_table_model_row_deleted(E_TABLE_MODEL(model), i);
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
			if ( !strcmp(e_card_simple_get_id(model->data[i]), e_card_get_id(E_CARD(cards->data))) ) {
				gtk_object_unref(GTK_OBJECT(model->data[i]));
				model->data[i] = e_card_simple_new(E_CARD(cards->data));
				gtk_object_ref(GTK_OBJECT(model->data[i]));
				e_table_model_row_changed(E_TABLE_MODEL(model), i);
				break;
			}
		}
	}
}

static void
e_addressbook_model_class_init (GtkObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;

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
	
	model_class->column_count = addressbook_col_count;
	model_class->row_count = addressbook_row_count;
	model_class->value_at = addressbook_value_at;
	model_class->set_value_at = addressbook_set_value_at;
	model_class->is_cell_editable = addressbook_is_cell_editable;
	model_class->append_row = addressbook_append_row;
	model_class->duplicate_value = addressbook_duplicate_value;
	model_class->free_value = addressbook_free_value;
	model_class->initialize_value = addressbook_initialize_value;
	model_class->value_is_empty = addressbook_value_is_empty;
	model_class->value_to_string = addressbook_value_to_string;
}

static void
e_addressbook_model_init (GtkObject *object)
{
	EAddressbookModel *model = E_ADDRESSBOOK_MODEL(object);
	model->book = NULL;
	model->query = g_strdup("(contains \"full_name\" \"\")");
	model->book_view = NULL;
	model->get_view_idle = 0;
	model->create_card_id = 0;
	model->remove_card_id = 0;
	model->modify_card_id = 0;
	model->data = NULL;
	model->data_count = 0;
	model->editable = TRUE;
}

static void
book_view_loaded (EBook *book, EBookStatus status, EBookView *book_view, gpointer closure)
{
	EAddressbookModel *model = closure;
	int i;
	if (model->book_view && model->create_card_id)
		gtk_signal_disconnect(GTK_OBJECT (model->book_view),
				      model->create_card_id);
	if (model->book_view && model->remove_card_id)
		gtk_signal_disconnect(GTK_OBJECT (model->book_view),
				      model->remove_card_id);
	if (model->book_view && model->modify_card_id)
		gtk_signal_disconnect(GTK_OBJECT (model->book_view),
				      model->modify_card_id);
	if (model->book_view)
		gtk_object_unref(GTK_OBJECT(model->book_view));
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

	for ( i = 0; i < model->data_count; i++ ) {
		gtk_object_unref(GTK_OBJECT(model->data[i]));
	}
	g_free(model->data);
	model->data = NULL;
	model->data_count = 0;
	e_table_model_changed(E_TABLE_MODEL(model));
}

static gboolean
get_view(EAddressbookModel *model)
{
	if (model->book && model->query)
		e_book_get_book_view(model->book, model->query, book_view_loaded, model);

	model->get_view_idle = 0;
	return FALSE;
}

ECard *
e_addressbook_model_get_card(EAddressbookModel *model,
			     int                row)
{
	if (model->data && row < model->data_count) {
		ECard *card;
		gtk_object_get(GTK_OBJECT(model->data[row]),
			       "card", &card,
			       NULL);
		gtk_object_ref(GTK_OBJECT(card));
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

ETableModel *
e_addressbook_model_new (void)
{
	EAddressbookModel *et;

	et = gtk_type_new (e_addressbook_model_get_type ());
	
	return E_TABLE_MODEL(et);
}
