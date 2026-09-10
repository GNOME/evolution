/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <glib/gi18n-lib.h>
#include "e-alert-dialog.h"
#include "e-misc-utils.h"
#include "e-ui-action.h"
#include "e-ui-manager.h"
#include "e-util-enumtypes.h"
#include "e-virtual-tree.h"
#include "e-virtual-tree-accessible.h"
#include "e-virtual-tree-customize-popover.h"
#include "e-virtual-tree-printable.h"

/* Fallback values; actual values are read from GtkTreeView style properties */
#define DEFAULT_EXPANDER_SIZE 16
#define DEFAULT_HORIZONTAL_SEPARATOR 4

#define SCROLL_STEP_PIXELS 50.0

#define MIN_PROPORTIONAL_COLUMN_WIDTH 24

#define E_TYPE_CELL_RENDERER_EXPANDER (e_cell_renderer_expander_get_type ())
G_DECLARE_FINAL_TYPE (ECellRendererExpander, e_cell_renderer_expander, E, CELL_RENDERER_EXPANDER, GtkCellRenderer)

enum {
	EXP_PROP_0,
	EXP_PROP_DEPTH,
	EXP_PROP_EXPANDABLE,
	EXP_PROP_EXPANDED,
	EXP_N_PROPS
};

struct _ECellRendererExpander {
	GtkCellRenderer parent_instance;
	guint depth;
	gboolean expandable;
	gboolean expanded;
};

G_DEFINE_FINAL_TYPE (ECellRendererExpander, e_cell_renderer_expander, GTK_TYPE_CELL_RENDERER)

/* Match GtkTreeView's internal gtk_tree_view_get_expander_size():
 *   expander_size (per-level step) = style "expander-size" + style "horizontal-separator" / 2
 *   expander_render_size (drawn)   = style "expander-size"
 *   padding before triangle        = (expander_size - expander_render_size) = hsep / 2
 */
static void
get_expander_metrics (GtkWidget *widget,
		      gint *out_expander_size,
		      gint *out_expander_render_size)
{
	gint style_expander_size = DEFAULT_EXPANDER_SIZE;
	gint horizontal_separator = DEFAULT_HORIZONTAL_SEPARATOR;

	if (GTK_IS_TREE_VIEW (widget)) {
		gtk_widget_style_get (widget,
			"expander-size", &style_expander_size,
			"horizontal-separator", &horizontal_separator,
			NULL);
	}

	if (out_expander_size)
		*out_expander_size = style_expander_size + horizontal_separator / 2;
	if (out_expander_render_size)
		*out_expander_render_size = style_expander_size;
}

static void
e_cell_renderer_expander_get_preferred_width (GtkCellRenderer *cell,
					      GtkWidget *widget,
					      gint *minimum,
					      gint *natural)
{
	ECellRendererExpander *self = E_CELL_RENDERER_EXPANDER (cell);
	gint expander_size, width;

	get_expander_metrics (widget, &expander_size, NULL);
	width = ((gint) self->depth + 1) * expander_size;

	if (minimum)
		*minimum = width;
	if (natural)
		*natural = width;
}

static void
e_cell_renderer_expander_get_preferred_height (GtkCellRenderer *cell,
					       GtkWidget *widget,
					       gint *minimum,
					       gint *natural)
{
	gint expander_render_size;

	get_expander_metrics (widget, NULL, &expander_render_size);

	if (minimum)
		*minimum = expander_render_size;
	if (natural)
		*natural = expander_render_size;
}

static void
e_cell_renderer_expander_render (GtkCellRenderer *cell,
				 cairo_t *cr,
				 GtkWidget *widget,
				 const GdkRectangle *background_area,
				 const GdkRectangle *cell_area,
				 GtkCellRendererState flags)
{
	ECellRendererExpander *self = E_CELL_RENDERER_EXPANDER (cell);
	GtkStyleContext *context;
	gint expander_size, expander_render_size;
	gint x, y, area_height;

	if (!self->expandable)
		return;

	get_expander_metrics (widget, &expander_size, &expander_render_size);

	context = gtk_widget_get_style_context (widget);
	gtk_style_context_save (context);
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_EXPANDER);
	gtk_style_context_set_state (context,
		(gtk_style_context_get_state (context) & ~GTK_STATE_FLAG_PRELIGHT) |
		(self->expanded ? GTK_STATE_FLAG_CHECKED : 0));

	if (g_object_get_data (G_OBJECT (cell), "center-in-area"))
		x = cell_area->x + (cell_area->width - expander_render_size) / 2;
	else if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
		x = cell_area->x + cell_area->width - ((gint) (self->depth + 1) * expander_size) + (expander_size - expander_render_size);
	else
		x = cell_area->x + (gint) self->depth * expander_size + (expander_size - expander_render_size);

	area_height = cell_area->height;
	if (area_height % 2 != expander_render_size % 2)
		area_height -= 1;

	y = cell_area->y + (area_height - expander_render_size) / 2;

	gtk_render_expander (context, cr,
		(gdouble) x, (gdouble) y,
		(gdouble) expander_render_size,
		(gdouble) expander_render_size);

	gtk_style_context_restore (context);
}

