/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc.
 */

#include <config.h>
#include "e-test-model.h"
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <gnome.h>

#define PARENT_TYPE e_table_model_get_type()
/*
 * ETestModel callbacks
n * These are the callbacks that define the behavior of our custom model.
 */

static void
test_destroy(GtkObject *object)
{
	ETestModel *model = E_TEST_MODEL(object);
	int i;
	if (model->book)
		gtk_object_unref(GTK_OBJECT(model->book));
	if (model->book_view)
		gtk_object_unref(GTK_OBJECT(model->book_view));
	for ( i = 0; i < model->data_count; i++ ) {
		gtk_object_unref(GTK_OBJECT(model->data[i]));
	}
	g_free(model->data);
	g_free(model->uri);
}

/* This function returns the number of columns in our ETableModel. */
static int
test_col_count (ETableModel *etc)
{
	return LAST_COL;
}

/* This function returns the number of rows in our ETableModel. */
static int
test_row_count (ETableModel *etc)
{
	ETestModel *test = E_TEST_MODEL(etc);
	return test->data_count;
}

/* This function returns the value at a particular point in our ETableModel. */
static void *
test_value_at (ETableModel *etc, int col, int row)
{
	ETestModel *test = E_TEST_MODEL(etc);
	ECardList *list;
	ECardIterator *iterator;
	gchar *string;
	if ( col >= LAST_COL || row >= test->data_count )
		return NULL;
	switch (col) {
	case EMAIL:
		gtk_object_get(GTK_OBJECT(test->data[row]),
			       "email", &list,
			       NULL);
		iterator = e_card_list_get_iterator(list);
		if (e_card_iterator_get(iterator))
			return (void *) e_card_iterator_get(iterator);
		else
			return "";
		gtk_object_unref(GTK_OBJECT(iterator));
		break;
	case FULL_NAME:
		gtk_object_get(GTK_OBJECT(test->data[row]),
			       "full_name", &string,
			       NULL);
		if (string)
			return string;
		else
			return "";
		break;
	case STREET:
		gtk_object_get(GTK_OBJECT(test->data[row]),
			       "street", &list,
			       NULL);
		iterator = e_card_list_get_iterator(list);
		if (e_card_iterator_get(iterator))
			return ((ECardDeliveryAddress *)e_card_iterator_get(iterator))->street;
		else
			return "";
		gtk_object_unref(GTK_OBJECT(iterator));
		break;
	case PHONE:
		gtk_object_get(GTK_OBJECT(test->data[row]),
			       "phone", &list,
			       NULL);
		iterator = e_card_list_get_iterator(list);
		if (e_card_iterator_get(iterator))
			return ((ECardPhone *)e_card_iterator_get(iterator))->number;
		else
			return "";
		gtk_object_unref(GTK_OBJECT(iterator));
		break;
	default:
		return NULL;
	}
}

/* This function sets the value at a particular point in our ETableModel. */
static void
test_set_value_at (ETableModel *etc, int col, int row, const void *val)
{
	ETestModel *test = E_TEST_MODEL(etc);
	ECardList *list;
	ECardIterator *iterator;
	if ( col >= LAST_COL || row >= test->data_count )
		return;
	switch (col) {
	case EMAIL:
		gtk_object_get(GTK_OBJECT(test->data[row]),
			       "email", &list,
			       NULL);
		iterator = e_card_list_get_iterator(list);
		if (e_card_iterator_is_valid(iterator)) {
			e_card_iterator_set(iterator, val);
		} else {
			e_card_list_append(list, val);
		}
		gtk_object_unref(GTK_OBJECT(iterator));
		break;
	case FULL_NAME:
		gtk_object_set(GTK_OBJECT(test->data[row]),
			       "full_name", val,
			       NULL);
		break;
	case STREET:
		gtk_object_get(GTK_OBJECT(test->data[row]),
			       "address", &list,
			       NULL);
		iterator = e_card_list_get_iterator(list);
		if (e_card_iterator_is_valid(iterator)) {
			const ECardDeliveryAddress *address = e_card_iterator_get(iterator);
			ECardDeliveryAddress *address_copy = e_card_delivery_address_copy(address);
			g_free(address_copy->street);
			address_copy->street = g_strdup(val);
			e_card_iterator_set(iterator, address_copy);
			e_card_delivery_address_free(address_copy);
		} else {
			ECardDeliveryAddress *address = g_new(ECardDeliveryAddress, 1);
			address->po = NULL;
			address->ext = NULL;
			address->street = g_strdup(val);
			address->city = NULL;
			address->region = NULL;
			address->code = NULL;
			address->country = NULL;
			address->flags = 0;
			e_card_list_append(list, address);
			e_card_delivery_address_free(address);
		}
		gtk_object_unref(GTK_OBJECT(iterator));
		break;
	case PHONE:
		gtk_object_get(GTK_OBJECT(test->data[row]),
			       "phone", &list,
			       NULL);
		iterator = e_card_list_get_iterator(list);
		if (e_card_iterator_is_valid(iterator)) {
			const ECardPhone *phone = e_card_iterator_get(iterator);
			ECardPhone *phone_copy = e_card_phone_copy(phone);
			g_free(phone_copy->number);
			phone_copy->number = g_strdup(val);
			e_card_iterator_set(iterator, phone_copy);
			e_card_phone_free(phone_copy);
		} else {
			ECardPhone *phone = g_new(ECardPhone, 1);
			phone->number = g_strdup(val);
			phone->flags = 0;
			e_card_list_append(list, phone);
			e_card_phone_free(phone);
		}
		gtk_object_unref(GTK_OBJECT(iterator));
		break;
	default:
		return;
	}
	e_book_commit_card(test->book, test->data[row], NULL, NULL);
	if ( !etc->frozen )
		e_table_model_cell_changed(etc, col, row);
}

