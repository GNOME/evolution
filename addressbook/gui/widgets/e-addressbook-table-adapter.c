/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include "e-addressbook-model.h"
#include "e-addressbook-table-adapter.h"
#include "eab-gui-util.h"
#include "util/eab-destination.h"
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <gnome.h>

struct _EAddressbookTableAdapterPrivate {
	EABModel *model;

	int create_contact_id, remove_contact_id, modify_contact_id, model_changed_id;
};

#define PARENT_TYPE e_table_model_get_type()
static ETableModelClass *parent_class;

#define COLS (E_CONTACT_FIELD_LAST)

static void
unlink_model(EAddressbookTableAdapter *adapter)
{
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	g_signal_handler_disconnect (priv->model,
				     priv->create_contact_id);
	g_signal_handler_disconnect (priv->model,
				     priv->remove_contact_id);
	g_signal_handler_disconnect (priv->model,
				     priv->modify_contact_id);
	g_signal_handler_disconnect (priv->model,
				     priv->model_changed_id);

	priv->create_contact_id = 0;
	priv->remove_contact_id = 0;
	priv->modify_contact_id = 0;
	priv->model_changed_id = 0;

	g_object_unref (priv->model);

	priv->model = NULL;
}

static void
addressbook_dispose(GObject *object)
{
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(object);

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
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	return eab_model_contact_count (priv->model);
}

/* This function returns the value at a particular point in our ETableModel. */
static void *
addressbook_value_at (ETableModel *etc, int col, int row)
{
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	const char *value;

	if ( col >= COLS || row >= eab_model_contact_count (priv->model) )
		return NULL;

	value = e_contact_get_const((EContact*)eab_model_contact_at (priv->model, row), col);

	if (value && !strncmp (value, "<?xml", 5)) {
		EABDestination *dest = eab_destination_import (value);
		if (dest) {
			/* XXX blech, we leak this */
			value = g_strdup (eab_destination_get_textrep (dest, TRUE));
			g_object_unref (dest);
		}
	}


	return (void *)(value ? value : "");
}

/* This function sets the value at a particular point in our ETableModel. */
#if 0
static void
card_modified_cb (EBook* book, EBookStatus status,
		  gpointer user_data)
{
	if (status != E_BOOK_ERROR_OK)
		eab_error_dialog (_("Error modifying card"), status);
}
#endif

static void
addressbook_set_value_at (ETableModel *etc, int col, int row, const void *val)
{
#if 0
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	if (eab_model_editable (priv->model)) {
		ECard *card;

		if ( col >= COLS|| row >= eab_model_card_count (priv->model) )
			return;

		e_table_model_pre_change(etc);

		e_card_simple_set(priv->simples[row],
				  col,
				  val);
		g_object_get(priv->simples[row],
			     "card", &card,
			     NULL);

		e_card_merging_book_commit_card(eab_model_get_ebook(priv->model),
						card, card_modified_cb, NULL);
		g_object_unref (card);

		/* XXX do we need this?  shouldn't the commit_card generate a changed signal? */
		e_table_model_cell_changed(etc, col, row);
	}
#endif
}

/* This function returns whether a particular cell is editable. */
static gboolean
addressbook_is_cell_editable (ETableModel *etc, int col, int row)
{
#if 0
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	ECard *card;

	if (row >= 0 && row < eab_model_card_count (priv->model))
		card = eab_model_card_at (priv->model, row);
	else
		card = NULL;

	if (!eab_model_editable(priv->model))
		return FALSE;
	else if (card && e_card_evolution_list (card))
		/* we only allow editing of the name and file as for
                   lists */
		return col == E_CARD_SIMPLE_FIELD_FULL_NAME || col == E_CARD_SIMPLE_FIELD_FILE_AS; 
	else
		return col < E_CARD_SIMPLE_FIELD_LAST_SIMPLE_STRING;
#else
	return FALSE;
#endif
}

static void
addressbook_append_row (ETableModel *etm, ETableModel *source, gint row)
{
#if 0
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(etm);
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
	e_card_merging_book_add_card (eab_model_get_ebook (priv->model), card, NULL, NULL);
	g_object_unref (simple);
	g_object_unref (card);
#endif
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
eab_table_adapter_class_init (GObjectClass *object_class)
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
eab_table_adapter_init (GObject *object)
{
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(object);
	EAddressbookTableAdapterPrivate *priv;

	priv = adapter->priv = g_new0 (EAddressbookTableAdapterPrivate, 1);

	priv->create_contact_id = 0;
	priv->remove_contact_id = 0;
	priv->modify_contact_id = 0;
	priv->model_changed_id = 0;
}


static void
create_contact (EABModel *model,
		gint index, gint count,
		EAddressbookTableAdapter *adapter)
{
	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	e_table_model_rows_inserted (E_TABLE_MODEL (adapter), index, count);
}

static void
remove_contact (EABModel *model,
		gint index,
		EAddressbookTableAdapter *adapter)
{
	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	e_table_model_rows_deleted (E_TABLE_MODEL (adapter), index, 1);
}

static void
modify_contact (EABModel *model,
		gint index,
		EAddressbookTableAdapter *adapter)
{
	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	e_table_model_row_changed (E_TABLE_MODEL (adapter), index);
}

static void
model_changed (EABModel *model,
	       EAddressbookTableAdapter *adapter)
{
	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	e_table_model_changed (E_TABLE_MODEL (adapter));
}

GType
eab_table_adapter_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (EAddressbookTableAdapterClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) eab_table_adapter_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EAddressbookTableAdapter),
			0,             /* n_preallocs */
			(GInstanceInitFunc) eab_table_adapter_init,
		};

		type = g_type_register_static (PARENT_TYPE, "EAddressbookTableAdapter", &info, 0);
	}

	return type;
}

void
eab_table_adapter_construct (EAddressbookTableAdapter *adapter,
				       EABModel *model)
{
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	priv->model = model;
	g_object_ref (priv->model);

	priv->create_contact_id = g_signal_connect(priv->model,
						   "contact_added",
						   G_CALLBACK(create_contact),
						   adapter);
	priv->remove_contact_id = g_signal_connect(priv->model,
						   "contact_removed",
						   G_CALLBACK(remove_contact),
						   adapter);
	priv->modify_contact_id = g_signal_connect(priv->model,
						   "contact_changed",
						   G_CALLBACK(modify_contact),
						   adapter);
	priv->model_changed_id = g_signal_connect(priv->model,
						  "model_changed",
						  G_CALLBACK(model_changed),
						  adapter);
}

ETableModel *
eab_table_adapter_new (EABModel *model)
{
	EAddressbookTableAdapter *et;

	et = g_object_new(E_TYPE_AB_TABLE_ADAPTER, NULL);

	eab_table_adapter_construct (et, model);

	return E_TABLE_MODEL(et);
}
