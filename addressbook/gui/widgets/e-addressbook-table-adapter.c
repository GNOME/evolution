/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include "e-addressbook-model.h"
#include "e-addressbook-table-adapter.h"
#include "e-card-merging.h"
#include "e-addressbook-util.h"
#include "ebook/e-destination.h"
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <gnome.h>

struct _EAddressbookTableAdapterPrivate {
	EAddressbookModel *model;

	ECardSimple **simples;
	int count;

	int create_card_id, remove_card_id, modify_card_id, model_changed_id;
};

#define PARENT_TYPE e_table_model_get_type()
static ETableModelClass *parent_class;

#define COLS (E_CARD_SIMPLE_FIELD_LAST)

static void
unlink_model(EAddressbookTableAdapter *adapter)
{
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	int i;

	g_signal_handler_disconnect (priv->model,
				     priv->create_card_id);
	g_signal_handler_disconnect (priv->model,
				     priv->remove_card_id);
	g_signal_handler_disconnect (priv->model,
				     priv->modify_card_id);
	g_signal_handler_disconnect (priv->model,
				     priv->model_changed_id);

	priv->create_card_id = 0;
	priv->remove_card_id = 0;
	priv->modify_card_id = 0;
	priv->model_changed_id = 0;

	/* free up the existing mapping if there is one */
	if (priv->simples) {
		for (i = 0; i < priv->count; i ++)
			g_object_unref (priv->simples[i]);
		g_free (priv->simples);
		priv->simples = NULL;
	}

	g_object_unref (priv->model);

	priv->model = NULL;
}

static void
build_simple_mapping(EAddressbookTableAdapter *adapter)
{
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	int i;

	/* free up the existing mapping if there is one */
	if (priv->simples) {
		for (i = 0; i < priv->count; i ++)
			g_object_unref (priv->simples[i]);
		g_free (priv->simples);
	}

	/* build up our mapping to ECardSimple*'s */
	priv->count = e_addressbook_model_card_count (priv->model);
	priv->simples = g_new (ECardSimple*, priv->count);
	for (i = 0; i < priv->count; i ++) {
		priv->simples[i] = e_card_simple_new (e_addressbook_model_card_at (priv->model, i));
		g_object_ref (priv->simples[i]);
	}
}