static void
e_cell_renderer_expander_set_property (GObject *object,
				       guint prop_id,
				       const GValue *value,
				       GParamSpec *pspec)
{
	ECellRendererExpander *self = E_CELL_RENDERER_EXPANDER (object);

	switch (prop_id) {
	case EXP_PROP_DEPTH:
		self->depth = g_value_get_uint (value);
		break;
	case EXP_PROP_EXPANDABLE:
		self->expandable = g_value_get_boolean (value);
		break;
	case EXP_PROP_EXPANDED:
		self->expanded = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
e_cell_renderer_expander_get_property (GObject *object,
				       guint prop_id,
				       GValue *value,
				       GParamSpec *pspec)
{
	ECellRendererExpander *self = E_CELL_RENDERER_EXPANDER (object);

	switch (prop_id) {
	case EXP_PROP_DEPTH:
		g_value_set_uint (value, self->depth);
		break;
	case EXP_PROP_EXPANDABLE:
		g_value_set_boolean (value, self->expandable);
		break;
	case EXP_PROP_EXPANDED:
		g_value_set_boolean (value, self->expanded);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
e_cell_renderer_expander_class_init (ECellRendererExpanderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

	object_class->set_property = e_cell_renderer_expander_set_property;
	object_class->get_property = e_cell_renderer_expander_get_property;

	cell_class->get_preferred_width = e_cell_renderer_expander_get_preferred_width;
	cell_class->get_preferred_height = e_cell_renderer_expander_get_preferred_height;
	cell_class->render = e_cell_renderer_expander_render;

	g_object_class_install_property (object_class, EXP_PROP_DEPTH,
		g_param_spec_uint ("depth", NULL, NULL, 0, G_MAXUINT, 0,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class, EXP_PROP_EXPANDABLE,
		g_param_spec_boolean ("expandable", NULL, NULL, FALSE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class, EXP_PROP_EXPANDED,
		g_param_spec_boolean ("expanded", NULL, NULL, FALSE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
e_cell_renderer_expander_init (ECellRendererExpander *self)
{
}

#define E_CELL_RENDERER_BUTTON_MARGIN 2

struct _ECellRendererButton {
	GtkCellRendererText parent_instance;
	GtkWidget *dummy_button;
};

G_DEFINE_FINAL_TYPE (ECellRendererButton, e_cell_renderer_button, GTK_TYPE_CELL_RENDERER_TEXT)

static GtkStyleContext *
e_cell_renderer_button_ensure_context (ECellRendererButton *self)
{
	if (!self->dummy_button)
		self->dummy_button = g_object_ref_sink (gtk_button_new ());

	return gtk_widget_get_style_context (self->dummy_button);
}

static void
e_cell_renderer_button_render (GtkCellRenderer *cell,
			       cairo_t *cr,
			       GtkWidget *widget,
			       const GdkRectangle *background_area,
			       const GdkRectangle *cell_area,
			       GtkCellRendererState flags)
{
	ECellRendererButton *self = E_CELL_RENDERER_BUTTON (cell);
	GtkStyleContext *button_context = e_cell_renderer_button_ensure_context (self);
	GtkStyleContext *widget_context;
	GtkStateFlags widget_state;
	GtkStateFlags state = GTK_STATE_FLAG_NORMAL;
	GdkRectangle centered_area;
	GdkRectangle button_rect;
	gint natural_height;

	gtk_cell_renderer_get_preferred_height (cell, widget, NULL, &natural_height);

	centered_area = *background_area;
	centered_area.height = MIN (natural_height, background_area->height);
	centered_area.y = background_area->y + (background_area->height - centered_area.height) / 2;

	button_rect = centered_area;
	button_rect.x += E_CELL_RENDERER_BUTTON_MARGIN;
	button_rect.width = MAX (centered_area.width - 2 * E_CELL_RENDERER_BUTTON_MARGIN, 0);

	if ((flags & GTK_CELL_RENDERER_PRELIT) != 0)
		state |= GTK_STATE_FLAG_PRELIGHT;
	if ((flags & GTK_CELL_RENDERER_INSENSITIVE) != 0)
		state |= GTK_STATE_FLAG_INSENSITIVE;

	gtk_style_context_set_state (button_context, state);

	gtk_render_background (button_context, cr,
		button_rect.x, button_rect.y, button_rect.width, button_rect.height);
	gtk_render_frame (button_context, cr,
		button_rect.x, button_rect.y, button_rect.width, button_rect.height);

	widget_context = gtk_widget_get_style_context (widget);
	widget_state = gtk_style_context_get_state (widget_context);

	gtk_style_context_save (widget_context);
	gtk_style_context_set_state (widget_context, widget_state & ~GTK_STATE_FLAG_SELECTED);

	GTK_CELL_RENDERER_CLASS (e_cell_renderer_button_parent_class)->render (
		cell, cr, widget, &centered_area, &centered_area, flags & ~GTK_CELL_RENDERER_SELECTED);

	gtk_style_context_restore (widget_context);
}

static void
e_cell_renderer_button_finalize (GObject *object)
{
	ECellRendererButton *self = E_CELL_RENDERER_BUTTON (object);

	if (self->dummy_button) {
		gtk_widget_destroy (self->dummy_button);
		g_clear_object (&self->dummy_button);
	}

	G_OBJECT_CLASS (e_cell_renderer_button_parent_class)->finalize (object);
}

static void
e_cell_renderer_button_class_init (ECellRendererButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = e_cell_renderer_button_finalize;

	GTK_CELL_RENDERER_CLASS (klass)->render = e_cell_renderer_button_render;
}

static void
e_cell_renderer_button_init (ECellRendererButton *self)
{
	g_object_set (self, "xalign", 0.5f,
		"xpad", E_CELL_RENDERER_BUTTON_MARGIN + 6,
		"ypad", 3,
		NULL);
}

/**
 * e_cell_renderer_button_new:
 *
 * Creates a new #GtkCellRendererText subclass which paints itself as
 * a themed button (background and frame from the "button" style
 * class) before drawing its text, for actions embedded in an
 * #EVirtualTree row.
 *
 * Returns: (transfer full): a new #GtkCellRenderer
 *
 * Since: 3.64
 **/
GtkCellRenderer *
e_cell_renderer_button_new (void)
{
	return g_object_new (E_TYPE_CELL_RENDERER_BUTTON, NULL);
}

struct _ECellRendererToggle {
	GtkCellRendererToggle parent_instance;
};

G_DEFINE_FINAL_TYPE (ECellRendererToggle, e_cell_renderer_toggle, GTK_TYPE_CELL_RENDERER_TOGGLE)

static void
e_cell_renderer_toggle_render (GtkCellRenderer *cell,
			       cairo_t *cr,
			       GtkWidget *widget,
			       const GdkRectangle *background_area,
			       const GdkRectangle *cell_area,
			       GtkCellRendererState flags)
{
	GtkStyleContext *context;
	GtkWidgetPath *path;
	GtkStateFlags state;
	GtkBorder padding, border;
	gboolean radio = FALSE, active = FALSE, inconsistent = FALSE, activatable = FALSE;
	gint indicator_size = 0, indicator_width, indicator_height;
	gint calc_width, calc_height;
	gint xoffset, yoffset, width, height;
	gint xpad, ypad;
	gfloat xalign, yalign;

	g_object_get (cell,
		"radio", &radio,
		"active", &active,
		"inconsistent", &inconsistent,
		"activatable", &activatable,
		"indicator-size", &indicator_size,
		NULL);

	gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

	state = gtk_cell_renderer_get_state (cell, widget, flags);

	if (!activatable)
		state |= GTK_STATE_FLAG_INSENSITIVE;

	state &= ~(GTK_STATE_FLAG_INCONSISTENT | GTK_STATE_FLAG_CHECKED);

	if (inconsistent)
		state |= GTK_STATE_FLAG_INCONSISTENT;
	if (active)
		state |= GTK_STATE_FLAG_CHECKED;

	path = gtk_widget_path_copy (gtk_widget_get_path (widget));
	gtk_widget_path_append_type (path, G_TYPE_NONE);
	gtk_widget_path_iter_set_object_name (path, -1, radio ? "radio" : "check");

	context = gtk_style_context_new ();
	gtk_style_context_set_path (context, path);
	gtk_style_context_set_parent (context, gtk_widget_get_style_context (widget));
	gtk_widget_path_unref (path);

	gtk_style_context_set_state (context, state);

	gtk_style_context_get_padding (context, gtk_style_context_get_state (context), &padding);
	gtk_style_context_get_border (context, gtk_style_context_get_state (context), &border);

	if (indicator_size != 0) {
		indicator_width = indicator_size;
		indicator_height = indicator_size;
	} else {
		gtk_style_context_get (context, gtk_style_context_get_state (context),
			"min-width", &indicator_width,
			"min-height", &indicator_height,
			NULL);

		if (indicator_width == 0)
			indicator_width = 16;
		if (indicator_height == 0)
			indicator_height = 16;
	}

	calc_width = indicator_width + xpad * 2 + padding.left + padding.right + border.left + border.right;
	calc_height = indicator_height + ypad * 2 + padding.top + padding.bottom + border.top + border.bottom;

	gtk_cell_renderer_get_alignment (cell, &xalign, &yalign);

	xoffset = MAX (((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
		(1.0 - xalign) : xalign) * (cell_area->width - calc_width), 0);
	yoffset = MAX (yalign * (cell_area->height - calc_height), 0);

	width = calc_width - xpad * 2;
	height = calc_height - ypad * 2;

	if (width <= 0 || height <= 0) {
		g_object_unref (context);
		return;
	}

	cairo_save (cr);

	gdk_cairo_rectangle (cr, cell_area);
	cairo_clip (cr);

	gtk_render_background (context, cr,
		cell_area->x + xoffset + xpad,
		cell_area->y + yoffset + ypad,
		width, height);

	gtk_render_frame (context, cr,
		cell_area->x + xoffset + xpad,
		cell_area->y + yoffset + ypad,
		width, height);

	if (radio) {
		gtk_render_option (context, cr,
			cell_area->x + xoffset + xpad + padding.left + border.left,
			cell_area->y + yoffset + ypad + padding.top + border.top,
			width - padding.left - padding.right - border.left - border.right,
			height - padding.top - padding.bottom - border.top - border.bottom);
	} else {
		gtk_render_check (context, cr,
			cell_area->x + xoffset + xpad + padding.left + border.left,
			cell_area->y + yoffset + ypad + padding.top + border.top,
			width - padding.left - padding.right - border.left - border.right,
			height - padding.top - padding.bottom - border.top - border.bottom);
	}

	g_object_unref (context);
	cairo_restore (cr);
}

static void
e_cell_renderer_toggle_class_init (ECellRendererToggleClass *klass)
{
	GTK_CELL_RENDERER_CLASS (klass)->render = e_cell_renderer_toggle_render;
}

static void
e_cell_renderer_toggle_init (ECellRendererToggle *self)
{
}

/**
 * e_cell_renderer_toggle_new:
 *
 * Creates a new #GtkCellRendererToggle subclass which paints the same
 * check/radio indicator as its parent class. All other behavior
 * (activation, editing) is inherited unchanged.
 *
 * Returns: (transfer full): a new #GtkCellRenderer
 *
 * Since: 3.64
 **/
GtkCellRenderer *
e_cell_renderer_toggle_new (void)
{
	return g_object_new (E_TYPE_CELL_RENDERER_TOGGLE, NULL);
}

#define E_TYPE_CELL_AREA_LINES (e_cell_area_lines_get_type ())
G_DECLARE_FINAL_TYPE (ECellAreaLines, e_cell_area_lines, E, CELL_AREA_LINES, GtkCellArea)

struct _ECellAreaLines {
	GtkCellArea parent_instance;
	GPtrArray *renderers;

	gboolean alternating_enabled;
	GdkRGBA *alternating_color;
	gboolean prelight_enabled;

	/* Set by cell_data_func_bridge() right before render(), which has
	 * no direct access to the row being rendered. */
	struct _row_render_data {
		GdkRGBA *custom_color;
		GdkRGBA *background_color;
		guint visible_index;
		gboolean is_group;
	} row_render_data;
};

G_DEFINE_FINAL_TYPE (ECellAreaLines, e_cell_area_lines, GTK_TYPE_CELL_AREA)

#define LINE_DATA_KEY "evtree-line"
#define EXPAND_DATA_KEY "evtree-expand"
#define SPAN_DATA_KEY "evtree-span"

static guint
renderer_get_line (GtkCellRenderer *renderer)
{
	return GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (renderer), LINE_DATA_KEY));
}

static gboolean
renderer_get_expand (GtkCellRenderer *renderer)
{
	return GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (renderer), EXPAND_DATA_KEY));
}

static gboolean
renderer_get_span (GtkCellRenderer *renderer)
{
	return GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (renderer), SPAN_DATA_KEY));
}

static guint
cell_area_lines_get_max_line (ECellAreaLines *self)
{
	guint ii, max_line = 0;

	for (ii = 0; ii < self->renderers->len; ii++) {
		GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
		guint line = renderer_get_line (renderer);

		if (line != G_MAXUINT)
			max_line = MAX (max_line, line);
	}

	return max_line;
}

static gint
cell_area_lines_get_span_width (ECellAreaLines *self,
				GtkWidget *widget)
{
	guint ii;
	gint span_width = 0;

	for (ii = 0; ii < self->renderers->len; ii++) {
		GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
		gint nat_w;

		if (!renderer_get_span (renderer))
			continue;
		if (!gtk_cell_renderer_get_visible (renderer))
			continue;

		gtk_cell_renderer_get_preferred_width (renderer, widget, NULL, &nat_w);
		span_width += nat_w;
	}

	return span_width;
}

static GtkCellRenderer *
cell_area_lines_find_expander (ECellAreaLines *self,
			       guint *out_line)
{
	guint ii;

	for (ii = 0; ii < self->renderers->len; ii++) {
		GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);

		if (E_IS_CELL_RENDERER_EXPANDER (renderer)) {
			if (out_line)
				*out_line = renderer_get_line (renderer);
			return renderer;
		}
	}

	return NULL;
}

static gint
cell_area_lines_get_indent (ECellAreaLines *self,
			    GtkWidget *widget,
			    guint *out_expander_line)
{
	GtkCellRenderer *exp;
	guint exp_line = 0;
	gint indent = 0;

	exp = cell_area_lines_find_expander (self, &exp_line);
	if (exp && gtk_cell_renderer_get_visible (exp)) {
		gtk_cell_renderer_get_preferred_width (exp, widget, &indent, NULL);
		if (out_expander_line)
			*out_expander_line = exp_line;
	}

	return indent;
}

static gint
cell_area_lines_get_trailing_fixed_width (ECellAreaLines *self,
					  GtkWidget *widget,
					  guint after_index,
					  guint line)
{
	guint jj;
	gint total = 0;

	for (jj = after_index + 1; jj < self->renderers->len; jj++) {
		GtkCellRenderer *other = g_ptr_array_index (self->renderers, jj);
		gint other_nat_w;

		if (renderer_get_span (other))
			continue;
		if (renderer_get_line (other) != line)
			continue;
		if (!gtk_cell_renderer_get_visible (other))
			continue;
		if (E_IS_CELL_RENDERER_EXPANDER (other))
			continue;
		if (renderer_get_expand (other))
			continue;

		gtk_cell_renderer_get_preferred_width (other, widget, NULL, &other_nat_w);
		total += other_nat_w;
	}

	return total;
}

static void
e_cell_area_lines_add (GtkCellArea *area,
		       GtkCellRenderer *renderer)
{
	ECellAreaLines *self = E_CELL_AREA_LINES (area);

	g_ptr_array_add (self->renderers, g_object_ref_sink (renderer));
}

static void
e_cell_area_lines_remove (GtkCellArea *area,
			  GtkCellRenderer *renderer)
{
	ECellAreaLines *self = E_CELL_AREA_LINES (area);

	g_ptr_array_remove (self->renderers, renderer);
}

static void
e_cell_area_lines_foreach (GtkCellArea *area,
			   GtkCellCallback callback,
			   gpointer user_data)
{
	ECellAreaLines *self = E_CELL_AREA_LINES (area);
	guint ii;

	for (ii = 0; ii < self->renderers->len; ii++) {
		callback (g_ptr_array_index (self->renderers, ii), user_data);
	}
}

static void
e_cell_area_lines_foreach_alloc (GtkCellArea *area,
				 GtkCellAreaContext *context,
				 GtkWidget *widget,
				 const GdkRectangle *cell_area,
				 const GdkRectangle *background_area,
				 GtkCellAllocCallback callback,
				 gpointer user_data)
{
	ECellAreaLines *self = E_CELL_AREA_LINES (area);
	gboolean is_rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
	guint max_line, line, ii;
	gint y_offset = 0;
	gint indent, span_width, span_height = 0, span_x_offset;

	max_line = cell_area_lines_get_max_line (self);
	indent = cell_area_lines_get_indent (self, widget, NULL);
	span_width = cell_area_lines_get_span_width (self, widget);

	span_x_offset = indent;
	for (ii = 0; ii < self->renderers->len; ii++) {
		GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
		GdkRectangle alloc, bg;
		gint nat_w, min_h;

		if (!renderer_get_span (renderer))
			continue;
		if (!gtk_cell_renderer_get_visible (renderer))
			continue;

		gtk_cell_renderer_get_preferred_width (renderer, widget, NULL, &nat_w);
		gtk_cell_renderer_get_preferred_height (renderer, widget, &min_h, NULL);
		span_height = MAX (span_height, min_h);

		alloc.x = cell_area->x + span_x_offset;
		alloc.y = cell_area->y;
		alloc.width = nat_w;
		alloc.height = cell_area->height;

		if (is_rtl)
			alloc.x = cell_area->x + cell_area->width - (alloc.x - cell_area->x) - alloc.width;

		bg = alloc;
		callback (renderer, &alloc, &bg, user_data);

		span_x_offset += nat_w;
	}

	for (line = 0; line <= max_line; line++) {
		gint line_height = 0;
		gint x_offset = indent + span_width;

		for (ii = 0; ii < self->renderers->len; ii++) {
			GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
			gint min_h;

			if (renderer_get_span (renderer))
				continue;
			if (renderer_get_line (renderer) != line)
				continue;
			if (!gtk_cell_renderer_get_visible (renderer))
				continue;

			gtk_cell_renderer_get_preferred_height (renderer, widget, &min_h, NULL);
			line_height = MAX (line_height, min_h);
		}

		for (ii = 0; ii < self->renderers->len; ii++) {
			GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
			GdkRectangle alloc, bg;
			gint min_w, nat_w, w;

			if (renderer_get_span (renderer))
				continue;
			if (renderer_get_line (renderer) != line)
				continue;
			if (!gtk_cell_renderer_get_visible (renderer))
				continue;

			if (E_IS_CELL_RENDERER_EXPANDER (renderer)) {
				alloc.x = cell_area->x;
				alloc.width = indent;
				if (span_height > 0) {
					alloc.y = cell_area->y;
					alloc.height = span_height;
				} else if (span_width > 0) {
					alloc.y = cell_area->y;
					alloc.height = cell_area->height;
				} else {
					alloc.y = cell_area->y + y_offset;
					alloc.height = line_height;
				}

				if (is_rtl)
					alloc.x = cell_area->x + cell_area->width - alloc.width;

				bg = alloc;
				callback (renderer, &alloc, &bg, user_data);
				continue;
			}

			gtk_cell_renderer_get_preferred_width (renderer, widget, &min_w, &nat_w);

			w = nat_w;
			if (renderer_get_expand (renderer)) {
				gint remaining = cell_area->width - x_offset - cell_area_lines_get_trailing_fixed_width (self, widget, ii, line);
				if (remaining > 0)
					w = remaining;
			} else if (x_offset + w > cell_area->width) {
				w = MAX (cell_area->width - x_offset, 0);
			}

			alloc.x = cell_area->x + x_offset;
			alloc.y = cell_area->y + y_offset;
			alloc.width = w;
			alloc.height = line_height;

			if (is_rtl)
				alloc.x = cell_area->x + cell_area->width - (alloc.x - cell_area->x) - alloc.width;

			bg = alloc;

			callback (renderer, &alloc, &bg, user_data);

			x_offset += w;
		}

		y_offset += line_height;
	}
}

#define ROW_ZEBRA_TINT_AMOUNT 0.04
#define ROW_PRELIT_TINT_AMOUNT 0.06

static void
apply_brightness_tint (GdkRGBA *color,
		       gdouble amount)
{
	e_utils_tint_color (color, e_utils_get_color_brightness (color) < 128.0 ? amount : -amount);
}

static gboolean
cell_area_lines_compute_row_background (ECellAreaLines *self,
					GtkWidget *widget,
					GtkCellRendererState flags,
					GdkRGBA *out_color,
					gboolean *out_is_custom)
{
	gboolean selected = (flags & GTK_CELL_RENDERER_SELECTED) != 0;
	gboolean prelit = self->prelight_enabled && (flags & GTK_CELL_RENDERER_PRELIT) != 0;
	gboolean striped = self->alternating_enabled && (self->row_render_data.visible_index % 2) == 0;
	gboolean focused;

	*out_is_custom = FALSE;

	if (self->row_render_data.is_group)
		return FALSE;

	if (selected && self->row_render_data.custom_color) {
		*out_color = *self->row_render_data.custom_color;
		*out_is_custom = TRUE;

		if (striped)
			apply_brightness_tint (out_color, ROW_ZEBRA_TINT_AMOUNT);

		if (prelit)
			apply_brightness_tint (out_color, ROW_PRELIT_TINT_AMOUNT);

		return TRUE;
	}

	if (!selected && self->row_render_data.background_color) {
		*out_color = *self->row_render_data.background_color;
		*out_is_custom = TRUE;

		if (prelit)
			apply_brightness_tint (out_color, ROW_PRELIT_TINT_AMOUNT);

		return TRUE;
	}

	if (selected) {
		focused = gtk_widget_has_focus (widget);
		e_utils_get_selected_bg_color (widget, focused, out_color);

		if (striped)
			apply_brightness_tint (out_color, ROW_ZEBRA_TINT_AMOUNT);

		if (prelit)
			apply_brightness_tint (out_color, ROW_PRELIT_TINT_AMOUNT);

		return TRUE;
	}

	if (!striped && !prelit)
		return FALSE;

	if (striped && self->alternating_color) {
		*out_color = *self->alternating_color;
	} else {
		e_utils_get_theme_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, out_color);

		if (striped)
			apply_brightness_tint (out_color, ROW_ZEBRA_TINT_AMOUNT);
	}

	if (prelit)
		apply_brightness_tint (out_color, ROW_PRELIT_TINT_AMOUNT);

	return TRUE;
}

static void
virtual_tree_render_cell (GtkCellRenderer *renderer,
			  cairo_t *cr,
			  GtkWidget *widget,
			  const GdkRectangle *background_area,
			  const GdkRectangle *cell_area,
			  GtkCellRendererState flags,
			  const GdkRGBA *custom_row_foreground)
{
	if (GTK_IS_CELL_RENDERER_PIXBUF (renderer)) {
		GtkStyleContext *widget_context = gtk_widget_get_style_context (widget);
		GtkStateFlags saved_state = gtk_style_context_get_state (widget_context);

		gtk_style_context_set_state (widget_context, saved_state & ~GTK_STATE_FLAG_PRELIGHT);
		gtk_cell_renderer_render (renderer, cr, widget, background_area, cell_area, flags & ~GTK_CELL_RENDERER_PRELIT);
		gtk_style_context_set_state (widget_context, saved_state);
	} else if (custom_row_foreground && GTK_IS_CELL_RENDERER_TEXT (renderer) && !E_IS_CELL_RENDERER_BUTTON (renderer)) {
		g_object_set (renderer, "foreground-rgba", custom_row_foreground, "foreground-set", TRUE, NULL);
		gtk_cell_renderer_render (renderer, cr, widget, background_area, cell_area, flags & ~GTK_CELL_RENDERER_SELECTED);
	} else if (GTK_IS_CELL_RENDERER_TOGGLE (renderer)) {
		gtk_cell_renderer_render (renderer, cr, widget, background_area, cell_area,
			flags & ~(GTK_CELL_RENDERER_PRELIT | GTK_CELL_RENDERER_SELECTED));
	} else {
		gtk_cell_renderer_render (renderer, cr, widget, background_area, cell_area, flags);
	}
}

static void
e_cell_area_lines_render (GtkCellArea *area,
			  GtkCellAreaContext *context,
			  GtkWidget *widget,
			  cairo_t *cr,
			  const GdkRectangle *background_area,
			  const GdkRectangle *cell_area,
			  GtkCellRendererState flags,
			  gboolean paint_focus)
{
	ECellAreaLines *self = E_CELL_AREA_LINES (area);
	gboolean is_rtl = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
	guint max_line, line, ii;
	gint y_offset = 0;
	gint indent, span_width, span_height = 0, span_x_offset;
	GdkRGBA row_background;
	GdkRGBA row_foreground;
	gboolean has_custom_background = FALSE;
	const GdkRGBA *custom_row_foreground = NULL;

	if (cell_area_lines_compute_row_background (self, widget, flags, &row_background, &has_custom_background)) {
		cairo_save (cr);
		gdk_cairo_rectangle (cr, background_area);
		gdk_cairo_set_source_rgba (cr, &row_background);
		cairo_fill (cr);
		cairo_restore (cr);
	}

	if (has_custom_background) {
		row_foreground = e_utils_get_text_color_for_background (&row_background);
		custom_row_foreground = &row_foreground;
	}

	max_line = cell_area_lines_get_max_line (self);
	indent = cell_area_lines_get_indent (self, widget, NULL);
	span_width = cell_area_lines_get_span_width (self, widget);

	span_x_offset = indent;
	for (ii = 0; ii < self->renderers->len; ii++) {
		GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
		GdkRectangle r_cell, r_bg;
		gint nat_w, min_h;

		if (!renderer_get_span (renderer))
			continue;
		if (!gtk_cell_renderer_get_visible (renderer))
			continue;

		gtk_cell_renderer_get_preferred_width (renderer, widget, NULL, &nat_w);
		gtk_cell_renderer_get_preferred_height (renderer, widget, &min_h, NULL);
		span_height = MAX (span_height, min_h);

		r_cell.x = cell_area->x + span_x_offset;
		r_cell.y = cell_area->y;
		r_cell.width = nat_w;
		r_cell.height = cell_area->height;

		if (is_rtl)
			r_cell.x = cell_area->x + cell_area->width - (r_cell.x - cell_area->x) - r_cell.width;

		r_bg = r_cell;
		virtual_tree_render_cell (renderer, cr, widget, &r_bg, &r_cell, flags, custom_row_foreground);

		span_x_offset += nat_w;
	}

	for (line = 0; line <= max_line; line++) {
		gint line_height = 0;
		gint x_offset = indent + span_width;

		for (ii = 0; ii < self->renderers->len; ii++) {
			GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
			gint min_h;

			if (renderer_get_span (renderer))
				continue;
			if (renderer_get_line (renderer) != line)
				continue;
			if (!gtk_cell_renderer_get_visible (renderer))
				continue;

			gtk_cell_renderer_get_preferred_height (renderer, widget, &min_h, NULL);
			line_height = MAX (line_height, min_h);
		}

		if (max_line == 0)
			line_height = cell_area->height;

		for (ii = 0; ii < self->renderers->len; ii++) {
			GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
			GdkRectangle r_cell, r_bg;
			gint min_w, nat_w, w;

			if (renderer_get_span (renderer))
				continue;
			if (renderer_get_line (renderer) != line)
				continue;
			if (!gtk_cell_renderer_get_visible (renderer))
				continue;

			if (E_IS_CELL_RENDERER_EXPANDER (renderer)) {
				r_cell.x = cell_area->x;
				r_cell.width = indent;
				if (span_height > 0) {
					r_cell.y = cell_area->y;
					r_cell.height = span_height;
				} else if (span_width > 0) {
					r_cell.y = cell_area->y;
					r_cell.height = cell_area->height;
				} else {
					r_cell.y = cell_area->y + y_offset;
					r_cell.height = line_height;
				}

				if (is_rtl)
					r_cell.x = cell_area->x + cell_area->width - r_cell.width;

				r_bg = r_cell;
				virtual_tree_render_cell (renderer, cr, widget, &r_bg, &r_cell, flags, custom_row_foreground);
				continue;
			}

			gtk_cell_renderer_get_preferred_width (renderer, widget, &min_w, &nat_w);

			w = nat_w;
			if (renderer_get_expand (renderer)) {
				gint remaining = cell_area->width - x_offset - cell_area_lines_get_trailing_fixed_width (self, widget, ii, line);
				if (remaining > 0)
					w = remaining;
			} else if (x_offset + w > cell_area->width) {
				w = MAX (cell_area->width - x_offset, 0);
			}

			r_cell.x = cell_area->x + x_offset;
			r_cell.y = cell_area->y + y_offset;
			r_cell.width = w;
			r_cell.height = line_height;

			if (is_rtl)
				r_cell.x = cell_area->x + cell_area->width - (r_cell.x - cell_area->x) - r_cell.width;

			r_bg = r_cell;

			virtual_tree_render_cell (renderer, cr, widget, &r_bg, &r_cell, flags, custom_row_foreground);

			x_offset += w;
		}

		y_offset += line_height;
	}
}

static void
e_cell_area_lines_get_preferred_width (GtkCellArea *area,
				       GtkCellAreaContext *context,
				       GtkWidget *widget,
				       gint *minimum,
				       gint *natural)
{
	ECellAreaLines *self = E_CELL_AREA_LINES (area);
	guint max_line, line, ii;
	gint total_min = 0, total_nat = 0;
	gint indent, span_width;

	max_line = cell_area_lines_get_max_line (self);
	indent = cell_area_lines_get_indent (self, widget, NULL);
	span_width = cell_area_lines_get_span_width (self, widget);

	for (line = 0; line <= max_line; line++) {
		gint line_min = indent + span_width, line_nat = indent + span_width;

		for (ii = 0; ii < self->renderers->len; ii++) {
			GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
			gint min_w, nat_w;

			if (renderer_get_span (renderer))
				continue;
			if (E_IS_CELL_RENDERER_EXPANDER (renderer))
				continue;
			if (renderer_get_line (renderer) != line)
				continue;
			if (!gtk_cell_renderer_get_visible (renderer))
				continue;

			gtk_cell_renderer_get_preferred_width (renderer, widget, &min_w, &nat_w);
			line_min += min_w;
			line_nat += nat_w;
		}

		total_min = MAX (total_min, line_min);
		total_nat = MAX (total_nat, line_nat);
	}

	if (minimum)
		*minimum = total_min;
	if (natural)
		*natural = total_nat;
}

static void
e_cell_area_lines_get_preferred_height (GtkCellArea *area,
					GtkCellAreaContext *context,
					GtkWidget *widget,
					gint *minimum,
					gint *natural)
{
	ECellAreaLines *self = E_CELL_AREA_LINES (area);
	guint max_line, line, ii;
	gint total = 0, span_height = 0;

	max_line = cell_area_lines_get_max_line (self);

	for (ii = 0; ii < self->renderers->len; ii++) {
		GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
		gint min_h;

		if (!renderer_get_span (renderer))
			continue;
		if (!gtk_cell_renderer_get_visible (renderer))
			continue;

		gtk_cell_renderer_get_preferred_height (renderer, widget, &min_h, NULL);
		span_height = MAX (span_height, min_h);
	}

	for (line = 0; line <= max_line; line++) {
		gint line_height = 0;

		for (ii = 0; ii < self->renderers->len; ii++) {
			GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
			gint min_h;

			if (renderer_get_span (renderer))
				continue;
			if (renderer_get_line (renderer) != line)
				continue;
			if (!gtk_cell_renderer_get_visible (renderer))
				continue;

			gtk_cell_renderer_get_preferred_height (renderer, widget, &min_h, NULL);
			line_height = MAX (line_height, min_h);
		}

		total += line_height;
	}

	total = MAX (total, span_height);

	if (minimum)
		*minimum = total;
	if (natural)
		*natural = total;
}

static GtkCellAreaContext *
e_cell_area_lines_create_context (GtkCellArea *area)
{
	return g_object_new (GTK_TYPE_CELL_AREA_CONTEXT,
		"area", area, NULL);
}

static GtkCellAreaContext *
e_cell_area_lines_copy_context (GtkCellArea *area,
				GtkCellAreaContext *context)
{
	return g_object_new (GTK_TYPE_CELL_AREA_CONTEXT,
		"area", area, NULL);
}

static GtkSizeRequestMode
e_cell_area_lines_get_request_mode (GtkCellArea *area)
{
	return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
e_cell_area_lines_finalize (GObject *object)
{
	ECellAreaLines *self = E_CELL_AREA_LINES (object);

	g_clear_pointer (&self->renderers, g_ptr_array_unref);
	g_clear_pointer (&self->row_render_data.custom_color, gdk_rgba_free);
	g_clear_pointer (&self->row_render_data.background_color, gdk_rgba_free);
	g_clear_pointer (&self->alternating_color, gdk_rgba_free);

	G_OBJECT_CLASS (e_cell_area_lines_parent_class)->finalize (object);
}

static gboolean
e_cell_area_lines_focus (GtkCellArea *area,
			 GtkDirectionType direction)
{
	return FALSE;
}

static GtkCellRenderer *
e_cell_area_lines_get_renderer_at_pos (ECellAreaLines *self,
				       GtkWidget *widget,
				       gint cell_area_width,
				       gint cell_area_height,
				       gint x,
				       gint y)
{
	guint max_line, line, ii;
	gint y_offset = 0;
	gint indent, span_width, span_x_offset;

	if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
		x = cell_area_width - 1 - x;

	max_line = cell_area_lines_get_max_line (self);
	indent = cell_area_lines_get_indent (self, widget, NULL);
	span_width = cell_area_lines_get_span_width (self, widget);

	span_x_offset = indent;
	for (ii = 0; ii < self->renderers->len; ii++) {
		GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
		gint nat_w;

		if (!renderer_get_span (renderer))
			continue;
		if (!gtk_cell_renderer_get_visible (renderer))
			continue;

		gtk_cell_renderer_get_preferred_width (renderer, widget, NULL, &nat_w);

		if (x >= span_x_offset && x < span_x_offset + nat_w &&
		    y >= 0 && y < cell_area_height)
			return renderer;

		span_x_offset += nat_w;
	}

	for (line = 0; line <= max_line; line++) {
		gint line_height = 0;
		gint x_offset = indent + span_width;

		for (ii = 0; ii < self->renderers->len; ii++) {
			GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
			gint min_h;

			if (renderer_get_span (renderer))
				continue;
			if (renderer_get_line (renderer) != line)
				continue;
			if (!gtk_cell_renderer_get_visible (renderer))
				continue;

			gtk_cell_renderer_get_preferred_height (renderer, widget, &min_h, NULL);
			line_height = MAX (line_height, min_h);
		}

		if (max_line == 0)
			line_height = cell_area_height;

		if (y >= y_offset && y < y_offset + line_height) {
			for (ii = 0; ii < self->renderers->len; ii++) {
				GtkCellRenderer *renderer = g_ptr_array_index (self->renderers, ii);
				gint min_w, nat_w, w;

				if (renderer_get_span (renderer))
					continue;
				if (renderer_get_line (renderer) != line)
					continue;
				if (!gtk_cell_renderer_get_visible (renderer))
					continue;

				if (E_IS_CELL_RENDERER_EXPANDER (renderer)) {
					if (x >= 0 && x < indent && E_CELL_RENDERER_EXPANDER (renderer)->expandable)
						return renderer;
					continue;
				}

				gtk_cell_renderer_get_preferred_width (renderer, widget, &min_w, &nat_w);

				w = nat_w;
				if (renderer_get_expand (renderer)) {
					gint remaining = cell_area_width - x_offset;
					if (remaining > 0)
						w = remaining;
				} else if (x_offset + w > cell_area_width) {
					w = MAX (cell_area_width - x_offset, 0);
				}

				if (x >= x_offset && x < x_offset + w)
					return renderer;

				x_offset += w;
			}

			return NULL;
		}

		y_offset += line_height;
	}

	return NULL;
}

static void
e_cell_area_lines_class_init (ECellAreaLinesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkCellAreaClass *area_class = GTK_CELL_AREA_CLASS (klass);

	object_class->finalize = e_cell_area_lines_finalize;

	area_class->add = e_cell_area_lines_add;
	area_class->remove = e_cell_area_lines_remove;
	area_class->foreach = e_cell_area_lines_foreach;
	area_class->foreach_alloc = e_cell_area_lines_foreach_alloc;
	area_class->render = e_cell_area_lines_render;
	area_class->create_context = e_cell_area_lines_create_context;
	area_class->copy_context = e_cell_area_lines_copy_context;
	area_class->get_request_mode = e_cell_area_lines_get_request_mode;
	area_class->get_preferred_width = e_cell_area_lines_get_preferred_width;
	area_class->get_preferred_height = e_cell_area_lines_get_preferred_height;
	area_class->focus = e_cell_area_lines_focus;
}

static void
e_cell_area_lines_init (ECellAreaLines *self)
{
	self->renderers = g_ptr_array_new_with_free_func (g_object_unref);
}

/* Internal list store columns */
enum {
	COL_ROW_OBJECT,
	COL_DEPTH,
	COL_EXPANDABLE,
	COL_EXPANDED,
	COL_IS_GROUP,
	COL_VISIBLE_ROW_IDX,
	N_INTERNAL_COLUMNS
};

enum {
	PROP_0,
	PROP_MODEL,
	PROP_GROUP_DEPTH,
	PROP_EDITABLE,
	PROP_SELECTION_MODE,
	PROP_EMPTY_MESSAGE,
	PROP_HEADER_CLICK_SORT_POLICY,
	N_PROPERTIES,
	PROP_HADJUSTMENT = N_PROPERTIES,
	PROP_VADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VSCROLL_POLICY
};

enum {
	ROW_ACTIVATED,
	SELECTION_CHANGED,
	CURSOR_CHANGED,
	ROW_EXPANDED,
	CELL_CLICKED,
	RIGHT_CLICK,
	TREE_DRAG_BEGIN,
	TREE_DRAG_END,
	TREE_DRAG_DATA_GET,
	TREE_DRAG_DATA_RECEIVED,
	TREE_DRAG_DROP,
	TREE_DRAG_MOTION,
	CELL_EDITED,
	HEADER_CLICKED,
	COLUMN_STATE_CHANGED,
	GET_LEGACY_ETABLE_COLUMN_MAP,
	LAST_SIGNAL
};

static GParamSpec *properties[N_PROPERTIES] = { NULL };
static guint widget_signals[LAST_SIGNAL] = { 0 };

typedef struct {
	GtkCellRenderer *renderer;
	guint line;
	gboolean expand;
	gboolean pack_end;
	EVirtualTreeCellDataFunc func;
	gpointer user_data;
	GDestroyNotify destroy;
} RendererInfo;

static void
renderer_info_free (gpointer data)
{
	RendererInfo *info = data;

	if (info->destroy && info->user_data)
		info->destroy (info->user_data);

	g_free (info);
}

typedef struct {
	GtkTreeViewColumn *tree_column;
	gchar *column_id;
	gchar *title;
	GPtrArray *renderers; /* of RendererInfo */
	gint sort_priority;
	gint group_priority;
	gboolean groupable;
} ColumnInfo;

static void
column_info_free (gpointer data)
{
	ColumnInfo *info = data;

	g_free (info->column_id);
	g_free (info->title);
	g_ptr_array_unref (info->renderers);
	g_free (info);
}

struct _EVirtualTree {
	GtkBox parent_instance;

	EVirtualTreeModel *model;

	GtkTreeView *tree_view;
	GtkListStore *list_store;

	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	GtkScrollablePolicy hscroll_policy;
	GtkScrollablePolicy vscroll_policy;

	GPtrArray *columns; /* of ColumnInfo */

	GtkCellRenderer *expander_renderer;
	gint expander_column_idx;
	guint expander_line;
	gboolean expander_hidden;

	guint first_visible_row;
	GObject *top_visible_object;
	guint visible_count;
	gint cell_height;
	gint row_spacing;
	gint row_stride;
	gint last_alloc_height;
	gboolean metrics_valid;
	gboolean cursor_scroll_pending;
	gint cursor_scroll_pending_row;

	GHashTable *selected_keys;
	EVirtualTreeKeyType key_type;
	gboolean inverted_selection;
	gint cursor_row;
	GObject *cursor_object;
	gint anchor_row;
	gint shift_end_row;

	guint group_depth;
	gboolean editable;
	GtkSelectionMode selection_mode;
	EAutomaticActionPolicy header_click_sort_policy;

	gulong model_rows_changed_id;
	gulong model_rows_inserted_id;
	gulong model_rows_removed_id;
	gulong model_row_count_changed_id;
	gulong model_before_rebuild_id;
	gulong model_after_rebuild_id;

	gulong selection_changed_id;
	gulong cursor_changed_id;

	guint refill_idle_id;
	guint focus_restore_idle_id;
	gboolean in_refill;
	gboolean editing_active;

	EVirtualTreeSearchFunc search_func;
	gpointer search_user_data;
	GDestroyNotify search_destroy;
	GString *search_buffer;
	guint search_timeout_id;

	EVirtualTreeRowColorFunc row_selected_color_func;
	gpointer row_selected_color_user_data;
	GDestroyNotify row_selected_color_destroy;

	EVirtualTreeRowColorFunc row_unselected_color_func;
	gpointer row_unselected_color_user_data;
	GDestroyNotify row_unselected_color_destroy;

	EVirtualTreeRowTooltipMarkupFunc row_tooltip_markup_func;
	gpointer row_tooltip_markup_user_data;
	GDestroyNotify row_tooltip_markup_destroy;

	gchar *empty_message;

	gboolean pending_rebuild;
	gpointer pending_cursor_key;
	gboolean pending_cursor_was_visible;
	guint restore_selection_id;
	guint pending_restore_giveup_id;

	EUIManager *ui_manager;
	guint popup_col_idx;

	gchar *column_state_filename;
	guint save_state_id;

	guint state_freeze_count;
	gboolean state_changed_while_frozen;

	gboolean in_drag;
	struct _drag {
		GtkTargetList *source_target_list;
		GdkDragAction source_actions;
		GdkModifierType source_start_mask;
		gboolean source_enabled;
		gboolean button_down;
		gint start_x;
		gint start_y;
		guint start_button;
		gint defer_row;
		guint last_time;
	} drag;
	gboolean alternating_row_colors;
	GdkRGBA *alternating_row_color;
	gboolean prelight_row_colors;

	gint *proportional_weights;
	gboolean *proportional_manual;
	gint *proportional_prev_width;
	gboolean *proportional_pinned;
	gint *proportional_pinned_width;
	guint proportional_n_columns;
	gint proportional_last_width;
	gboolean applying_proportional_widths;
};

static void e_virtual_tree_scrollable_init (GtkScrollableInterface *iface);

guint		_e_virtual_tree_get_first_visible_row	(EVirtualTree *self);
guint		_e_virtual_tree_get_visible_count	(EVirtualTree *self);
gint		_e_virtual_tree_get_row_stride		(EVirtualTree *self);
GtkAdjustment *	_e_virtual_tree_get_vadjustment		(EVirtualTree *self);
guint		_e_virtual_tree_get_n_columns		(EVirtualTree *self);
GArray *	_e_virtual_tree_get_display_column_order
							(EVirtualTree *self);
const gchar *	_e_virtual_tree_get_column_title	(EVirtualTree *self,
							 guint column_index);
gchar *		_e_virtual_tree_get_cell_text		(EVirtualTree *self,
							 guint row_index,
							 guint column_index);
gboolean	_e_virtual_tree_get_cell_toggle		(EVirtualTree *self,
							 guint row_index,
							 guint column_index,
							 gboolean *out_active);
gboolean	_e_virtual_tree_column_is_printable	(EVirtualTree *self,
							 guint column_index);
gboolean	_e_virtual_tree_column_is_toggle	(EVirtualTree *self,
							 guint column_index);

G_DEFINE_FINAL_TYPE_WITH_CODE (EVirtualTree, e_virtual_tree, GTK_TYPE_BOX,
	G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, e_virtual_tree_scrollable_init))

/* Forward declarations */
static void virtual_tree_refill (EVirtualTree *self);
static void virtual_tree_move_cursor (EVirtualTree *self, gint row, gboolean centered);
static void virtual_tree_scroll_to_show_cursor_centered (EVirtualTree *self);
static void virtual_tree_update_scrollbar (EVirtualTree *self);
static void virtual_tree_invalidate_metrics (EVirtualTree *self);
static void virtual_tree_disconnect_model (EVirtualTree *self);
static void virtual_tree_save_column_state (EVirtualTree *self);
static void schedule_save_state (EVirtualTree *self);
static void on_column_state_notify (GObject *object, GParamSpec *pspec, gpointer user_data);
static void on_tree_view_columns_changed (GtkTreeView *tree_view, gpointer user_data);
static void virtual_tree_clear_pending_selection (EVirtualTree *self);

static inline gboolean
virtual_tree_is_key_selected (EVirtualTree *self,
			      gconstpointer key)
{
	if (self->inverted_selection)
		return !g_hash_table_contains (self->selected_keys, key);
	return g_hash_table_contains (self->selected_keys, key);
}

static inline gpointer
virtual_tree_copy_key (EVirtualTree *self,
		       gconstpointer key)
{
	if (self->key_type.copy_func)
		return self->key_type.copy_func ((gpointer) key);
	return (gpointer) key;
}

static inline void
virtual_tree_free_key (EVirtualTree *self,
		       gpointer key)
{
	if (key && self->key_type.free_func)
		self->key_type.free_func (key);
}

static inline void
virtual_tree_select_key (EVirtualTree *self,
			 gconstpointer key)
{
	if (self->inverted_selection)
		g_hash_table_remove (self->selected_keys, key);
	else
		g_hash_table_replace (self->selected_keys, virtual_tree_copy_key (self, key), NULL);
}

static inline void
virtual_tree_deselect_key (EVirtualTree *self,
			   gconstpointer key)
{
	if (self->inverted_selection)
		g_hash_table_replace (self->selected_keys, virtual_tree_copy_key (self, key), NULL);
	else
		g_hash_table_remove (self->selected_keys, key);
}

static inline gboolean
virtual_tree_selection_is_empty (EVirtualTree *self)
{
	if (self->inverted_selection)
		return self->model && g_hash_table_size (self->selected_keys) >= e_virtual_tree_model_get_row_count (self->model);
	return g_hash_table_size (self->selected_keys) == 0;
}

static void
set_cursor_object (EVirtualTree *self,
		   GObject *object)
{
	g_set_object (&self->cursor_object, object);
}

static gint
virtual_tree_get_bin_height (EVirtualTree *self)
{
	gint alloc_height, header_y = 0;

	alloc_height = gtk_widget_get_allocated_height (GTK_WIDGET (self->tree_view));

	if (gtk_widget_get_realized (GTK_WIDGET (self->tree_view)))
		gtk_tree_view_convert_bin_window_to_widget_coords (self->tree_view, 0, 0, NULL, &header_y);

	return alloc_height - header_y;
}

static void
e_virtual_tree_scrollable_init (GtkScrollableInterface *iface)
{
}

static void virtual_tree_scroll_changed (GtkAdjustment *adj, EVirtualTree *self);

static void
virtual_tree_set_vadjustment (EVirtualTree *self,
			      GtkAdjustment *adj)
{
	if (self->vadjustment == adj)
		return;

	if (self->vadjustment) {
		g_signal_handlers_disconnect_by_func (self->vadjustment, virtual_tree_scroll_changed, self);
		g_clear_object (&self->vadjustment);
	}

	if (adj) {
		self->vadjustment = g_object_ref_sink (adj);
		g_signal_connect (self->vadjustment, "value-changed",
			G_CALLBACK (virtual_tree_scroll_changed), self);
	}

	virtual_tree_update_scrollbar (self);

	g_object_notify (G_OBJECT (self), "vadjustment");
}

static void
virtual_tree_set_hadjustment (EVirtualTree *self,
			      GtkAdjustment *adj)
{
	if (self->hadjustment == adj)
		return;

	g_clear_object (&self->hadjustment);

	if (adj)
		self->hadjustment = g_object_ref_sink (adj);

	if (self->tree_view)
		gtk_scrollable_set_hadjustment (GTK_SCROLLABLE (self->tree_view), adj);

	g_object_notify (G_OBJECT (self), "hadjustment");
}

static void
virtual_tree_measure_row_height (EVirtualTree *self)
{
	GtkTreeViewColumn *column;
	GtkRequisition min_size;
	gint vertical_separator = 0;

	if (self->metrics_valid)
		return;

	gtk_widget_style_get (
		GTK_WIDGET (self->tree_view),
		"vertical-separator", &vertical_separator,
		NULL);

	self->row_spacing = vertical_separator;
	self->cell_height = 0;

	if (self->columns->len == 0 || !self->model) {
		self->row_stride = 1;
		return;
	}

	/* Use a throwaway store - self->list_store may hold stale rows until the next refill. */
	if (e_virtual_tree_model_get_row_count (self->model) > 0) {
		GPtrArray *rows;

		rows = e_virtual_tree_model_dup_rows (self->model, 0, 0, FALSE);

		if (rows && rows->len > 0) {
			GObject *row_obj = g_ptr_array_index (rows, 0);
			GtkListStore *measure_store;
			GtkTreeIter iter;
			guint cc;

			measure_store = gtk_list_store_new (N_INTERNAL_COLUMNS,
				G_TYPE_OBJECT,
				G_TYPE_UINT,
				G_TYPE_BOOLEAN,
				G_TYPE_BOOLEAN,
				G_TYPE_BOOLEAN,
				G_TYPE_UINT);

			gtk_list_store_append (measure_store, &iter);
			gtk_list_store_set (measure_store, &iter,
				COL_ROW_OBJECT, row_obj,
				COL_DEPTH, e_virtual_tree_model_get_depth (self->model, row_obj),
				COL_EXPANDABLE, e_virtual_tree_model_is_expandable (self->model, row_obj),
				COL_EXPANDED, e_virtual_tree_model_get_expanded (self->model, row_obj),
				COL_IS_GROUP, FALSE,
				COL_VISIBLE_ROW_IDX, 0u,
				-1);

			for (cc = 0; cc < self->columns->len; cc++) {
				ColumnInfo *info = g_ptr_array_index (self->columns, cc);
				GList *cells, *link;
				gint width, height;

				if (!gtk_tree_view_column_get_visible (info->tree_column))
					continue;

				gtk_tree_view_column_cell_set_cell_data (
					info->tree_column,
					GTK_TREE_MODEL (measure_store),
					&iter, FALSE, FALSE);

				cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (info->tree_column));
				for (link = cells; link; link = link->next) {
					gtk_cell_renderer_set_visible (link->data, TRUE);
				}
				g_list_free (cells);

				gtk_tree_view_column_cell_get_size (
					info->tree_column, NULL, NULL, NULL,
					&width, &height);

				if (height > self->cell_height)
					self->cell_height = height;
			}

			g_object_unref (measure_store);
		}

		g_clear_pointer (&rows, g_ptr_array_unref);
	}

	if (self->cell_height == 0) {
		GList *cells;

		column = ((ColumnInfo *) g_ptr_array_index (self->columns, 0))->tree_column;
		cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
		if (cells) {
			gtk_cell_renderer_get_preferred_size (cells->data,
				GTK_WIDGET (self->tree_view), &min_size, NULL);
			self->cell_height = MAX (min_size.height, 1);
			g_list_free (cells);
		}
	}

	self->row_stride = self->cell_height + self->row_spacing;
	self->metrics_valid = TRUE;
}

static void
virtual_tree_invalidate_metrics (EVirtualTree *self)
{
	self->metrics_valid = FALSE;
}

static void
virtual_tree_update_scrollbar (EVirtualTree *self)
{
	GtkAdjustment *adj;
	guint total_rows;
	gdouble upper, page_size;
	gint view_height;

	if (!self->vadjustment || !self->tree_view)
		return;

	adj = self->vadjustment;

	if (!self->model) {
		gtk_adjustment_configure (adj, 0, 0, 0, 0, 0, 0);
		return;
	}

	virtual_tree_measure_row_height (self);

	total_rows = e_virtual_tree_model_get_row_count (self->model);
	view_height = virtual_tree_get_bin_height (self);

	upper = (gdouble) total_rows * self->row_stride;
	page_size = (gdouble) view_height;

	if (upper < page_size)
		upper = page_size;

	gtk_adjustment_configure (adj,
		MIN (gtk_adjustment_get_value (adj), MAX (upper - page_size, 0)),
		0, upper,
		SCROLL_STEP_PIXELS,
		self->row_stride * MAX (self->visible_count, 1),
		page_size);
}

static gboolean
virtual_tree_refill_idle_cb (gpointer user_data)
{
	EVirtualTree *self = user_data;

	self->refill_idle_id = 0;
	virtual_tree_refill (self);

	return G_SOURCE_REMOVE;
}

static void
virtual_tree_schedule_refill (EVirtualTree *self)
{
	if (self->refill_idle_id == 0)
		self->refill_idle_id = g_idle_add (virtual_tree_refill_idle_cb, self);
}

static void
virtual_tree_scroll_changed (GtkAdjustment *adj,
			     EVirtualTree *self)
{
	guint new_first;

	if (!self->metrics_valid || self->row_stride <= 0)
		return;

	new_first = (guint) (gtk_adjustment_get_value (adj) / self->row_stride);

	if (new_first != self->first_visible_row) {
		self->first_visible_row = new_first;
		virtual_tree_schedule_refill (self);
	} else if (gtk_widget_get_realized (GTK_WIDGET (self->tree_view))) {
		gint offset;

		offset = (gint) fmod (gtk_adjustment_get_value (adj), (gdouble) self->row_stride);
		gtk_tree_view_scroll_to_point (self->tree_view, -1, offset);
	}
}

static void
update_area_row_state (EVirtualTree *self,
		       GtkTreeViewColumn *tree_column,
		       GObject *row_object,
		       guint visible_row,
		       gboolean is_group)
{
	GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (tree_column));
	ECellAreaLines *lines_area;
	GdkRGBA color;
	gboolean has_color = FALSE;
	GdkRGBA background_color;
	gboolean has_background_color = FALSE;

	if (!E_IS_CELL_AREA_LINES (area))
		return;

	lines_area = E_CELL_AREA_LINES (area);

	if (!is_group && self->row_selected_color_func)
		has_color = self->row_selected_color_func (self, row_object, visible_row, &color, self->row_selected_color_user_data);

	g_clear_pointer (&lines_area->row_render_data.custom_color, gdk_rgba_free);
	if (has_color)
		lines_area->row_render_data.custom_color = gdk_rgba_copy (&color);

	if (!is_group && self->row_unselected_color_func)
		has_background_color = self->row_unselected_color_func (self, row_object, visible_row, &background_color, self->row_unselected_color_user_data);

	g_clear_pointer (&lines_area->row_render_data.background_color, gdk_rgba_free);
	if (has_background_color)
		lines_area->row_render_data.background_color = gdk_rgba_copy (&background_color);

	g_clear_pointer (&lines_area->alternating_color, gdk_rgba_free);
	if (self->alternating_row_color)
		lines_area->alternating_color = gdk_rgba_copy (self->alternating_row_color);

	lines_area->row_render_data.visible_index = visible_row;
	lines_area->alternating_enabled = self->alternating_row_colors;
	lines_area->prelight_enabled = self->prelight_row_colors;
	lines_area->row_render_data.is_group = is_group;
}

