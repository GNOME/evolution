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

#include <sys/time.h>
#include <unistd.h>

#include "e-cell-size.h"

G_DEFINE_TYPE (ECellSize, e_cell_size, E_TYPE_CELL_TEXT)

static gchar *
ecd_get_text (ECellText *cell,
              ETableModel *model,
              gint col,
              gint row)
{
	gint size = GPOINTER_TO_INT (e_table_model_value_at (model, col, row));
	gfloat fsize;

	if (size < 1024) {
		return g_strdup_printf ("%d bytes", size);
	} else {
		fsize = ((gfloat) size) / 1024.0;
		if (fsize < 1024.0) {
			return g_strdup_printf ("%d K", (gint) fsize);
		} else {
			fsize /= 1024.0;
			return g_strdup_printf ("%.1f MB", fsize);
		}
	}
}

static void
ecd_free_text (ECellText *cell,
	       ETableModel *model,
	       gint col,
               gchar *text)
{
	g_free (text);
}

static void
e_cell_size_class_init (ECellSizeClass *class)
{
	ECellTextClass *ectc = E_CELL_TEXT_CLASS (class);

	ectc->get_text = ecd_get_text;
	ectc->free_text = ecd_free_text;
}

static void
e_cell_size_init (ECellSize *e_cell_size)
{
	g_object_set (e_cell_size, "use-tabular-numbers", TRUE, NULL);
}

/**
 * e_cell_size_new:
 * @fontname: font to be used to render on the screen
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render file sizes
 * that come from the model.  The value returned from the model
 * is interpreted as being a time_t.
 *
 * The ECellSize object support a large set of properties that can be
 * configured through the Gtk argument system and allows the user to
 * have a finer control of the way the string is displayed.  The
 * arguments supported allow the control of strikeout, underline,
 * bold, color and a size filter.
 *
 * The arguments "strikeout_column", "underline_column", "bold_column"
 * and "color_column" set and return an integer that points to a
 * column in the model that controls these settings.  So controlling
 * the way things are rendered is achieved by having special columns
 * in the model that will be used to flag whether the size should be
 * rendered with strikeout, underline, or bolded.  In the case of the
 * "color_column" argument, the column in the model is expected to
 * have a string that can be parsed by gdk_rgba_parse().
 *
 * Returns: an ECell object that can be used to render file sizes.  */
ECell *
e_cell_size_new (const gchar *fontname,
                 GtkJustification justify)
{
	ECellSize *ecd = g_object_new (E_TYPE_CELL_SIZE, NULL);

	e_cell_text_construct (E_CELL_TEXT (ecd), fontname, justify);

	return (ECell *) ecd;
}

