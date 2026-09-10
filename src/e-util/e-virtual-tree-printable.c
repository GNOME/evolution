/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-virtual-tree-printable.h"

guint		_e_virtual_tree_get_n_columns	(EVirtualTree *self);
GArray *	_e_virtual_tree_get_display_column_order
						(EVirtualTree *self);
const gchar *	_e_virtual_tree_get_column_title
						(EVirtualTree *self,
						 guint column_index);
gchar *		_e_virtual_tree_get_cell_text	(EVirtualTree *self,
						 guint row_index,
						 guint column_index);
gboolean	_e_virtual_tree_get_cell_toggle	(EVirtualTree *self,
						 guint row_index,
						 guint column_index,
						 gboolean *out_active);
gboolean	_e_virtual_tree_column_is_printable
						(EVirtualTree *self,
						 guint column_index);
gboolean	_e_virtual_tree_column_is_toggle
						(EVirtualTree *self,
						 guint column_index);

#define ROW_PAD 2.0
#define COLUMN_SPACING 8.0
#define MAX_CELL_LINES 5
#define GROUP_INDENT 12.0

typedef struct _PrintColumn {
	guint column_index;
	gdouble width_fraction;
	gboolean is_toggle;
} PrintColumn;

struct _EVirtualTreePrintable {
	EPrintable parent_instance;

	EVirtualTree *vtree;
	GPtrArray *rows; /* GObject *; the full, already-materialized row set */
	GArray *print_columns; /* PrintColumn */
	guint current_row;
	gdouble base_line_height;
};

G_DEFINE_FINAL_TYPE (EVirtualTreePrintable, e_virtual_tree_printable, E_TYPE_PRINTABLE)

static void
evtp_ensure_columns (EVirtualTreePrintable *self)
{
	GArray *display_order;
	gdouble total_width = 0.0;
	guint ii;

	if (self->print_columns)
		return;

	self->print_columns = g_array_new (FALSE, FALSE, sizeof (PrintColumn));

	if (!self->vtree)
		return;

	display_order = _e_virtual_tree_get_display_column_order (self->vtree);
	if (!display_order)
		return;

	for (ii = 0; ii < display_order->len; ii++) {
		guint column_index = g_array_index (display_order, guint, ii);
		GtkTreeViewColumn *tvc;
		PrintColumn pc;
		gint width;

		tvc = e_virtual_tree_get_column (self->vtree, column_index);
		if (!tvc || !gtk_tree_view_column_get_visible (tvc))
			continue;

		if (!_e_virtual_tree_column_is_printable (self->vtree, column_index))
			continue;

		width = gtk_tree_view_column_get_width (tvc);

		pc.column_index = column_index;
		pc.width_fraction = (gdouble) MAX (width, 1);
		pc.is_toggle = _e_virtual_tree_column_is_toggle (self->vtree, column_index);

		g_array_append_val (self->print_columns, pc);
		total_width += pc.width_fraction;
	}

	g_array_unref (display_order);

	if (total_width <= 0.0)
		return;

	for (ii = 0; ii < self->print_columns->len; ii++) {
		PrintColumn *pc = &g_array_index (self->print_columns, PrintColumn, ii);
		pc->width_fraction /= total_width;
	}
}

static gdouble
evtp_get_base_line_height (EVirtualTreePrintable *self,
			   GtkPrintContext *context)
{
	PangoLayout *layout;
	gint pango_height;

	if (self->base_line_height > 0.0)
		return self->base_line_height;

	layout = gtk_print_context_create_pango_layout (context);
	pango_layout_set_text (layout, "Mg", -1);
	pango_layout_get_pixel_size (layout, NULL, &pango_height);
	g_object_unref (layout);

	self->base_line_height = (gdouble) pango_height;

	return self->base_line_height;
}

typedef struct _CellPlan {
	gboolean is_toggle;
	gboolean toggle_active;
	gboolean is_group;
	PangoLayout *layout;
	gdouble column_width;
	gdouble indent;
} CellPlan;

