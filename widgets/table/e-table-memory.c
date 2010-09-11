/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"

#include "e-table-memory.h"

G_DEFINE_TYPE (ETableMemory, e_table_memory, E_TABLE_MODEL_TYPE)

struct ETableMemoryPriv {
	gpointer *data;
	gint num_rows;
	gint frozen;
};


/* virtual methods */

static void
etmm_finalize (GObject *object)
{
	ETableMemory *etmm = E_TABLE_MEMORY (object);
	ETableMemoryPriv *priv = etmm->priv;

	/* XXX lots of stuff to free here */
	if (priv) {
		g_free (priv->data);
		g_free (priv);
	}
	etmm->priv = NULL;

	G_OBJECT_CLASS (e_table_memory_parent_class)->finalize (object);
}

static gint
etmm_row_count (ETableModel *etm)
{
	ETableMemory *etmm = E_TABLE_MEMORY (etm);

	return etmm->priv->num_rows;
}


static void
e_table_memory_class_init (ETableMemoryClass *klass)
{
	ETableModelClass *table_class = E_TABLE_MODEL_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = etmm_finalize;

	table_class->row_count     = etmm_row_count;
}

static void
e_table_memory_init (ETableMemory *etmm)
{
	ETableMemoryPriv *priv;

	priv = g_new0 (ETableMemoryPriv, 1);
	etmm->priv = priv;

	priv->data = NULL;
	priv->num_rows = 0;
	priv->frozen = 0;
}

/**
 * e_table_memory_new
 *
 * XXX docs here.
 *
 * return values: a newly constructed ETableMemory.
 */
ETableMemory *
e_table_memory_new (void)
{
	return g_object_new (E_TABLE_MEMORY_TYPE, NULL);
}

/**
 * e_table_memory_get_data:
 * @etmm:
 * @row:
 *
 *
 *
 * Return value:
 **/
gpointer
e_table_memory_get_data (ETableMemory *etmm, gint row)
{
	g_return_val_if_fail (row >= 0, NULL);
	g_return_val_if_fail (row < etmm->priv->num_rows, NULL);

	return etmm->priv->data[row];
}

/**
 * e_table_memory_set_data:
 * @etmm:
 * @row:
 * @data:
 *
 *
 **/
void
e_table_memory_set_data (ETableMemory *etmm, gint row, gpointer data)
{
	g_return_if_fail (row >= 0);
	g_return_if_fail (row < etmm->priv->num_rows);

	etmm->priv->data[row] = data;
}

/**
 * e_table_memory_insert:
 * @table_model:
 * @parent_path:
 * @position:
 * @data:
 *
 *
 *
 * Return value:
 **/
void
e_table_memory_insert (ETableMemory *etmm,
		       gint row,
		       gpointer data)
{
	g_return_if_fail (row >= -1);
	g_return_if_fail (row <= etmm->priv->num_rows);

	if (!etmm->priv->frozen)
		e_table_model_pre_change (E_TABLE_MODEL (etmm));

	if (row == -1)
		row = etmm->priv->num_rows;
	etmm->priv->data = g_renew (gpointer, etmm->priv->data, etmm->priv->num_rows + 1);
	memmove (
		etmm->priv->data + row + 1,
		etmm->priv->data + row,
		(etmm->priv->num_rows - row) * sizeof (gpointer));
	etmm->priv->data[row] = data;
	etmm->priv->num_rows++;
	if (!etmm->priv->frozen)
		e_table_model_row_inserted (E_TABLE_MODEL (etmm), row);
}



/**
 * e_table_memory_remove:
 * @etable:
 * @path:
 *
 *
 *
 * Return value:
 **/
gpointer
e_table_memory_remove (ETableMemory *etmm, gint row)
{
	gpointer ret;

	g_return_val_if_fail (row >= 0, NULL);
	g_return_val_if_fail (row < etmm->priv->num_rows, NULL);

	if (!etmm->priv->frozen)
		e_table_model_pre_change (E_TABLE_MODEL (etmm));
	ret = etmm->priv->data[row];
	memmove (
		etmm->priv->data + row,
		etmm->priv->data + row + 1,
		(etmm->priv->num_rows - row - 1) * sizeof (gpointer));
	etmm->priv->num_rows--;
	if (!etmm->priv->frozen)
		e_table_model_row_deleted (E_TABLE_MODEL (etmm), row);
	return ret;
}

/**
 * e_table_memory_clear:
 * @etable:
 * @path:
 *
 *
 *
 * Return value:
 **/
void
e_table_memory_clear (ETableMemory *etmm)
{
	if (!etmm->priv->frozen)
		e_table_model_pre_change (E_TABLE_MODEL (etmm));
	g_free (etmm->priv->data);
	etmm->priv->data = NULL;
	etmm->priv->num_rows = 0;
	if (!etmm->priv->frozen)
		e_table_model_changed (E_TABLE_MODEL (etmm));
}

/**
 * e_table_memory_freeze:
 * @etmm: the ETableModel to freeze.
 *
 * This function prepares an ETableModel for a period of much change.
 * All signals regarding changes to the table are deferred until we
 * thaw the table.
 *
 **/
void
e_table_memory_freeze (ETableMemory *etmm)
{
	ETableMemoryPriv *priv = etmm->priv;

	if (priv->frozen == 0)
		e_table_model_pre_change (E_TABLE_MODEL (etmm));

	priv->frozen++;
}

/**
 * e_table_memory_thaw:
 * @etmm: the ETableMemory to thaw.
 *
 * This function thaws an ETableMemory.  All the defered signals can add
 * up to a lot, we don't know - so we just emit a model_changed
 * signal.
 *
 **/
void
e_table_memory_thaw (ETableMemory *etmm)
{
	ETableMemoryPriv *priv = etmm->priv;

	if (priv->frozen > 0)
		priv->frozen--;
	if (priv->frozen == 0) {
		e_table_model_changed (E_TABLE_MODEL (etmm));
	}
}