static void
cell_data_func_bridge (GtkTreeViewColumn *tree_column,
		       GtkCellRenderer *renderer,
		       GtkTreeModel *model,
		       GtkTreeIter *iter,
		       gpointer data)
{
	RendererInfo *info = data;
	EVirtualTree *self;
	GObject *row_object = NULL;
	guint visible_row = 0;
	gboolean is_group = FALSE;

	self = g_object_get_data (G_OBJECT (tree_column), "e-virtual-tree");
	if (!self || !self->model)
		return;

	gtk_tree_model_get (model, iter,
		COL_VISIBLE_ROW_IDX, &visible_row,
		COL_IS_GROUP, &is_group,
		-1);

	row_object = e_virtual_tree_model_dup_row (self->model, visible_row);
	if (!row_object)
		return;

	update_area_row_state (self, tree_column, row_object, visible_row, is_group);

	if (is_group && self->group_depth > 0) {
		ColumnInfo *first_col = NULL;
		RendererInfo *first_renderer = NULL;

		if (self->columns->len > 0)
			first_col = g_ptr_array_index (self->columns, 0);

		if (first_col && first_col->renderers->len > 0)
			first_renderer = g_ptr_array_index (first_col->renderers, 0);

		if (info == first_renderer) {
			g_object_set (renderer, "visible", TRUE, NULL);
			if (GTK_IS_CELL_RENDERER_TEXT (renderer))
				g_object_set (renderer, "text", " ", NULL);
		} else {
			g_object_set (renderer, "visible", FALSE, NULL);
		}

		g_object_unref (row_object);
		return;
	}

	g_object_set (renderer, "visible", TRUE, NULL);

	if (GTK_IS_CELL_RENDERER_TEXT (renderer)) {
		g_object_set (renderer,
			"weight", PANGO_WEIGHT_NORMAL,
			"editable", self->editable && !is_group,
			"foreground-set", FALSE,
			NULL);
	}

	if (info->func)
		info->func (self, renderer, row_object, visible_row, info->user_data);

	if (GTK_IS_CELL_RENDERER_TEXT (renderer) && !E_IS_CELL_RENDERER_BUTTON (renderer) && self->row_unselected_color_func) {
		GdkRGBA bg_color;
		gboolean is_selected;

		/* 'iter' may belong to a throwaway measurement model (see
		 * virtual_tree_measure_row_height()), which is not the tree
		 * view's own model, so gtk_tree_selection_iter_is_selected()
		 * cannot be used with it. */
		is_selected = model == GTK_TREE_MODEL (self->list_store) &&
			gtk_tree_selection_iter_is_selected (gtk_tree_view_get_selection (self->tree_view), iter);

		if (self->row_unselected_color_func (self, row_object, visible_row, &bg_color, self->row_unselected_color_user_data) &&
		    !is_selected) {
			GdkRGBA fg_color = e_utils_get_text_color_for_background (&bg_color);

			g_object_set (renderer, "foreground-rgba", &fg_color, NULL);
		}
	}

	g_object_unref (row_object);
}

static gboolean
virtual_tree_deny_select_cb (GtkTreeSelection *selection,
			     GtkTreeModel *model,
			     GtkTreePath *path,
			     gboolean path_currently_selected,
			     gpointer user_data)
{
	return FALSE;
}

static void
virtual_tree_refill (EVirtualTree *self)
{
	GPtrArray *rows;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GObject *top_row;
	guint total_rows, last_row, ii;
	gint view_height;

	if (!self->model || !self->tree_view || !self->list_store)
		return;

	if (self->editing_active)
		return;

	virtual_tree_measure_row_height (self);

	g_clear_pointer (&self->alternating_row_color, gdk_rgba_free);
	gtk_widget_style_get (GTK_WIDGET (self),
		"alternating-row-colors", &self->alternating_row_colors,
		"alternating-row-color", &self->alternating_row_color,
		"prelight-row-colors", &self->prelight_row_colors,
		NULL);

	view_height = virtual_tree_get_bin_height (self);
	if (view_height <= 0 || self->row_stride <= 0)
		return;

	/* Sync first_visible_row from the adjustment value */
	if (self->vadjustment && self->row_stride > 0)
		self->first_visible_row = (guint) (gtk_adjustment_get_value (self->vadjustment) / self->row_stride);

	self->visible_count = (guint) (view_height / self->row_stride) + 2;

	total_rows = e_virtual_tree_model_get_row_count (self->model);

	if (total_rows == 0) {
		self->in_refill = TRUE;
		gtk_list_store_clear (self->list_store);
		self->in_refill = FALSE;
		if (self->empty_message && self->empty_message[0])
			gtk_widget_queue_draw (GTK_WIDGET (self->tree_view));
		return;
	}

	if (self->first_visible_row >= total_rows)
		self->first_visible_row = total_rows > 0 ? total_rows - 1 : 0;

	last_row = MIN (self->first_visible_row + self->visible_count - 1, total_rows - 1);

	rows = e_virtual_tree_model_dup_rows (self->model, self->first_visible_row, last_row, FALSE);
	if (!rows)
		return;

	self->in_refill = TRUE;

	selection = gtk_tree_view_get_selection (self->tree_view);
	g_signal_handler_block (selection, self->selection_changed_id);
	g_signal_handler_block (self->tree_view, self->cursor_changed_id);

	gtk_list_store_clear (self->list_store);

	for (ii = 0; ii < rows->len; ii++) {
		GObject *row_object = g_ptr_array_index (rows, ii);
		guint visible_idx = self->first_visible_row + ii;
		guint depth;
		gboolean expandable, expanded, is_group;
		gconstpointer key;

		if (!row_object)
			continue;

		if (self->cursor_object && row_object == self->cursor_object)
			self->cursor_row = (gint) visible_idx;

		depth = e_virtual_tree_model_get_depth (self->model, row_object);
		expandable = e_virtual_tree_model_is_expandable (self->model, row_object);
		expanded = e_virtual_tree_model_get_expanded (self->model, row_object);
		is_group = (self->group_depth > 0 && depth < self->group_depth);

		gtk_list_store_append (self->list_store, &iter);
		gtk_list_store_set (self->list_store, &iter,
			COL_ROW_OBJECT, row_object,
			COL_DEPTH, depth,
			COL_EXPANDABLE, expandable,
			COL_EXPANDED, expanded,
			COL_IS_GROUP, is_group,
			COL_VISIBLE_ROW_IDX, visible_idx,
			-1);

		key = e_virtual_tree_model_get_row_key (self->model, row_object);
		if (key && virtual_tree_is_key_selected (self, key))
			gtk_tree_selection_select_iter (selection, &iter);
	}

	top_row = rows->len > 0 ? g_ptr_array_index (rows, 0) : NULL;
	g_set_object (&self->top_visible_object, top_row);

	if (!self->cursor_object && self->cursor_row >= 0) {
		guint clamped = MIN ((guint) self->cursor_row, total_rows - 1);
		self->cursor_row = (gint) clamped;

		if (clamped >= self->first_visible_row && clamped <= last_row &&
		    (clamped - self->first_visible_row) < rows->len)
			set_cursor_object (self, g_ptr_array_index (rows, clamped - self->first_visible_row));
	}

	if (self->cursor_row >= 0 &&
	    (guint) self->cursor_row >= self->first_visible_row &&
	    (guint) self->cursor_row <= last_row) {
		GtkTreePath *path;
		guint offset = (guint) self->cursor_row - self->first_visible_row;

		path = gtk_tree_path_new_from_indices (offset, -1);

		/* Set GTK cursor for focus rectangle; this auto-selects
		 * the row, so undo that below when needed. The auto-scroll
		 * side effect is suppressed by on_tv_vadjustment_changed. */
		gtk_tree_view_set_cursor (self->tree_view, path, NULL, FALSE);

		if (self->selection_mode == GTK_SELECTION_MULTIPLE) {
			GtkTreeIter sel_iter;
			gboolean valid;

			valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->list_store), &sel_iter);
			while (valid) {
				GObject *row_obj = NULL;
				gconstpointer key;

				gtk_tree_model_get (GTK_TREE_MODEL (self->list_store), &sel_iter, COL_ROW_OBJECT, &row_obj, -1);
				key = row_obj ? e_virtual_tree_model_get_row_key (self->model, row_obj) : NULL;

				if (key && virtual_tree_is_key_selected (self, key))
					gtk_tree_selection_select_iter (selection, &sel_iter);
				else
					gtk_tree_selection_unselect_iter (selection, &sel_iter);

				g_clear_object (&row_obj);
				valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->list_store), &sel_iter);
			}
		}

		gtk_tree_path_free (path);
	} else if (self->selection_mode != GTK_SELECTION_MULTIPLE) {
		gtk_tree_selection_unselect_all (selection);

		if (self->cursor_row >= 0 && gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->list_store), &iter)) {
			GtkTreePath *anchor_path = gtk_tree_path_new_from_indices (0, -1);

			gtk_tree_selection_set_select_function (selection, virtual_tree_deny_select_cb, NULL, NULL);
			gtk_tree_view_set_cursor (self->tree_view, anchor_path, NULL, FALSE);
			gtk_tree_selection_set_select_function (selection, NULL, NULL, NULL);

			gtk_tree_path_free (anchor_path);
		}
	}

	g_signal_handler_unblock (selection, self->selection_changed_id);
	g_signal_handler_unblock (self->tree_view, self->cursor_changed_id);

	/* Apply sub-row scroll offset: the virtual scroll position may be
	 * partway through the first visible row. Shift the internal tree
	 * view by that fractional amount so partial rows render correctly
	 * at the bottom boundary. */
	if (self->vadjustment && self->row_stride > 0 &&
	    gtk_widget_get_realized (GTK_WIDGET (self->tree_view))) {
		gint offset;

		offset = (gint) fmod (gtk_adjustment_get_value (self->vadjustment), (gdouble) self->row_stride);

		gtk_tree_view_scroll_to_point (self->tree_view, -1, offset);
	}

	self->in_refill = FALSE;

	if (self->cursor_scroll_pending) {
		gint pending_row = self->cursor_scroll_pending_row;

		self->cursor_scroll_pending = FALSE;
		self->cursor_scroll_pending_row = -1;

		if (pending_row >= 0) {
			if (self->cursor_row != pending_row) {
				GObject *cursor_obj;

				self->cursor_row = pending_row;
				cursor_obj = self->model ? e_virtual_tree_model_dup_row (self->model, (guint) pending_row) : NULL;
				set_cursor_object (self, cursor_obj);
				g_clear_object (&cursor_obj);
			}
		} else if (self->cursor_row >= 0) {
			GObject *cursor_obj;

			cursor_obj = self->model ? e_virtual_tree_model_dup_row (self->model, (guint) self->cursor_row) : NULL;
			set_cursor_object (self, cursor_obj);
			g_clear_object (&cursor_obj);

			if (self->anchor_row < 0)
				self->anchor_row = self->cursor_row;
		}
	}

	gtk_widget_queue_draw (GTK_WIDGET (self->tree_view));

	g_ptr_array_unref (rows);
}

static void
virtual_tree_scroll_to_show_cursor_centered (EVirtualTree *self)
{
	if (self->cursor_row < 0)
		return;

	virtual_tree_measure_row_height (self);

	if (self->visible_count > 2 && self->row_stride > 0 && self->vadjustment) {
		gdouble adj_value = gtk_adjustment_get_value (self->vadjustment);
		gdouble row_top = (gdouble) self->cursor_row * self->row_stride;
		gdouble row_bottom = row_top + self->row_stride;
		gdouble view_height = gtk_adjustment_get_page_size (self->vadjustment);

		if (row_top < adj_value || row_bottom > adj_value + view_height) {
			gdouble center_offset = (view_height - self->row_stride) / 2.0;
			gdouble upper = gtk_adjustment_get_upper (self->vadjustment);
			gdouble new_value = row_top - center_offset;

			new_value = CLAMP (new_value, 0.0, MAX (upper - view_height, 0.0));

			self->first_visible_row = (guint) (new_value / self->row_stride);
			gtk_adjustment_set_value (self->vadjustment, new_value);
		}
	}
}

static void
on_model_rows_changed (EVirtualTreeModel *model,
		       guint first_row,
		       guint last_row,
		       EVirtualTree *self)
{
	guint view_last = self->first_visible_row + self->visible_count;

	if (last_row >= self->first_visible_row && first_row < view_last)
		virtual_tree_schedule_refill (self);
}

static void
on_model_rows_inserted (EVirtualTreeModel *model,
			guint first_row,
			guint last_row,
			EVirtualTree *self)
{
	guint count = last_row - first_row + 1;
	gint old_cursor = self->cursor_row;

	if (self->cursor_row >= 0) {
		gconstpointer cursor_key = self->cursor_object ? e_virtual_tree_model_get_row_key (self->model, self->cursor_object) : NULL;
		guint new_idx = cursor_key ? e_virtual_tree_model_find_row_by_key (self->model, cursor_key) : G_MAXUINT;

		if (new_idx != G_MAXUINT)
			self->cursor_row = (gint) new_idx;
		else if ((guint) self->cursor_row >= first_row)
			self->cursor_row += count;
	}

	if (self->anchor_row >= 0 && (guint) self->anchor_row >= first_row)
		self->anchor_row += count;

	if (self->shift_end_row >= 0 && (guint) self->shift_end_row >= first_row)
		self->shift_end_row += count;

	/* Update the adjustment's bounds for the new (larger) row count
	 * before setting its value below - otherwise gtk_adjustment_set_value()
	 * clamps against the stale, too-small upper bound and silently
	 * truncates the scroll position. */
	virtual_tree_update_scrollbar (self);

	if (first_row <= self->first_visible_row && self->vadjustment && self->row_stride > 0) {
		self->first_visible_row += count;
		gtk_adjustment_set_value (self->vadjustment,
			(gdouble) self->first_visible_row * self->row_stride);
	}

	if (self->cursor_row != old_cursor) {
		virtual_tree_refill (self);

		if (self->cursor_object)
			g_signal_emit (self, widget_signals[CURSOR_CHANGED], 0, (guint) self->cursor_row, self->cursor_object);
	} else {
		virtual_tree_schedule_refill (self);
	}
}

static void
on_model_rows_removed (EVirtualTreeModel *model,
		       guint first_row,
		       guint last_row,
		       EVirtualTree *self)
{
	guint count = last_row - first_row + 1;
	guint total_rows;
	gint old_cursor = self->cursor_row;
	gboolean cursor_was_removed = FALSE;
	gboolean selection_changed = FALSE;

	total_rows = e_virtual_tree_model_get_row_count (self->model);

	if (self->cursor_row >= 0) {
		gconstpointer cursor_key = self->cursor_object ? e_virtual_tree_model_get_row_key (self->model, self->cursor_object) : NULL;
		guint new_idx = cursor_key ? e_virtual_tree_model_find_row_by_key (self->model, cursor_key) : G_MAXUINT;

		if (new_idx != G_MAXUINT) {
			self->cursor_row = (gint) new_idx;
		} else if (cursor_key ||
			   ((guint) self->cursor_row >= first_row && (guint) self->cursor_row <= last_row)) {
			if (cursor_key && g_hash_table_remove (self->selected_keys, cursor_key))
				selection_changed = TRUE;
			cursor_was_removed = TRUE;
			set_cursor_object (self, NULL);
			if (total_rows > 0)
				self->cursor_row = (gint) MIN (first_row, total_rows - 1);
			else
				self->cursor_row = -1;
		} else if ((guint) self->cursor_row > last_row) {
			self->cursor_row -= count;
		}
	}

	if (self->anchor_row >= 0) {
		if ((guint) self->anchor_row >= first_row && (guint) self->anchor_row <= last_row)
			self->anchor_row = -1;
		else if ((guint) self->anchor_row > last_row)
			self->anchor_row -= count;
	}

	if (self->shift_end_row >= 0) {
		if ((guint) self->shift_end_row >= first_row && (guint) self->shift_end_row <= last_row)
			self->shift_end_row = -1;
		else if ((guint) self->shift_end_row > last_row)
			self->shift_end_row -= count;
	}

	if (last_row < self->first_visible_row && self->vadjustment && self->row_stride > 0) {
		if (self->first_visible_row >= count)
			self->first_visible_row -= count;
		else
			self->first_visible_row = 0;
		gtk_adjustment_set_value (self->vadjustment,
			(gdouble) self->first_visible_row * self->row_stride);
	}

	virtual_tree_update_scrollbar (self);

	if (cursor_was_removed && self->cursor_row >= 0 &&
	    virtual_tree_selection_is_empty (self)) {
		GObject *obj = e_virtual_tree_model_dup_row (self->model, (guint) self->cursor_row);

		if (obj) {
			gconstpointer key = e_virtual_tree_model_get_row_key (self->model, obj);

			if (key) {
				virtual_tree_select_key (self, key);
				selection_changed = TRUE;
			}
			g_clear_object (&obj);
		}
	}

	if (self->cursor_row != old_cursor || cursor_was_removed || selection_changed) {
		virtual_tree_refill (self);

		if (self->cursor_row >= 0 && !self->cursor_object) {
			GObject *obj = e_virtual_tree_model_dup_row (self->model, (guint) self->cursor_row);

			set_cursor_object (self, obj);
			g_clear_object (&obj);
		}

		if (self->cursor_row >= 0 && self->cursor_object &&
		    (self->cursor_row != old_cursor || cursor_was_removed))
			g_signal_emit (self, widget_signals[CURSOR_CHANGED], 0, (guint) self->cursor_row, self->cursor_object);
		if (selection_changed)
			g_signal_emit (self, widget_signals[SELECTION_CHANGED], 0);
	} else {
		virtual_tree_schedule_refill (self);
	}
}

static void
on_model_row_count_changed (EVirtualTreeModel *model,
			    EVirtualTree *self)
{
	gboolean cursor_was_visible = self->cursor_row >= 0 &&
		(guint) self->cursor_row >= self->first_visible_row &&
		(guint) self->cursor_row < self->first_visible_row + self->visible_count;
	gint old_cursor = self->cursor_row;
	gboolean cursor_was_removed = FALSE;
	gboolean selection_changed = FALSE;

	virtual_tree_invalidate_metrics (self);
	virtual_tree_update_scrollbar (self);

	/* Follow the top row even if its index shifted without its own
	 * rows-removed/rows-inserted signal. */
	if (self->top_visible_object && self->row_stride > 0 && self->vadjustment) {
		gconstpointer top_key = e_virtual_tree_model_get_row_key (self->model, self->top_visible_object);
		guint new_top_idx = top_key ? e_virtual_tree_model_find_row_by_key (self->model, top_key) : G_MAXUINT;

		if (new_top_idx != G_MAXUINT && new_top_idx != self->first_visible_row) {
			self->first_visible_row = new_top_idx;
			gtk_adjustment_set_value (self->vadjustment, (gdouble) new_top_idx * self->row_stride);
		}
	}

	if (self->cursor_row >= 0) {
		gconstpointer cursor_key = self->cursor_object ? e_virtual_tree_model_get_row_key (self->model, self->cursor_object) : NULL;
		guint new_idx = cursor_key ? e_virtual_tree_model_find_row_by_key (self->model, cursor_key) : G_MAXUINT;

		if (new_idx != G_MAXUINT) {
			GObject *obj;

			self->cursor_row = (gint) new_idx;
			obj = e_virtual_tree_model_dup_row (self->model, new_idx);
			set_cursor_object (self, obj);
			g_clear_object (&obj);
		} else {
			guint total_rows = e_virtual_tree_model_get_row_count (self->model);

			if (cursor_key && g_hash_table_remove (self->selected_keys, cursor_key))
				selection_changed = TRUE;

			cursor_was_removed = TRUE;
			set_cursor_object (self, NULL);

			if (total_rows > 0)
				self->cursor_row = (gint) MIN ((guint) self->cursor_row, total_rows - 1);
			else
				self->cursor_row = -1;
		}
	}

	if (cursor_was_removed && self->cursor_row >= 0) {
		GObject *obj = e_virtual_tree_model_dup_row (self->model, (guint) self->cursor_row);

		set_cursor_object (self, obj);

		if (obj && virtual_tree_selection_is_empty (self)) {
			gconstpointer key = e_virtual_tree_model_get_row_key (self->model, obj);

			if (key) {
				virtual_tree_select_key (self, key);
				selection_changed = TRUE;
			}
		}

		g_clear_object (&obj);
	}

	if (cursor_was_visible)
		virtual_tree_scroll_to_show_cursor_centered (self);

	if (self->refill_idle_id != 0) {
		g_source_remove (self->refill_idle_id);
		self->refill_idle_id = 0;
	}
	virtual_tree_refill (self);

	if (self->cursor_row >= 0 && self->cursor_object &&
	    (self->cursor_row != old_cursor || cursor_was_removed))
		g_signal_emit (self, widget_signals[CURSOR_CHANGED], 0, (guint) self->cursor_row, self->cursor_object);
	if (selection_changed)
		g_signal_emit (self, widget_signals[SELECTION_CHANGED], 0);

	e_util_call_malloc_trim_limited ();
}

static void
on_tree_view_size_allocate (GtkWidget *widget,
			    GdkRectangle *allocation,
			    EVirtualTree *self)
{
	gint new_height;

	if (self->in_refill)
		return;

	new_height = allocation->height;

	if (new_height == self->last_alloc_height)
		return;

	self->last_alloc_height = new_height;
	virtual_tree_update_scrollbar (self);
	self->cursor_scroll_pending = TRUE;
	self->cursor_scroll_pending_row = self->cursor_row;
	virtual_tree_schedule_refill (self);
}

static void
on_tree_view_style_updated (GtkWidget *widget,
			    EVirtualTree *self)
{
	gint old_stride = self->row_stride;
	gdouble old_value = self->vadjustment ? gtk_adjustment_get_value (self->vadjustment) : 0.0;

	virtual_tree_invalidate_metrics (self);
	virtual_tree_update_scrollbar (self);

	if (self->row_stride != old_stride && old_stride > 0) {
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection (self->tree_view);
		g_signal_handler_block (selection, self->selection_changed_id);
		g_signal_handler_block (self->tree_view, self->cursor_changed_id);

		if (self->vadjustment && self->row_stride > 0)
			gtk_adjustment_set_value (self->vadjustment,
				old_value * ((gdouble) self->row_stride / (gdouble) old_stride));

		virtual_tree_refill (self);

		g_signal_handler_unblock (self->tree_view, self->cursor_changed_id);
		g_signal_handler_unblock (selection, self->selection_changed_id);
	}
}

static gboolean
on_tree_view_draw (GtkWidget *widget,
		   cairo_t *cr,
		   EVirtualTree *self)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkStyleContext *context;
	GdkRGBA bg_color;
	PangoLayout *layout;
	gint content_top = 0;
	gboolean valid, have_color = FALSE;

	if (self->empty_message && self->empty_message[0] &&
	    (!self->model || e_virtual_tree_model_get_row_count (self->model) == 0) &&
	    gtk_tree_model_iter_n_children (GTK_TREE_MODEL (self->list_store), NULL) == 0) {
		GdkRGBA color;
		gint w, header_y = 0, padding = 20;

		context = gtk_widget_get_style_context (widget);
		gtk_style_context_save (context);
		gtk_style_context_set_state (context,
			gtk_style_context_get_state (context) | GTK_STATE_FLAG_INSENSITIVE);
		gtk_style_context_get_color (context,
			gtk_style_context_get_state (context), &color);
		gtk_style_context_restore (context);

		if (gtk_widget_get_realized (widget))
			gtk_tree_view_convert_bin_window_to_widget_coords (GTK_TREE_VIEW (widget), 0, 0, NULL, &header_y);

		layout = gtk_widget_create_pango_layout (widget, self->empty_message);
		w = gtk_widget_get_allocated_width (widget) - 2 * padding;
		if (w > 0)
			pango_layout_set_width (layout, w * PANGO_SCALE);
		pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);

		gdk_cairo_set_source_rgba (cr, &color);
		cairo_move_to (cr, (gdouble) padding, (gdouble) (header_y + padding));
		pango_cairo_show_layout (cr, layout);

		g_object_unref (layout);
		return FALSE;
	}

	if (self->group_depth == 0)
		return FALSE;

	gtk_tree_view_convert_bin_window_to_widget_coords (self->tree_view, 0, 0, NULL, &content_top);

	cairo_save (cr);
	cairo_rectangle (cr, 0.0, (gdouble) content_top,
		(gdouble) gtk_widget_get_allocated_width (widget),
		(gdouble) (gtk_widget_get_allocated_height (widget) - content_top));
	cairo_clip (cr);

	context = gtk_widget_get_style_context (widget);
	gtk_style_context_save (context);
	gtk_style_context_add_class (context, "virtual-tree-group");
	have_color = gtk_style_context_lookup_color (context, "theme_bg_color", &bg_color);
	gtk_style_context_restore (context);

	if (!have_color) {
		bg_color.red = 0.85;
		bg_color.green = 0.85;
		bg_color.blue = 0.85;
		bg_color.alpha = 1.0;
	} else {
		bg_color.red *= 0.85;
		bg_color.green *= 0.85;
		bg_color.blue *= 0.85;
	}

	layout = gtk_widget_create_pango_layout (widget, NULL);
	pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);

	model = GTK_TREE_MODEL (self->list_store);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		gboolean is_group = FALSE;

		gtk_tree_model_get (model, &iter, COL_IS_GROUP, &is_group, -1);

		if (is_group) {
			GObject *row_object = NULL;
			GtkTreePath *path;
			GdkRectangle rect;
			gint expander_size;
			guint depth;
			PangoAttrList *attrs;

			gtk_tree_model_get (model, &iter,
				COL_ROW_OBJECT, &row_object,
				COL_DEPTH, &depth,
				-1);

			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_view_get_background_area (
				self->tree_view, path, NULL, &rect);
			gtk_tree_path_free (path);

			gtk_tree_view_convert_bin_window_to_widget_coords (
				self->tree_view, rect.x, rect.y, &rect.x, &rect.y);

			if (row_object) {
				ColumnInfo *first_col;
				RendererInfo *first_ri;
				gchar *text = NULL;
				GdkRGBA fg_color;
				gint expander_render_size;
				gint vis_w;
				gboolean expandable, expanded;

				/* Full-row background */
				vis_w = gtk_widget_get_allocated_width (widget);
				gdk_cairo_set_source_rgba (cr, &bg_color);
				cairo_rectangle (cr,
					0.0, (gdouble) rect.y,
					(gdouble) vis_w, (gdouble) rect.height);
				cairo_fill (cr);

				get_expander_metrics (widget, &expander_size, &expander_render_size);

				gtk_tree_model_get (model, &iter,
					COL_EXPANDABLE, &expandable,
					COL_EXPANDED, &expanded,
					-1);

				/* Draw expander triangle */
				if (expandable) {
					gint ex, ey, area_h;

					gtk_style_context_save (context);
					gtk_style_context_add_class (context, GTK_STYLE_CLASS_EXPANDER);
					if (expanded)
						gtk_style_context_set_state (context, gtk_style_context_get_state (context) | GTK_STATE_FLAG_CHECKED);

					if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
						ex = rect.x + vis_w - ((gint) (depth + 1) * expander_size) + (expander_size - expander_render_size);
					else
						ex = rect.x + (gint) depth * expander_size + (expander_size - expander_render_size);
					area_h = rect.height;
					if (area_h % 2 != expander_render_size % 2)
						area_h -= 1;
					ey = rect.y + (area_h - expander_render_size) / 2;

					gtk_render_expander (context, cr,
						(gdouble) ex, (gdouble) ey,
						(gdouble) expander_render_size,
						(gdouble) expander_render_size);
					gtk_style_context_restore (context);
				}

				/* Draw bold label spanning full row */
				first_col = g_ptr_array_index (self->columns, 0);
				first_ri = g_ptr_array_index (first_col->renderers, 0);

				if (first_ri->func) {
					GtkCellRenderer *tmp = gtk_cell_renderer_text_new ();
					first_ri->func (self, tmp, row_object, 0, first_ri->user_data);
					g_object_get (tmp, "text", &text, NULL);
					g_object_ref_sink (tmp);
					g_object_unref (tmp);
				}

				if (text) {
					gint text_x, text_h, visible_width;

					visible_width = vis_w;
					if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) {
						text_x = rect.x;
						pango_layout_set_width (layout, (visible_width - (gint) (depth + 1) * expander_size) * PANGO_SCALE);
						pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);
					} else {
						text_x = rect.x + (gint) (depth + 1) * expander_size;
						pango_layout_set_width (layout, (visible_width - text_x) * PANGO_SCALE);
					}
					pango_layout_set_text (layout, text, -1);

					attrs = pango_attr_list_new ();
					pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
					pango_layout_set_attributes (layout, attrs);
					pango_attr_list_unref (attrs);

					gtk_style_context_get_color (context, gtk_style_context_get_state (context), &fg_color);

					pango_layout_get_pixel_size (layout, NULL, &text_h);
					gdk_cairo_set_source_rgba (cr, &fg_color);
					cairo_move_to (cr, text_x, rect.y + (rect.height - text_h) / 2);
					pango_cairo_show_layout (cr, layout);

					g_free (text);
				}

				g_object_unref (row_object);
			}
		}

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	g_object_unref (layout);

	cairo_restore (cr);

	return FALSE;
}