static void
cell_plan_clear (gpointer data)
{
	CellPlan *plan = data;

	g_clear_object (&plan->layout);
}

#define TOGGLE_SIZE_RATIO 0.75

static void
evtp_draw_toggle (cairo_t *cr,
		  gdouble xx,
		  gdouble yy,
		  gdouble column_width,
		  gdouble row_height,
		  gdouble base_line_height,
		  gboolean active)
{
	gdouble side = base_line_height * TOGGLE_SIZE_RATIO;
	gdouble box_x = xx + (column_width - side) / 2.0;
	gdouble box_y = yy + (row_height - side) / 2.0;

	cairo_save (cr);
	cairo_set_line_width (cr, 1.0);
	cairo_rectangle (cr, box_x, box_y, side, side);
	cairo_stroke (cr);

	if (active) {
		cairo_move_to (cr, box_x + side * 0.2, box_y + side * 0.55);
		cairo_line_to (cr, box_x + side * 0.42, box_y + side * 0.8);
		cairo_line_to (cr, box_x + side * 0.85, box_y + side * 0.2);
		cairo_stroke (cr);
	}

	cairo_restore (cr);
}

static gdouble
evtp_measure_row (EVirtualTreePrintable *self,
		  GtkPrintContext *context,
		  gdouble width,
		  gboolean is_header,
		  guint row_index,
		  gboolean is_group_row,
		  guint group_row_depth,
		  GArray **out_cells)
{
	GArray *cells;
	gdouble row_height = 0.0;
	guint ii;

	cells = g_array_new (FALSE, TRUE, sizeof (CellPlan));
	g_array_set_clear_func (cells, cell_plan_clear);

	if (is_group_row) {
		PrintColumn *pc0 = &g_array_index (self->print_columns, PrintColumn, 0);
		CellPlan plan = { 0 };
		PangoLayout *layout;
		PangoAttrList *attrs;
		gchar *text;
		gint pango_height;

		text = _e_virtual_tree_get_cell_text (self->vtree, row_index, pc0->column_index);

		plan.indent = group_row_depth * GROUP_INDENT;
		plan.column_width = width;
		plan.is_group = TRUE;

		layout = gtk_print_context_create_pango_layout (context);
		pango_layout_set_text (layout, text ? text : "", -1);
		pango_layout_set_width (layout, (gint) (MAX (width - plan.indent - COLUMN_SPACING, 1.0) * PANGO_SCALE));
		pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);

		attrs = pango_attr_list_new ();
		pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
		pango_layout_set_attributes (layout, attrs);
		pango_attr_list_unref (attrs);

		pango_layout_get_pixel_size (layout, NULL, &pango_height);
		row_height = (gdouble) pango_height + 2.0 * ROW_PAD;

		plan.layout = layout;
		g_free (text);

		g_array_append_val (cells, plan);

		*out_cells = cells;

		return row_height;
	}

	for (ii = 0; ii < self->print_columns->len; ii++) {
		PrintColumn *pc = &g_array_index (self->print_columns, PrintColumn, ii);
		gdouble column_width = width * pc->width_fraction;
		CellPlan plan = { 0 };

		plan.column_width = column_width;

		if (!is_header && pc->is_toggle) {
			gboolean toggle_active = FALSE;

			_e_virtual_tree_get_cell_toggle (self->vtree, row_index, pc->column_index, &toggle_active);

			plan.is_toggle = TRUE;
			plan.toggle_active = toggle_active;

			row_height = MAX (row_height, evtp_get_base_line_height (self, context));
		} else {
			PangoLayout *layout;
			gchar *text;
			gint pango_height;

			text = is_header ?
				g_strdup (_e_virtual_tree_get_column_title (self->vtree, pc->column_index)) :
				_e_virtual_tree_get_cell_text (self->vtree, row_index, pc->column_index);

			layout = gtk_print_context_create_pango_layout (context);
			pango_layout_set_text (layout, text ? text : "", -1);
			pango_layout_set_width (layout, (gint) (MAX (column_width - COLUMN_SPACING, 1.0) * PANGO_SCALE));

			if (is_header) {
				PangoAttrList *attrs;

				pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);

				attrs = pango_attr_list_new ();
				pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
				pango_layout_set_attributes (layout, attrs);
				pango_attr_list_unref (attrs);
			} else {
				pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
				pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
				pango_layout_set_height (layout, -MAX_CELL_LINES);
				pango_layout_set_single_paragraph_mode (layout, TRUE);
			}

			pango_layout_get_pixel_size (layout, NULL, &pango_height);
			row_height = MAX (row_height, (gdouble) pango_height);

			plan.layout = layout;
			g_free (text);
		}

		g_array_append_val (cells, plan);
	}

	row_height += 2.0 * ROW_PAD;

	*out_cells = cells;

	return row_height;
}

