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
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"

#include "e-table-col.h"

G_DEFINE_TYPE (ETableCol, e_table_col, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_COMPARE_COL
};

static void
etc_dispose (GObject *object)
{
	ETableCol *etc = E_TABLE_COL (object);

	if (etc->ecell)
		g_object_unref (etc->ecell);
	etc->ecell = NULL;

	if (etc->pixbuf)
		g_object_unref (etc->pixbuf);
	etc->pixbuf = NULL;

	if (etc->text)
		g_free (etc->text);
	etc->text = NULL;

	if (G_OBJECT_CLASS (e_table_col_parent_class)->dispose)
		G_OBJECT_CLASS (e_table_col_parent_class)->dispose (object);
}

static void
etc_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ETableCol *etc = E_TABLE_COL (object);

	switch (prop_id) {
	case PROP_COMPARE_COL:
		etc->compare_col = g_value_get_int (value);
		break;
	default:
		break;
	}
}

static void
etc_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ETableCol *etc = E_TABLE_COL (object);

	switch (prop_id) {
	case PROP_COMPARE_COL:
		g_value_set_int (value, etc->compare_col);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_table_col_class_init (ETableColClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = etc_dispose;
	object_class->set_property = etc_set_property;
	object_class->get_property = etc_get_property;

	g_object_class_install_property (object_class, PROP_COMPARE_COL,
					 g_param_spec_int ("compare_col",
							   _( "Width" ),
							   "Width",
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));
}

static void
e_table_col_init (ETableCol *etc)
{
	etc->width = 0;
	etc->sortable = 1;
	etc->groupable = 1;
	etc->justification = GTK_JUSTIFY_LEFT;
	etc->priority = 0;
}

/**
 * e_table_col_new:
 * @col_idx: the column we represent in the model
 * @text: a title for this column
 * @expansion: FIXME
 * @min_width: minimum width in pixels for this column
 * @ecell: the renderer to be used for this column
 * @compare: comparision function for the elements stored in this column
 * @resizable: whether the column can be resized interactively by the user
 * @priority: FIXME
 *
 * The ETableCol represents a column to be used inside an ETable.  The
 * ETableCol objects are inserted inside an ETableHeader (which is just a collection
 * of ETableCols).  The ETableHeader is the definition of the order in which
 * columns are shown to the user.
 *
 * The @text argument is the the text that will be shown as a header to the
 * user. @col_idx reflects where the data for this ETableCol object will
 * be fetch from an ETableModel.  So even if the user changes the order
 * of the columns being viewed (the ETableCols in the ETableHeader), the
 * column will always point to the same column inside the ETableModel.
 *
 * The @ecell argument is an ECell object that needs to know how to render the
 * data in the ETableModel for this specific row.
 *
 * Returns: the newly created ETableCol object.
 */
ETableCol *
e_table_col_new (gint col_idx, const gchar *text, double expansion, gint min_width,
		 ECell *ecell, GCompareFunc compare, gboolean resizable, gboolean disabled, gint priority)
{
	ETableCol *etc;

	g_return_val_if_fail (expansion >= 0, NULL);
	g_return_val_if_fail (min_width >= 0, NULL);
	g_return_val_if_fail (ecell != NULL, NULL);
	g_return_val_if_fail (compare != NULL, NULL);
	g_return_val_if_fail (text != NULL, NULL);

	etc = g_object_new (E_TABLE_COL_TYPE, NULL);

	etc->is_pixbuf = FALSE;

	etc->col_idx = col_idx;
	etc->compare_col = col_idx;
	etc->text = g_strdup (text);
	etc->pixbuf = NULL;
	etc->expansion = expansion;
	etc->min_width = min_width;
	etc->ecell = ecell;
	etc->compare = compare;
	etc->disabled = disabled;
	etc->priority = priority;

	etc->selected = 0;
	etc->resizable = resizable;

	g_object_ref (etc->ecell);

	return etc;
}

/**
 * e_table_col_new_with_pixbuf:
 * @col_idx: the column we represent in the model
 * @pixbuf: the image to be used for the header
 * @expansion: FIXME
 * @min_width: minimum width in pixels for this column
 * @ecell: the renderer to be used for this column
 * @compare: comparision function for the elements stored in this column
 * @resizable: whether the column can be resized interactively by the user
 *
 * The ETableCol represents a column to be used inside an ETable.  The
 * ETableCol objects are inserted inside an ETableHeader (which is just a collection
 * of ETableCols).  The ETableHeader is the definition of the order in which
 * columns are shown to the user.
 *
 * The @text argument is the the text that will be shown as a header to the
 * user. @col_idx reflects where the data for this ETableCol object will
 * be fetch from an ETableModel.  So even if the user changes the order
 * of the columns being viewed (the ETableCols in the ETableHeader), the
 * column will always point to the same column inside the ETableModel.
 *
 * The @ecell argument is an ECell object that needs to know how to render the
 * data in the ETableModel for this specific row.
 *
 * Returns: the newly created ETableCol object.
 */
ETableCol *
e_table_col_new_with_pixbuf (gint col_idx, const gchar *text, GdkPixbuf *pixbuf, double expansion, gint min_width,
			     ECell *ecell, GCompareFunc compare, gboolean resizable, gboolean disabled, gint priority)
{
	ETableCol *etc;

	g_return_val_if_fail (expansion >= 0, NULL);
	g_return_val_if_fail (min_width >= 0, NULL);
	g_return_val_if_fail (ecell != NULL, NULL);
	g_return_val_if_fail (compare != NULL, NULL);
	g_return_val_if_fail (pixbuf != NULL, NULL);

	etc = g_object_new (E_TABLE_COL_TYPE, NULL);

	etc->is_pixbuf = TRUE;

	etc->col_idx = col_idx;
	etc->compare_col = col_idx;
	etc->text = g_strdup(text);
	etc->pixbuf = pixbuf;
	etc->expansion = expansion;
	etc->min_width = min_width;
	etc->ecell = ecell;
	etc->compare = compare;
	etc->disabled = disabled;
	etc->priority = priority;

	etc->selected = 0;
	etc->resizable = resizable;

	g_object_ref (etc->ecell);
	g_object_ref (etc->pixbuf);

	return etc;
}
