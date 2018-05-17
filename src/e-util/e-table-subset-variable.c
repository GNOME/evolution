/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdlib.h>
#include <string.h>

#include "e-table-subset-variable.h"

G_DEFINE_TYPE (
	ETableSubsetVariable,
	e_table_subset_variable,
	E_TYPE_TABLE_SUBSET)

#define INCREMENT_AMOUNT 10

static void
etssv_add (ETableSubsetVariable *etssv,
           gint row)
{
	ETableModel *etm = E_TABLE_MODEL (etssv);
	ETableSubset *etss = E_TABLE_SUBSET (etssv);

	e_table_model_pre_change (etm);

	if (etss->n_map + 1 > etssv->n_vals_allocated) {
		etssv->n_vals_allocated += INCREMENT_AMOUNT;
		etss->map_table = g_realloc (
			etss->map_table,
			etssv->n_vals_allocated * sizeof (gint));
	}

	etss->map_table[etss->n_map++] = row;

	e_table_model_row_inserted (etm, etss->n_map - 1);
}

static void
etssv_add_array (ETableSubsetVariable *etssv,
                 const gint *array,
                 gint count)
{
	ETableModel *etm = E_TABLE_MODEL (etssv);
	ETableSubset *etss = E_TABLE_SUBSET (etssv);
	gint i;

	e_table_model_pre_change (etm);

	if (etss->n_map + count > etssv->n_vals_allocated) {
		etssv->n_vals_allocated += MAX (INCREMENT_AMOUNT, count);
		etss->map_table = g_realloc (
			etss->map_table,
			etssv->n_vals_allocated * sizeof (gint));
	}
	for (i = 0; i < count; i++)
		etss->map_table[etss->n_map++] = array[i];

	e_table_model_changed (etm);
}

static void
etssv_add_all (ETableSubsetVariable *etssv)
{
	ETableModel *etm = E_TABLE_MODEL (etssv);
	ETableSubset *etss = E_TABLE_SUBSET (etssv);
	ETableModel *source_model;
	gint rows;
	gint i;

	e_table_model_pre_change (etm);

	source_model = e_table_subset_get_source_model (etss);
	rows = e_table_model_row_count (source_model);

	if (etss->n_map + rows > etssv->n_vals_allocated) {
		etssv->n_vals_allocated += MAX (INCREMENT_AMOUNT, rows);
		etss->map_table = g_realloc (
			etss->map_table,
			etssv->n_vals_allocated * sizeof (gint));
	}
	for (i = 0; i < rows; i++)
		etss->map_table[etss->n_map++] = i;

	e_table_model_changed (etm);
}

static gboolean
etssv_remove (ETableSubsetVariable *etssv,
              gint row)
{
	ETableModel *etm = E_TABLE_MODEL (etssv);
	ETableSubset *etss = E_TABLE_SUBSET (etssv);
	gint i;

	for (i = 0; i < etss->n_map; i++) {
		if (etss->map_table[i] == row) {
			e_table_model_pre_change (etm);
			memmove (
				etss->map_table + i,
				etss->map_table + i + 1,
				(etss->n_map - i - 1) * sizeof (gint));
			etss->n_map--;

			e_table_model_row_deleted (etm, i);
			return TRUE;
		}
	}
	return FALSE;
}

static void
e_table_subset_variable_class_init (ETableSubsetVariableClass *class)
{
	class->add = etssv_add;
	class->add_array = etssv_add_array;
	class->add_all = etssv_add_all;
	class->remove = etssv_remove;
}

static void
e_table_subset_variable_init (ETableSubsetVariable *etssv)
{
	/* nothing to do */
}

ETableModel *
e_table_subset_variable_construct (ETableSubsetVariable *etssv,
                                   ETableModel *source)
{
	if (e_table_subset_construct (E_TABLE_SUBSET (etssv), source, 1) == NULL)
		return NULL;
	E_TABLE_SUBSET (etssv)->n_map = 0;

	return E_TABLE_MODEL (etssv);
}

