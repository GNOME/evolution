/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *
 * Author:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * (C) 1999 Ximian, Inc.
 */

#include <config.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <string.h>

#include "e-cardlist-model.h"

#define PARENT_TYPE e_table_model_get_type()

static void
e_cardlist_model_dispose(GObject *object)
{
	ECardlistModel *model = E_CARDLIST_MODEL(object);
	int i;

	for ( i = 0; i < model->data_count; i++ ) {
		g_object_unref(model->data[i]);
	}
	g_free(model->data);
}

/* This function returns the number of columns in our ETableModel. */
static int
e_cardlist_model_col_count (ETableModel *etc)
{
	return E_CARD_SIMPLE_FIELD_LAST;
}

/* This function returns the number of rows in our ETableModel. */
static int
e_cardlist_model_row_count (ETableModel *etc)
{
	ECardlistModel *e_cardlist_model = E_CARDLIST_MODEL(etc);
	return e_cardlist_model->data_count;
}

/* This function returns the value at a particular point in our ETableModel. */
static void *
e_cardlist_model_value_at (ETableModel *etc, int col, int row)
{
	ECardlistModel *e_cardlist_model = E_CARDLIST_MODEL(etc);
	const char *value;
	if ( col >= E_CARD_SIMPLE_FIELD_LAST - 1|| row >= e_cardlist_model->data_count )
		return NULL;
	value = e_card_simple_get_const(e_cardlist_model->data[row], 
					col + 1);
	return (void *)(value ? value : "");
}

/* This function sets the value at a particular point in our ETableModel. */
static void
e_cardlist_model_set_value_at (ETableModel *etc, int col, int row, const void *val)
{
	ECardlistModel *e_cardlist_model = E_CARDLIST_MODEL(etc);

	if ( col >= E_CARD_SIMPLE_FIELD_LAST - 1|| row >= e_cardlist_model->data_count )
		return;
	e_table_model_pre_change(etc);
	e_card_simple_set(e_cardlist_model->data[row],
			  col + 1,
			  val);

	e_table_model_cell_changed(etc, col, row);
}

/* This function returns whether a particular cell is editable. */
static gboolean
e_cardlist_model_is_cell_editable (ETableModel *etc, int col, int row)
{
	return TRUE;
}

/* This function duplicates the value passed to it. */
static void *
e_cardlist_model_duplicate_value (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
}

/* This function frees the value passed to it. */
static void
e_cardlist_model_free_value (ETableModel *etc, int col, void *value)
{
	g_free(value);
}

static void *
e_cardlist_model_initialize_value (ETableModel *etc, int col)
{
	return g_strdup("");
}

static gboolean
e_cardlist_model_value_is_empty (ETableModel *etc, int col, const void *value)
{
	return !(value && *(char *)value);
}

static char *
e_cardlist_model_value_to_string (ETableModel *etc, int col, const void *value)
{
	return g_strdup(value);
}

void
e_cardlist_model_add(ECardlistModel *model,
		     ECard **cards,
		     int count)
{
	int i;
	model->data = g_realloc(model->data, model->data_count + count * sizeof(ECard *));
	for (i = 0; i < count; i++) {
		gboolean found = FALSE;
		const gchar *id = e_card_get_id(cards[i]);
		for ( i = 0; i < model->data_count; i++) {
			if ( !strcmp(e_card_simple_get_id(model->data[i]), id) ) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			e_table_model_pre_change(E_TABLE_MODEL(model));
			g_object_ref(cards[i]);
			model->data[model->data_count++] = e_card_simple_new (cards[i]);
			e_table_model_row_inserted(E_TABLE_MODEL(model), model->data_count - 1);
		}
	}
}

void
e_cardlist_model_remove(ECardlistModel *model,
			const char *id)
{
	int i;
	for ( i = 0; i < model->data_count; i++) {
		if ( !strcmp(e_card_simple_get_id(model->data[i]), id) ) {
			e_table_model_pre_change(E_TABLE_MODEL(model));
			g_object_unref(model->data[i]);
			memmove(model->data + i, model->data + i + 1, (model->data_count - i - 1) * sizeof (ECard *));
			e_table_model_row_deleted(E_TABLE_MODEL(model), i);
		}
	}
}

static void
e_cardlist_model_class_init (GObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;
	
	object_class->dispose = e_cardlist_model_dispose;

	model_class->column_count = e_cardlist_model_col_count;
	model_class->row_count = e_cardlist_model_row_count;
	model_class->value_at = e_cardlist_model_value_at;
	model_class->set_value_at = e_cardlist_model_set_value_at;
	model_class->is_cell_editable = e_cardlist_model_is_cell_editable;
	model_class->duplicate_value = e_cardlist_model_duplicate_value;
	model_class->free_value = e_cardlist_model_free_value;
	model_class->initialize_value = e_cardlist_model_initialize_value;
	model_class->value_is_empty = e_cardlist_model_value_is_empty;
	model_class->value_to_string = e_cardlist_model_value_to_string;
}

static void
e_cardlist_model_init (GObject *object)
{
	ECardlistModel *model = E_CARDLIST_MODEL(object);
	model->data = NULL;
	model->data_count = 0;
}

ECard *
e_cardlist_model_get(ECardlistModel *model,
		     int                row)
{
	if (model->data && row < model->data_count) {
		ECard *card;
		g_object_get(model->data[row],
			     "card", &card,
			     NULL);
		return card;
	}
	return NULL;
}

GType
e_cardlist_model_get_type (void)
{
	static GType aw_type = 0;

	if (!aw_type) {
		static const GTypeInfo aw_info =  {
			sizeof (ECardlistModelClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_cardlist_model_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ECardlistModel),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_cardlist_model_init,
		};

		aw_type = g_type_register_static (PARENT_TYPE, "ECardlistModel", &aw_info, 0);
	}

	return aw_type;
}

ETableModel *
e_cardlist_model_new (void)
{
	ECardlistModel *et;

	et = g_object_new (E_TYPE_CARDLIST_MODEL, NULL);
	
	return E_TABLE_MODEL(et);
}
