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

#include "e-cell-number.h"

#include <sys/time.h>
#include <unistd.h>

#include <glib/gi18n.h>

#include "e-misc-utils.h"

G_DEFINE_TYPE (ECellNumber, e_cell_number, E_TYPE_CELL_TEXT)

static gchar *
ecn_get_text (ECellText *cell,
              ETableModel *model,
              gint col,
              gint row)
{
	gpointer value;

	value = e_table_model_value_at (model, col, row);

	return e_format_number (GPOINTER_TO_INT (value));
}

static void
ecn_free_text (ECellText *cell,
	       ETableModel *model,
	       gint col,
               gchar *text)
{
	g_free (text);
}

static void
e_cell_number_class_init (ECellNumberClass *class)
{
	ECellTextClass *ectc = E_CELL_TEXT_CLASS (class);

	ectc->get_text = ecn_get_text;
	ectc->free_text = ecn_free_text;
}

static void
e_cell_number_init (ECellNumber *cell_number)
{
}

/**
 * e_cell_number_new:
 * @fontname: font to be used to render on the screen
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render numbers that
 * that come from the model.  The value returned from the model is
 * interpreted as being an int.
 *
 * See ECellText for other features.
 *
 * Returns: an ECell object that can be used to render numbers.
 */
ECell *
e_cell_number_new (const gchar *fontname,
                   GtkJustification justify)
{
	ECellNumber *ecn = g_object_new (E_TYPE_CELL_NUMBER, NULL);

	e_cell_text_construct (E_CELL_TEXT (ecn), fontname, justify);

	return (ECell *) ecn;
}