static gboolean
on_tree_view_scroll_event (GtkWidget *widget,
			   GdkEventScroll *event,
			   EVirtualTree *self)
{
	GtkAdjustment *adj;
	gdouble value, delta;
	gdouble old_value, max_value;

	if (!self->vadjustment)
		return FALSE;

	adj = self->vadjustment;

	switch (event->direction) {
	case GDK_SCROLL_UP:
		delta = -SCROLL_STEP_PIXELS;
		break;
	case GDK_SCROLL_DOWN:
		delta = SCROLL_STEP_PIXELS;
		break;
	case GDK_SCROLL_SMOOTH:
		delta = event->delta_y * SCROLL_STEP_PIXELS;
		break;
	default:
		return FALSE;
	}

	old_value = gtk_adjustment_get_value (adj);
	max_value = gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj);

	value = CLAMP (old_value + delta, 0, max_value);

	if (value == old_value &&
	    ((delta < 0 && old_value <= 0) ||
	     (delta > 0 && old_value >= max_value)))
		return FALSE;

	gtk_adjustment_set_value (adj, value);

	return TRUE;
}

static void
on_tv_vadjustment_changed (GtkAdjustment *adj,
			    EVirtualTree *self)
{
	gdouble expected = 0.0;

	/* Allow the sub-row offset set by gtk_tree_view_scroll_to_point
	 * in refill, but clamp anything larger (from gtk_tree_view_set_cursor
	 * auto-scroll). */
	if (self->vadjustment && self->row_stride > 0)
		expected = fmod (gtk_adjustment_get_value (self->vadjustment), (gdouble) self->row_stride);

	if (gtk_adjustment_get_value (adj) != expected)
		gtk_adjustment_set_value (adj, expected);
}

static void
on_selection_changed (GtkTreeSelection *selection,
		      EVirtualTree *self)
{
	GtkTreeModel *model;
	GList *selected_paths, *link;
	GHashTable *before_visible_selected;
	gboolean changed = FALSE;

	if (self->in_refill || !self->model)
		return;

	before_visible_selected = g_hash_table_new_full (self->key_type.hash_func, self->key_type.equal_func, self->key_type.free_func, NULL);

	/* Rebuild selection from the GTK tree selection within the visible window.
	 * First, deselect all visible-window keys from our tracking. */
	if (self->visible_count > 0) {
		GtkTreeIter vis_iter;
		gboolean valid;

		valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->list_store), &vis_iter);
		while (valid) {
			GObject *row_obj;
			guint visible_row = 0;
			gconstpointer key;

			gtk_tree_model_get (GTK_TREE_MODEL (self->list_store), &vis_iter, COL_VISIBLE_ROW_IDX, &visible_row, -1);
			row_obj = e_virtual_tree_model_dup_row (self->model, visible_row);
			key = row_obj ? e_virtual_tree_model_get_row_key (self->model, row_obj) : NULL;

			if (key) {
				if (virtual_tree_is_key_selected (self, key))
					g_hash_table_add (before_visible_selected, virtual_tree_copy_key (self, key));

				virtual_tree_deselect_key (self, key);
			}

			g_clear_object (&row_obj);
			valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->list_store), &vis_iter);
		}
	}

	/* Then add back what's actually selected */
	model = GTK_TREE_MODEL (self->list_store);
	selected_paths = gtk_tree_selection_get_selected_rows (selection, &model);

	for (link = selected_paths; link; link = link->next) {
		GtkTreePath *path = link->data;
		GtkTreeIter iter;

		if (gtk_tree_model_get_iter (model, &iter, path)) {
			GObject *row_obj;
			guint visible_row = 0;
			gconstpointer key;

			gtk_tree_model_get (model, &iter, COL_VISIBLE_ROW_IDX, &visible_row, -1);
			row_obj = e_virtual_tree_model_dup_row (self->model, visible_row);
			key = row_obj ? e_virtual_tree_model_get_row_key (self->model, row_obj) : NULL;

			if (key) {
				if (!g_hash_table_remove (before_visible_selected, key))
					changed = TRUE;

				virtual_tree_select_key (self, key);
			}

			g_clear_object (&row_obj);
		}
	}

	g_list_free_full (selected_paths, (GDestroyNotify) gtk_tree_path_free);

	if (g_hash_table_size (before_visible_selected) > 0)
		changed = TRUE;

	g_hash_table_unref (before_visible_selected);

	if (changed)
		g_signal_emit (self, widget_signals[SELECTION_CHANGED], 0);
}

static guint
find_column_index (EVirtualTree *self,
		   GtkTreeViewColumn *tree_column)
{
	guint ii;

	for (ii = 0; ii < self->columns->len; ii++) {
		ColumnInfo *info = g_ptr_array_index (self->columns, ii);

		if (info->tree_column == tree_column)
			return ii;
	}

	return 0;
}

static gint
find_column_index_by_id (EVirtualTree *self,
			 const gchar *column_id)
{
	guint ii;

	for (ii = 0; ii < self->columns->len; ii++) {
		ColumnInfo *info = g_ptr_array_index (self->columns, ii);

		if (g_strcmp0 (info->column_id, column_id) == 0)
			return (gint) ii;
	}

	return -1;
}

static void
virtual_tree_select_range (EVirtualTree *self,
			   gint from,
			   gint to)
{
	gint lo = MIN (from, to);
	gint hi = MAX (from, to);
	gint ii;

	for (ii = lo; ii <= hi; ii++) {
		GObject *obj = e_virtual_tree_model_dup_row (self->model, (guint) ii);

		if (obj) {
			gconstpointer key = e_virtual_tree_model_get_row_key (self->model, obj);

			if (key)
				virtual_tree_select_key (self, key);
			g_clear_object (&obj);
		}
	}
}

static void
virtual_tree_deselect_range (EVirtualTree *self,
			     gint from,
			     gint to)
{
	gint lo = MIN (from, to);
	gint hi = MAX (from, to);
	gint ii;

	for (ii = lo; ii <= hi; ii++) {
		GObject *obj = e_virtual_tree_model_dup_row (self->model, (guint) ii);

		if (obj) {
			gconstpointer key = e_virtual_tree_model_get_row_key (self->model, obj);

			if (key)
				virtual_tree_deselect_key (self, key);
			g_clear_object (&obj);
		}
	}
}

static void
virtual_tree_apply_selection (EVirtualTree *self,
			      gint new_cursor,
			      gboolean selection_changed)
{
	gint old_cursor = self->cursor_row;

	virtual_tree_clear_pending_selection (self);

	virtual_tree_move_cursor (self, new_cursor, FALSE);
	virtual_tree_refill (self);

	/* Re-assert cursor after refill for focus rectangle */
	if (self->cursor_row >= 0 && (guint) self->cursor_row >= self->first_visible_row) {
		GtkTreePath *re_path;
		guint re_offset = (guint) self->cursor_row - self->first_visible_row;

		re_path = gtk_tree_path_new_from_indices (re_offset, -1);
		g_signal_handler_block (self->tree_view, self->cursor_changed_id);
		g_signal_handler_block (gtk_tree_view_get_selection (self->tree_view), self->selection_changed_id);
		gtk_tree_view_set_cursor (self->tree_view, re_path, NULL, FALSE);

		if (self->selection_mode == GTK_SELECTION_MULTIPLE) {
			GtkTreeSelection *sel;
			GtkTreeIter sel_iter;
			gboolean valid;

			sel = gtk_tree_view_get_selection (self->tree_view);
			valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->list_store), &sel_iter);
			while (valid) {
				GObject *row_obj = NULL;
				gconstpointer key;

				gtk_tree_model_get (GTK_TREE_MODEL (self->list_store), &sel_iter, COL_ROW_OBJECT, &row_obj, -1);
				key = row_obj ? e_virtual_tree_model_get_row_key (self->model, row_obj) : NULL;

				if (key && virtual_tree_is_key_selected (self, key))
					gtk_tree_selection_select_iter (sel, &sel_iter);
				else
					gtk_tree_selection_unselect_iter (sel, &sel_iter);

				g_clear_object (&row_obj);
				valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->list_store), &sel_iter);
			}
		}

		g_signal_handler_unblock (gtk_tree_view_get_selection (self->tree_view), self->selection_changed_id);
		g_signal_handler_unblock (self->tree_view, self->cursor_changed_id);
		gtk_tree_path_free (re_path);
	}

	if (old_cursor != new_cursor && self->cursor_object)
		g_signal_emit (self, widget_signals[CURSOR_CHANGED], 0, (guint) new_cursor, self->cursor_object);
	if (selection_changed)
		g_signal_emit (self, widget_signals[SELECTION_CHANGED], 0);
}

static gboolean
on_button_press_event (GtkWidget *widget,
		       GdkEventButton *event,
		       EVirtualTree *self)
{
	GtkTreePath *path = NULL;
	GtkTreeViewColumn *column = NULL;
	gint cell_x, cell_y;

	if (event->button != 1 && event->button != 3)
		return FALSE;

	if (event->type == GDK_2BUTTON_PRESS) {
		if (event->button == 1 && self->cursor_row >= 0 &&
		    self->cursor_object) {
			g_signal_emit (self, widget_signals[ROW_ACTIVATED], 0, (guint) self->cursor_row, self->cursor_object);
		}
		return TRUE;
	}

	if (event->type != GDK_BUTTON_PRESS)
		return TRUE;

	if (!gtk_tree_view_get_path_at_pos (self->tree_view, (gint) event->x, (gint) event->y, &path, &column, &cell_x, &cell_y)) {
		if (event->button == 3) {
			gboolean handled = FALSE;

			g_signal_emit (self, widget_signals[RIGHT_CLICK], 0,
				(guint) 0, NULL, (GdkEvent *) event, &handled);

			return handled;
		}

		return FALSE;
	}

	if (path && column) {
		GtkTreeIter iter;

		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (self->list_store), &iter, path)) {
			GObject *row_object = NULL;
			GtkCellRenderer *hit_renderer = NULL;
			gboolean expandable = FALSE;
			gboolean is_group_row = FALSE;
			guint depth = 0;
			guint visible_row = 0;

			gtk_tree_model_get (GTK_TREE_MODEL (self->list_store), &iter,
				COL_EXPANDABLE, &expandable,
				COL_DEPTH, &depth,
				COL_VISIBLE_ROW_IDX, &visible_row,
				COL_IS_GROUP, &is_group_row,
				-1);
			row_object = e_virtual_tree_model_dup_row (self->model, visible_row);

			if (row_object && event->button == 3) {
				gboolean handled = FALSE;
				gconstpointer click_key = e_virtual_tree_model_get_row_key (self->model, row_object);

				if (click_key && !virtual_tree_is_key_selected (self, click_key)) {
					self->inverted_selection = FALSE;
					g_hash_table_remove_all (self->selected_keys);
					virtual_tree_select_key (self, click_key);
					virtual_tree_apply_selection (self, (gint) visible_row, TRUE);
				}

				g_signal_emit (self, widget_signals[RIGHT_CLICK], 0,
					visible_row, row_object,
					(GdkEvent *) event, &handled);
				g_object_unref (row_object);
				gtk_tree_path_free (path);
				return handled;
			}

			if (row_object && expandable) {
				GtkCellArea *click_area;
				gboolean has_expander = FALSE;

				click_area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));
				if (E_IS_CELL_AREA_LINES (click_area))
					has_expander = (cell_area_lines_find_expander (E_CELL_AREA_LINES (click_area), NULL) != NULL);

				if (has_expander) {
				gint expander_size, expander_render_size, arrow_start, arrow_end;

				get_expander_metrics (GTK_WIDGET (self->tree_view), &expander_size, &expander_render_size);

				if (gtk_widget_get_direction (GTK_WIDGET (self->tree_view)) == GTK_TEXT_DIR_RTL) {
					gint column_width = gtk_tree_view_column_get_width (column);

					arrow_start = column_width - (gint) (depth + 1) * expander_size + (expander_size - expander_render_size);
					arrow_end = column_width - (gint) depth * expander_size;
				} else {
					arrow_start = (gint) depth * expander_size + (expander_size - expander_render_size);
					arrow_end = (gint) (depth + 1) * expander_size;
				}

				if (cell_x >= arrow_start && cell_x < arrow_end) {
					gboolean expanded;

					expanded = e_virtual_tree_model_get_expanded (self->model, row_object);
					e_virtual_tree_model_set_expanded (self->model, row_object, !expanded);

					g_signal_emit (self, widget_signals[ROW_EXPANDED], 0,
						visible_row, row_object, !expanded);

					g_object_unref (row_object);
					gtk_tree_path_free (path);
					return TRUE;
				}
				}
			}

			if (row_object) {
				guint col_idx = find_column_index (self, column);
				GtkCellArea *area;
				GdkRectangle cell_rect;
				gboolean cell_click_handled = FALSE;

				area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));
				if (E_IS_CELL_AREA_LINES (area)) {
					gtk_tree_view_column_cell_set_cell_data (column, GTK_TREE_MODEL (self->list_store), &iter, FALSE, FALSE);
					gtk_tree_view_get_cell_area (self->tree_view, path, column, &cell_rect);
					hit_renderer = e_cell_area_lines_get_renderer_at_pos (
						E_CELL_AREA_LINES (area),
						GTK_WIDGET (self->tree_view),
						cell_rect.width,
						cell_rect.height,
						(gint) event->x - cell_rect.x,
						(gint) event->y - cell_rect.y);
				}

				g_signal_emit (self, widget_signals[CELL_CLICKED], 0,
					visible_row, row_object, col_idx, hit_renderer, &cell_click_handled);
				g_object_unref (row_object);

				if (cell_click_handled) {
					gtk_tree_path_free (path);
					return TRUE;
				}
			}

			if (event->button == 1) {
				GObject *click_row = e_virtual_tree_model_dup_row (self->model, visible_row);
				gconstpointer click_key = click_row ? e_virtual_tree_model_get_row_key (self->model, click_row) : NULL;
				gboolean sel_changed = TRUE;
				gboolean already_selected = click_key && virtual_tree_is_key_selected (self, click_key);
				gboolean is_sole_selected = click_key && (
					(!self->inverted_selection &&
					 g_hash_table_size (self->selected_keys) == 1 &&
					 g_hash_table_contains (self->selected_keys, click_key)) ||
					(self->inverted_selection &&
					 self->model &&
					 e_virtual_tree_model_get_row_count (self->model) - g_hash_table_size (self->selected_keys) == 1 &&
					 !g_hash_table_contains (self->selected_keys, click_key)));
				gboolean has_ctrl = (event->state & GDK_CONTROL_MASK) != 0;
				gboolean has_shift = (event->state & GDK_SHIFT_MASK) != 0;

				/* A plain click on an already-selected row within a bigger selection
				 * does not collapse the selection right away, so that the whole
				 * selection can still be dragged; it is collapsed on button release,
				 * unless a drag has started in the meantime. */
				self->drag.defer_row = (self->selection_mode == GTK_SELECTION_MULTIPLE &&
					!has_ctrl && !has_shift && already_selected && !is_sole_selected) ?
					(gint) visible_row : -1;

				if (self->drag.defer_row < 0) {
					if (self->selection_mode == GTK_SELECTION_MULTIPLE) {
						if (has_shift && self->anchor_row >= 0) {
							if (!has_ctrl && self->shift_end_row >= 0)
								virtual_tree_deselect_range (self, self->anchor_row, self->shift_end_row);
							virtual_tree_select_range (self, self->anchor_row, (gint) visible_row);
							self->shift_end_row = (gint) visible_row;
						} else if (has_ctrl && click_key) {
							if (virtual_tree_is_key_selected (self, click_key))
								virtual_tree_deselect_key (self, click_key);
							else
								virtual_tree_select_key (self, click_key);
							self->anchor_row = (gint) visible_row;
							self->shift_end_row = -1;
						} else {
							sel_changed = !is_sole_selected;
							self->inverted_selection = FALSE;
							g_hash_table_remove_all (self->selected_keys);
							if (click_key)
								virtual_tree_select_key (self, click_key);
							self->anchor_row = (gint) visible_row;
							self->shift_end_row = -1;
						}
					} else {
						sel_changed = !is_sole_selected;
						self->inverted_selection = FALSE;
						g_hash_table_remove_all (self->selected_keys);
						if (click_key)
							virtual_tree_select_key (self, click_key);
					}

					if (!gtk_widget_has_focus (GTK_WIDGET (self->tree_view))) {
						g_signal_handler_block (self->tree_view, self->cursor_changed_id);
						g_signal_handler_block (gtk_tree_view_get_selection (self->tree_view), self->selection_changed_id);
						gtk_widget_grab_focus (GTK_WIDGET (self->tree_view));
						g_signal_handler_unblock (gtk_tree_view_get_selection (self->tree_view), self->selection_changed_id);
						g_signal_handler_unblock (self->tree_view, self->cursor_changed_id);
					}
					virtual_tree_apply_selection (self, (gint) visible_row, sel_changed);

					if (self->editable && !sel_changed && column &&
					    (!hit_renderer || GTK_IS_CELL_RENDERER_TEXT (hit_renderer))) {
						g_signal_handler_block (self->tree_view, self->cursor_changed_id);
						gtk_tree_view_set_cursor_on_cell (self->tree_view, path, column, hit_renderer, TRUE);
						g_signal_handler_unblock (self->tree_view, self->cursor_changed_id);
					}
				}

				if (!is_group_row && self->drag.source_enabled && self->drag.source_target_list &&
				    (self->drag.source_start_mask & GDK_BUTTON1_MASK) != 0) {
					self->drag.button_down = TRUE;
					self->drag.start_x = (gint) event->x;
					self->drag.start_y = (gint) event->y;
					self->drag.start_button = event->button;
				}

				g_clear_object (&click_row);
				gtk_tree_path_free (path);
				return TRUE;
			}
		}
	}

	g_clear_pointer (&path, gtk_tree_path_free);
	return FALSE;
}

static gboolean
on_motion_notify_event (GtkWidget *widget,
			GdkEventMotion *event,
			EVirtualTree *self)
{
	GdkDragContext *context;

	if (!self->drag.button_down)
		return FALSE;

	if (!gtk_drag_check_threshold (widget, self->drag.start_x, self->drag.start_y, (gint) event->x, (gint) event->y))
		return FALSE;

	self->drag.button_down = FALSE;
	self->drag.defer_row = -1;

	context = gtk_drag_begin_with_coordinates (GTK_WIDGET (self->tree_view), self->drag.source_target_list,
		self->drag.source_actions, (gint) self->drag.start_button, (GdkEvent *) event, -1, -1);

	if (context)
		gtk_drag_set_icon_default (context);

	return TRUE;
}

static gboolean
on_button_release_event (GtkWidget *widget,
			 GdkEventButton *event,
			 EVirtualTree *self)
{
	guint row;
	GObject *row_object;
	gconstpointer key;

	if (event->button != 1)
		return FALSE;

	self->drag.button_down = FALSE;

	if (self->drag.defer_row < 0)
		return FALSE;

	row = (guint) self->drag.defer_row;
	self->drag.defer_row = -1;

	row_object = e_virtual_tree_model_dup_row (self->model, row);
	if (!row_object)
		return FALSE;

	key = e_virtual_tree_model_get_row_key (self->model, row_object);

	self->inverted_selection = FALSE;
	g_hash_table_remove_all (self->selected_keys);
	if (key)
		virtual_tree_select_key (self, key);
	self->anchor_row = (gint) row;
	self->shift_end_row = -1;

	virtual_tree_apply_selection (self, (gint) row, TRUE);

	g_object_unref (row_object);

	return FALSE;
}

static gboolean
virtual_tree_search_timeout_cb (gpointer user_data)
{
	EVirtualTree *self = user_data;

	if (self->search_buffer)
		g_string_truncate (self->search_buffer, 0);
	self->search_timeout_id = 0;

	return G_SOURCE_REMOVE;
}

static gboolean
on_key_press_event (GtkWidget *widget,
		    GdkEventKey *event,
		    EVirtualTree *self)
{
	guint total_rows;
	gint cursor, new_cursor;
	gunichar uni_ch;

	g_signal_stop_emission_by_name (widget, "key-press-event");

	if (!self->model)
		return FALSE;

	total_rows = e_virtual_tree_model_get_row_count (self->model);
	if (total_rows == 0)
		return FALSE;

	if (!gtk_widget_has_focus (GTK_WIDGET (self->tree_view))) {
		g_signal_handler_block (self->tree_view, self->cursor_changed_id);
		g_signal_handler_block (gtk_tree_view_get_selection (self->tree_view), self->selection_changed_id);
		gtk_widget_grab_focus (GTK_WIDGET (self->tree_view));
		g_signal_handler_unblock (gtk_tree_view_get_selection (self->tree_view), self->selection_changed_id);
		g_signal_handler_unblock (self->tree_view, self->cursor_changed_id);
	}

	cursor = self->cursor_row;

	switch (event->keyval) {
	case GDK_KEY_Up:
		new_cursor = MAX (cursor - 1, 0);
		break;
	case GDK_KEY_Down:
		new_cursor = cursor + 1;
		if (new_cursor >= (gint) total_rows)
			new_cursor = cursor;
		break;
	case GDK_KEY_Page_Up: {
		guint page = self->visible_count > 2 ? self->visible_count - 2 : 1;
		new_cursor = MAX (cursor - (gint) page, 0);
		break;
	}
	case GDK_KEY_Page_Down: {
		guint page = self->visible_count > 2 ? self->visible_count - 2 : 1;
		new_cursor = MIN (cursor + (gint) page, (gint) total_rows - 1);
		break;
	}
	case GDK_KEY_Home:
		new_cursor = 0;
		break;
	case GDK_KEY_End:
		new_cursor = (gint) total_rows - 1;
		break;
	case GDK_KEY_space:
		if (self->selection_mode == GTK_SELECTION_MULTIPLE &&
		    (event->state & GDK_CONTROL_MASK) != 0 &&
		    cursor >= 0) {
			GObject *row_obj = e_virtual_tree_model_dup_row (self->model, (guint) cursor);
			if (row_obj) {
				gconstpointer key = e_virtual_tree_model_get_row_key (self->model, row_obj);
				if (key) {
					if (virtual_tree_is_key_selected (self, key))
						virtual_tree_deselect_key (self, key);
					else
						virtual_tree_select_key (self, key);
				}
				g_clear_object (&row_obj);
			}
			self->anchor_row = cursor;
			self->shift_end_row = -1;
			virtual_tree_apply_selection (self, cursor, TRUE);
			return TRUE;
		}
		new_cursor = -1;
		break;

	case GDK_KEY_Right:
		if (cursor >= 0) {
			GObject *obj = e_virtual_tree_model_dup_row (self->model, cursor);
			if (obj && e_virtual_tree_model_is_expandable (self->model, obj) && !e_virtual_tree_model_get_expanded (self->model, obj)) {
				e_virtual_tree_model_set_expanded (self->model, obj, TRUE);
				g_signal_emit (self, widget_signals[ROW_EXPANDED], 0, (guint) cursor, obj, TRUE);
				virtual_tree_move_cursor (self, cursor, TRUE);
			}
			g_clear_object (&obj);
		}
		return TRUE;

	case GDK_KEY_Left:
		if (cursor >= 0) {
			GObject *obj = e_virtual_tree_model_dup_row (self->model, cursor);
			if (obj && e_virtual_tree_model_is_expandable (self->model, obj) && e_virtual_tree_model_get_expanded (self->model, obj)) {
				e_virtual_tree_model_set_expanded (self->model, obj, FALSE);
				g_signal_emit (self, widget_signals[ROW_EXPANDED], 0, (guint) cursor, obj, FALSE);
				virtual_tree_move_cursor (self, cursor, TRUE);
			}
			g_clear_object (&obj);
		}
		return TRUE;

	default:
		new_cursor = -1;
		break;
	}

	if (new_cursor >= 0 && new_cursor != cursor) {
		if (self->selection_mode == GTK_SELECTION_MULTIPLE) {
			gboolean has_ctrl = (event->state & GDK_CONTROL_MASK) != 0;
			gboolean has_shift = (event->state & GDK_SHIFT_MASK) != 0;

			if (has_ctrl && !has_shift) {
				virtual_tree_apply_selection (self, new_cursor, FALSE);
			} else if (has_shift && self->anchor_row >= 0) {
				if (self->shift_end_row >= 0)
					virtual_tree_deselect_range (self, self->anchor_row, self->shift_end_row);
				virtual_tree_select_range (self, self->anchor_row, new_cursor);
				self->shift_end_row = new_cursor;
				virtual_tree_apply_selection (self, new_cursor, TRUE);
			} else {
				GObject *row_obj = e_virtual_tree_model_dup_row (self->model, (guint) new_cursor);
				gconstpointer key = row_obj ? e_virtual_tree_model_get_row_key (self->model, row_obj) : NULL;
				self->inverted_selection = FALSE;
				g_hash_table_remove_all (self->selected_keys);
				if (key)
					virtual_tree_select_key (self, key);
				self->anchor_row = new_cursor;
				self->shift_end_row = -1;
				virtual_tree_apply_selection (self, new_cursor, TRUE);
				g_clear_object (&row_obj);
			}
		} else {
			GObject *row_obj = e_virtual_tree_model_dup_row (self->model, (guint) new_cursor);
			gconstpointer key = row_obj ? e_virtual_tree_model_get_row_key (self->model, row_obj) : NULL;
			self->inverted_selection = FALSE;
			g_hash_table_remove_all (self->selected_keys);
			if (key)
				virtual_tree_select_key (self, key);
			virtual_tree_apply_selection (self, new_cursor, TRUE);
			g_clear_object (&row_obj);
		}

		return TRUE;
	}

	if (new_cursor >= 0)
		return TRUE;

	/* Type-ahead search */
	uni_ch = gdk_keyval_to_unicode (event->keyval);

	if (self->search_func && uni_ch != 0 && g_unichar_isprint (uni_ch) &&
	    (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK)) == 0) {
		gchar ch_utf8[6];
		gint ch_len;
		guint ii;

		if (!self->search_buffer)
			self->search_buffer = g_string_new (NULL);

		ch_len = g_unichar_to_utf8 (uni_ch, ch_utf8);
		g_string_append_len (self->search_buffer, ch_utf8, ch_len);

		if (self->search_timeout_id)
			g_source_remove (self->search_timeout_id);

		self->search_timeout_id = g_timeout_add (300,
			virtual_tree_search_timeout_cb, self);

		for (ii = 0; ii < total_rows; ii++) {
			GObject *obj = e_virtual_tree_model_dup_row (self->model, ii);

			if (obj) {
				gchar *text = self->search_func (self, obj, self->search_user_data);

				if (text) {
					gchar *lower_text = g_utf8_strdown (text, -1);
					gchar *lower_search = g_utf8_strdown (self->search_buffer->str, -1);

					if (g_str_has_prefix (lower_text, lower_search)) {
						e_virtual_tree_set_cursor (self, ii);
						g_free (lower_text);
						g_free (lower_search);
						g_free (text);
						g_clear_object (&obj);
						return TRUE;
					}

					g_free (lower_text);
					g_free (lower_search);
					g_free (text);
				}
				g_clear_object (&obj);
			}
		}

		return TRUE;
	}

	return FALSE;
}

static void
on_renderer_editing_started (GtkCellRenderer *renderer,
			     GtkCellEditable *editable,
			     const gchar *path_str,
			     EVirtualTree *self)
{
	self->editing_active = TRUE;
}

static void
on_renderer_editing_canceled (GtkCellRenderer *renderer,
			      EVirtualTree *self)
{
	self->editing_active = FALSE;
	virtual_tree_schedule_refill (self);
}

