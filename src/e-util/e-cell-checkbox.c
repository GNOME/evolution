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
ecc_draw (ECellView *ecell_view,
	  cairo_t *cr,
	  gint model_col,
	  gint view_col,
	  gint row,
	  ECellFlags flags,
	  gint x1,
	  gint y1,
	  gint x2,
	  gint y2)
{
	GtkStyleContext *style_context;
	GtkWidgetPath *widget_path;
	gint xx, yy, ww, hh;
	const gint value = GPOINTER_TO_INT (e_table_model_value_at (ecell_view->e_table_model, model_col, row));

	if (value != 0 && value != 1)
		return;

	xx = x1;
	yy = y1;
	ww = x2 - x1;
	hh = y2 - y1;

	if (ww > 16) {
		xx += (ww - 16) / 2;
		ww = 16;
	}

	if (hh > 16) {
		yy += (hh - 16) / 2;
		hh = 16;
	}

	widget_path = gtk_widget_path_new ();
	gtk_widget_path_append_type (widget_path, G_TYPE_NONE);
	gtk_widget_path_iter_set_object_name (widget_path, -1, "check");

	style_context = gtk_style_context_new ();
	gtk_style_context_set_path (style_context, widget_path);
	gtk_style_context_set_state (style_context,
		(value ? GTK_STATE_FLAG_CHECKED : 0) |
		((flags & E_CELL_SELECTED) != 0 ? GTK_STATE_FLAG_SELECTED : 0));

	gtk_render_frame (style_context, cr, xx, yy, ww, hh);
	gtk_render_check (style_context, cr, xx, yy, ww, hh);

	gtk_widget_path_unref (widget_path);
	g_object_unref (style_context);
}

static void
e_cell_checkbox_class_init (ECellCheckboxClass *class)
{
	ECellClass *ecc = E_CELL_CLASS (class);

	ecc->print = ecc_print;
	ecc->draw = ecc_draw;
}

static GdkPixbuf *
ecc_get_check_singleton (gboolean the_empty)
{
	static GdkPixbuf *check_empty = NULL;
	static GdkPixbuf *check_filled = NULL;
	GdkPixbuf **pcheck;

	if (the_empty)
		pcheck = &check_empty;
	else
		pcheck = &check_filled;

	if (*pcheck)
		return g_object_ref (*pcheck);

	*pcheck = gdk_pixbuf_new_from_xpm_data (the_empty ? check_empty_xpm : check_filled_xpm);

	g_object_weak_ref (G_OBJECT (*pcheck), (GWeakNotify) g_nullify_pointer, pcheck);

	return *pcheck;
}

static void
e_cell_checkbox_init (ECellCheckbox *eccb)
{
	GPtrArray *pixbufs;

	pixbufs = e_cell_toggle_get_pixbufs (E_CELL_TOGGLE (eccb));

	g_ptr_array_add (pixbufs, ecc_get_check_singleton (TRUE));
	g_ptr_array_add (pixbufs, ecc_get_check_singleton (FALSE));
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