static void
evtp_draw_prepared_row (EVirtualTreePrintable *self,
			GtkPrintContext *context,
			gdouble width,
			gdouble yy,
			gdouble row_height,
			gboolean is_header,
			GArray *cells)
{
	cairo_t *cr = gtk_print_context_get_cairo_context (context);
	gdouble xx = 0.0;
	guint ii;

	for (ii = 0; ii < cells->len; ii++) {
		CellPlan *plan = &g_array_index (cells, CellPlan, ii);

		if (plan->is_group) {
			cairo_save (cr);
			cairo_move_to (cr, plan->indent, yy + ROW_PAD);
			pango_cairo_show_layout (cr, plan->layout);
			cairo_restore (cr);
		} else if (plan->is_toggle) {
			evtp_draw_toggle (cr, xx, yy, plan->column_width, row_height, evtp_get_base_line_height (self, context), plan->toggle_active);
		} else {
			cairo_save (cr);
			cairo_move_to (cr, xx, yy + ROW_PAD);
			pango_cairo_show_layout (cr, plan->layout);
			cairo_restore (cr);
		}

		xx += plan->column_width;
	}

	if (is_header) {
		cairo_save (cr);
		cairo_set_line_width (cr, 0.5);
		cairo_move_to (cr, 0.0, yy + row_height);
		cairo_line_to (cr, width, yy + row_height);
		cairo_stroke (cr);
		cairo_restore (cr);
	}
}

static gdouble
evtp_layout (EPrintable *printable,
	     GtkPrintContext *context,
	     gdouble width,
	     gdouble max_height,
	     gboolean do_draw)
{
	EVirtualTreePrintable *self = E_VIRTUAL_TREE_PRINTABLE (printable);
	EVirtualTreeModel *model;
	guint group_depth;
	gdouble yy = 0.0;
	gdouble header_height;
	GArray *header_cells = NULL;

	evtp_ensure_columns (self);

	if (!self->print_columns || self->print_columns->len == 0 || !self->rows)
		return 0.0;

	model = e_virtual_tree_get_model (self->vtree);
	group_depth = e_virtual_tree_get_group_depth (self->vtree);

	header_height = evtp_measure_row (self, context, width, TRUE, 0, FALSE, 0, &header_cells);

	if (yy + header_height > max_height) {
		g_array_unref (header_cells);
		return yy;
	}

	if (do_draw)
		evtp_draw_prepared_row (self, context, width, yy, header_height, TRUE, header_cells);

	g_array_unref (header_cells);

	yy += header_height;

	while (self->current_row < self->rows->len) {
		GObject *row_object = g_ptr_array_index (self->rows, self->current_row);
		guint row_depth = model ? e_virtual_tree_model_get_depth (model, row_object) : 0;
		gboolean is_group_row = group_depth > 0 && row_depth < group_depth;
		GArray *row_cells = NULL;
		gdouble row_height = evtp_measure_row (self, context, width, FALSE, self->current_row, is_group_row, row_depth, &row_cells);

		if (yy + row_height > max_height) {
			g_array_unref (row_cells);
			break;
		}

		if (do_draw)
			evtp_draw_prepared_row (self, context, width, yy, row_height, FALSE, row_cells);

		g_array_unref (row_cells);

		yy += row_height;

		if (do_draw)
			self->current_row++;
		else
			break;
	}

	return yy;
}

