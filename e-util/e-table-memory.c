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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "e-table-memory.h"
#include "e-xml-utils.h"

#define E_TABLE_MEMORY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TABLE_MEMORY, ETableMemoryPrivate))

/* Forward Declarations */
static void	e_table_memory_table_model_init
					(ETableModelInterface *interface);

G_DEFINE_TYPE_WITH_CODE (
	ETableMemory,
	e_table_memory,
	G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_TABLE_MODEL,
		e_table_memory_table_model_init))

struct _ETableMemoryPrivate {
	gpointer *data;
	gint num_rows;
	gint frozen;
};

static void
table_memory_finalize (GObject *object)
{
	ETableMemoryPrivate *priv;

	priv = E_TABLE_MEMORY_GET_PRIVATE (object);

	g_free (priv->data);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_table_memory_parent_class)->finalize (object);
}

static gint
table_memory_row_count (ETableModel *table_model)
{
	ETableMemory *table_memory = E_TABLE_MEMORY (table_model);

	return table_memory->priv->num_rows;
}

static void
e_table_memory_class_init (ETableMemoryClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ETableMemoryPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = table_memory_finalize;
}

static void
e_table_memory_table_model_init (ETableModelInterface *interface)
{
	interface->row_count = table_memory_row_count;
}

static void
e_table_memory_init (ETableMemory *table_memory)
{
	table_memory->priv = E_TABLE_MEMORY_GET_PRIVATE (table_memory);
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
	return g_object_new (E_TYPE_TABLE_MEMORY, NULL);
}

/**
 * e_table_memory_get_data:
 * @table_memory:
 * @row:
 *
 * Return value:
 **/
gpointer
e_table_memory_get_data (ETableMemory *table_memory,
                         gint row)
{
	g_return_val_if_fail (row >= 0, NULL);
	g_return_val_if_fail (row < table_memory->priv->num_rows, NULL);

	return table_memory->priv->data[row];
}

/**
 * e_table_memory_set_data:
 * @table_memory:
 * @row:
 * @data:
 *
 **/
void
e_table_memory_set_data (ETableMemory *table_memory,
                         gint row,
                         gpointer data)
{
	g_return_if_fail (row >= 0);
	g_return_if_fail (row < table_memory->priv->num_rows);

	table_memory->priv->data[row] = data;
}

/**
 * e_table_memory_insert:
 * @table_memory:
 * @row:
 * @data:
 *
 * Return value:
 **/
void
e_table_memory_insert (ETableMemory *table_memory,
                       gint row,
                       gpointer data)
{
	g_return_if_fail (row >= -1);
	g_return_if_fail (row <= table_memory->priv->num_rows);

	if (!table_memory->priv->frozen)
		e_table_model_pre_change (E_TABLE_MODEL (table_memory));

	if (row == -1)
		row = table_memory->priv->num_rows;
	table_memory->priv->data = g_renew (gpointer, table_memory->priv->data, table_memory->priv->num_rows + 1);
	memmove (
		table_memory->priv->data + row + 1,
		table_memory->priv->data + row,
		(table_memory->priv->num_rows - row) * sizeof (gpointer));
	table_memory->priv->data[row] = data;
	table_memory->priv->num_rows++;
	if (!table_memory->priv->frozen)
		e_table_model_row_inserted (E_TABLE_MODEL (table_memory), row);
}

/**
 * e_table_memory_remove:
 * @table_memory:
 * @row:
 *
 * Return value:
 **/
gpointer
e_table_memory_remove (ETableMemory *table_memory,
                       gint row)
{
	gpointer ret;

	g_return_val_if_fail (row >= 0, NULL);
	g_return_val_if_fail (row < table_memory->priv->num_rows, NULL);

	if (!table_memory->priv->frozen)
		e_table_model_pre_change (E_TABLE_MODEL (table_memory));
	ret = table_memory->priv->data[row];
	memmove (
		table_memory->priv->data + row,
		table_memory->priv->data + row + 1,
		(table_memory->priv->num_rows - row - 1) * sizeof (gpointer));
	table_memory->priv->num_rows--;
	if (!table_memory->priv->frozen)
		e_table_model_row_deleted (E_TABLE_MODEL (table_memory), row);
	return ret;
}

/**
 * e_table_memory_clear:
 * @table_memory:
 **/
void
e_table_memory_clear (ETableMemory *table_memory)
{
	if (!table_memory->priv->frozen)
		e_table_model_pre_change (E_TABLE_MODEL (table_memory));
	g_free (table_memory->priv->data);
	table_memory->priv->data = NULL;
	table_memory->priv->num_rows = 0;
	if (!table_memory->priv->frozen)
		e_table_model_changed (E_TABLE_MODEL (table_memory));
}

/**
 * e_table_memory_freeze:
 * @table_memory: the ETableModel to freeze.
 *
 * This function prepares an ETableModel for a period of much change.
 * All signals regarding changes to the table are deferred until we
 * thaw the table.
 *
 **/
void
e_table_memory_freeze (ETableMemory *table_memory)
{
	ETableMemoryPrivate *priv = table_memory->priv;

	if (priv->frozen == 0)
		e_table_model_pre_change (E_TABLE_MODEL (table_memory));

	priv->frozen++;
}

/**
 * e_table_memory_thaw:
 * @table_memory: the ETableMemory to thaw.
 *
 * This function thaws an ETableMemory.  All the defered signals can add
 * up to a lot, we don't know - so we just emit a model_changed
 * signal.
 *
 **/
void
e_table_memory_thaw (ETableMemory *table_memory)
{
	ETableMemoryPrivate *priv = table_memory->priv;

	if (priv->frozen > 0)
		priv->frozen--;
	if (priv->frozen == 0) {
		e_table_model_changed (E_TABLE_MODEL (table_memory));
	}
}