ETableModel *
e_table_subset_variable_new (ETableModel *source)
{
	ETableSubsetVariable *etssv = g_object_new (E_TYPE_TABLE_SUBSET_VARIABLE, NULL);

	if (e_table_subset_variable_construct (etssv, source) == NULL) {
		g_object_unref (etssv);
		return NULL;
	}

	return (ETableModel *) etssv;
}

void
e_table_subset_variable_add (ETableSubsetVariable *etssv,
                             gint row)
{
	ETableSubsetVariableClass *klass;

	g_return_if_fail (etssv != NULL);
	g_return_if_fail (E_IS_TABLE_SUBSET_VARIABLE (etssv));

	klass = E_TABLE_SUBSET_VARIABLE_GET_CLASS (etssv);
	g_return_if_fail (klass != NULL);

	if (klass->add)
		klass->add (etssv, row);
}

void
e_table_subset_variable_add_array (ETableSubsetVariable *etssv,
                                   const gint *array,
                                   gint count)
{
	ETableSubsetVariableClass *klass;

	g_return_if_fail (etssv != NULL);
	g_return_if_fail (E_IS_TABLE_SUBSET_VARIABLE (etssv));

	klass = E_TABLE_SUBSET_VARIABLE_GET_CLASS (etssv);
	g_return_if_fail (klass != NULL);

	if (klass->add_array)
		klass->add_array (etssv, array, count);
}

void
e_table_subset_variable_add_all (ETableSubsetVariable *etssv)
{
	ETableSubsetVariableClass *klass;

	g_return_if_fail (etssv != NULL);
	g_return_if_fail (E_IS_TABLE_SUBSET_VARIABLE (etssv));

	klass = E_TABLE_SUBSET_VARIABLE_GET_CLASS (etssv);
	g_return_if_fail (klass != NULL);

	if (klass->add_all)
		klass->add_all (etssv);
}

gboolean
e_table_subset_variable_remove (ETableSubsetVariable *etssv,
                                gint row)
{
	ETableSubsetVariableClass *klass;

	g_return_val_if_fail (etssv != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_SUBSET_VARIABLE (etssv), FALSE);

	klass = E_TABLE_SUBSET_VARIABLE_GET_CLASS (etssv);
	g_return_val_if_fail (klass != NULL, FALSE);

	if (klass->remove)
		return klass->remove (etssv, row);
	else
		return FALSE;
}

void
e_table_subset_variable_clear (ETableSubsetVariable *etssv)
{
	ETableModel *etm = E_TABLE_MODEL (etssv);
	ETableSubset *etss = E_TABLE_SUBSET (etssv);

	e_table_model_pre_change (etm);
	etss->n_map = 0;
	g_free (etss->map_table);
	etss->map_table = (gint *) g_new (guint, 1);
	etssv->n_vals_allocated = 1;

	e_table_model_changed (etm);
}

void
e_table_subset_variable_increment (ETableSubsetVariable *etssv,
                                   gint position,
                                   gint amount)
{
	gint i;
	ETableSubset *etss = E_TABLE_SUBSET (etssv);
	for (i = 0; i < etss->n_map; i++) {
		if (etss->map_table[i] >= position)
			etss->map_table[i] += amount;
	}
}

void
e_table_subset_variable_decrement (ETableSubsetVariable *etssv,
                                   gint position,
                                   gint amount)
{
	gint i;
	ETableSubset *etss = E_TABLE_SUBSET (etssv);
	for (i = 0; i < etss->n_map; i++) {
		if (etss->map_table[i] >= position)
			etss->map_table[i] -= amount;
	}
}

void
e_table_subset_variable_set_allocation (ETableSubsetVariable *etssv,
                                        gint total)
{
	ETableSubset *etss = E_TABLE_SUBSET (etssv);
	if (total <= 0)
		total = 1;
	if (total > etss->n_map) {
		etss->map_table = g_realloc (etss->map_table, total * sizeof (gint));
	}
}