static void
on_renderer_edited (GtkCellRendererText *renderer,
		    const gchar *path_str,
		    const gchar *new_text,
		    EVirtualTree *self)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	self->editing_active = FALSE;

	path = gtk_tree_path_new_from_string (path_str);
	if (path && gtk_tree_model_get_iter (GTK_TREE_MODEL (self->list_store), &iter, path)) {
		GObject *row_object = NULL;
		guint visible_row = 0;
		guint col_idx = 0;
		guint ii, jj;

		gtk_tree_model_get (GTK_TREE_MODEL (self->list_store), &iter,
			COL_VISIBLE_ROW_IDX, &visible_row,
			-1);
		row_object = e_virtual_tree_model_dup_row (self->model, visible_row);

		for (ii = 0; ii < self->columns->len; ii++) {
			ColumnInfo *col = g_ptr_array_index (self->columns, ii);

			for (jj = 0; jj < col->renderers->len; jj++) {
				RendererInfo *ri = g_ptr_array_index (col->renderers, jj);

				if (ri->renderer == GTK_CELL_RENDERER (renderer)) {
					col_idx = ii;
					goto found;
				}
			}
		}
 found:

		if (row_object) {
			g_signal_emit (self, widget_signals[CELL_EDITED], 0,
				visible_row, row_object, col_idx, renderer, new_text);
			g_object_unref (row_object);
		}
	}

	g_clear_pointer (&path, gtk_tree_path_free);
}

static gboolean
get_row_at_pos (EVirtualTree *self,
		gint x,
		gint y,
		guint *out_row,
		GObject **out_object)
{
	GtkTreePath *path = NULL;
	GtkTreeIter iter;

	if (!gtk_tree_view_get_path_at_pos (self->tree_view, x, y, &path, NULL, NULL, NULL))
		return FALSE;

	if (!path)
		return FALSE;

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (self->list_store), &iter, path)) {
		guint visible_row = 0;
		GObject *row_object = NULL;

		gtk_tree_model_get (GTK_TREE_MODEL (self->list_store), &iter,
			COL_VISIBLE_ROW_IDX, &visible_row,
			-1);
		row_object = e_virtual_tree_model_dup_row (self->model, visible_row);

		gtk_tree_path_free (path);

		if (out_row)
			*out_row = visible_row;
		if (out_object)
			*out_object = row_object;
		else
			g_clear_object (&row_object);

		return TRUE;
	}

	gtk_tree_path_free (path);
	return FALSE;
}

static void
on_drag_begin (GtkWidget *widget,
	       GdkDragContext *context,
	       EVirtualTree *self)
{
	guint row = 0;
	GObject *obj = NULL;

	if (self->cursor_row >= 0 && get_row_at_pos (self, 0, 0, &row, &obj)) {
		row = (guint) self->cursor_row;
	}

	self->in_drag = TRUE;
	self->drag.last_time = 0;
	g_signal_emit (self, widget_signals[TREE_DRAG_BEGIN], 0, row, context);
	g_clear_object (&obj);
}

static void
on_drag_end (GtkWidget *widget,
	     GdkDragContext *context,
	     EVirtualTree *self)
{
	self->in_drag = FALSE;
	self->drag.last_time = 0;
	g_signal_emit (self, widget_signals[TREE_DRAG_END], 0, context);
}

static void
on_drag_data_get (GtkWidget *widget,
		  GdkDragContext *context,
		  GtkSelectionData *selection_data,
		  guint info,
		  guint time,
		  EVirtualTree *self)
{
	g_signal_emit (self, widget_signals[TREE_DRAG_DATA_GET], 0,
		context, selection_data, info, time);
}

static void
on_drag_data_received (GtkWidget *widget,
		       GdkDragContext *context,
		       gint x,
		       gint y,
		       GtkSelectionData *selection_data,
		       guint info,
		       guint time,
		       EVirtualTree *self)
{
	guint row = G_MAXUINT;

	/* can be repeated through GtkTreeView internals */
	if (self->drag.last_time == time)
		return;

	self->drag.last_time = time;

	get_row_at_pos (self, x, y, &row, NULL);
	g_signal_emit (self, widget_signals[TREE_DRAG_DATA_RECEIVED], 0,
		context, row, selection_data, info, time);
}

static gboolean
on_drag_drop (GtkWidget *widget,
	      GdkDragContext *context,
	      gint x,
	      gint y,
	      guint time,
	      EVirtualTree *self)
{
	guint row = G_MAXUINT;
	gboolean handled = FALSE;

	/* can be repeated through GtkTreeView internals */
	if (self->drag.last_time == time)
		return handled;

	self->drag.last_time = time;

	get_row_at_pos (self, x, y, &row, NULL);
	g_signal_emit (self, widget_signals[TREE_DRAG_DROP], 0,
		context, row, time, &handled);
	return handled;
}

static gboolean
on_drag_motion (GtkWidget *widget,
		GdkDragContext *context,
		gint x,
		gint y,
		guint time,
		EVirtualTree *self)
{
	guint row = G_MAXUINT;
	gboolean handled = FALSE;

	get_row_at_pos (self, x, y, &row, NULL);
	g_signal_emit (self, widget_signals[TREE_DRAG_MOTION], 0,
		context, row, time, &handled);
	return handled;
}

static void
on_row_activated (GtkTreeView *tree_view,
		  GtkTreePath *path,
		  GtkTreeViewColumn *column,
		  EVirtualTree *self)
{
	GtkTreeIter iter;
	GObject *row_object = NULL;
	guint visible_row = 0;

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (self->list_store), &iter, path)) {
		gtk_tree_model_get (GTK_TREE_MODEL (self->list_store), &iter,
			COL_VISIBLE_ROW_IDX, &visible_row,
			-1);
		row_object = e_virtual_tree_model_dup_row (self->model, visible_row);

		if (row_object) {
			g_signal_emit (self, widget_signals[ROW_ACTIVATED], 0, visible_row, row_object);
			g_object_unref (row_object);
		}
	}
}

static gboolean
focus_gain_restore_idle_cb (gpointer user_data)
{
	EVirtualTree *self = user_data;
	GtkTreeSelection *selection;

	self->focus_restore_idle_id = 0;

	g_signal_handler_unblock (self->tree_view, self->cursor_changed_id);
	selection = gtk_tree_view_get_selection (self->tree_view);
	g_signal_handler_unblock (selection, self->selection_changed_id);

	virtual_tree_refill (self);

	return G_SOURCE_REMOVE;
}

static gboolean
on_tree_view_focus_in (GtkWidget *widget,
		       GdkEventFocus *event,
		       EVirtualTree *self)
{
	GtkStyleContext *context;
	GtkTreeSelection *selection;

	context = gtk_widget_get_style_context (widget);
	gtk_style_context_remove_class (context, "evtree-unfocused");

	g_signal_handler_block (self->tree_view, self->cursor_changed_id);
	selection = gtk_tree_view_get_selection (self->tree_view);
	g_signal_handler_block (selection, self->selection_changed_id);

	if (self->focus_restore_idle_id == 0)
		self->focus_restore_idle_id = g_idle_add (focus_gain_restore_idle_cb, self);

	return GDK_EVENT_PROPAGATE;
}

static gboolean
on_tree_view_focus_out (GtkWidget *widget,
			GdkEventFocus *event,
			EVirtualTree *self)
{
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, "evtree-unfocused");

	return GDK_EVENT_PROPAGATE;
}

static void
on_cursor_changed (GtkTreeView *tree_view,
		   EVirtualTree *self)
{
	GtkTreePath *path = NULL;
	GtkTreeIter iter;

	if (self->cursor_row >= 0)
		return;

	gtk_tree_view_get_cursor (tree_view, &path, NULL);

	if (path && gtk_tree_model_get_iter (GTK_TREE_MODEL (self->list_store), &iter, path)) {
		GObject *row_object;
		guint visible_row = 0;

		gtk_tree_model_get (GTK_TREE_MODEL (self->list_store), &iter,
			COL_VISIBLE_ROW_IDX, &visible_row,
			-1);
		row_object = e_virtual_tree_model_dup_row (self->model, visible_row);

		self->cursor_row = (gint) visible_row;
		set_cursor_object (self, row_object);

		if (row_object) {
			g_signal_emit (self, widget_signals[CURSOR_CHANGED], 0, visible_row, row_object);
			g_object_unref (row_object);
		}
	}

	g_clear_pointer (&path, gtk_tree_path_free);
}

static void
virtual_tree_clear_pending_selection (EVirtualTree *self)
{
	if (self->restore_selection_id) {
		g_source_remove (self->restore_selection_id);
		self->restore_selection_id = 0;
	}

	if (self->pending_restore_giveup_id) {
		g_source_remove (self->pending_restore_giveup_id);
		self->pending_restore_giveup_id = 0;
	}

	self->pending_rebuild = FALSE;
	self->pending_cursor_was_visible = FALSE;

	if (self->pending_cursor_key) {
		virtual_tree_free_key (self, self->pending_cursor_key);
		self->pending_cursor_key = NULL;
	}
}

static void
on_before_rebuild (EVirtualTreeModel *model,
		   EVirtualTree *self)
{
	if (self->pending_rebuild)
		return;

	virtual_tree_clear_pending_selection (self);

	if (self->cursor_row >= 0) {
		gconstpointer cursor_key = self->cursor_object ? e_virtual_tree_model_get_row_key (model, self->cursor_object) : NULL;

		if (cursor_key)
			self->pending_cursor_key = virtual_tree_copy_key (self, cursor_key);

		self->pending_cursor_was_visible =
			(guint) self->cursor_row >= self->first_visible_row &&
			(guint) self->cursor_row < self->first_visible_row + self->visible_count;
	} else {
		self->pending_cursor_was_visible = FALSE;
	}

	self->pending_rebuild = TRUE;
	self->cursor_row = -1;
	set_cursor_object (self, NULL);
	self->anchor_row = -1;
	self->shift_end_row = -1;
	self->cursor_scroll_pending = FALSE;
	self->cursor_scroll_pending_row = -1;
}

static gboolean
remove_stale_key_cb (gpointer key,
		     gpointer value,
		     gpointer user_data)
{
	EVirtualTreeModel *model = user_data;

	return e_virtual_tree_model_find_row_by_key (model, key) == G_MAXUINT;
}

#define PENDING_RESTORE_GIVEUP_MS 500

static void
virtual_tree_finalize_pending_restore (EVirtualTree *self)
{
	gboolean selection_changed = FALSE;
	gboolean cursor_was_visible;

	if (!self->model || !self->pending_rebuild) {
		virtual_tree_clear_pending_selection (self);
		return;
	}

	if (g_hash_table_foreach_remove (self->selected_keys, remove_stale_key_cb, self->model) > 0)
		selection_changed = TRUE;

	if (self->pending_cursor_key) {
		guint cursor_idx = e_virtual_tree_model_find_row_by_key (self->model, self->pending_cursor_key);

		if (cursor_idx != G_MAXUINT) {
			GObject *cursor_obj = e_virtual_tree_model_dup_row (self->model, cursor_idx);
			self->cursor_row = (gint) cursor_idx;
			self->anchor_row = (gint) cursor_idx;
			set_cursor_object (self, cursor_obj);
			g_clear_object (&cursor_obj);
		}
	}

	cursor_was_visible = self->pending_cursor_was_visible;

	virtual_tree_clear_pending_selection (self);

	virtual_tree_update_scrollbar (self);
	virtual_tree_refill (self);

	if (cursor_was_visible && self->cursor_row >= 0)
		virtual_tree_scroll_to_show_cursor_centered (self);

	if (selection_changed)
		g_signal_emit (self, widget_signals[SELECTION_CHANGED], 0);
	if (self->cursor_row >= 0 && self->cursor_object)
		g_signal_emit (self, widget_signals[CURSOR_CHANGED], 0, (guint) self->cursor_row, self->cursor_object);
}

static gboolean
pending_restore_giveup_cb (gpointer user_data)
{
	EVirtualTree *self = user_data;

	self->pending_restore_giveup_id = 0;

	virtual_tree_finalize_pending_restore (self);

	return G_SOURCE_REMOVE;
}

static gboolean
restore_selection_idle_cb (gpointer user_data)
{
	EVirtualTree *self = user_data;

	self->restore_selection_id = 0;

	if (!self->model || !self->pending_rebuild || !self->pending_cursor_key) {
		virtual_tree_finalize_pending_restore (self);
		return G_SOURCE_REMOVE;
	}

	if (e_virtual_tree_model_find_row_by_key (self->model, self->pending_cursor_key) != G_MAXUINT) {
		virtual_tree_finalize_pending_restore (self);
		return G_SOURCE_REMOVE;
	}

	if (self->pending_restore_giveup_id)
		g_source_remove (self->pending_restore_giveup_id);
	self->pending_restore_giveup_id = g_timeout_add (PENDING_RESTORE_GIVEUP_MS,
		pending_restore_giveup_cb, self);

	return G_SOURCE_REMOVE;
}

static void
on_after_rebuild (EVirtualTreeModel *model,
		  EVirtualTree *self)
{
	if (!self->pending_rebuild)
		return;

	if (self->restore_selection_id)
		g_source_remove (self->restore_selection_id);

	if (self->pending_restore_giveup_id) {
		g_source_remove (self->pending_restore_giveup_id);
		self->pending_restore_giveup_id = 0;
	}

	self->restore_selection_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE + 10,
		restore_selection_idle_cb, self, NULL);
}

static void
virtual_tree_disconnect_model (EVirtualTree *self)
{
	gboolean had_selection;

	if (!self->model)
		return;

	had_selection = self->tree_view != NULL &&
		(self->cursor_object != NULL || g_hash_table_size (self->selected_keys) > 0);

	if (self->model_rows_changed_id) {
		g_signal_handler_disconnect (self->model, self->model_rows_changed_id);
		self->model_rows_changed_id = 0;
	}
	if (self->model_rows_inserted_id) {
		g_signal_handler_disconnect (self->model, self->model_rows_inserted_id);
		self->model_rows_inserted_id = 0;
	}
	if (self->model_rows_removed_id) {
		g_signal_handler_disconnect (self->model, self->model_rows_removed_id);
		self->model_rows_removed_id = 0;
	}
	if (self->model_row_count_changed_id) {
		g_signal_handler_disconnect (self->model, self->model_row_count_changed_id);
		self->model_row_count_changed_id = 0;
	}
	if (self->model_before_rebuild_id) {
		g_signal_handler_disconnect (self->model, self->model_before_rebuild_id);
		self->model_before_rebuild_id = 0;
	}
	if (self->model_after_rebuild_id) {
		g_signal_handler_disconnect (self->model, self->model_after_rebuild_id);
		self->model_after_rebuild_id = 0;
	}

	virtual_tree_clear_pending_selection (self);
	g_hash_table_remove_all (self->selected_keys);

	if (self->list_store) {
		/* To not have emitted selection/cursor-changed signals */
		if (self->tree_view && self->selection_changed_id)
			g_signal_handler_block (gtk_tree_view_get_selection (self->tree_view), self->selection_changed_id);
		if (self->tree_view && self->cursor_changed_id)
			g_signal_handler_block (self->tree_view, self->cursor_changed_id);

		gtk_list_store_clear (self->list_store);

		if (self->tree_view && self->cursor_changed_id)
			g_signal_handler_unblock (self->tree_view, self->cursor_changed_id);
		if (self->tree_view && self->selection_changed_id)
			g_signal_handler_unblock (gtk_tree_view_get_selection (self->tree_view), self->selection_changed_id);
	}

	g_hash_table_remove_all (self->selected_keys);
	g_clear_object (&self->model);
	set_cursor_object (self, NULL);
	self->cursor_row = -1;
	self->anchor_row = -1;
	self->shift_end_row = -1;
	self->cursor_scroll_pending = FALSE;
	self->cursor_scroll_pending_row = -1;

	if (had_selection)
		g_signal_emit (self, widget_signals[SELECTION_CHANGED], 0);
}

static void
virtual_tree_connect_model (EVirtualTree *self,
			    EVirtualTreeModel *model)
{
	virtual_tree_disconnect_model (self);

	if (!model)
		return;

	self->model = g_object_ref (model);

	self->key_type = e_virtual_tree_model_get_key_type (model);
	g_clear_pointer (&self->selected_keys, g_hash_table_unref);
	self->selected_keys = g_hash_table_new_full (self->key_type.hash_func, self->key_type.equal_func, self->key_type.free_func, NULL);

	self->model_rows_changed_id = g_signal_connect (model, "rows-changed",
		G_CALLBACK (on_model_rows_changed), self);
	self->model_rows_inserted_id = g_signal_connect (model, "rows-inserted",
		G_CALLBACK (on_model_rows_inserted), self);
	self->model_rows_removed_id = g_signal_connect (model, "rows-removed",
		G_CALLBACK (on_model_rows_removed), self);
	self->model_row_count_changed_id = g_signal_connect (model, "row-count-changed",
		G_CALLBACK (on_model_row_count_changed), self);
	self->model_before_rebuild_id = g_signal_connect (model, "before-rebuild",
		G_CALLBACK (on_before_rebuild), self);
	self->model_after_rebuild_id = g_signal_connect (model, "after-rebuild",
		G_CALLBACK (on_after_rebuild), self);

	virtual_tree_invalidate_metrics (self);
	virtual_tree_update_scrollbar (self);
	virtual_tree_refill (self);
}