/* This function returns whether a particular cell is editable. */
static gboolean
test_is_cell_editable (ETableModel *etc, int col, int row)
{
	return TRUE;
}

/* This function duplicates the value passed to it. */
static void *
test_duplicate_value (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
}

/* This function frees the value passed to it. */
static void
test_free_value (ETableModel *etc, int col, void *value)
{
	g_free(value);
}

/* This function is for when the model is unfrozen.  This can mostly
   be ignored for simple models.  */
static void
test_thaw (ETableModel *etc)
{
	e_table_model_changed(etc);
}

static void
e_test_model_class_init (GtkObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;
	
	object_class->destroy = test_destroy;

	model_class->column_count = test_col_count;
	model_class->row_count = test_row_count;
	model_class->value_at = test_value_at;
	model_class->set_value_at = test_set_value_at;
	model_class->is_cell_editable = test_is_cell_editable;
	model_class->duplicate_value = test_duplicate_value;
	model_class->free_value = test_free_value;
	model_class->thaw = test_thaw;
}

static void
e_test_model_init (GtkObject *object)
{
	ETestModel *model = E_TEST_MODEL(object);
	model->data = NULL;
	model->data_count = 0;
	model->book = NULL;
	model->book_view = NULL;
}

GtkType
e_test_model_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETestModel",
			sizeof (ETestModel),
			sizeof (ETestModelClass),
			(GtkClassInitFunc) e_test_model_class_init,
			(GtkObjectInitFunc) e_test_model_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

void
e_test_model_add_column (ETestModel *model, Address *newadd)
{
#if 0
	model->data = g_realloc(model->data, (++model->data_count) * sizeof(Address *));
	model->data[model->data_count - 1] = newadd;
	e_test_model_queue_save(model);
	if ( model && !E_TABLE_MODEL(model)->frozen )
		e_table_model_changed(E_TABLE_MODEL(model));
#endif
}

static void
e_test_model_card_added(EBookView *book_view,
			const GList *cards,
			ETestModel *model)
{
	model->data = g_realloc(model->data, (model->data_count + g_list_length((GList *)cards)) * sizeof(ECard *));
	for ( ; cards; cards = cards->next) {
		gtk_object_ref(GTK_OBJECT(cards->data));
		model->data[model->data_count++] = E_CARD (cards->data);
	}
	e_table_model_changed(E_TABLE_MODEL(model));
}

static void
e_test_model_card_removed(EBookView *book_view,
			  const char *id,
			  ETestModel *model)
{
	int i;
	for ( i = 0; i < model->data_count; i++) {
		if ( !strcmp(e_card_get_id(model->data[i]), id) ) {
			gtk_object_unref(GTK_OBJECT(model->data[i]));
			memmove(model->data + i, model->data + i + 1, (model->data_count - i - 1) * sizeof (ECard *));
		}
	}
	e_table_model_changed(E_TABLE_MODEL(model));
}

static void
e_test_model_card_changed(EBookView *book_view,
			  const GList *cards,
			  ETestModel *model)
{
	for ( ; cards; cards = cards->next) {
		int i;
		for ( i = 0; i < model->data_count; i++) {
			if ( !strcmp(e_card_get_id(model->data[i]), e_card_get_id(E_CARD(cards->data))) ) {
				gtk_object_unref(GTK_OBJECT(model->data[i]));
				model->data[i] = E_CARD(cards->data);
				gtk_object_ref(GTK_OBJECT(model->data[i]));
				e_table_model_row_changed(E_TABLE_MODEL(model), i);
				break;
			}
		}
	}
}

static void
e_test_model_book_respond_get_view(EBook *book,
				   EBookStatus status,
				   EBookView *book_view,
				   ETestModel *model)
{
	if (status == E_BOOK_STATUS_SUCCESS) {
		model->book_view = book_view;
		gtk_object_ref(GTK_OBJECT(book_view));
		gtk_signal_connect(GTK_OBJECT(book_view),
				   "card_changed",
				   GTK_SIGNAL_FUNC(e_test_model_card_changed),
				   model);
		gtk_signal_connect(GTK_OBJECT(book_view),
				   "card_removed",
				   GTK_SIGNAL_FUNC(e_test_model_card_removed),
				   model);
		gtk_signal_connect(GTK_OBJECT(book_view),
				   "card_added",
				   GTK_SIGNAL_FUNC(e_test_model_card_added),
				   model);
	}
}

static void
e_test_model_uri_loaded(EBook *book,
			EBookStatus status,
			ETestModel *model)
{
	if (status == E_BOOK_STATUS_SUCCESS) {
		e_book_get_book_view (book,
				      "",
				      (EBookBookViewCallback) e_test_model_book_respond_get_view,
				      model);
	}
}

ETableModel *
e_test_model_new (gchar *uri)
{
	ETestModel *et;

	et = gtk_type_new (e_test_model_get_type ());
	
	et->uri = g_strdup(uri);
	et->book = e_book_new();
	e_book_load_uri(et->book,
			et->uri,
			(EBookCallback) e_test_model_uri_loaded,
			et);

	return E_TABLE_MODEL(et);
}
