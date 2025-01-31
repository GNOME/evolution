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

#include "e-cell-date.h"
#include "e-cell-date-int.h"

struct _ECellDateIntPrivate {
	gint dummy;
};

G_DEFINE_TYPE_WITH_PRIVATE (ECellDateInt, e_cell_date_int, E_TYPE_CELL_DATE)

static gchar *
ecdi_get_text (ECellText *cell,
               ETableModel *model,
               gint col,
               gint row)
{
	gint int_date = GPOINTER_TO_INT (e_table_model_value_at (model, col, row));
	GDate *date;
	struct tm tm;

	if (int_date <= 0)
		return g_strdup ("");

	date = g_date_new_dmy (int_date % 100, (int_date / 100) % 100, int_date / 10000);
	if (!date || !g_date_valid (date)) {
		if (date)
			g_date_free (date);
		return g_strdup ("");
	}

	g_date_to_struct_tm (date, &tm);

	g_date_free (date);

	return e_cell_date_tm_to_text (E_CELL_DATE (cell), &tm, TRUE);
}

static void
e_cell_date_int_class_init (ECellDateIntClass *class)
{
	ECellTextClass *ectc = E_CELL_TEXT_CLASS (class);

	ectc->get_text = ecdi_get_text;
}

static void
e_cell_date_int_init (ECellDateInt *ecdi)
{
	ecdi->priv = e_cell_date_int_get_instance_private (ecdi);
}

/**
 * e_cell_date_int_new:
 * @fontname: font to be used to render on the screen
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render dates that
 * that come from the model. The value returned from the model is
 * interpreted as being an integer with the format YYYYMMDD.
 *
 * The ECellDate object support a large set of properties that can be
 * configured through the Gtk argument system and allows the user to have
 * a finer control of the way the string is displayed.  The arguments supported
 * allow the control of strikeout, bold, color and a date filter.
 *
 * The arguments "strikeout_column", "underline_column", "bold_column"
 * and "color_column" set and return an integer that points to a
 * column in the model that controls these settings.  So controlling
 * the way things are rendered is achieved by having special columns
 * in the model that will be used to flag whether the date should be
 * rendered with strikeout, underline, or bolded.  In the case of the
 * "color_column" argument, the column in the model is expected to
 * have a string that can be parsed by gdk_rgba_parse().
 *
 * Returns: an ECell object that can be used to render dates encoded as integers.
 */
ECell *
e_cell_date_int_new (const gchar *fontname,
		     GtkJustification justify)
{
	ECellDateInt *ecdi = g_object_new (E_TYPE_CELL_DATE_INT, NULL);

	e_cell_text_construct (E_CELL_TEXT (ecdi), fontname, justify);

	return (ECell *) ecdi;
}