static void
addressbook_dispose(GObject *object)
{
	EAddressbookTableAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER(object);

	if (adapter->priv) {
		unlink_model(adapter);

		g_free (adapter->priv);
		adapter->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

/* This function returns the number of columns in our ETableModel. */
static int
addressbook_col_count (ETableModel *etc)
{
	return COLS;
}

/* This function returns the number of rows in our ETableModel. */
static int
addressbook_row_count (ETableModel *etc)
{
	EAddressbookTableAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	return e_addressbook_model_card_count (priv->model);
}

/* This function returns the value at a particular point in our ETableModel. */
static void *
addressbook_value_at (ETableModel *etc, int col, int row)
{
	EAddressbookTableAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	const char *value;

	if ( col >= COLS || row >= e_addressbook_model_card_count (priv->model) )
		return NULL;

	value = e_card_simple_get_const(priv->simples[row], col);

	if (value && !strncmp (value, "<?xml", 5)) {
		EDestination *dest = e_destination_import (value);
		if (dest) {
			/* XXX blech, we leak this */
			value = g_strdup (e_destination_get_textrep (dest, TRUE));
			g_object_unref (dest);
		}
	}


	return (void *)(value ? value : "");
}

/* This function sets the value at a particular point in our ETableModel. */
static void
card_modified_cb (EBook* book, EBookStatus status,
		  gpointer user_data)
{
	if (status != E_BOOK_STATUS_SUCCESS)
		e_addressbook_error_dialog (_("Error modifying card"), status);
}
static void
addressbook_set_value_at (ETableModel *etc, int col, int row, const void *val)
{
	EAddressbookTableAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	if (e_addressbook_model_editable (priv->model)) {
		ECard *card;

		if ( col >= COLS|| row >= e_addressbook_model_card_count (priv->model) )
			return;

		e_table_model_pre_change(etc);

		e_card_simple_set(priv->simples[row],
				  col,
				  val);
		g_object_get(priv->simples[row],
			     "card", &card,
			     NULL);

		e_card_merging_book_commit_card(e_addressbook_model_get_ebook(priv->model),
						card, card_modified_cb, NULL);
		g_object_unref (card);

		/* XXX do we need this?  shouldn't the commit_card generate a changed signal? */
		e_table_model_cell_changed(etc, col, row);
	}
}

/* This function returns whether a particular cell is editable. */
static gboolean
addressbook_is_cell_editable (ETableModel *etc, int col, int row)
{
	EAddressbookTableAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	ECard *card;

	if (row >= 0 && row < e_addressbook_model_card_count (priv->model))
		card = e_addressbook_model_card_at (priv->model, row);
	else
		card = NULL;

	if (!e_addressbook_model_editable(priv->model))
		return FALSE;
	else if (card && e_card_evolution_list (card))
		/* we only allow editing of the name and file as for
                   lists */
		return col == E_CARD_SIMPLE_FIELD_FULL_NAME || col == E_CARD_SIMPLE_FIELD_FILE_AS; 
	else
		return col < E_CARD_SIMPLE_FIELD_LAST_SIMPLE_STRING;
}

static void
addressbook_append_row (ETableModel *etm, ETableModel *source, gint row)
{
	EAddressbookTableAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER(etm);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	ECard *card;
	ECardSimple *simple;
	int col;

	card = e_card_new("");
	simple = e_card_simple_new(card);

	for (col = 0; col < E_CARD_SIMPLE_FIELD_LAST_SIMPLE_STRING; col++) {
		const void *val = e_table_model_value_at(source, col, row);
		e_card_simple_set(simple, col, val);
	}
	e_card_simple_sync_card(simple);
	e_card_merging_book_add_card (e_addressbook_model_get_ebook (priv->model), card, NULL, NULL);
	g_object_unref (simple);
	g_object_unref (card);
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
e_addressbook_table_adapter_class_init (GObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;

	parent_class = g_type_class_peek_parent (object_class);

	object_class->dispose = addressbook_dispose;

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
e_addressbook_table_adapter_init (GObject *object)
{
	EAddressbookTableAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER(object);
	EAddressbookTableAdapterPrivate *priv;

	priv = adapter->priv = g_new0 (EAddressbookTableAdapterPrivate, 1);

	priv->create_card_id = 0;
	priv->remove_card_id = 0;
	priv->modify_card_id = 0;
	priv->model_changed_id = 0;
	priv->simples = NULL;
	priv->count = 0;
}


static void
create_card (EAddressbookModel *model,
	     gint index, gint count,
	     EAddressbookTableAdapter *adapter)
{
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	int i;

	priv->count += count;
	priv->simples = g_renew(ECardSimple *, priv->simples, priv->count);
	memmove (priv->simples + index + count, priv->simples + index, (priv->count - index - count) * sizeof (ECardSimple *));

	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	for (i = 0; i < count; i ++) {
		priv->simples[index + i] = e_card_simple_new (e_addressbook_model_card_at (priv->model, index + i));
	}
	e_table_model_rows_inserted (E_TABLE_MODEL (adapter), index, count);
}

static void
remove_card (EAddressbookModel *model,
	     gint index,
	     EAddressbookTableAdapter *adapter)
{
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	e_table_model_pre_change (E_TABLE_MODEL (adapter));

	g_object_unref (priv->simples[index]);
	memmove (priv->simples + index, priv->simples + index + 1, (priv->count - index - 1) * sizeof (ECardSimple *));
	priv->count --;
	e_table_model_rows_deleted (E_TABLE_MODEL (adapter), index, 1);
}

static void
modify_card (EAddressbookModel *model,
	     gint index,
	     EAddressbookTableAdapter *adapter)
{
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	e_table_model_pre_change (E_TABLE_MODEL (adapter));

	g_object_unref (priv->simples[index]);
	priv->simples[index] = e_card_simple_new (e_addressbook_model_card_at (priv->model, index));
	g_object_ref (priv->simples[index]);
	e_table_model_row_changed (E_TABLE_MODEL (adapter), index);
}

static void
model_changed (EAddressbookModel *model,
	       EAddressbookTableAdapter *adapter)
{
	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	build_simple_mapping (adapter);
	e_table_model_changed (E_TABLE_MODEL (adapter));
}

GType
e_addressbook_table_adapter_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (EAddressbookTableAdapterClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_addressbook_table_adapter_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EAddressbookTableAdapter),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_addressbook_table_adapter_init,
		};

		type = g_type_register_static (PARENT_TYPE, "EAddressbookTableAdapter", &info, 0);
	}

	return type;
}

void
e_addressbook_table_adapter_construct (EAddressbookTableAdapter *adapter,
				       EAddressbookModel *model)
{
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	priv->model = model;
	g_object_ref (priv->model);

	priv->create_card_id = g_signal_connect(priv->model,
						"card_added",
						G_CALLBACK(create_card),
						adapter);
	priv->remove_card_id = g_signal_connect(priv->model,
						"card_removed",
						G_CALLBACK(remove_card),
						adapter);
	priv->modify_card_id = g_signal_connect(priv->model,
						"card_changed",
						G_CALLBACK(modify_card),
						adapter);
	priv->model_changed_id = g_signal_connect(priv->model,
						  "model_changed",
						  G_CALLBACK(model_changed),
						  adapter);

	build_simple_mapping (adapter);
}

ETableModel *
e_addressbook_table_adapter_new (EAddressbookModel *model)
{
	EAddressbookTableAdapter *et;

	et = g_object_new(E_TYPE_ADDRESSBOOK_TABLE_ADAPTER, NULL);

	e_addressbook_table_adapter_construct (et, model);

	return E_TABLE_MODEL(et);
}
