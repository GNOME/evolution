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
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n.h>

#include "e-table-col.h"

G_DEFINE_TYPE (ETableCol, e_table_col, G_TYPE_OBJECT)

static void
etc_load_icon (ETableCol *etc)
{
	/* FIXME This ignores theme changes. */

	GtkIconTheme *icon_theme;
	gint width, height;
	GError *error = NULL;

	icon_theme = gtk_icon_theme_get_default ();
	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, &height);

	etc->pixbuf = gtk_icon_theme_load_icon (
		icon_theme, etc->icon_name, height, GTK_ICON_LOOKUP_FORCE_SIZE, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
etc_dispose (GObject *object)
{
	ETableCol *etc = E_TABLE_COL (object);

	g_clear_object (&etc->spec);

	if (etc->ecell)
		g_object_unref (etc->ecell);
	etc->ecell = NULL;

	if (etc->pixbuf)
		g_object_unref (etc->pixbuf);
	etc->pixbuf = NULL;

	g_free (etc->text);
	etc->text = NULL;

	g_free (etc->icon_name);
	etc->icon_name = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_col_parent_class)->dispose (object);
}

static void
e_table_col_class_init (ETableColClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = etc_dispose;
}

static void
e_table_col_init (ETableCol *etc)
{
	etc->width = 0;
	etc->justification = GTK_JUSTIFY_LEFT;
}

/**
 * e_table_col_new:
 * @spec: an #ETableColumnSpecification
 * @text: a title for this column
 * @icon_name: name of the icon to be used for the header, or %NULL
 * @ecell: the renderer to be used for this column
 * @compare: comparision function for the elements stored in this column
 *
 * The ETableCol represents a column to be used inside an ETable.  The
 * ETableCol objects are inserted inside an ETableHeader (which is just a
 * collection of ETableCols).  The ETableHeader is the definition of the
 * order in which columns are shown to the user.
 *
 * The @text argument is the the text that will be shown as a header to the
 * user. @col_idx reflects where the data for this ETableCol object will
 * be fetch from an ETableModel.  So even if the user changes the order
 * of the columns being viewed (the ETableCols in the ETableHeader), the
 * column will always point to the same column inside the ETableModel.
 *
 * The @ecell argument is an ECell object that needs to know how to
 * render the data in the ETableModel for this specific row.
 *
 * Data passed to @compare can be (if not %NULL) a cmp_cache, which
 * can be accessed by e_table_sorting_utils_add_to_cmp_cache() and
 * e_table_sorting_utils_lookup_cmp_cache().
 *
 * Returns: the newly created ETableCol object.
 */
ETableCol *
e_table_col_new (ETableColumnSpecification *spec,
                 const gchar *text,
                 const gchar *icon_name,
                 ECell *ecell,
                 GCompareDataFunc compare)
{
	ETableCol *etc;

	g_return_val_if_fail (E_IS_TABLE_COLUMN_SPECIFICATION (spec), NULL);
	g_return_val_if_fail (ecell != NULL, NULL);
	g_return_val_if_fail (compare != NULL, NULL);
	g_return_val_if_fail (text != NULL, NULL);

	etc = g_object_new (E_TYPE_TABLE_COL, NULL);

	etc->spec = g_object_ref (spec);
	etc->text = g_strdup (text);
	etc->icon_name = g_strdup (icon_name);
	etc->pixbuf = NULL;
	etc->min_width = spec->minimum_width;
	etc->expansion = spec->expansion;
	etc->ecell = g_object_ref (ecell);
	etc->compare = compare;

	etc->selected = 0;

	if (etc->icon_name != NULL)
		etc_load_icon (etc);

	return etc;
}