static void
evtp_print_page (EPrintable *printable,
		 GtkPrintContext *context,
		 gdouble width,
		 gdouble height,
		 gboolean quantized)
{
	EVirtualTreePrintable *self = E_VIRTUAL_TREE_PRINTABLE (printable);
	guint before = self->current_row;

	evtp_layout (printable, context, width, height, TRUE);

	/* Guard against an infinite print loop if not even one row fits. */
	if (self->current_row == before && self->rows &&
	    self->current_row < self->rows->len)
		self->current_row++;
}

static gboolean
evtp_data_left (EPrintable *printable)
{
	EVirtualTreePrintable *self = E_VIRTUAL_TREE_PRINTABLE (printable);

	return self->rows && self->current_row < self->rows->len;
}

static void
evtp_reset (EPrintable *printable)
{
	EVirtualTreePrintable *self = E_VIRTUAL_TREE_PRINTABLE (printable);
	EVirtualTreeModel *model;
	guint total;

	g_clear_pointer (&self->rows, g_ptr_array_unref);
	self->current_row = 0;

	if (!self->vtree)
		return;

	model = e_virtual_tree_get_model (self->vtree);
	if (!model) {
		self->rows = g_ptr_array_new_with_free_func (g_object_unref);
		return;
	}

	total = e_virtual_tree_model_get_row_count (model);

	self->rows = total > 0 ?
		e_virtual_tree_model_dup_rows (model, 0, total - 1, FALSE) :
		g_ptr_array_new_with_free_func (g_object_unref);
}

static gdouble
evtp_height (EPrintable *printable,
	     GtkPrintContext *context,
	     gdouble width,
	     gdouble max_height,
	     gboolean quantized)
{
	EVirtualTreePrintable *self = E_VIRTUAL_TREE_PRINTABLE (printable);
	guint saved_row = self->current_row;
	gdouble result;

	result = evtp_layout (printable, context, width, max_height, FALSE);

	self->current_row = saved_row;

	return result;
}

static gboolean
evtp_will_fit (EPrintable *printable,
	       GtkPrintContext *context,
	       gdouble width,
	       gdouble max_height,
	       gboolean quantized)
{
	EVirtualTreePrintable *self = E_VIRTUAL_TREE_PRINTABLE (printable);

	return !self->rows || self->current_row >= self->rows->len ||
		evtp_height (printable, context, width, max_height, quantized) < max_height;
}

static void
e_virtual_tree_printable_dispose (GObject *object)
{
	EVirtualTreePrintable *self = E_VIRTUAL_TREE_PRINTABLE (object);

	g_clear_object (&self->vtree);
	g_clear_pointer (&self->rows, g_ptr_array_unref);
	g_clear_pointer (&self->print_columns, g_array_unref);

	G_OBJECT_CLASS (e_virtual_tree_printable_parent_class)->dispose (object);
}

static void
e_virtual_tree_printable_class_init (EVirtualTreePrintableClass *class)
{
	GObjectClass *object_class;
	EPrintableClass *printable_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = e_virtual_tree_printable_dispose;

	printable_class = E_PRINTABLE_CLASS (class);
	printable_class->print_page = evtp_print_page;
	printable_class->data_left = evtp_data_left;
	printable_class->reset = evtp_reset;
	printable_class->height = evtp_height;
	printable_class->will_fit = evtp_will_fit;
}

static void
e_virtual_tree_printable_init (EVirtualTreePrintable *self)
{
}

EPrintable *
e_virtual_tree_printable_new (EVirtualTree *vtree)
{
	EVirtualTreePrintable *self;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (vtree), NULL);

	self = g_object_new (E_TYPE_VIRTUAL_TREE_PRINTABLE, NULL);
	self->vtree = g_object_ref (vtree);

	return E_PRINTABLE (self);
}