static void
e_virtual_tree_set_property (GObject *object,
			     guint property_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	EVirtualTree *self = E_VIRTUAL_TREE (object);

	switch (property_id) {
	case PROP_MODEL:
		e_virtual_tree_set_model (self, g_value_get_object (value));
		break;
	case PROP_GROUP_DEPTH:
		e_virtual_tree_set_group_depth (self, g_value_get_uint (value));
		break;
	case PROP_EDITABLE:
		e_virtual_tree_set_editable (self, g_value_get_boolean (value));
		break;
	case PROP_SELECTION_MODE:
		e_virtual_tree_set_selection_mode (self, g_value_get_enum (value));
		break;
	case PROP_EMPTY_MESSAGE:
		e_virtual_tree_set_empty_message (self, g_value_get_string (value));
		break;
	case PROP_HEADER_CLICK_SORT_POLICY:
		e_virtual_tree_set_header_click_sort_policy (self, g_value_get_enum (value));
		break;
	case PROP_HADJUSTMENT:
		virtual_tree_set_hadjustment (self, g_value_get_object (value));
		break;
	case PROP_VADJUSTMENT:
		virtual_tree_set_vadjustment (self, g_value_get_object (value));
		break;
	case PROP_HSCROLL_POLICY:
		self->hscroll_policy = g_value_get_enum (value);
		break;
	case PROP_VSCROLL_POLICY:
		self->vscroll_policy = g_value_get_enum (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
e_virtual_tree_get_property (GObject *object,
			     guint property_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	EVirtualTree *self = E_VIRTUAL_TREE (object);

	switch (property_id) {
	case PROP_MODEL:
		g_value_set_object (value, self->model);
		break;
	case PROP_GROUP_DEPTH:
		g_value_set_uint (value, self->group_depth);
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, self->editable);
		break;
	case PROP_SELECTION_MODE:
		g_value_set_enum (value, self->selection_mode);
		break;
	case PROP_EMPTY_MESSAGE:
		g_value_set_string (value, self->empty_message);
		break;
	case PROP_HEADER_CLICK_SORT_POLICY:
		g_value_set_enum (value, self->header_click_sort_policy);
		break;
	case PROP_HADJUSTMENT:
		g_value_set_object (value, self->hadjustment);
		break;
	case PROP_VADJUSTMENT:
		g_value_set_object (value, self->vadjustment);
		break;
	case PROP_HSCROLL_POLICY:
		g_value_set_enum (value, self->hscroll_policy);
		break;
	case PROP_VSCROLL_POLICY:
		g_value_set_enum (value, self->vscroll_policy);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static gboolean
on_query_tooltip (GtkWidget *widget,
		  gint x,
		  gint y,
		  gboolean keyboard_tip,
		  GtkTooltip *tooltip,
		  EVirtualTree *self)
{
	GtkTreeViewColumn *tree_column = NULL;
	GtkTreePath *path = NULL;
	ColumnInfo *col_info;
	GdkRectangle cell_rect;
	ECellAreaLines *cell_area;
	GtkTreeIter iter;
	guint col_idx;
	guint max_line, line, ii;
	gint indent, span_width, mouse_x, mouse_y;
	gboolean found = FALSE;

	if (keyboard_tip)
		return FALSE;

	gtk_tree_view_convert_widget_to_bin_window_coords (self->tree_view, x, y, &x, &y);

	if (!gtk_tree_view_get_path_at_pos (self->tree_view, x, y, &path, &tree_column, NULL, NULL)) {
		return FALSE;
	}

	if (!tree_column || !path) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (self->list_store), &iter, path)) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	col_idx = find_column_index (self, tree_column);
	col_info = g_ptr_array_index (self->columns, col_idx);

	gtk_tree_view_column_cell_set_cell_data (tree_column,
		GTK_TREE_MODEL (self->list_store),
		&iter, FALSE, FALSE);

	gtk_tree_view_get_cell_area (self->tree_view, path, tree_column, &cell_rect);

	if (self->row_tooltip_markup_func) {
		GObject *row_object = NULL;
		gchar *markup;

		gtk_tree_model_get (GTK_TREE_MODEL (self->list_store), &iter, COL_ROW_OBJECT, &row_object, -1);

		markup = row_object ? self->row_tooltip_markup_func (self, row_object, self->row_tooltip_markup_user_data) : NULL;

		g_clear_object (&row_object);

		if (markup) {
			gtk_tooltip_set_markup (tooltip, markup);
			gtk_tooltip_set_tip_area (tooltip, &cell_rect);
			g_free (markup);
			gtk_tree_path_free (path);

			return TRUE;
		}
	}

	cell_area = E_CELL_AREA_LINES (gtk_cell_layout_get_area (GTK_CELL_LAYOUT (col_info->tree_column)));
	max_line = cell_area_lines_get_max_line (cell_area);
	indent = cell_area_lines_get_indent (cell_area, GTK_WIDGET (self->tree_view), NULL);
	span_width = cell_area_lines_get_span_width (cell_area, GTK_WIDGET (self->tree_view));
	mouse_x = x - cell_rect.x;
	mouse_y = y - cell_rect.y;

	if (gtk_widget_get_direction (GTK_WIDGET (self->tree_view)) == GTK_TEXT_DIR_RTL)
		mouse_x = cell_rect.width - 1 - mouse_x;

	for (line = 0; line <= max_line && !found; line++) {
		gint x_offset = indent + span_width;
		gint y_offset = 0, line_height = 0;
		guint ll;

		for (ll = 0; ll < line; ll++) {
			gint lh = 0;

			for (ii = 0; ii < cell_area->renderers->len; ii++) {
				GtkCellRenderer *rr = g_ptr_array_index (cell_area->renderers, ii);
				gint min_h;

				if (renderer_get_span (rr))
					continue;
				if (renderer_get_line (rr) != ll)
					continue;
				if (!gtk_cell_renderer_get_visible (rr))
					continue;

				gtk_cell_renderer_get_preferred_height (rr,
					GTK_WIDGET (self->tree_view),
					&min_h, NULL);
				lh = MAX (lh, min_h);
			}

			y_offset += lh;
		}

		for (ii = 0; ii < cell_area->renderers->len; ii++) {
			GtkCellRenderer *rr = g_ptr_array_index (cell_area->renderers, ii);
			gint min_h;

			if (renderer_get_span (rr))
				continue;
			if (renderer_get_line (rr) != line)
				continue;
			if (!gtk_cell_renderer_get_visible (rr))
				continue;

			gtk_cell_renderer_get_preferred_height (rr, GTK_WIDGET (self->tree_view), &min_h, NULL);
			line_height = MAX (line_height, min_h);
		}

		if (mouse_y < y_offset || mouse_y >= y_offset + line_height)
			continue;

		for (ii = 0; ii < cell_area->renderers->len; ii++) {
			GtkCellRenderer *renderer = g_ptr_array_index (cell_area->renderers, ii);
			gint min_w, nat_w, w;

			if (renderer_get_span (renderer))
				continue;
			if (E_IS_CELL_RENDERER_EXPANDER (renderer))
				continue;
			if (renderer_get_line (renderer) != line)
				continue;
			if (!gtk_cell_renderer_get_visible (renderer))
				continue;

			gtk_cell_renderer_get_preferred_width (renderer,
				GTK_WIDGET (self->tree_view),
				&min_w, &nat_w);

			if (renderer_get_expand (renderer)) {
				w = cell_rect.width - x_offset;
				if (w < 0)
					w = 0;
			} else {
				w = nat_w;
				if (x_offset + w > cell_rect.width)
					w = MAX (cell_rect.width - x_offset, 0);
			}

			if (mouse_x >= x_offset && mouse_x < x_offset + w &&
			    GTK_IS_CELL_RENDERER_TEXT (renderer) && nat_w > w) {
				gchar *text = NULL;
				g_object_get (renderer, "text", &text, NULL);

				if (text && *text) {
					GdkRectangle tip_rect;
					gint wx, wy;

					tip_rect.x = cell_rect.x + x_offset;
					tip_rect.y = cell_rect.y + y_offset;
					tip_rect.width = w;
					tip_rect.height = line_height;
					gtk_tree_view_convert_bin_window_to_widget_coords (self->tree_view, tip_rect.x, tip_rect.y, &wx, &wy);
					tip_rect.x = wx;
					tip_rect.y = wy;
					gtk_tooltip_set_tip_area (tooltip, &tip_rect);
					gtk_tooltip_set_text (tooltip, text);
					found = TRUE;
				}

				g_free (text);
				if (found)
					break;
			}

			x_offset += w;
		}
	}

	gtk_tree_path_free (path);
	return found;
}

static void
e_virtual_tree_constructed (GObject *object)
{
	EVirtualTree *self = E_VIRTUAL_TREE (object);
	GtkStyleContext *context;
	GtkCssProvider *css_provider;
	GtkTreeSelection *selection;
	GtkAdjustment *tv_vadj;

	G_OBJECT_CLASS (e_virtual_tree_parent_class)->constructed (object);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);

	/* Internal list store */
	self->list_store = gtk_list_store_new (N_INTERNAL_COLUMNS,
		G_TYPE_OBJECT,
		G_TYPE_UINT,
		G_TYPE_BOOLEAN,
		G_TYPE_BOOLEAN,
		G_TYPE_BOOLEAN,
		G_TYPE_UINT);

	/* Tree view */
	self->tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (self->list_store)));
	gtk_tree_view_set_headers_visible (self->tree_view, TRUE);
	gtk_tree_view_set_enable_search (self->tree_view, FALSE);
	gtk_tree_view_set_search_column (self->tree_view, -1);
	gtk_tree_view_set_show_expanders (self->tree_view, FALSE);
	gtk_tree_selection_set_mode (
		gtk_tree_view_get_selection (self->tree_view),
		self->selection_mode);

	gtk_box_pack_start (GTK_BOX (self), GTK_WIDGET (self->tree_view), TRUE, TRUE, 0);

	context = gtk_widget_get_style_context (GTK_WIDGET (self->tree_view));
	gtk_style_context_add_class (context, "evtree-unfocused");
	gtk_style_context_add_class (context, "evtree");

	css_provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (css_provider,
		"treeview.view.evtree-unfocused.cell:selected {"
		"  background-color: @theme_unfocused_selected_bg_color;"
		"  color: @theme_unfocused_selected_fg_color;"
		"}", -1, NULL);
	gtk_style_context_add_provider (context,
		GTK_STYLE_PROVIDER (css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (css_provider);

	css_provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (css_provider,
		"treeview.evtree header button {"
		"  min-height: 1.8em;"
		"}", -1, NULL);
	gtk_style_context_add_provider_for_screen (
		gtk_widget_get_screen (GTK_WIDGET (self->tree_view)),
		GTK_STYLE_PROVIDER (css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (css_provider);

	/* Create a default vadjustment if one wasn't set via the property */
	if (!self->vadjustment)
		virtual_tree_set_vadjustment (self, gtk_adjustment_new (0, 0, 0, 1, 10, 0));

	gtk_widget_show_all (GTK_WIDGET (self));

	/* Signal connections */
	g_signal_connect (self->tree_view, "size-allocate",
		G_CALLBACK (on_tree_view_size_allocate), self);
	g_signal_connect (self->tree_view, "style-updated",
		G_CALLBACK (on_tree_view_style_updated), self);
	g_signal_connect_after (self->tree_view, "draw",
		G_CALLBACK (on_tree_view_draw), self);
	g_signal_connect (self->tree_view, "scroll-event",
		G_CALLBACK (on_tree_view_scroll_event), self);
	g_signal_connect (self->tree_view, "button-press-event",
		G_CALLBACK (on_button_press_event), self);
	g_signal_connect (self->tree_view, "button-release-event",
		G_CALLBACK (on_button_release_event), self);
	g_signal_connect (self->tree_view, "motion-notify-event",
		G_CALLBACK (on_motion_notify_event), self);
	g_signal_connect (self->tree_view, "key-press-event",
		G_CALLBACK (on_key_press_event), self);
	g_signal_connect (self->tree_view, "focus-in-event",
		G_CALLBACK (on_tree_view_focus_in), self);
	g_signal_connect (self->tree_view, "focus-out-event",
		G_CALLBACK (on_tree_view_focus_out), self);
	g_signal_connect (self->tree_view, "row-activated",
		G_CALLBACK (on_row_activated), self);
	self->cursor_changed_id = g_signal_connect (self->tree_view, "cursor-changed",
		G_CALLBACK (on_cursor_changed), self);
	tv_vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->tree_view));
	if (tv_vadj)
		g_signal_connect (tv_vadj, "value-changed", G_CALLBACK (on_tv_vadjustment_changed), self);

	/* Tooltip for ellipsized text */
	gtk_widget_set_has_tooltip (GTK_WIDGET (self->tree_view), TRUE);
	g_signal_connect (self->tree_view, "query-tooltip",
		G_CALLBACK (on_query_tooltip), self);

	/* DnD signals */
	g_signal_connect (self->tree_view, "drag-begin",
		G_CALLBACK (on_drag_begin), self);
	g_signal_connect (self->tree_view, "drag-end",
		G_CALLBACK (on_drag_end), self);
	g_signal_connect (self->tree_view, "drag-data-get",
		G_CALLBACK (on_drag_data_get), self);
	g_signal_connect (self->tree_view, "drag-data-received",
		G_CALLBACK (on_drag_data_received), self);
	g_signal_connect (self->tree_view, "drag-drop",
		G_CALLBACK (on_drag_drop), self);
	g_signal_connect (self->tree_view, "drag-motion",
		G_CALLBACK (on_drag_motion), self);
	g_signal_connect (self->tree_view, "columns-changed",
		G_CALLBACK (on_tree_view_columns_changed), self);

	selection = gtk_tree_view_get_selection (self->tree_view);
	self->selection_changed_id = g_signal_connect (selection, "changed",
		G_CALLBACK (on_selection_changed), self);
}

static void
e_virtual_tree_dispose (GObject *object)
{
	EVirtualTree *self = E_VIRTUAL_TREE (object);

	if (self->save_state_id) {
		g_source_remove (self->save_state_id);
		self->save_state_id = 0;
		if (self->tree_view)
			virtual_tree_save_column_state (self);
	}

	if (self->restore_selection_id) {
		g_source_remove (self->restore_selection_id);
		self->restore_selection_id = 0;
	}

	if (self->pending_restore_giveup_id) {
		g_source_remove (self->pending_restore_giveup_id);
		self->pending_restore_giveup_id = 0;
	}

	if (self->refill_idle_id) {
		g_source_remove (self->refill_idle_id);
		self->refill_idle_id = 0;
	}

	if (self->focus_restore_idle_id) {
		g_source_remove (self->focus_restore_idle_id);
		self->focus_restore_idle_id = 0;
	}

	if (self->search_timeout_id) {
		g_source_remove (self->search_timeout_id);
		self->search_timeout_id = 0;
	}

	if (self->tree_view) {
		g_signal_handlers_disconnect_by_data (self->tree_view, self);
		self->tree_view = NULL;
	}

	set_cursor_object (self, NULL);
	g_clear_object (&self->top_visible_object);

	if (self->vadjustment) {
		g_signal_handlers_disconnect_by_func (self->vadjustment, virtual_tree_scroll_changed, self);
		g_clear_object (&self->vadjustment);
	}
	g_clear_object (&self->hadjustment);

	virtual_tree_disconnect_model (self);
	g_clear_object (&self->list_store);

	if (self->search_destroy && self->search_user_data)
		self->search_destroy (self->search_user_data);
	self->search_func = NULL;
	self->search_user_data = NULL;
	self->search_destroy = NULL;

	if (self->row_selected_color_destroy && self->row_selected_color_user_data)
		self->row_selected_color_destroy (self->row_selected_color_user_data);
	self->row_selected_color_func = NULL;
	self->row_selected_color_user_data = NULL;
	self->row_selected_color_destroy = NULL;

	if (self->row_unselected_color_destroy && self->row_unselected_color_user_data)
		self->row_unselected_color_destroy (self->row_unselected_color_user_data);
	self->row_unselected_color_func = NULL;
	self->row_unselected_color_user_data = NULL;
	self->row_unselected_color_destroy = NULL;

	if (self->row_tooltip_markup_destroy && self->row_tooltip_markup_user_data)
		self->row_tooltip_markup_destroy (self->row_tooltip_markup_user_data);
	self->row_tooltip_markup_func = NULL;
	self->row_tooltip_markup_user_data = NULL;
	self->row_tooltip_markup_destroy = NULL;

	g_clear_object (&self->ui_manager);

	G_OBJECT_CLASS (e_virtual_tree_parent_class)->dispose (object);
}

static void
e_virtual_tree_finalize (GObject *object)
{
	EVirtualTree *self = E_VIRTUAL_TREE (object);

	g_clear_pointer (&self->selected_keys, g_hash_table_unref);
	g_ptr_array_unref (self->columns);

	if (self->search_buffer)
		g_string_free (self->search_buffer, TRUE);

	g_free (self->empty_message);
	g_free (self->column_state_filename);
	g_free (self->proportional_weights);
	g_free (self->proportional_manual);
	g_free (self->proportional_prev_width);
	g_free (self->proportional_pinned);
	g_free (self->proportional_pinned_width);
	g_clear_pointer (&self->alternating_row_color, gdk_rgba_free);
	g_clear_pointer (&self->drag.source_target_list, gtk_target_list_unref);

	G_OBJECT_CLASS (e_virtual_tree_parent_class)->finalize (object);
}

static void
virtual_tree_init_proportional_weights (EVirtualTree *self,
					guint n_columns)
{
	guint ii;

	g_clear_pointer (&self->proportional_weights, g_free);
	g_clear_pointer (&self->proportional_manual, g_free);
	g_clear_pointer (&self->proportional_prev_width, g_free);
	g_clear_pointer (&self->proportional_pinned, g_free);
	g_clear_pointer (&self->proportional_pinned_width, g_free);

	self->proportional_weights = g_new0 (gint, n_columns);
	self->proportional_manual = g_new0 (gboolean, n_columns);
	self->proportional_prev_width = g_new0 (gint, n_columns);
	self->proportional_pinned = g_new0 (gboolean, n_columns);
	self->proportional_pinned_width = g_new0 (gint, n_columns);
	self->proportional_n_columns = n_columns;
	self->proportional_last_width = -1;

	for (ii = 0; ii < n_columns && ii < self->columns->len; ii++) {
		ColumnInfo *ci = g_ptr_array_index (self->columns, ii);
		gint width;

		if (!gtk_tree_view_column_get_resizable (ci->tree_column))
			continue;

		width = gtk_tree_view_column_get_width (ci->tree_column);
		if (width <= 0)
			width = gtk_tree_view_column_get_fixed_width (ci->tree_column);
		if (width <= 0)
			width = 100;

		self->proportional_weights[ii] = width;
		self->proportional_prev_width[ii] = width;
	}
}

static gint
virtual_tree_get_columns_width (EVirtualTree *self)
{
	GList *columns, *link;
	gint total = 0;

	columns = gtk_tree_view_get_columns (self->tree_view);
	for (link = columns; link; link = link->next) {
		GtkTreeViewColumn *col = link->data;
		gint width;

		if (!gtk_tree_view_column_get_visible (col))
			continue;

		if (gtk_tree_view_column_get_sizing (col) == GTK_TREE_VIEW_COLUMN_FIXED)
			width = gtk_tree_view_column_get_fixed_width (col);
		else
			width = gtk_tree_view_column_get_width (col);

		total += width;
	}
	g_list_free (columns);

	return total;
}

static void
virtual_tree_apply_proportional_widths (EVirtualTree *self,
					gint available_width)
{
	gint fixed_total = 0, weight_total = 0;
	gint remaining;
	gint budget;
	gint weight_left;
	guint ii;
	gboolean *pinned = self->proportional_pinned;
	gint *pinned_width = self->proportional_pinned_width;
	gboolean changed;

	for (ii = 0; ii < self->proportional_n_columns && ii < self->columns->len; ii++) {
		ColumnInfo *ci = g_ptr_array_index (self->columns, ii);

		if (!gtk_tree_view_column_get_visible (ci->tree_column))
			continue;

		if (!gtk_tree_view_column_get_resizable (ci->tree_column) || self->proportional_manual[ii]) {
			gint col_width = gtk_tree_view_column_get_width (ci->tree_column);
			if (col_width <= 0)
				col_width = gtk_tree_view_column_get_fixed_width (ci->tree_column);

			if (col_width <= 0)
				col_width = self->proportional_weights[ii];

			if (col_width <= 0)
				gtk_tree_view_column_cell_get_size (ci->tree_column, NULL, NULL, NULL, &col_width, NULL);

			if (col_width > 0) {
				fixed_total += col_width;
				self->proportional_prev_width[ii] = col_width;
			}
		} else if (self->proportional_weights[ii] > 0) {
			weight_total += self->proportional_weights[ii];
		}
	}

	remaining = available_width - fixed_total;
	if (remaining < 0)
		remaining = 0;

	self->applying_proportional_widths = TRUE;

	memset (pinned, 0, sizeof (gboolean) * self->proportional_n_columns);
	memset (pinned_width, 0, sizeof (gint) * self->proportional_n_columns);

	budget = remaining;
	weight_left = weight_total;

	/* water-fill pass: pin columns below their min-width, so the
	 * shortfall is shared by the remaining flexible columns */
	do {
		changed = FALSE;

		for (ii = 0; ii < self->proportional_n_columns && ii < self->columns->len; ii++) {
			ColumnInfo *ci = g_ptr_array_index (self->columns, ii);
			gint min_width;
			gint share;

			if (pinned[ii])
				continue;

			if (!gtk_tree_view_column_get_visible (ci->tree_column) || self->proportional_weights[ii] <= 0)
				continue;

			if (!gtk_tree_view_column_get_resizable (ci->tree_column) || self->proportional_manual[ii])
				continue;

			if (weight_left <= 0)
				continue;

			min_width = gtk_tree_view_column_get_min_width (ci->tree_column);
			if (min_width < MIN_PROPORTIONAL_COLUMN_WIDTH)
				min_width = MIN_PROPORTIONAL_COLUMN_WIDTH;

			share = budget * self->proportional_weights[ii] / weight_left;

			if (share < min_width) {
				pinned[ii] = TRUE;
				pinned_width[ii] = min_width;
				budget -= pinned_width[ii];
				weight_left -= self->proportional_weights[ii];
				changed = TRUE;
			}
		}
	} while (changed);

	for (ii = 0; ii < self->proportional_n_columns && ii < self->columns->len; ii++) {
		ColumnInfo *ci = g_ptr_array_index (self->columns, ii);
		gint width;

		if (!gtk_tree_view_column_get_visible (ci->tree_column) || self->proportional_weights[ii] <= 0)
			continue;

		if (!gtk_tree_view_column_get_resizable (ci->tree_column) || self->proportional_manual[ii])
			continue;

		if (pinned[ii]) {
			width = pinned_width[ii];
		} else if (weight_left > 0) {
			width = budget * self->proportional_weights[ii] / weight_left;

			if (width > budget)
				width = budget;
			if (width < MIN_PROPORTIONAL_COLUMN_WIDTH)
				width = MIN_PROPORTIONAL_COLUMN_WIDTH;

			budget -= width;
			weight_left -= self->proportional_weights[ii];
		} else {
			continue;
		}

		self->proportional_prev_width[ii] = width;

		if (gtk_tree_view_column_get_sizing (ci->tree_column) == GTK_TREE_VIEW_COLUMN_FIXED &&
		    gtk_tree_view_column_get_fixed_width (ci->tree_column) == width)
			continue;

		gtk_tree_view_column_set_sizing (ci->tree_column, GTK_TREE_VIEW_COLUMN_FIXED);
		gtk_tree_view_column_set_fixed_width (ci->tree_column, width);
	}

	self->applying_proportional_widths = FALSE;
	self->proportional_last_width = available_width;
}

static void
e_virtual_tree_size_allocate (GtkWidget *widget,
			      GtkAllocation *allocation)
{
	EVirtualTree *self = E_VIRTUAL_TREE (widget);
	gdouble old_value = 0;

	if (self->hadjustment)
		old_value = gtk_adjustment_get_value (self->hadjustment);

	if (allocation->width > 1) {
		if (!self->proportional_weights)
			virtual_tree_init_proportional_weights (self, self->columns->len);

		if (self->proportional_last_width != allocation->width)
			virtual_tree_apply_proportional_widths (self, allocation->width);
	}

	GTK_WIDGET_CLASS (e_virtual_tree_parent_class)->size_allocate (widget, allocation);

	if (self->tree_view && self->hadjustment) {
		gint columns_width = virtual_tree_get_columns_width (self);
		gint upper = MAX (columns_width, allocation->width);

		gtk_adjustment_configure (self->hadjustment,
			MIN (old_value, MAX (upper - allocation->width, 0)),
			0, (gdouble) upper,
			10, allocation->width * 0.9,
			(gdouble) allocation->width);
	}
}

static void
e_virtual_tree_class_init (EVirtualTreeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->set_property = e_virtual_tree_set_property;
	object_class->get_property = e_virtual_tree_get_property;
	object_class->constructed = e_virtual_tree_constructed;
	object_class->dispose = e_virtual_tree_dispose;
	object_class->finalize = e_virtual_tree_finalize;

	widget_class->size_allocate = e_virtual_tree_size_allocate;

	gtk_widget_class_set_css_name (widget_class, "evirtualtree");
	gtk_widget_class_set_accessible_type (widget_class, E_TYPE_VIRTUAL_TREE_ACCESSIBLE);

	properties[PROP_MODEL] = g_param_spec_object (
		"model", NULL, NULL,
		E_TYPE_VIRTUAL_TREE_MODEL,
		G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_GROUP_DEPTH] = g_param_spec_uint (
		"group-depth", NULL, NULL,
		0, G_MAXUINT, 0,
		G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_EDITABLE] = g_param_spec_boolean (
		"editable", NULL, NULL,
		FALSE,
		G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_SELECTION_MODE] = g_param_spec_enum (
		"selection-mode", NULL, NULL,
		GTK_TYPE_SELECTION_MODE,
		GTK_SELECTION_SINGLE,
		G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_EMPTY_MESSAGE] = g_param_spec_string (
		"empty-message", NULL, NULL,
		NULL,
		G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_HEADER_CLICK_SORT_POLICY] = g_param_spec_enum (
		"header-click-sort-policy", NULL, NULL,
		E_TYPE_AUTOMATIC_ACTION_POLICY,
		E_AUTOMATIC_ACTION_POLICY_ASK,
		G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	g_object_class_override_property (object_class, PROP_HADJUSTMENT, "hadjustment");
	g_object_class_override_property (object_class, PROP_VADJUSTMENT, "vadjustment");
	g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
	g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

	gtk_widget_class_install_style_property (
		widget_class,
		g_param_spec_boolean (
			"alternating-row-colors",
			"Alternating Row Colors",
			"Whether to use alternating row colors",
			TRUE,
			G_PARAM_READABLE));

	/**
	 * EVirtualTree:alternating-row-color:
	 *
	 * Background color for alternating (even) rows; if unset, a
	 * theme-derived shade is used instead.
	 *
	 * Since: 3.64
	 **/
	gtk_widget_class_install_style_property (
		widget_class,
		g_param_spec_boxed (
			"alternating-row-color",
			NULL, NULL,
			GDK_TYPE_RGBA,
			G_PARAM_READABLE));

	/**
	 * EVirtualTree:prelight-row-colors:
	 *
	 * Whether to tint a row's background when the mouse hovers over it.
	 *
	 * Since: 3.64
	 **/
	gtk_widget_class_install_style_property (
		widget_class,
		g_param_spec_boolean (
			"prelight-row-colors",
			NULL, NULL,
			TRUE,
			G_PARAM_READABLE));

	widget_signals[ROW_ACTIVATED] = g_signal_new (
		"row-activated",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		G_TYPE_UINT,
		G_TYPE_OBJECT);

	widget_signals[SELECTION_CHANGED] = g_signal_new (
		"selection-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	widget_signals[CURSOR_CHANGED] = g_signal_new (
		"cursor-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		G_TYPE_UINT,
		G_TYPE_OBJECT);

	widget_signals[ROW_EXPANDED] = g_signal_new (
		"row-expanded",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 3,
		G_TYPE_UINT,
		G_TYPE_OBJECT,
		G_TYPE_BOOLEAN);

	widget_signals[CELL_CLICKED] = g_signal_new (
		"cell-clicked",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_BOOLEAN, 4,
		G_TYPE_UINT,
		G_TYPE_OBJECT,
		G_TYPE_UINT,
		GTK_TYPE_CELL_RENDERER);

	widget_signals[RIGHT_CLICK] = g_signal_new (
		"right-click",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_UINT,
		G_TYPE_OBJECT,
		GDK_TYPE_EVENT);

	widget_signals[TREE_DRAG_BEGIN] = g_signal_new (
		"tree-drag-begin",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		G_TYPE_UINT,
		GDK_TYPE_DRAG_CONTEXT);

	widget_signals[TREE_DRAG_END] = g_signal_new (
		"tree-drag-end",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		GDK_TYPE_DRAG_CONTEXT);

	widget_signals[TREE_DRAG_DATA_GET] = g_signal_new (
		"tree-drag-data-get",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 4,
		GDK_TYPE_DRAG_CONTEXT,
		GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		G_TYPE_UINT,
		G_TYPE_UINT);

	widget_signals[TREE_DRAG_DATA_RECEIVED] = g_signal_new (
		"tree-drag-data-received",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 5,
		GDK_TYPE_DRAG_CONTEXT,
		G_TYPE_UINT,
		GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		G_TYPE_UINT,
		G_TYPE_UINT);

	widget_signals[TREE_DRAG_DROP] = g_signal_new (
		"tree-drag-drop",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_BOOLEAN, 3,
		GDK_TYPE_DRAG_CONTEXT,
		G_TYPE_UINT,
		G_TYPE_UINT);

	widget_signals[TREE_DRAG_MOTION] = g_signal_new (
		"tree-drag-motion",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_BOOLEAN, 3,
		GDK_TYPE_DRAG_CONTEXT,
		G_TYPE_UINT,
		G_TYPE_UINT);

	widget_signals[CELL_EDITED] = g_signal_new (
		"cell-edited",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 5,
		G_TYPE_UINT,
		G_TYPE_OBJECT,
		G_TYPE_UINT,
		GTK_TYPE_CELL_RENDERER,
		G_TYPE_STRING);

	widget_signals[HEADER_CLICKED] = g_signal_new (
		"header-clicked",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 3,
		G_TYPE_UINT,
		G_TYPE_INT,
		G_TYPE_BOOLEAN);

	widget_signals[COLUMN_STATE_CHANGED] = g_signal_new (
		"column-state-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	widget_signals[GET_LEGACY_ETABLE_COLUMN_MAP] = g_signal_new (
		"get-legacy-etable-column-map",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0, NULL, NULL, NULL,
		G_TYPE_POINTER, 1,
		G_TYPE_POINTER);
}

static void
e_virtual_tree_init (EVirtualTree *self)
{
	self->proportional_last_width = -1;
	self->columns = g_ptr_array_new_with_free_func (column_info_free);
	self->key_type.hash_func = g_direct_hash;
	self->key_type.equal_func = g_direct_equal;
	self->key_type.copy_func = NULL;
	self->key_type.free_func = NULL;
	self->selected_keys = g_hash_table_new (g_direct_hash, g_direct_equal);
	self->cursor_row = -1;
	self->cursor_scroll_pending_row = -1;
	self->anchor_row = -1;
	self->shift_end_row = -1;
	self->drag.defer_row = -1;
	self->expander_column_idx = -1;
	self->first_visible_row = 0;
	self->visible_count = 0;
	self->metrics_valid = FALSE;
	self->selection_mode = GTK_SELECTION_SINGLE;
	self->header_click_sort_policy = E_AUTOMATIC_ACTION_POLICY_ASK;
	self->alternating_row_colors = TRUE;
	self->prelight_row_colors = TRUE;
}

GtkWidget *
e_virtual_tree_new (EVirtualTreeModel *model)
{
	return g_object_new (E_TYPE_VIRTUAL_TREE,
		"model", model,
		NULL);
}

void
e_virtual_tree_set_model (EVirtualTree *self,
			  EVirtualTreeModel *model)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	if (model)
		g_return_if_fail (E_IS_VIRTUAL_TREE_MODEL (model));

	if (self->model == model)
		return;

	virtual_tree_connect_model (self, model);
	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

EVirtualTreeModel *
e_virtual_tree_get_model (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);

	return self->model;
}

static gboolean
virtual_tree_confirm_header_click_sort (EVirtualTree *self)
{
	GtkWidget *toplevel;
	gint response;
	gboolean can_sort;

	if (self->header_click_sort_policy == E_AUTOMATIC_ACTION_POLICY_ALWAYS)
		return TRUE;

	if (self->header_click_sort_policy == E_AUTOMATIC_ACTION_POLICY_NEVER)
		return FALSE;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	if (!GTK_IS_WINDOW (toplevel))
		toplevel = NULL;

	response = e_alert_run_dialog_for_args (GTK_WINDOW (toplevel), "widgets:header-click-can-sort", NULL);

	switch (response) {
	case GTK_RESPONSE_YES:
		return TRUE;
	case GTK_RESPONSE_NO:
		return FALSE;
	case GTK_RESPONSE_ACCEPT:
	case GTK_RESPONSE_CANCEL:
		can_sort = response == GTK_RESPONSE_ACCEPT;
		e_virtual_tree_set_header_click_sort_policy (self, can_sort ? E_AUTOMATIC_ACTION_POLICY_ALWAYS : E_AUTOMATIC_ACTION_POLICY_NEVER);
		return can_sort;
	default:
		return FALSE;
	}
}

static void
on_column_clicked (GtkTreeViewColumn *tree_column,
		   EVirtualTree *self)
{
	guint col_idx = find_column_index (self, tree_column);
	GtkSortType current_order;
	gint current_prio;
	gboolean was_sorted;
	gboolean has_ctrl = FALSE;
	gboolean has_shift = FALSE;
	GdkEvent *event;

	if (!virtual_tree_confirm_header_click_sort (self))
		return;

	event = gtk_get_current_event ();
	if (event) {
		GdkModifierType state;

		if (gdk_event_get_state (event, &state)) {
			has_ctrl = (state & GDK_CONTROL_MASK) != 0;
			has_shift = (state & GDK_SHIFT_MASK) != 0;
		}
		gdk_event_free (event);
	}

	was_sorted = e_virtual_tree_get_column_sort (self, col_idx, &current_order, &current_prio);

	if (has_ctrl) {
		if (was_sorted) {
			if (current_order == GTK_SORT_ASCENDING) {
				e_virtual_tree_set_column_sort (self, col_idx, GTK_SORT_DESCENDING, current_prio);
			} else {
				e_virtual_tree_set_column_sort (self, col_idx, GTK_SORT_ASCENDING, 0);
			}
		} else {
			guint ii;
			gint max_prio = 0;

			for (ii = 0; ii < self->columns->len; ii++) {
				ColumnInfo *info = g_ptr_array_index (self->columns, ii);

				if (info->sort_priority > max_prio)
					max_prio = info->sort_priority;
			}

			if (has_shift) {
				for (ii = 0; ii < self->columns->len; ii++) {
					ColumnInfo *info = g_ptr_array_index (self->columns, ii);

					if (info->sort_priority > 0)
						info->sort_priority++;
				}
				e_virtual_tree_set_column_sort (self, col_idx, GTK_SORT_ASCENDING, 1);
			} else {
				e_virtual_tree_set_column_sort (self, col_idx, GTK_SORT_ASCENDING, max_prio + 1);
			}
		}
	} else {
		e_virtual_tree_clear_sort (self);

		if (was_sorted) {
			if (current_order == GTK_SORT_ASCENDING)
				e_virtual_tree_set_column_sort (self, col_idx, GTK_SORT_DESCENDING, 1);
		} else {
			e_virtual_tree_set_column_sort (self, col_idx, GTK_SORT_ASCENDING, 1);
		}
	}

	g_signal_emit (self, widget_signals[COLUMN_STATE_CHANGED], 0);
}

static void
on_header_label_size_allocate (GtkWidget *label,
			       GdkRectangle *allocation,
			       gpointer user_data)
{
	GtkWidget *button = user_data;
	PangoLayout *layout;

	layout = gtk_label_get_layout (GTK_LABEL (label));

	if (pango_layout_is_ellipsized (layout))
		gtk_widget_set_tooltip_text (button, gtk_label_get_text (GTK_LABEL (label)));
	else
		gtk_widget_set_tooltip_text (button, NULL);
}

static void show_header_context_menu (EVirtualTree *self, guint col_idx, GdkEventButton *event);

static void
on_header_button_secondary_press (GtkGestureMultiPress *gesture,
				  gint n_press,
				  gdouble event_x,
				  gdouble event_y,
				  gpointer user_data)
{
	EVirtualTree *self = user_data;
	GtkWidget *button;
	GtkTreeViewColumn *tree_column;
	const GdkEvent *event;

	gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

	button = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
	tree_column = g_object_get_data (G_OBJECT (button), "e-virtual-tree-column");
	if (!tree_column)
		return;

	event = gtk_gesture_get_last_event (GTK_GESTURE (gesture),
		gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture)));

	show_header_context_menu (self, find_column_index (self, tree_column), (GdkEventButton *) event);
}

static GtkWidget *
find_label_in_container (GtkWidget *widget)
{
	if (GTK_IS_LABEL (widget))
		return widget;

	if (GTK_IS_CONTAINER (widget)) {
		GList *children, *link;
		GtkWidget *found = NULL;

		children = gtk_container_get_children (GTK_CONTAINER (widget));

		for (link = children; link && !found; link = link->next) {
			found = find_label_in_container (link->data);
		}

		g_list_free (children);

		return found;
	}

	return NULL;
}

GtkTreeViewColumn *
e_virtual_tree_get_column (EVirtualTree *self,
			   guint column_index)
{
	ColumnInfo *info;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);
	g_return_val_if_fail (column_index < self->columns->len, NULL);

	info = g_ptr_array_index (self->columns, column_index);

	return info->tree_column;
}

guint
e_virtual_tree_add_column (EVirtualTree *self,
			   const gchar *column_id,
			   const gchar *title)
{
	ColumnInfo *info;
	GtkTreeViewColumn *tree_column;
	GtkWidget *button;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), 0);
	g_return_val_if_fail (column_id != NULL, 0);

	tree_column = gtk_tree_view_column_new_with_area (g_object_new (E_TYPE_CELL_AREA_LINES, NULL));
	gtk_tree_view_column_set_title (tree_column, title ? title : "");
	gtk_tree_view_column_set_resizable (tree_column, TRUE);
	gtk_tree_view_column_set_clickable (tree_column, self->header_click_sort_policy != E_AUTOMATIC_ACTION_POLICY_NEVER);
	gtk_tree_view_column_set_reorderable (tree_column, TRUE);
	gtk_tree_view_append_column (self->tree_view, tree_column);

	g_object_set_data (G_OBJECT (tree_column), "e-virtual-tree", self);

	button = gtk_tree_view_column_get_button (tree_column);
	if (button) {
		GtkWidget *label;
		GtkGesture *secondary_press;

		g_object_set_data (G_OBJECT (button), "e-virtual-tree-column", tree_column);

		secondary_press = gtk_gesture_multi_press_new (button);
		gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (secondary_press), GTK_PHASE_CAPTURE);
		gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (secondary_press), GDK_BUTTON_SECONDARY);
		g_signal_connect (secondary_press, "pressed",
			G_CALLBACK (on_header_button_secondary_press), self);
		g_object_set_data_full (G_OBJECT (button), "e-virtual-tree-secondary-press", secondary_press, g_object_unref);

		label = find_label_in_container (button);
		if (label) {
			gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
			g_signal_connect (label, "size-allocate",
				G_CALLBACK (on_header_label_size_allocate),
				button);
		}
	}

	g_signal_connect (tree_column, "clicked",
		G_CALLBACK (on_column_clicked), self);
	g_signal_connect (tree_column, "notify::visible",
		G_CALLBACK (on_column_state_notify), self);
	g_signal_connect (tree_column, "notify::width",
		G_CALLBACK (on_column_state_notify), self);
	g_signal_connect (tree_column, "notify::fixed-width",
		G_CALLBACK (on_column_state_notify), self);

	info = g_new0 (ColumnInfo, 1);
	info->tree_column = tree_column;
	info->column_id = g_strdup (column_id);
	info->title = g_strdup (title ? title : "");
	info->renderers = g_ptr_array_new_with_free_func (renderer_info_free);
	info->group_priority = -1;

	g_ptr_array_add (self->columns, info);

	return self->columns->len - 1;
}

static void
virtual_tree_pack_renderer (EVirtualTree *self,
			    guint column_index,
			    guint line,
			    GtkCellRenderer *renderer,
			    gboolean expand,
			    gboolean pack_end,
			    EVirtualTreeCellDataFunc func,
			    gpointer user_data,
			    GDestroyNotify destroy)
{
	ColumnInfo *col_info;
	RendererInfo *rend_info;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));
	g_return_if_fail (column_index < self->columns->len);
	g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

	col_info = g_ptr_array_index (self->columns, column_index);

	rend_info = g_new0 (RendererInfo, 1);
	rend_info->renderer = renderer;
	rend_info->line = line;
	rend_info->expand = expand;
	rend_info->pack_end = pack_end;
	rend_info->func = func;
	rend_info->user_data = user_data;
	rend_info->destroy = destroy;

	g_ptr_array_add (col_info->renderers, rend_info);

	g_object_set_data (G_OBJECT (renderer), LINE_DATA_KEY, GUINT_TO_POINTER (line));
	g_object_set_data (G_OBJECT (renderer), EXPAND_DATA_KEY, GUINT_TO_POINTER (expand));
	g_object_set_data (G_OBJECT (renderer), SPAN_DATA_KEY, GUINT_TO_POINTER (line == G_MAXUINT));

	if (pack_end)
		gtk_tree_view_column_pack_end (col_info->tree_column, renderer, expand);
	else
		gtk_tree_view_column_pack_start (col_info->tree_column, renderer, expand);

	gtk_tree_view_column_set_cell_data_func (
		col_info->tree_column, renderer,
		cell_data_func_bridge, rend_info, NULL);

	if (GTK_IS_CELL_RENDERER_TEXT (renderer)) {
		g_object_set (renderer,
			"ellipsize", PANGO_ELLIPSIZE_END,
			"single-paragraph-mode", TRUE,
			NULL);
		g_signal_connect (renderer, "edited",
			G_CALLBACK (on_renderer_edited), self);
		g_signal_connect (renderer, "editing-started",
			G_CALLBACK (on_renderer_editing_started), self);
		g_signal_connect (renderer, "editing-canceled",
			G_CALLBACK (on_renderer_editing_canceled), self);
	}
}

