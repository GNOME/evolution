/*
 *
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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "e-table-item.h"
#include "e-cell-checkbox.h"

#include "data/xpm/check-empty.xpm"
#include "data/xpm/check-filled.xpm"

G_DEFINE_TYPE (ECellCheckbox, e_cell_checkbox, E_TYPE_CELL_TOGGLE)

static GdkPixbuf *checks[2];

static void
ecc_print (ECellView *ecell_view,
           GtkPrintContext *context,
           gint model_col,
           gint view_col,
           gint row,
           gdouble width,
           gdouble height)
{
	cairo_t *cr = gtk_print_context_get_cairo_context (context);
	const gint value = GPOINTER_TO_INT (
		e_table_model_value_at (
			ecell_view->e_table_model, model_col, row));
	cairo_save (cr);

	if (value == 1) {
		cairo_set_line_width (cr, 2);
		cairo_move_to (cr, 3, 11);
		cairo_line_to (cr, 7, 14);
		cairo_line_to (cr, 11, 5);
		cairo_stroke (cr);
	}

	cairo_restore (cr);
}

static void
e_cell_checkbox_class_init (ECellCheckboxClass *class)
{
	ECellClass *ecc = E_CELL_CLASS (class);

	ecc->print = ecc_print;
	checks[0] = gdk_pixbuf_new_from_xpm_data (check_empty_xpm);
	checks[1] = gdk_pixbuf_new_from_xpm_data (check_filled_xpm);
}

static void
e_cell_checkbox_init (ECellCheckbox *eccb)
{
	GPtrArray *pixbufs;

	pixbufs = e_cell_toggle_get_pixbufs (E_CELL_TOGGLE (eccb));

	g_ptr_array_add (pixbufs, g_object_ref (checks[0]));
	g_ptr_array_add (pixbufs, g_object_ref (checks[1]));
}

/**
 * e_cell_checkbox_new:
 *
 * Creates a new ECell renderer that can be used to render check
 * boxes.  the data provided from the model is cast to an integer.
 * zero is used for the off display, and non-zero for checked status.
 *
 * Returns: an ECell object that can be used to render checkboxes.
 */
ECell *
e_cell_checkbox_new (void)
{
	return g_object_new (E_TYPE_CELL_CHECKBOX, NULL);
}
