/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *
 * Author:
 *   Christopher James Lahey <clahey@helixcode.com>
 *
 * (C) 1999 Helix Code, Inc.
 */

#include <config.h>
#include "e-cardlist-model.h"
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <gnome.h>

#define PARENT_TYPE e_table_model_get_type()

static void
e_cardlist_model_destroy(GtkObject *object)
{
	ECardlistModel *model = E_CARDLIST_MODEL(object);
	int i;

	for ( i = 0; i < model->data_count; i++ ) {
		gtk_object_unref(GTK_OBJECT(model->data[i]));
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
	ECard *card;
	if ( col >= E_CARD_SIMPLE_FIELD_LAST - 1|| row >= e_cardlist_model->data_count )
		return;
	e_card_simple_set(e_cardlist_model->data[row],
			  col + 1,
			  val);
	gtk_object_get(GTK_OBJECT(e_cardlist_model->data[row]),
		       "card", &card,
		       NULL);
	if ( !etc->frozen )
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

/* This function is for when the model is unfrozen.  This can mostly
   be ignored for simple models.  */
static void
e_cardlist_model_thaw (ETableModel *etc)
{
	e_table_model_changed(etc);
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
		gchar *id = e_card_get_id(cards[i]);
		for ( i = 0; i < model->data_count; i++) {
			if ( !strcmp(e_card_simple_get_id(model->data[i]), id) ) {
				found = TRUE;
			}
		}
		if (!found) {
			gtk_object_ref(GTK_OBJECT(cards[i]));
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
			gtk_object_unref(GTK_OBJECT(model->data[i]));
			memmove(model->data + i, model->data + i + 1, (model->data_count - i - 1) * sizeof (ECard *));
			e_table_model_row_deleted(E_TABLE_MODEL(model), i);
		}
	}
}

static void
e_cardlist_model_class_init (GtkObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;
	
	object_class->destroy = e_cardlist_model_destroy;

	model_class->column_count = e_cardlist_model_col_count;
	model_class->row_count = e_cardlist_model_row_count;
	model_class->value_at = e_cardlist_model_value_at;
	model_class->set_value_at = e_cardlist_model_set_value_at;
	model_class->is_cell_editable = e_cardlist_model_is_cell_editable;
	model_class->duplicate_value = e_cardlist_model_duplicate_value;
	model_class->free_value = e_cardlist_model_free_value;
	model_class->initialize_value = e_cardlist_model_initialize_value;
	model_class->value_is_empty = e_cardlist_model_value_is_empty;
	model_class->thaw = e_cardlist_model_thaw;
}

static void
e_cardlist_model_init (GtkObject *object)
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
		gtk_object_get(GTK_OBJECT(model->data[row]),
			       "card", &card,
			       NULL);
		gtk_object_ref(GTK_OBJECT(card));
		return card;
	}
	return NULL;
}

GtkType
e_cardlist_model_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ECardlistModel",
			sizeof (ECardlistModel),
			sizeof (ECardlistModelClass),
			(GtkClassInitFunc) e_cardlist_model_class_init,
			(GtkObjectInitFunc) e_cardlist_model_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

ETableModel *
e_cardlist_model_new (void)
{
	ECardlistModel *et;

	et = gtk_type_new (e_cardlist_model_get_type ());
	
	return E_TABLE_MODEL(et);
}