void
e_virtual_tree_column_pack_start (EVirtualTree *self,
				  guint column_index,
				  guint line,
				  GtkCellRenderer *renderer,
				  gboolean expand,
				  EVirtualTreeCellDataFunc func,
				  gpointer user_data,
				  GDestroyNotify destroy)
{
	virtual_tree_pack_renderer (self, column_index, line,
		renderer, expand, FALSE, func, user_data, destroy);
}

void
e_virtual_tree_column_pack_end (EVirtualTree *self,
				guint column_index,
				guint line,
				GtkCellRenderer *renderer,
				gboolean expand,
				EVirtualTreeCellDataFunc func,
				gpointer user_data,
				GDestroyNotify destroy)
{
	virtual_tree_pack_renderer (self, column_index, line,
		renderer, expand, TRUE, func, user_data, destroy);
}

void
e_virtual_tree_column_pack_span (EVirtualTree *self,
				 guint column_index,
				 GtkCellRenderer *renderer,
				 EVirtualTreeCellDataFunc func,
				 gpointer user_data,
				 GDestroyNotify destroy)
{
	virtual_tree_pack_renderer (self, column_index, G_MAXUINT,
		renderer, FALSE, FALSE, func, user_data, destroy);
}

void
e_virtual_tree_column_set_accessible_name (EVirtualTree *self,
					    guint column_index,
					    const gchar *name)
{
	GtkTreeViewColumn *tvc;
	GtkWidget *button;
	AtkObject *atk_obj;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));
	g_return_if_fail (column_index < self->columns->len);

	tvc = e_virtual_tree_get_column (self, column_index);
	button = gtk_tree_view_column_get_button (tvc);
	if (button) {
		atk_obj = gtk_widget_get_accessible (button);
		if (atk_obj)
			atk_object_set_name (atk_obj, name);
	}
}

void
e_virtual_tree_column_clear_renderers (EVirtualTree *self,
				       guint column_index)
{
	ColumnInfo *col_info;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));
	g_return_if_fail (column_index < self->columns->len);

	col_info = g_ptr_array_index (self->columns, column_index);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (col_info->tree_column));
	g_ptr_array_set_size (col_info->renderers, 0);

	if (self->expander_column_idx == (gint) column_index) {
		self->expander_renderer = NULL;
		self->expander_column_idx = -1;
	}

	virtual_tree_invalidate_metrics (self);
}

static void
expander_cell_data_func (GtkTreeViewColumn *column,
			 GtkCellRenderer *renderer,
			 GtkTreeModel *model,
			 GtkTreeIter *iter,
			 gpointer data)
{
	EVirtualTree *self = data;
	guint visible_row = 0;
	gboolean is_group = FALSE;

	gtk_tree_model_get (model, iter,
		COL_IS_GROUP, &is_group,
		COL_VISIBLE_ROW_IDX, &visible_row,
		-1);

	g_object_set (renderer, "visible",
		!self->expander_hidden &&
		!(is_group && self->group_depth > 0), NULL);
}

void
e_virtual_tree_set_column_expand (EVirtualTree *self,
				  guint column_index,
				  gboolean expand)
{
	ColumnInfo *info;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));
	g_return_if_fail (column_index < self->columns->len);

	info = g_ptr_array_index (self->columns, column_index);
	gtk_tree_view_column_set_expand (info->tree_column, expand);
}

void
e_virtual_tree_set_column_min_width (EVirtualTree *self,
				     guint column_index,
				     gint min_width)
{
	ColumnInfo *info;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));
	g_return_if_fail (column_index < self->columns->len);

	info = g_ptr_array_index (self->columns, column_index);
	gtk_tree_view_column_set_min_width (info->tree_column, min_width);
}

void
e_virtual_tree_set_expander_column (EVirtualTree *self,
				    guint column_index,
				    guint line)
{
	ColumnInfo *col;
	GtkCellArea *area;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));
	g_return_if_fail (column_index < self->columns->len);

	self->expander_column_idx = (gint) column_index;
	self->expander_line = line;

	col = g_ptr_array_index (self->columns, column_index);

	self->expander_renderer = g_object_new (E_TYPE_CELL_RENDERER_EXPANDER, NULL);

	g_object_set_data (G_OBJECT (self->expander_renderer), LINE_DATA_KEY, GUINT_TO_POINTER (line));

	area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (col->tree_column));
	if (E_IS_CELL_AREA_LINES (area)) {
		ECellAreaLines *lines = E_CELL_AREA_LINES (area);

		g_ptr_array_insert (lines->renderers, 0, g_object_ref_sink (self->expander_renderer));
	} else {
		gtk_tree_view_column_pack_start (col->tree_column, self->expander_renderer, FALSE);
	}

	gtk_tree_view_column_add_attribute (col->tree_column, self->expander_renderer, "depth", COL_DEPTH);
	gtk_tree_view_column_add_attribute (col->tree_column, self->expander_renderer, "expandable", COL_EXPANDABLE);
	gtk_tree_view_column_add_attribute (col->tree_column, self->expander_renderer, "expanded", COL_EXPANDED);

	gtk_tree_view_column_set_cell_data_func (col->tree_column, self->expander_renderer, expander_cell_data_func, self, NULL);
}

void
e_virtual_tree_set_expander_visible (EVirtualTree *self,
				     gboolean visible)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	self->expander_hidden = !visible;

	if (self->expander_renderer) {
		gtk_cell_renderer_set_visible (self->expander_renderer, visible);

		if (self->expander_column_idx >= 0 && (guint) self->expander_column_idx < self->columns->len) {
			ColumnInfo *col = g_ptr_array_index (self->columns, self->expander_column_idx);
			gtk_tree_view_column_queue_resize (col->tree_column);
		}
	}
}

void
e_virtual_tree_set_group_depth (EVirtualTree *self,
				guint depth)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	if (self->group_depth == depth)
		return;

	self->group_depth = depth;
	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GROUP_DEPTH]);
	self->first_visible_row = 0;
	self->last_alloc_height = -1;
	virtual_tree_update_scrollbar (self);
	virtual_tree_refill (self);
}

guint
e_virtual_tree_get_group_depth (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), 0);

	return self->group_depth;
}

void
e_virtual_tree_set_editable (EVirtualTree *self,
			     gboolean editable)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	editable = !!editable;

	if (self->editable == editable)
		return;

	self->editable = editable;
	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EDITABLE]);
}

gboolean
e_virtual_tree_get_editable (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), FALSE);

	return self->editable;
}

void
e_virtual_tree_set_header_click_sort_policy (EVirtualTree *self,
					      EAutomaticActionPolicy policy)
{
	guint ii;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	if (self->header_click_sort_policy == policy)
		return;

	self->header_click_sort_policy = policy;

	for (ii = 0; ii < self->columns->len; ii++) {
		ColumnInfo *info = g_ptr_array_index (self->columns, ii);

		gtk_tree_view_column_set_clickable (info->tree_column, policy != E_AUTOMATIC_ACTION_POLICY_NEVER);
	}

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HEADER_CLICK_SORT_POLICY]);
}

EAutomaticActionPolicy
e_virtual_tree_get_header_click_sort_policy (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), E_AUTOMATIC_ACTION_POLICY_ASK);

	return self->header_click_sort_policy;
}

void
e_virtual_tree_set_selection_mode (EVirtualTree *self,
				   GtkSelectionMode mode)
{
	GtkTreeSelection *selection;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	if (self->selection_mode == mode)
		return;

	self->selection_mode = mode;

	if (self->tree_view) {
		selection = gtk_tree_view_get_selection (self->tree_view);
		gtk_tree_selection_set_mode (selection, mode);
		virtual_tree_schedule_refill (self);
	}

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTION_MODE]);
}

GtkSelectionMode
e_virtual_tree_get_selection_mode (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), GTK_SELECTION_SINGLE);

	return self->selection_mode;
}

GPtrArray *
e_virtual_tree_get_selected_rows (EVirtualTree *self)
{
	GPtrArray *array;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);

	array = g_ptr_array_new_with_free_func (g_object_unref);

	if (!self->model)
		return array;

	if (self->inverted_selection) {
		guint total = e_virtual_tree_model_get_row_count (self->model);
		guint ii;

		for (ii = 0; ii < total; ii++) {
			GObject *row_obj = e_virtual_tree_model_dup_row (self->model, ii);

			if (row_obj) {
				gconstpointer key = e_virtual_tree_model_get_row_key (self->model, row_obj);
				if (key && !g_hash_table_contains (self->selected_keys, key))
					g_ptr_array_add (array, row_obj);
				else
					g_clear_object (&row_obj);
			}
		}
	} else {
		GHashTableIter ht_iter;
		gpointer ht_key;

		g_hash_table_iter_init (&ht_iter, self->selected_keys);
		while (g_hash_table_iter_next (&ht_iter, &ht_key, NULL)) {
			guint idx = e_virtual_tree_model_find_row_by_key (self->model, ht_key);

			if (idx != G_MAXUINT) {
				GObject *row_obj = e_virtual_tree_model_dup_row (self->model, idx);
				if (row_obj)
					g_ptr_array_add (array, row_obj);
			}
		}
	}

	return array;
}

void
e_virtual_tree_select_row (EVirtualTree *self,
			   guint row)
{
	GObject *row_obj;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	row_obj = self->model ? e_virtual_tree_model_dup_row (self->model, row) : NULL;
	if (row_obj) {
		gconstpointer key = e_virtual_tree_model_get_row_key (self->model, row_obj);
		if (key)
			virtual_tree_select_key (self, key);
		g_clear_object (&row_obj);
	}
	virtual_tree_refill (self);
	g_signal_emit (self, widget_signals[SELECTION_CHANGED], 0);
}

void
e_virtual_tree_unselect_row (EVirtualTree *self,
			     guint row)
{
	GObject *row_obj;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	row_obj = self->model ? e_virtual_tree_model_dup_row (self->model, row) : NULL;
	if (row_obj) {
		gconstpointer key = e_virtual_tree_model_get_row_key (self->model, row_obj);
		if (key)
			virtual_tree_deselect_key (self, key);
		g_clear_object (&row_obj);
	}
	virtual_tree_refill (self);
	g_signal_emit (self, widget_signals[SELECTION_CHANGED], 0);
}

void
e_virtual_tree_select_all (EVirtualTree *self)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	g_hash_table_remove_all (self->selected_keys);
	self->inverted_selection = TRUE;
	virtual_tree_refill (self);
	g_signal_emit (self, widget_signals[SELECTION_CHANGED], 0);
}

void
e_virtual_tree_unselect_all (EVirtualTree *self)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	g_hash_table_remove_all (self->selected_keys);
	self->inverted_selection = FALSE;
	virtual_tree_refill (self);
	g_signal_emit (self, widget_signals[SELECTION_CHANGED], 0);
}

gboolean
e_virtual_tree_row_is_selected (EVirtualTree *self,
				guint row)
{
	GObject *row_obj;
	gboolean selected = FALSE;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), FALSE);

	row_obj = self->model ? e_virtual_tree_model_dup_row (self->model, row) : NULL;
	if (row_obj) {
		gconstpointer key = e_virtual_tree_model_get_row_key (self->model, row_obj);
		if (key)
			selected = virtual_tree_is_key_selected (self, key);
		g_clear_object (&row_obj);
	}

	return selected;
}

static void
virtual_tree_move_cursor (EVirtualTree *self,
			  gint row,
			  gboolean centered)
{
	self->cursor_scroll_pending = FALSE;
	self->cursor_scroll_pending_row = -1;

	self->cursor_row = row;

	if (row < 0) {
		set_cursor_object (self, NULL);
		virtual_tree_refill (self);
		return;
	}

	if (self->model) {
		GObject *cursor_obj = e_virtual_tree_model_dup_row (self->model, (guint) row);
		set_cursor_object (self, cursor_obj);
		g_clear_object (&cursor_obj);
	}

	virtual_tree_measure_row_height (self);

	if (self->visible_count > 2 && self->row_stride > 0 && self->vadjustment) {
		gdouble adj_value = gtk_adjustment_get_value (self->vadjustment);
		gdouble row_top = (gdouble) row * self->row_stride;
		gdouble row_bottom = row_top + self->row_stride;
		gdouble view_height = gtk_adjustment_get_page_size (self->vadjustment);
		gdouble new_value = adj_value;

		if (centered) {
			if (row_top < adj_value || row_bottom > adj_value + view_height) {
				gdouble center_offset = (view_height - self->row_stride) / 2.0;
				gdouble upper = gtk_adjustment_get_upper (self->vadjustment);

				new_value = CLAMP (row_top - center_offset, 0.0, MAX (upper - view_height, 0.0));
			}
		} else {
			/* Keep cursor visible with a margin of ~1.1 rows above and below,
			 * so the user can preview neighboring rows when navigating near the edges. */
			gint margin_pixels = self->row_stride + self->row_stride / 10;

			if (row_top - margin_pixels < adj_value)
				new_value = MAX (row_top - margin_pixels, 0.0);
			else if (row_bottom + margin_pixels > adj_value + view_height)
				new_value = MAX (row_bottom + margin_pixels - view_height, 0.0);
		}

		if (new_value != adj_value) {
			self->first_visible_row = (guint) (new_value / self->row_stride);
			gtk_adjustment_set_value (self->vadjustment, new_value);
		}
	}

	virtual_tree_refill (self);
}

void
e_virtual_tree_set_cursor (EVirtualTree *self,
			   gint row)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	virtual_tree_move_cursor (self, row, FALSE);

	self->anchor_row = row;
	self->shift_end_row = -1;
}

void
e_virtual_tree_set_cursor_centered (EVirtualTree *self,
				    gint row)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	virtual_tree_move_cursor (self, row, TRUE);

	self->anchor_row = row;
	self->shift_end_row = -1;
}

gboolean
e_virtual_tree_row_is_visible (EVirtualTree *self,
			       guint row)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), FALSE);

	return row >= self->first_visible_row &&
		row < self->first_visible_row + self->visible_count;
}

void
e_virtual_tree_scroll_cursor_into_view (EVirtualTree *self)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	if (self->cursor_row < 0)
		return;

	virtual_tree_move_cursor (self, self->cursor_row, TRUE);
}

gint
e_virtual_tree_get_cursor (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), -1);

	return self->cursor_row;
}

void
e_virtual_tree_scroll_to_row (EVirtualTree *self,
			      guint row)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	if (!self->vadjustment)
		return;

	virtual_tree_measure_row_height (self);

	gtk_adjustment_set_value (self->vadjustment, (gdouble) row * self->row_stride);
}

GtkTreeView *
e_virtual_tree_get_tree_view (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);

	return self->tree_view;
}

gboolean
e_virtual_tree_get_cell_rect (EVirtualTree *self,
			      guint column_index,
			      guint visible_row,
			      GdkRectangle *out_rect)
{
	ColumnInfo *col_info;
	GtkTreeIter iter;
	gboolean found = FALSE;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), FALSE);
	g_return_val_if_fail (out_rect != NULL, FALSE);
	g_return_val_if_fail (column_index < self->columns->len, FALSE);

	if (!self->tree_view || !self->list_store)
		return FALSE;

	col_info = g_ptr_array_index (self->columns, column_index);

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->list_store), &iter)) {
		do {
			guint row_idx = 0;
			gboolean is_group = FALSE;

			gtk_tree_model_get (GTK_TREE_MODEL (self->list_store), &iter,
				COL_VISIBLE_ROW_IDX, &row_idx,
				COL_IS_GROUP, &is_group,
				-1);

			if (!is_group && row_idx == visible_row) {
				found = TRUE;
				break;
			}
		} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->list_store), &iter));
	}

	if (found) {
		GtkTreePath *path;
		gint wx, wy;

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->list_store), &iter);
		gtk_tree_view_get_cell_area (self->tree_view, path, col_info->tree_column, out_rect);
		gtk_tree_view_convert_bin_window_to_widget_coords (self->tree_view, out_rect->x, out_rect->y, &wx, &wy);
		out_rect->x = wx;
		out_rect->y = wy;
		gtk_tree_path_free (path);
	}

	return found;
}

void
e_virtual_tree_enable_drag_source (EVirtualTree *self,
				   GdkModifierType start_button_mask,
				   const GtkTargetEntry *targets,
				   gint n_targets,
				   GdkDragAction actions)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	g_clear_pointer (&self->drag.source_target_list, gtk_target_list_unref);

	self->drag.source_target_list = gtk_target_list_new (targets, n_targets);
	self->drag.source_start_mask = start_button_mask;
	self->drag.source_actions = actions;
	self->drag.source_enabled = TRUE;
}

void
e_virtual_tree_enable_drag_dest (EVirtualTree *self,
				 const GtkTargetEntry *targets,
				 gint n_targets,
				 GdkDragAction actions)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	gtk_tree_view_enable_model_drag_dest (self->tree_view,
		targets, n_targets, actions);

	gtk_drag_dest_set (GTK_WIDGET (self->tree_view),
		GTK_DEST_DEFAULT_ALL, targets, n_targets, actions);
}

/* --- Column state persistence --- */

void
e_virtual_tree_save_column_state_to_key_file (EVirtualTree *self,
					      GKeyFile *key_file)
{
	GList *display_columns, *link;
	guint order = 0;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));
	g_return_if_fail (key_file != NULL);

	display_columns = gtk_tree_view_get_columns (self->tree_view);
	for (link = display_columns; link; link = link->next) {
		GtkTreeViewColumn *tvc = link->data;
		guint col_idx = find_column_index (self, tvc);
		ColumnInfo *ci = g_ptr_array_index (self->columns, col_idx);
		gboolean visible = gtk_tree_view_column_get_visible (ci->tree_column);
		gchar *group;
		gint width;

		group = g_strdup_printf ("Column-%s", ci->column_id);

		g_key_file_set_integer (key_file, group, "order", (gint) order);
		order++;

		g_key_file_set_boolean (key_file, group, "visible", visible);

		if (visible && !gtk_tree_view_column_get_expand (ci->tree_column)) {
			width = gtk_tree_view_column_get_width (ci->tree_column);
			if (width > 0)
				g_key_file_set_integer (key_file, group, "width", width);
		}

		if (ci->sort_priority > 0) {
			GtkSortType sort_order = gtk_tree_view_column_get_sort_order (ci->tree_column);

			g_key_file_set_integer (key_file, group, "sort-order", ci->sort_priority - 1);
			g_key_file_set_string (key_file, group, "sort-dir",
				sort_order == GTK_SORT_ASCENDING ? "ascending" : "descending");
		}

		if (ci->group_priority >= 0) {
			GtkSortType group_order = gtk_tree_view_column_get_sort_order (ci->tree_column);

			g_key_file_set_integer (key_file, group, "group-order", ci->group_priority);
			g_key_file_set_string (key_file, group, "group-dir", group_order == GTK_SORT_ASCENDING ? "ascending" : "descending");
		}

		g_free (group);
	}
	g_list_free (display_columns);
}

void
e_virtual_tree_load_column_state_from_key_file (EVirtualTree *self,
						GKeyFile *key_file)
{
	guint *sorted_indices;
	guint ii, n_visible;
	gint *order_map;
	guint n_columns;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));
	g_return_if_fail (key_file != NULL);

	e_virtual_tree_freeze_state_changed (self);

	n_columns = self->columns->len;
	order_map = g_new (gint, n_columns);
	for (ii = 0; ii < n_columns; ii++) {
		order_map[ii] = (gint) ii;
	}

	virtual_tree_init_proportional_weights (self, n_columns);

	e_virtual_tree_clear_sort (self);
	e_virtual_tree_clear_group (self);

	n_visible = 0;

	for (ii = 0; ii < n_columns; ii++) {
		ColumnInfo *ci = g_ptr_array_index (self->columns, ii);
		gchar *group = g_strdup_printf ("Column-%s", ci->column_id);

		if (g_key_file_has_group (key_file, group)) {
			gboolean visible = TRUE;

			if (g_key_file_has_key (key_file, group, "visible", NULL))
				visible = g_key_file_get_boolean (key_file, group, "visible", NULL);

			gtk_tree_view_column_set_visible (ci->tree_column, visible);

			if (visible)
				n_visible++;

			if (g_key_file_has_key (key_file, group, "width", NULL)) {
				gint width = g_key_file_get_integer (key_file, group, "width", NULL);
				if (width > 0)
					self->proportional_weights[ii] = width;
			}

			if (g_key_file_has_key (key_file, group, "order", NULL))
				order_map[ii] = g_key_file_get_integer (key_file, group, "order", NULL);

			if (g_key_file_has_key (key_file, group, "sort-order", NULL)) {
				gint sort_prio = g_key_file_get_integer (key_file, group, "sort-order", NULL);
				gchar *dir_str = g_key_file_get_string (key_file, group, "sort-dir", NULL);
				GtkSortType sort_dir = GTK_SORT_ASCENDING;

				if (g_strcmp0 (dir_str, "descending") == 0)
					sort_dir = GTK_SORT_DESCENDING;

				e_virtual_tree_set_column_sort (self, ii, sort_dir, sort_prio + 1);
				g_free (dir_str);
			}

			if (g_key_file_has_key (key_file, group, "group-order", NULL)) {
				gint group_prio = g_key_file_get_integer (key_file, group, "group-order", NULL);
				gchar *dir_str = g_key_file_get_string (key_file, group, "group-dir", NULL);
				GtkSortType group_dir = GTK_SORT_ASCENDING;

				if (g_strcmp0 (dir_str, "descending") == 0)
					group_dir = GTK_SORT_DESCENDING;

				e_virtual_tree_set_column_group (self, ii, group_dir, group_prio);
				g_free (dir_str);
			}
		} else {
			gtk_tree_view_column_set_visible (ci->tree_column, FALSE);
		}

		g_free (group);
	}

	if (n_visible == 0) {
		g_free (order_map);
		e_virtual_tree_thaw_state_changed (self);
		gtk_widget_queue_resize (GTK_WIDGET (self));
		return;
	}

	/* Reorder columns by saved order values */
	sorted_indices = g_new (guint, n_columns);
	for (ii = 0; ii < n_columns; ii++) {
		sorted_indices[ii] = ii;
	}

	for (ii = 1; ii < n_columns; ii++) {
		guint key_idx = sorted_indices[ii];
		gint key_val = order_map[key_idx];
		gint jj = (gint) ii - 1;

		while (jj >= 0 && order_map[sorted_indices[jj]] > key_val) {
			sorted_indices[jj + 1] = sorted_indices[jj];
			jj--;
		}
		sorted_indices[jj + 1] = key_idx;
	}

	for (ii = 0; ii < n_columns; ii++) {
		ColumnInfo *ci = g_ptr_array_index (self->columns, sorted_indices[ii]);
		GtkTreeViewColumn *after = NULL;

		if (ii > 0) {
			ColumnInfo *prev_ci = g_ptr_array_index (self->columns, sorted_indices[ii - 1]);
			after = prev_ci->tree_column;
		}

		gtk_tree_view_move_column_after (self->tree_view, ci->tree_column, after);
	}

	g_free (sorted_indices);
	g_free (order_map);

	e_virtual_tree_thaw_state_changed (self);
	gtk_widget_queue_resize (GTK_WIDGET (self));
}

GKeyFile *
e_virtual_tree_convert_legacy_etable_state (const gchar *xml_data,
					    gssize length,
					    const gchar * const *legacy_column_ids,
					    guint n_legacy_column_ids)
{
	xmlDocPtr doc;
	xmlNodePtr root, node;
	GKeyFile *key_file;
	gint order = 0, sort_order = 0, group_order = 0;
	gboolean any_column = FALSE;

	g_return_val_if_fail (xml_data != NULL, NULL);

	doc = xmlReadMemory (xml_data, length >= 0 ? (gint) length : (gint) strlen (xml_data),
		NULL, NULL, XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
	if (!doc)
		return NULL;

	root = xmlDocGetRootElement (doc);
	if (!root || g_strcmp0 ((const gchar *) root->name, "ETableState") != 0) {
		xmlFreeDoc (doc);
		return NULL;
	}

	key_file = g_key_file_new ();

	for (node = root->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;

		if (g_strcmp0 ((const gchar *) node->name, "column") == 0) {
			xmlChar *source_str;

			source_str = xmlGetProp (node, (const xmlChar *) "source");
			if (source_str) {
				gint source_idx = atoi ((const gchar *) source_str);

				if (source_idx >= 0 && (guint) source_idx < n_legacy_column_ids &&
				    legacy_column_ids[source_idx]) {
					xmlChar *expansion_str;
					gchar *group;
					gdouble expansion = 1.0;

					expansion_str = xmlGetProp (node, (const xmlChar *) "expansion");
					if (expansion_str) {
						expansion = g_ascii_strtod ((const gchar *) expansion_str, NULL);
						xmlFree (expansion_str);
					}

					group = g_strdup_printf ("Column-%s", legacy_column_ids[source_idx]);
					g_key_file_set_integer (key_file, group, "order", order);
					g_key_file_set_boolean (key_file, group, "visible", TRUE);
					if (expansion > 0.0)
						g_key_file_set_integer (key_file, group, "width", (gint) (expansion * 1000.0 + 0.5));
					order++;
					any_column = TRUE;
					g_free (group);
				}

				xmlFree (source_str);
			}
		} else if (g_strcmp0 ((const gchar *) node->name, "grouping") == 0) {
			xmlNodePtr child;

			for (child = node->children; child; child = child->next) {
				const gchar *elem_name;
				xmlChar *col_str, *asc_str;
				gchar *group;
				gint col_idx;
				gboolean ascending;
				gboolean is_group;

				if (child->type != XML_ELEMENT_NODE)
					continue;

				elem_name = (const gchar *) child->name;
				is_group = g_strcmp0 (elem_name, "group") == 0;
				if (!is_group && g_strcmp0 (elem_name, "leaf") != 0)
					continue;

				col_str = xmlGetProp (child, (const xmlChar *) "column");
				if (!col_str)
					continue;

				col_idx = atoi ((const gchar *) col_str);
				xmlFree (col_str);

				if (col_idx < 0 || (guint) col_idx >= n_legacy_column_ids || !legacy_column_ids[col_idx])
					continue;

				asc_str = xmlGetProp (child, (const xmlChar *) "ascending");
				ascending = !asc_str || g_strcmp0 ((const gchar *) asc_str, "true") == 0;
				if (asc_str)
					xmlFree (asc_str);

				group = g_strdup_printf ("Column-%s", legacy_column_ids[col_idx]);

				if (is_group) {
					g_key_file_set_integer (key_file, group, "group-order", group_order);
					g_key_file_set_string (key_file, group, "group-dir", ascending ? "ascending" : "descending");
					group_order++;
				} else {
					g_key_file_set_integer (key_file, group, "sort-order", sort_order);
					g_key_file_set_string (key_file, group, "sort-dir", ascending ? "ascending" : "descending");
					sort_order++;
				}

				g_free (group);
			}
		}
	}

	xmlFreeDoc (doc);

	if (!any_column) {
		g_key_file_unref (key_file);
		return NULL;
	}

	return key_file;
}

const gchar * const *
e_virtual_tree_get_legacy_etable_column_map (EVirtualTree *self,
					     guint *out_n_column_ids)
{
	gpointer result = NULL;
	guint n_column_ids = 0;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);

	g_signal_emit (self, widget_signals[GET_LEGACY_ETABLE_COLUMN_MAP], 0, &n_column_ids, &result);

	if (out_n_column_ids)
		*out_n_column_ids = n_column_ids;

	return (const gchar * const *) result;
}

gboolean
e_virtual_tree_get_applying_proportional_widths (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), FALSE);

	return self->applying_proportional_widths;
}

static void
virtual_tree_save_column_state (EVirtualTree *self)
{
	GKeyFile *key_file;
	GError *local_error = NULL;
	gchar *data;

	if (!self->column_state_filename)
		return;

	key_file = g_key_file_new ();

	e_virtual_tree_save_column_state_to_key_file (self, key_file);

	data = g_key_file_to_data (key_file, NULL, &local_error);
	if (!data)
		g_warning ("Failed to serialize column state: %s", local_error ? local_error->message : "Unknown error");
	else if (!g_file_set_contents (self->column_state_filename, data, -1, &local_error))
		g_warning ("Failed to save column state to '%s': %s", self->column_state_filename, local_error ? local_error->message : "Unknown error");

	g_clear_error (&local_error);
	g_free (data);
	g_key_file_free (key_file);
}

