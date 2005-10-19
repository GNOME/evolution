/* Evolution memos - Data model for ETable
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
 *          Nathan Owens <pianocomp81@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <libgnome/gnome-i18n.h>
#include "e-cal-model-memos.h"
#include "e-cell-date-edit-text.h"
#include "misc.h"

#define d(x) (x)

struct _ECalModelMemosPrivate {
};

static void e_cal_model_memos_finalize (GObject *object);

static int ecmm_column_count (ETableModel *etm);
static void *ecmm_value_at (ETableModel *etm, int col, int row);
static void ecmm_set_value_at (ETableModel *etm, int col, int row, const void *value);
static gboolean ecmm_is_cell_editable (ETableModel *etm, int col, int row);
static void *ecmm_duplicate_value (ETableModel *etm, int col, const void *value);
static void ecmm_free_value (ETableModel *etm, int col, void *value);
static void *ecmm_initialize_value (ETableModel *etm, int col);
static gboolean ecmm_value_is_empty (ETableModel *etm, int col, const void *value);
static char *ecmm_value_to_string (ETableModel *etm, int col, const void *value);

static void ecmm_fill_component_from_model (ECalModel *model, ECalModelComponent *comp_data,
					    ETableModel *source_model, gint row);

G_DEFINE_TYPE (ECalModelMemos, e_cal_model_memos, E_TYPE_CAL_MODEL);

static void
e_cal_model_memos_class_init (ECalModelMemosClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ETableModelClass *etm_class = E_TABLE_MODEL_CLASS (klass);
	ECalModelClass *model_class = E_CAL_MODEL_CLASS (klass);

	object_class->finalize = e_cal_model_memos_finalize;

	etm_class->column_count = ecmm_column_count;
	etm_class->value_at = ecmm_value_at;
	etm_class->set_value_at = ecmm_set_value_at;
	etm_class->is_cell_editable = ecmm_is_cell_editable;
	etm_class->duplicate_value = ecmm_duplicate_value;
	etm_class->free_value = ecmm_free_value;
	etm_class->initialize_value = ecmm_initialize_value;
	etm_class->value_is_empty = ecmm_value_is_empty;
	etm_class->value_to_string = ecmm_value_to_string;

	model_class->fill_component_from_model = ecmm_fill_component_from_model;
}

static void
e_cal_model_memos_init (ECalModelMemos *model)
{
	ECalModelMemosPrivate *priv;

	priv = g_new0 (ECalModelMemosPrivate, 1);
	model->priv = priv;

	e_cal_model_set_component_kind (E_CAL_MODEL (model), ICAL_VJOURNAL_COMPONENT);
}

static void
e_cal_model_memos_finalize (GObject *object)
{
	ECalModelMemosPrivate *priv;
	ECalModelMemos *model = (ECalModelMemos *) object;

	g_return_if_fail (E_IS_CAL_MODEL_MEMOS (model));

	priv = model->priv;
	if (priv) {
		g_free (priv);
		model->priv = NULL;
	}

	if (G_OBJECT_CLASS (e_cal_model_memos_parent_class)->finalize)
		G_OBJECT_CLASS (e_cal_model_memos_parent_class)->finalize (object);
}

/* ETableModel methods */
static int
ecmm_column_count (ETableModel *etm)
{
	return E_CAL_MODEL_MEMOS_FIELD_LAST;
}

static void *
ecmm_value_at (ETableModel *etm, int col, int row)
{
	ECalModelComponent *comp_data;
	ECalModelMemosPrivate *priv;
	ECalModelMemos *model = (ECalModelMemos *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL_MEMOS (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, NULL);
	g_return_val_if_fail (row >= 0 && row < e_table_model_row_count (etm), NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->value_at (etm, col, row);

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data)
		return "";

	return "";
}


static void
ecmm_set_value_at (ETableModel *etm, int col, int row, const void *value)
{
	ECalModelComponent *comp_data;
	ECalModelMemos *model = (ECalModelMemos *) etm;

	g_return_if_fail (E_IS_CAL_MODEL_MEMOS (model));
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST);
	g_return_if_fail (row >= 0 && row < e_table_model_row_count (etm));

	if (col < E_CAL_MODEL_FIELD_LAST) {
		E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->set_value_at (etm, col, row, value);
		return;
	}

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (!comp_data){
		g_warning("couldn't get component data: row == %d", row);
		return;
	}

	/* TODO ask about mod type */
	if (!e_cal_modify_object (comp_data->client, comp_data->icalcomp, CALOBJ_MOD_ALL, NULL)) {
		g_warning (G_STRLOC ": Could not modify the object!");
		
		/* TODO Show error dialog */
	}
}

static gboolean
ecmm_is_cell_editable (ETableModel *etm, int col, int row)
{
	ECalModelMemos *model = (ECalModelMemos *) etm;
	gboolean retval = FALSE;

	g_return_val_if_fail (E_IS_CAL_MODEL_MEMOS (model), FALSE);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, FALSE);
	g_return_val_if_fail (row >= -1 || (row >= 0 && row < e_table_model_row_count (etm)), FALSE);

	if(col == E_CAL_MODEL_FIELD_SUMMARY)
		retval = FALSE;
	else if (col < E_CAL_MODEL_FIELD_LAST)
		retval = E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->is_cell_editable (etm, col, row);

	return retval;
}

static void *
ecmm_duplicate_value (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->duplicate_value (etm, col, value);

	return NULL;
}

static void
ecmm_free_value (ETableModel *etm, int col, void *value)
{
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST);

	if (col < E_CAL_MODEL_FIELD_LAST) {
		E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->free_value (etm, col, value);
		return;
	}
}

static void *
ecmm_initialize_value (ETableModel *etm, int col)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, NULL);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->initialize_value (etm, col);

	return NULL;
}

static gboolean
ecmm_value_is_empty (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, TRUE);

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->value_is_empty (etm, col, value);

	return TRUE;
}

static char *
ecmm_value_to_string (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_MEMOS_FIELD_LAST, g_strdup (""));

	if (col < E_CAL_MODEL_FIELD_LAST)
		return E_TABLE_MODEL_CLASS (e_cal_model_memos_parent_class)->value_to_string (etm, col, value);

	return g_strdup ("");
}

/* ECalModel class methods */

static void
ecmm_fill_component_from_model (ECalModel *model, ECalModelComponent *comp_data,
				ETableModel *source_model, gint row)
{
	g_return_if_fail (E_IS_CAL_MODEL_MEMOS (model));
	g_return_if_fail (comp_data != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (source_model));

}

/**
 * e_cal_model_memos_new
 */
ECalModelMemos *
e_cal_model_memos_new (void)
{
	return g_object_new (E_TYPE_CAL_MODEL_MEMOS, NULL);
}
