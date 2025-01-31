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

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include <glib/gi18n.h>

#include "e-datetime-format.h"
#include "e-unicode.h"

G_DEFINE_TYPE (ECellDate, e_cell_date, E_TYPE_CELL_TEXT)

static gchar *
ecd_get_text (ECellText *cell,
              ETableModel *model,
              gint col,
              gint row)
{
	gint64 *pdate = e_table_model_value_at (model, col, row);
	gchar *res;

	if (!pdate || *pdate == 0) {
		e_table_model_free_value (model, col, pdate);
		return g_strdup (_("?"));
	}

	res = e_cell_date_value_to_text (E_CELL_DATE (cell), *pdate, FALSE);

	e_table_model_free_value (model, col, pdate);

	return res;
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
e_cell_date_class_init (ECellDateClass *class)
{
	ECellTextClass *ectc = E_CELL_TEXT_CLASS (class);

	ectc->get_text = ecd_get_text;
	ectc->free_text = ecd_free_text;
}

static void
e_cell_date_init (ECellDate *ecd)
{
	g_object_set (ecd, "use-tabular-numbers", TRUE, NULL);
}

/**
 * e_cell_date_new:
 * @fontname: font to be used to render on the screen
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render dates that
 * that come from the model.  The value returned from the model is
 * interpreted as being a time_t.
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
 * Returns: an ECell object that can be used to render dates.
 */
ECell *
e_cell_date_new (const gchar *fontname,
                 GtkJustification justify)
{
	ECellDate *ecd = g_object_new (E_TYPE_CELL_DATE, NULL);

	e_cell_text_construct (E_CELL_TEXT (ecd), fontname, justify);

	return (ECell *) ecd;
}

void
e_cell_date_set_format_component (ECellDate *ecd,
                                  const gchar *fmt_component)
{
	g_return_if_fail (ecd != NULL);

	g_object_set_data_full (
		G_OBJECT (ecd), "fmt-component",
		g_strdup (fmt_component), g_free);
}

gchar *
e_cell_date_value_to_text (ECellDate *ecd,
			   gint64 value,
			   gboolean date_only)
{
	const gchar *fmt_component, *fmt_part = NULL;

	if (value == 0)
		return g_strdup (_("?"));

	fmt_component = g_object_get_data ((GObject *) ecd, "fmt-component");
	if (!fmt_component || !*fmt_component)
		fmt_component = "Default";
	else
		fmt_part = "table";

	return e_datetime_format_format (fmt_component, fmt_part,
		date_only ? DTFormatKindDate : DTFormatKindDateTime, (time_t) value);
}

gchar *
e_cell_date_tm_to_text (ECellDate *ecd,
			struct tm *tm_time,
			gboolean date_only)
{
	const gchar *fmt_component, *fmt_part = NULL;

	if (!tm_time)
		return g_strdup (_("?"));

	fmt_component = g_object_get_data ((GObject *) ecd, "fmt-component");
	if (!fmt_component || !*fmt_component)
		fmt_component = "Default";
	else
		fmt_part = "table";

	return e_datetime_format_format_tm (fmt_component, fmt_part,
		date_only ? DTFormatKindDate : DTFormatKindDateTime, tm_time);
}