static void
virtual_tree_load_column_state (EVirtualTree *self)
{
	GKeyFile *key_file;
	GError *local_error = NULL;

	if (!self->column_state_filename)
		return;

	key_file = g_key_file_new ();
	if (!g_key_file_load_from_file (key_file, self->column_state_filename, G_KEY_FILE_NONE, &local_error)) {
		if (!g_error_matches (local_error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
			g_warning ("Failed to load column state from '%s': %s", self->column_state_filename, local_error ? local_error->message : "Unknown error");
		g_clear_error (&local_error);
		g_key_file_free (key_file);
		return;
	}

	e_virtual_tree_load_column_state_from_key_file (self, key_file);

	g_key_file_free (key_file);
}

static gboolean
save_state_timeout_cb (gpointer user_data)
{
	EVirtualTree *self = user_data;

	self->save_state_id = 0;
	virtual_tree_save_column_state (self);

	return G_SOURCE_REMOVE;
}

static void
schedule_save_state (EVirtualTree *self)
{
	if (self->state_freeze_count > 0) {
		self->state_changed_while_frozen = TRUE;
		return;
	}

	if (!self->column_state_filename)
		return;

	if (self->save_state_id)
		g_source_remove (self->save_state_id);

	self->save_state_id = g_timeout_add (1000, save_state_timeout_cb, self);
}

static gboolean
virtual_tree_find_next_resizable_column (EVirtualTree *self,
					 guint from_col_idx,
					 guint *out_col_idx)
{
	ColumnInfo *from_ci;
	GList *columns, *link;
	gboolean found_from = FALSE;
	gboolean result = FALSE;

	from_ci = g_ptr_array_index (self->columns, from_col_idx);
	columns = gtk_tree_view_get_columns (self->tree_view);

	for (link = columns; link; link = link->next) {
		GtkTreeViewColumn *tvc = link->data;

		if (!found_from) {
			if (tvc == from_ci->tree_column)
				found_from = TRUE;
			continue;
		}

		if (gtk_tree_view_column_get_visible (tvc) &&
		    gtk_tree_view_column_get_resizable (tvc)) {
			*out_col_idx = find_column_index (self, tvc);
			result = TRUE;
			break;
		}
	}

	g_list_free (columns);

	return result;
}

static void
on_column_state_notify (GObject *object,
			GParamSpec *pspec,
			gpointer user_data)
{
	EVirtualTree *self = user_data;

	if (self->state_freeze_count > 0) {
		self->state_changed_while_frozen = TRUE;
		return;
	}

	if (g_strcmp0 (g_param_spec_get_name (pspec), "visible") == 0) {
		self->last_alloc_height = -1;
		self->proportional_last_width = -1;
		virtual_tree_invalidate_metrics (self);
		virtual_tree_update_scrollbar (self);
		virtual_tree_schedule_refill (self);
	}

	if (g_strcmp0 (g_param_spec_get_name (pspec), "fixed-width") == 0 &&
	    !self->applying_proportional_widths && self->proportional_weights) {
		guint col_idx = find_column_index (self, GTK_TREE_VIEW_COLUMN (object));

		if (col_idx < self->proportional_n_columns) {
			gint new_width = gtk_tree_view_column_get_fixed_width (GTK_TREE_VIEW_COLUMN (object));
			gint delta = new_width - self->proportional_prev_width[col_idx];
			guint partner_idx;

			self->proportional_manual[col_idx] = TRUE;
			self->proportional_prev_width[col_idx] = new_width;

			if (delta != 0 && virtual_tree_find_next_resizable_column (self, col_idx, &partner_idx)) {
				ColumnInfo *partner_ci = g_ptr_array_index (self->columns, partner_idx);
				gint partner_width = self->proportional_prev_width[partner_idx] - delta;

				if (partner_width < 1)
					partner_width = 1;

				self->applying_proportional_widths = TRUE;
				gtk_tree_view_column_set_sizing (partner_ci->tree_column, GTK_TREE_VIEW_COLUMN_FIXED);
				gtk_tree_view_column_set_fixed_width (partner_ci->tree_column, partner_width);
				self->applying_proportional_widths = FALSE;

				self->proportional_manual[partner_idx] = TRUE;
				self->proportional_prev_width[partner_idx] = partner_width;
			}

			self->proportional_last_width = -1;
		}
	}

	schedule_save_state (self);
}

static void
on_tree_view_columns_changed (GtkTreeView *tree_view,
			      gpointer user_data)
{
	EVirtualTree *self = user_data;

	if (self->state_freeze_count > 0) {
		self->state_changed_while_frozen = TRUE;
		return;
	}

	schedule_save_state (self);
}

void
e_virtual_tree_set_column_sort (EVirtualTree *self,
				guint column_index,
				GtkSortType sort_order,
				gint sort_priority)
{
	ColumnInfo *info;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));
	g_return_if_fail (column_index < self->columns->len);

	info = g_ptr_array_index (self->columns, column_index);
	info->sort_priority = sort_priority;

	if (sort_priority > 0) {
		gtk_tree_view_column_set_sort_indicator (info->tree_column, TRUE);
		gtk_tree_view_column_set_sort_order (info->tree_column, sort_order);
	} else {
		gtk_tree_view_column_set_sort_indicator (info->tree_column, FALSE);
	}

	schedule_save_state (self);
}

void
e_virtual_tree_clear_sort (EVirtualTree *self)
{
	guint ii;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	for (ii = 0; ii < self->columns->len; ii++) {
		ColumnInfo *info = g_ptr_array_index (self->columns, ii);

		info->sort_priority = 0;
		gtk_tree_view_column_set_sort_indicator (info->tree_column, FALSE);
		gtk_tree_view_column_set_title (info->tree_column, info->title);
	}

	schedule_save_state (self);
}

gboolean
e_virtual_tree_get_column_sort (EVirtualTree *self,
				guint column_index,
				GtkSortType *out_sort_order,
				gint *out_priority)
{
	ColumnInfo *info;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), FALSE);
	g_return_val_if_fail (column_index < self->columns->len, FALSE);

	info = g_ptr_array_index (self->columns, column_index);

	if (info->sort_priority <= 0)
		return FALSE;

	if (out_sort_order)
		*out_sort_order = gtk_tree_view_column_get_sort_order (info->tree_column);

	if (out_priority)
		*out_priority = info->sort_priority;

	return TRUE;
}

void
e_virtual_tree_set_column_group (EVirtualTree *self,
				 guint column_index,
				 GtkSortType sort_order,
				 gint group_priority)
{
	ColumnInfo *info;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));
	g_return_if_fail (column_index < self->columns->len);

	info = g_ptr_array_index (self->columns, column_index);
	info->group_priority = group_priority;

	if (group_priority >= 0)
		gtk_tree_view_column_set_sort_order (info->tree_column, sort_order);

	schedule_save_state (self);
}

void
e_virtual_tree_clear_group (EVirtualTree *self)
{
	guint ii;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	for (ii = 0; ii < self->columns->len; ii++) {
		ColumnInfo *info = g_ptr_array_index (self->columns, ii);

		info->group_priority = -1;
	}

	schedule_save_state (self);
}

gboolean
e_virtual_tree_get_column_group (EVirtualTree *self,
				 guint column_index,
				 GtkSortType *out_sort_order,
				 gint *out_priority)
{
	ColumnInfo *info;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), FALSE);
	g_return_val_if_fail (column_index < self->columns->len, FALSE);

	info = g_ptr_array_index (self->columns, column_index);

	if (info->group_priority < 0)
		return FALSE;

	if (out_sort_order)
		*out_sort_order = gtk_tree_view_column_get_sort_order (info->tree_column);

	if (out_priority)
		*out_priority = info->group_priority;

	return TRUE;
}

void
e_virtual_tree_set_column_groupable (EVirtualTree *self,
				     guint column_index,
				     gboolean groupable)
{
	ColumnInfo *info;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));
	g_return_if_fail (column_index < self->columns->len);

	info = g_ptr_array_index (self->columns, column_index);
	info->groupable = groupable;
}

gboolean
e_virtual_tree_get_column_groupable (EVirtualTree *self,
				     guint column_index)
{
	ColumnInfo *info;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), FALSE);
	g_return_val_if_fail (column_index < self->columns->len, FALSE);

	info = g_ptr_array_index (self->columns, column_index);

	return info->groupable;
}

gboolean
e_virtual_tree_get_allow_grouping (EVirtualTree *self)
{
	guint ii;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), FALSE);

	for (ii = 0; ii < self->columns->len; ii++) {
		ColumnInfo *info = g_ptr_array_index (self->columns, ii);

		if (info->groupable)
			return TRUE;
	}

	return FALSE;
}

guint
e_virtual_tree_get_n_columns (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), 0);

	return self->columns->len;
}

const gchar *
e_virtual_tree_get_column_id (EVirtualTree *self,
			      guint column_index)
{
	ColumnInfo *col_info;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);
	g_return_val_if_fail (column_index < self->columns->len, NULL);

	col_info = g_ptr_array_index (self->columns, column_index);

	return col_info->column_id;
}

const gchar *
e_virtual_tree_get_column_title (EVirtualTree *self,
				 guint column_index)
{
	ColumnInfo *col_info;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);
	g_return_val_if_fail (column_index < self->columns->len, NULL);

	col_info = g_ptr_array_index (self->columns, column_index);

	return col_info->title;
}

void
e_virtual_tree_freeze_state_changed (EVirtualTree *self)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	self->state_freeze_count++;
}

void
e_virtual_tree_thaw_state_changed (EVirtualTree *self)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));
	g_return_if_fail (self->state_freeze_count > 0);

	self->state_freeze_count--;

	if (self->state_freeze_count == 0 && self->state_changed_while_frozen) {
		self->state_changed_while_frozen = FALSE;

		self->last_alloc_height = -1;
		virtual_tree_invalidate_metrics (self);
		virtual_tree_update_scrollbar (self);
		virtual_tree_schedule_refill (self);

		schedule_save_state (self);
		g_signal_emit (self, widget_signals[COLUMN_STATE_CHANGED], 0);
	}
}

/* --- Column header context menu --- */

static void
on_sort_ascending (EUIAction *action,
		   GVariant *value,
		   gpointer user_data)
{
	EVirtualTree *self = user_data;

	e_virtual_tree_clear_sort (self);
	e_virtual_tree_set_column_sort (self, self->popup_col_idx, GTK_SORT_ASCENDING, 1);
	g_signal_emit (self, widget_signals[COLUMN_STATE_CHANGED], 0);
}

static void
on_sort_descending (EUIAction *action,
		    GVariant *value,
		    gpointer user_data)
{
	EVirtualTree *self = user_data;

	e_virtual_tree_clear_sort (self);
	e_virtual_tree_set_column_sort (self, self->popup_col_idx, GTK_SORT_DESCENDING, 1);
	g_signal_emit (self, widget_signals[COLUMN_STATE_CHANGED], 0);
}

static void
on_unsort (EUIAction *action,
	   GVariant *value,
	   gpointer user_data)
{
	EVirtualTree *self = user_data;

	e_virtual_tree_clear_sort (self);
	g_signal_emit (self, widget_signals[COLUMN_STATE_CHANGED], 0);
}

static void
on_remove_column (EUIAction *action,
		  GVariant *value,
		  gpointer user_data)
{
	EVirtualTree *self = user_data;
	ColumnInfo *ci;

	if (self->popup_col_idx >= self->columns->len)
		return;

	ci = g_ptr_array_index (self->columns, self->popup_col_idx);
	gtk_tree_view_column_set_visible (ci->tree_column, FALSE);
}

static void
on_show_column_change_state (EUIAction *action,
			     GVariant *value,
			     gpointer user_data)
{
	EVirtualTree *self = user_data;
	const gchar *full_name;
	const gchar *col_id;
	ColumnInfo *ci;
	gboolean new_visible;
	gint idx;

	full_name = g_action_get_name (G_ACTION (action));
	if (!g_str_has_prefix (full_name, "show-"))
		return;

	col_id = full_name + 5;
	idx = find_column_index_by_id (self, col_id);
	if (idx < 0)
		return;

	ci = g_ptr_array_index (self->columns, (guint) idx);
	new_visible = g_variant_get_boolean (value);

	gtk_tree_view_column_set_visible (ci->tree_column, new_visible);
	e_ui_action_set_state (action, value);
}

static void
on_customize_view (EUIAction *action,
		   GVariant *value,
		   gpointer user_data)
{
	e_virtual_tree_show_customize_popover (user_data);
}

static const EUIActionEntry header_menu_entries[] = {
	{ "sort-ascending", NULL, N_("Sort _Ascending"),
	  NULL, NULL, on_sort_ascending, NULL, NULL, NULL },
	{ "sort-descending", NULL, N_("Sort _Descending"),
	  NULL, NULL, on_sort_descending, NULL, NULL, NULL },
	{ "unsort", NULL, N_("_Unsort"),
	  NULL, NULL, on_unsort, NULL, NULL, NULL },
	{ "remove-column", NULL, N_("_Remove This Column"),
	  NULL, NULL, on_remove_column, NULL, NULL, NULL },
	{ "customize-view", NULL, N_("Custo_mize Current View…"),
	  NULL, NULL, on_customize_view, NULL, NULL, NULL }
};

static void
ensure_header_menu_actions (EVirtualTree *self)
{
	guint ii;

	if (self->ui_manager)
		return;

	self->ui_manager = e_ui_manager_new (NULL);

	e_ui_manager_add_actions (self->ui_manager, "vtree", NULL,
		header_menu_entries, G_N_ELEMENTS (header_menu_entries), self);

	for (ii = 0; ii < self->columns->len; ii++) {
		ColumnInfo *ci = g_ptr_array_index (self->columns, ii);
		gchar *action_name = g_strdup_printf ("show-%s", ci->column_id);
		gboolean visible = gtk_tree_view_column_get_visible (ci->tree_column);
		EUIAction *action;

		action = e_ui_action_new_stateful ("vtree", action_name, NULL,
			g_variant_new_boolean (visible));
		e_ui_action_set_label (action,
			(ci->title && *ci->title) ? ci->title : ci->column_id);

		e_ui_manager_add_action (self->ui_manager, "vtree", action,
			NULL, on_show_column_change_state, self);

		g_object_unref (action);
		g_free (action_name);
	}
}

static gboolean
has_multiple_visible_columns (EVirtualTree *self)
{
	guint ii, count = 0;

	for (ii = 0; ii < self->columns->len; ii++) {
		ColumnInfo *ci = g_ptr_array_index (self->columns, ii);

		if (gtk_tree_view_column_get_visible (ci->tree_column)) {
			count++;
			if (count > 1)
				return TRUE;
		}
	}

	return FALSE;
}

static gint
compare_column_info_by_title (gconstpointer aa,
			      gconstpointer bb)
{
	ColumnInfo *ci_a = *((ColumnInfo * const *) aa);
	ColumnInfo *ci_b = *((ColumnInfo * const *) bb);
	const gchar *title_a = (ci_a->title && *ci_a->title) ? ci_a->title : ci_a->column_id;
	const gchar *title_b = (ci_b->title && *ci_b->title) ? ci_b->title : ci_b->column_id;

	return g_utf8_collate (title_a, title_b);
}

static void
show_header_context_menu (EVirtualTree *self,
			  guint col_idx,
			  GdkEventButton *event)
{
	GMenu *menu, *section, *submenu;
	GtkWidget *popup;
	EUIAction *action;
	GPtrArray *hidden_columns;
	guint ii;

	ensure_header_menu_actions (self);

	self->popup_col_idx = col_idx;

	for (ii = 0; ii < self->columns->len; ii++) {
		ColumnInfo *ci = g_ptr_array_index (self->columns, ii);
		gchar *action_name = g_strdup_printf ("show-%s", ci->column_id);
		gboolean visible = gtk_tree_view_column_get_visible (ci->tree_column);

		action = e_ui_manager_get_action (self->ui_manager, action_name);
		if (action)
			e_ui_action_set_state (action, g_variant_new_boolean (visible));

		g_free (action_name);
	}

	action = e_ui_manager_get_action (self->ui_manager, "remove-column");
	if (action)
		e_ui_action_set_sensitive (action, has_multiple_visible_columns (self));

	menu = g_menu_new ();

	section = g_menu_new ();
	g_menu_append (section, _("Sort _Ascending"), "vtree.sort-ascending");
	g_menu_append (section, _("Sort _Descending"), "vtree.sort-descending");
	g_menu_append (section, _("_Unsort"), "vtree.unsort");
	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	section = g_menu_new ();
	g_menu_append (section, _("_Remove This Column"), "vtree.remove-column");
	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	submenu = g_menu_new ();
	hidden_columns = g_ptr_array_new ();
	for (ii = 0; ii < self->columns->len; ii++) {
		ColumnInfo *ci = g_ptr_array_index (self->columns, ii);

		if (!gtk_tree_view_column_get_visible (ci->tree_column))
			g_ptr_array_add (hidden_columns, ci);
	}
	g_ptr_array_sort (hidden_columns, compare_column_info_by_title);
	for (ii = 0; ii < hidden_columns->len; ii++) {
		ColumnInfo *ci = g_ptr_array_index (hidden_columns, ii);
		gchar *action_name = g_strdup_printf ("vtree.show-%s", ci->column_id);
		const gchar *label = (ci->title && *ci->title) ? ci->title : ci->column_id;

		g_menu_append (submenu, label, action_name);
		g_free (action_name);
	}
	g_ptr_array_free (hidden_columns, TRUE);
	if (g_menu_model_get_n_items (G_MENU_MODEL (submenu)) > 0)
		g_menu_append_submenu (menu, _("Available _Columns"), G_MENU_MODEL (submenu));
	g_object_unref (submenu);

	section = g_menu_new ();
	g_menu_append (section, _("Custo_mize Current View…"), "vtree.customize-view");
	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	popup = gtk_menu_new_from_model (G_MENU_MODEL (menu));
	g_object_unref (menu);

	e_ui_manager_add_action_groups_to_widget (self->ui_manager, popup);
	gtk_menu_attach_to_widget (GTK_MENU (popup), GTK_WIDGET (self->tree_view), NULL);
	e_util_connect_menu_detach_after_deactivate (GTK_MENU (popup));
	gtk_menu_popup_at_pointer (GTK_MENU (popup), (const GdkEvent *) event);
}

void
e_virtual_tree_set_column_state_filename (EVirtualTree *self,
					  const gchar *filename)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	if (g_strcmp0 (self->column_state_filename, filename) == 0)
		return;

	if (self->save_state_id) {
		g_source_remove (self->save_state_id);
		self->save_state_id = 0;
		virtual_tree_save_column_state (self);
	}

	g_free (self->column_state_filename);
	self->column_state_filename = g_strdup (filename);

	if (self->column_state_filename)
		virtual_tree_load_column_state (self);
}

const gchar *
e_virtual_tree_get_column_state_filename (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);

	return self->column_state_filename;
}

void
e_virtual_tree_set_search_func (EVirtualTree *self,
				EVirtualTreeSearchFunc func,
				gpointer user_data,
				GDestroyNotify destroy)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	if (self->search_destroy && self->search_user_data)
		self->search_destroy (self->search_user_data);

	self->search_func = func;
	self->search_user_data = user_data;
	self->search_destroy = destroy;
}

/**
 * e_virtual_tree_set_selected_row_color_func:
 * @self: an #EVirtualTree
 * @func: (nullable): an #EVirtualTreeRowColorFunc, or %NULL to unset
 * @user_data: data to pass to @func
 * @destroy: (nullable): a #GDestroyNotify to free @user_data, or %NULL
 *
 * Sets a function to be called to determine a row's own color. The
 * row's own color is used as its background outright when the row is
 * selected; when unselected it has no effect on the row background
 * painted by #EVirtualTree.
 *
 * Since: 3.64
 **/
void
e_virtual_tree_set_selected_row_color_func (EVirtualTree *self,
					    EVirtualTreeRowColorFunc func,
					    gpointer user_data,
					    GDestroyNotify destroy)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	if (self->row_selected_color_destroy && self->row_selected_color_user_data)
		self->row_selected_color_destroy (self->row_selected_color_user_data);

	self->row_selected_color_func = func;
	self->row_selected_color_user_data = user_data;
	self->row_selected_color_destroy = destroy;
}

/**
 * e_virtual_tree_set_unselected_row_color_func:
 * @self: an #EVirtualTree
 * @func: (nullable): an #EVirtualTreeRowColorFunc, or %NULL to unset
 * @user_data: data to pass to @func
 * @destroy: (nullable): a #GDestroyNotify to free @user_data, or %NULL
 *
 * Sets a function to be called to determine a row's own background
 * color, used only while the row is not selected; when selected, the
 * row falls back to the usual selection appearance instead. The text
 * color of any #GtkCellRendererText in the row is overridden with a
 * color computed by e_utils_get_text_color_for_background() to keep
 * good contrast with the row's background.
 *
 * Since: 3.64
 **/
void
e_virtual_tree_set_unselected_row_color_func (EVirtualTree *self,
					      EVirtualTreeRowColorFunc func,
					      gpointer user_data,
					      GDestroyNotify destroy)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	if (self->row_unselected_color_destroy && self->row_unselected_color_user_data)
		self->row_unselected_color_destroy (self->row_unselected_color_user_data);

	self->row_unselected_color_func = func;
	self->row_unselected_color_user_data = user_data;
	self->row_unselected_color_destroy = destroy;
}

/**
 * e_virtual_tree_set_row_tooltip_markup_func:
 * @self: an #EVirtualTree
 * @func: (nullable): an #EVirtualTreeRowTooltipMarkupFunc, or %NULL to unset
 * @user_data: data to pass to @func
 * @destroy: (nullable): a #GDestroyNotify to free @user_data, or %NULL
 *
 * Sets a function to be called to build a whole-row tooltip markup for
 * the row under the pointer. When @func returns non-%NULL markup for a
 * row, it is shown anchored to the whole row, instead of the default
 * per-cell ellipsized-text tooltip.
 *
 * Since: 3.64
 **/
void
e_virtual_tree_set_row_tooltip_markup_func (EVirtualTree *self,
					    EVirtualTreeRowTooltipMarkupFunc func,
					    gpointer user_data,
					    GDestroyNotify destroy)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	if (self->row_tooltip_markup_destroy && self->row_tooltip_markup_user_data)
		self->row_tooltip_markup_destroy (self->row_tooltip_markup_user_data);

	self->row_tooltip_markup_func = func;
	self->row_tooltip_markup_user_data = user_data;
	self->row_tooltip_markup_destroy = destroy;
}

void
e_virtual_tree_set_empty_message (EVirtualTree *self,
				  const gchar *message)
{
	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	if (g_strcmp0 (self->empty_message, message) == 0)
		return;

	g_free (self->empty_message);
	self->empty_message = g_strdup (message);

	if (!self->model || e_virtual_tree_model_get_row_count (self->model) == 0) {
		if (self->tree_view)
			gtk_widget_queue_draw (GTK_WIDGET (self->tree_view));
	}

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EMPTY_MESSAGE]);
}

const gchar *
e_virtual_tree_get_empty_message (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);

	return self->empty_message;
}

gconstpointer
e_virtual_tree_get_cursor_key (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);

	if (!self->cursor_object || !self->model)
		return NULL;

	return e_virtual_tree_model_get_row_key (self->model, self->cursor_object);
}

GObject *
e_virtual_tree_get_cursor_object (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);

	return self->cursor_object;
}

guint
e_virtual_tree_selected_count (EVirtualTree *self)
{
	guint n_excluded;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), 0);

	n_excluded = g_hash_table_size (self->selected_keys);

	if (self->inverted_selection) {
		guint total;

		if (!self->model)
			return 0;

		total = e_virtual_tree_model_get_row_count (self->model);
		if (total < n_excluded)
			return 0;

		return total - n_excluded;
	}

	return n_excluded;
}

gboolean
e_virtual_tree_is_dragging (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), FALSE);

	return self->in_drag;
}

guint
_e_virtual_tree_get_first_visible_row (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), 0);

	return self->first_visible_row;
}

guint
_e_virtual_tree_get_visible_count (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), 0);

	return self->visible_count;
}

gint
_e_virtual_tree_get_row_stride (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), 0);

	return self->row_stride;
}

GtkAdjustment *
_e_virtual_tree_get_vadjustment (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);

	return self->vadjustment;
}

guint
_e_virtual_tree_get_n_columns (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), 0);

	return self->columns->len;
}

GArray *
_e_virtual_tree_get_display_column_order (EVirtualTree *self)
{
	GArray *order;
	GList *display_columns, *link;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);

	order = g_array_new (FALSE, FALSE, sizeof (guint));
	display_columns = gtk_tree_view_get_columns (self->tree_view);

	for (link = display_columns; link; link = g_list_next (link)) {
		GtkTreeViewColumn *tvc = link->data;
		guint column_index = find_column_index (self, tvc);

		g_array_append_val (order, column_index);
	}

	g_list_free (display_columns);

	return order;
}

const gchar *
_e_virtual_tree_get_column_title (EVirtualTree *self,
				  guint column_index)
{
	ColumnInfo *col_info;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);
	g_return_val_if_fail (column_index < self->columns->len, NULL);

	col_info = g_ptr_array_index (self->columns, column_index);

	return col_info->title;
}

gchar *
_e_virtual_tree_get_cell_text (EVirtualTree *self,
			       guint row_index,
			       guint column_index)
{
	ColumnInfo *col_info;
	GObject *row_object;
	GString *result;
	gboolean is_rtl;
	guint idx, ii;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);
	g_return_val_if_fail (column_index < self->columns->len, NULL);

	if (!self->model)
		return NULL;

	row_object = e_virtual_tree_model_dup_row (self->model, row_index);
	if (!row_object)
		return NULL;

	col_info = g_ptr_array_index (self->columns, column_index);
	result = g_string_new (NULL);
	is_rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

	for (ii = 0; ii < col_info->renderers->len; ii++) {
		RendererInfo *ri;
		gchar *text = NULL;

		if (is_rtl)
			idx = col_info->renderers->len - 1 - ii;
		else
			idx = ii;

		ri = g_ptr_array_index (col_info->renderers, idx);

		if (ri->func)
			ri->func (self, ri->renderer, row_object, row_index, ri->user_data);

		if (!GTK_IS_CELL_RENDERER_TEXT (ri->renderer))
			continue;

		g_object_get (ri->renderer, "text", &text, NULL);
		if (text && *text) {
			if (result->len > 0)
				g_string_append_c (result, ' ');
			g_string_append (result, text);
		}
		g_free (text);
	}

	g_object_unref (row_object);

	return g_string_free (result, result->len == 0);
}

gboolean
_e_virtual_tree_get_cell_toggle (EVirtualTree *self,
				 guint row_index,
				 guint column_index,
				 gboolean *out_active)
{
	ColumnInfo *col_info;
	GObject *row_object;
	gboolean found = FALSE;
	guint ii;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), FALSE);
	g_return_val_if_fail (column_index < self->columns->len, FALSE);

	if (!self->model)
		return FALSE;

	row_object = e_virtual_tree_model_dup_row (self->model, row_index);
	if (!row_object)
		return FALSE;

	col_info = g_ptr_array_index (self->columns, column_index);

	for (ii = 0; ii < col_info->renderers->len && !found; ii++) {
		RendererInfo *ri = g_ptr_array_index (col_info->renderers, ii);

		if (ri->func)
			ri->func (self, ri->renderer, row_object, row_index, ri->user_data);

		if (!GTK_IS_CELL_RENDERER_TOGGLE (ri->renderer))
			continue;

		g_object_get (ri->renderer, "active", out_active, NULL);
		found = TRUE;
	}

	g_object_unref (row_object);

	return found;
}

gboolean
_e_virtual_tree_column_is_printable (EVirtualTree *self,
				     guint column_index)
{
	ColumnInfo *col_info;
	guint ii;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), FALSE);
	g_return_val_if_fail (column_index < self->columns->len, FALSE);

	col_info = g_ptr_array_index (self->columns, column_index);

	for (ii = 0; ii < col_info->renderers->len; ii++) {
		RendererInfo *ri = g_ptr_array_index (col_info->renderers, ii);

		if (GTK_IS_CELL_RENDERER_TEXT (ri->renderer) || GTK_IS_CELL_RENDERER_TOGGLE (ri->renderer))
			return TRUE;
	}

	return FALSE;
}

gboolean
_e_virtual_tree_column_is_toggle (EVirtualTree *self,
				  guint column_index)
{
	ColumnInfo *col_info;
	guint ii;

	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), FALSE);
	g_return_val_if_fail (column_index < self->columns->len, FALSE);

	col_info = g_ptr_array_index (self->columns, column_index);

	for (ii = 0; ii < col_info->renderers->len; ii++) {
		RendererInfo *ri = g_ptr_array_index (col_info->renderers, ii);

		if (GTK_IS_CELL_RENDERER_TOGGLE (ri->renderer))
			return TRUE;
	}

	return FALSE;
}

static gboolean
destroy_popover_idle_cb (gpointer user_data)
{
	gtk_widget_destroy (GTK_WIDGET (user_data));
	return G_SOURCE_REMOVE;
}

static void
on_customize_popover_closed (GtkPopover *popover,
			     gpointer user_data)
{
	g_idle_add (destroy_popover_idle_cb, popover);
}

void
e_virtual_tree_show_customize_popover (EVirtualTree *self)
{
	GtkWidget *popover;
	GtkAllocation alloc;
	GdkRectangle rect;

	g_return_if_fail (E_IS_VIRTUAL_TREE (self));

	popover = e_virtual_tree_customize_popover_new (self);

	gtk_widget_get_allocation (GTK_WIDGET (self->tree_view), &alloc);
	gtk_tree_view_convert_bin_window_to_widget_coords (self->tree_view, 0, 0, NULL, &rect.y);
	rect.x = alloc.width / 2;
	rect.width = 1;
	rect.height = 1;

	gtk_popover_set_relative_to (GTK_POPOVER (popover), GTK_WIDGET (self->tree_view));
	gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
	gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);

	g_signal_connect (popover, "closed",
		G_CALLBACK (on_customize_popover_closed), NULL);

	gtk_popover_popup (GTK_POPOVER (popover));
}

EPrintable *
e_virtual_tree_get_printable (EVirtualTree *self)
{
	g_return_val_if_fail (E_IS_VIRTUAL_TREE (self), NULL);

	return e_virtual_tree_printable_new (self);
}
