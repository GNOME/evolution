/*
 * e-cell-toggle.c - Multi-state image toggle cell object.
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

#include "gal-a11y-e-cell-toggle.h"
#include "gal-a11y-e-cell-registry.h"

#include "e-cell-toggle.h"
#include "e-table-item.h"

struct _ECellTogglePrivate {
	gchar **icon_names;
	gchar **icon_descriptions;
	guint n_icon_names;

	GdkPixbuf *empty;
	GPtrArray *surfaces; /* SurfaceData * */
	gint surface_scale;
	gint height;

	gint bg_color_column;
};

enum {
	PROP_0,
	PROP_BG_COLOR_COLUMN
};

G_DEFINE_TYPE_WITH_PRIVATE (ECellToggle, e_cell_toggle, E_TYPE_CELL)

typedef struct _SurfaceData {
	cairo_surface_t *surface;
	gint width;
	gint height;
} SurfaceData;

static void
surface_data_free (gpointer ptr)
{
	SurfaceData *sd = ptr;

	if (sd) {
		cairo_surface_destroy (sd->surface);
		g_free (sd);
	}
}

typedef struct {
	ECellView cell_view;
	GnomeCanvas *canvas;
} ECellToggleView;

static void
cell_toggle_ensure_icons (ECellToggle *cell_toggle,
			  ECellView *ecell_view)
{
	ETableItem *eti;
	GtkIconTheme *icon_theme;
	gint width, height, scale = 1;
	gint max_height = 0;
	guint ii;
	GError *error = NULL;

	eti = E_TABLE_ITEM (ecell_view->e_table_item_view);
	if (eti) {
		GtkStyleContext *style_context;

		style_context = gtk_widget_get_style_context (GTK_WIDGET (GNOME_CANVAS_ITEM (eti)->canvas));
		scale = gtk_style_context_get_scale (style_context);
	}

	if (cell_toggle->priv->surfaces->len > 0 &&
	    cell_toggle->priv->surface_scale == scale)
		return;

	g_ptr_array_set_size (cell_toggle->priv->surfaces, 0);

	icon_theme = gtk_icon_theme_get_default ();
	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, &height);

	for (ii = 0; ii < cell_toggle->priv->n_icon_names; ii++) {
		const gchar *icon_name = cell_toggle->priv->icon_names[ii];
		GdkPixbuf *pixbuf = NULL;
		SurfaceData *sd;
		gint unscaled_height;

		if (icon_name != NULL)
			pixbuf = gtk_icon_theme_load_icon_for_scale (
				icon_theme, icon_name, height, scale, GTK_ICON_LOOKUP_FORCE_SIZE, &error);

		if (error != NULL) {
			g_warning ("%s", error->message);
			g_clear_error (&error);
		}

		if (pixbuf == NULL) {
			pixbuf = g_object_ref (cell_toggle->priv->empty);
			unscaled_height = gdk_pixbuf_get_height (pixbuf);
		} else {
			unscaled_height = gdk_pixbuf_get_height (pixbuf) / (scale > 1 ? scale : 1);
		}

		sd = g_new0 (SurfaceData, 1);
		sd->surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, NULL);
		sd->width = width;
		sd->height = unscaled_height;

		g_ptr_array_add (cell_toggle->priv->surfaces, sd);

		max_height = MAX (max_height, unscaled_height);

		g_clear_object (&pixbuf);
	}

	cell_toggle->priv->height = max_height;
	cell_toggle->priv->surface_scale = scale;
}

static void
cell_toggle_dispose (GObject *object)
{
	ECellToggle *self = E_CELL_TOGGLE (object);

	g_clear_object (&self->priv->empty);

	/* This frees all the elements. */
	g_ptr_array_set_size (self->priv->surfaces, 0);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_cell_toggle_parent_class)->dispose (object);
}

static void
cell_toggle_finalize (GObject *object)
{
	ECellToggle *self = E_CELL_TOGGLE (object);
	guint ii;

	/* The array is not NULL-terminated,
	 * so g_strfreev() will not work. */
	for (ii = 0; ii < self->priv->n_icon_names; ii++)
		g_free (self->priv->icon_names[ii]);
	g_free (self->priv->icon_names);

	if (self->priv->icon_descriptions) {
		for (ii = 0; ii < self->priv->n_icon_names; ii++)
			g_free (self->priv->icon_descriptions[ii]);
		g_free (self->priv->icon_descriptions);
	}

	g_ptr_array_free (self->priv->surfaces, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cell_toggle_parent_class)->finalize (object);
}

static ECellView *
cell_toggle_new_view (ECell *ecell,
                      ETableModel *table_model,
                      gpointer e_table_item_view)
{
	ECellToggleView *toggle_view = g_new0 (ECellToggleView, 1);
	ETableItem *eti = E_TABLE_ITEM (e_table_item_view);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (eti)->canvas;

	toggle_view->cell_view.ecell = ecell;
	toggle_view->cell_view.e_table_model = table_model;
	toggle_view->cell_view.e_table_item_view = e_table_item_view;
	toggle_view->cell_view.kill_view_cb = NULL;
	toggle_view->cell_view.kill_view_cb_data = NULL;
	toggle_view->canvas = canvas;

	return (ECellView *) toggle_view;
}

static void
cell_toggle_kill_view (ECellView *ecell_view)
{
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;

	if (toggle_view->cell_view.kill_view_cb)
		toggle_view->cell_view.kill_view_cb (
			ecell_view, toggle_view->cell_view.kill_view_cb_data);

	if (toggle_view->cell_view.kill_view_cb_data)
		g_list_free (toggle_view->cell_view.kill_view_cb_data);

	g_free (ecell_view);
}

static void
cell_toggle_draw (ECellView *ecell_view,
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
	ECellToggle *self;
	ETableItem *eti;
	GtkStyleContext *style_context;
	SurfaceData *sd;
	gint x, y;

	const gint value = GPOINTER_TO_INT (
		e_table_model_value_at (ecell_view->e_table_model, model_col, row));

	cell_toggle_ensure_icons (E_CELL_TOGGLE (ecell_view->ecell), ecell_view);

	self = E_CELL_TOGGLE (ecell_view->ecell);

	if (value < 0 || value >= self->priv->surfaces->len)
		return;

	sd = g_ptr_array_index (self->priv->surfaces, value);

	if ((x2 - x1) < sd->width)
		x = x1;
	else
		x = x1 + ((x2 - x1) - sd->width) / 2;

	if ((y2 - y1) < sd->height)
		y = y1;
	else
		y = y1 + ((y2 - y1) - sd->height) / 2;

	eti = E_TABLE_ITEM (ecell_view->e_table_item_view);
	if (eti)
		style_context = gtk_widget_get_style_context (GTK_WIDGET (GNOME_CANVAS_ITEM (eti)->canvas));
	else
		style_context = NULL;

	cairo_save (cr);
	if (style_context)
		gtk_render_icon_surface (style_context, cr, sd->surface, x, y);
	cairo_restore (cr);
}

static void
etog_set_value (ECellToggleView *toggle_view,
                gint model_col,
                gint view_col,
                gint row,
                gint value)
{
	ECellToggle *self;

	cell_toggle_ensure_icons (E_CELL_TOGGLE (toggle_view->cell_view.ecell), &(toggle_view->cell_view));

	self = E_CELL_TOGGLE (toggle_view->cell_view.ecell);

	if (value >= self->priv->surfaces->len)
		value = 0;

	e_table_model_set_value_at (
		toggle_view->cell_view.e_table_model,
		model_col, row, GINT_TO_POINTER (value));
}

static gint
cell_toggle_event (ECellView *ecell_view,
                   GdkEvent *event,
                   gint model_col,
                   gint view_col,
                   gint row,
                   ECellFlags flags,
                   ECellActions *actions)
{
	ECellToggleView *toggle_view = (ECellToggleView *) ecell_view;
	gpointer _value = e_table_model_value_at (
		ecell_view->e_table_model, model_col, row);
	const gint value = GPOINTER_TO_INT (_value);

	switch (event->type) {
	case GDK_KEY_PRESS:
		if (event->key.keyval != GDK_KEY_space)
			return FALSE;
		/* Fall through */
	case GDK_BUTTON_PRESS:
		if (!e_table_model_is_cell_editable (
			ecell_view->e_table_model, model_col, row))
			return FALSE;

		etog_set_value (
			toggle_view, model_col, view_col, row, value + 1);

		return TRUE;

	default:
		return FALSE;
	}
}

static gint
cell_toggle_height (ECellView *ecell_view,
                    gint model_col,
                    gint view_col,
                    gint row)
{
	ECellToggle *self = E_CELL_TOGGLE (ecell_view->ecell);

	return self->priv->height;
}

static void
cell_toggle_print (ECellView *ecell_view,
                   GtkPrintContext *context,
                   gint model_col,
                   gint view_col,
                   gint row,
                   gdouble width,
                   gdouble height)
{
	ECellToggle *self;
	SurfaceData *sd;
	gdouble image_width, image_height;
	const gint value = GPOINTER_TO_INT (
			e_table_model_value_at (ecell_view->e_table_model, model_col, row));

	cairo_t *cr;

	cell_toggle_ensure_icons (E_CELL_TOGGLE (ecell_view->ecell), ecell_view);

	self = E_CELL_TOGGLE (ecell_view->ecell);

	if (value >= self->priv->surfaces->len)
		return;

	sd = g_ptr_array_index (self->priv->surfaces, value);
	if (sd) {
		ETableItem *eti;
		GtkStyleContext *style_context;

		eti = E_TABLE_ITEM (ecell_view->e_table_item_view);
		if (eti)
			style_context = gtk_widget_get_style_context (GTK_WIDGET (GNOME_CANVAS_ITEM (eti)->canvas));
		else
			style_context = NULL;

		cr = gtk_print_context_get_cairo_context (context);
		cairo_save (cr);
		cairo_translate (cr, 0 , 0);

		image_width = (gdouble) sd->width;
		image_height = (gdouble) sd->height;
		cairo_rectangle (
			cr,
			image_width / 7,
			image_height / 3,
			image_width - image_width / 4,
			image_width - image_height / 7);
		cairo_clip (cr);
		if (style_context)
			gtk_render_icon_surface (style_context, cr, sd->surface, 0, image_height / 4);
		cairo_restore (cr);
	}
}

static gdouble
cell_toggle_print_height (ECellView *ecell_view,
                          GtkPrintContext *context,
                          gint model_col,
                          gint view_col,
                          gint row,
                          gdouble width)
{
	ECellToggle *self = E_CELL_TOGGLE (ecell_view->ecell);

	return self->priv->height;
}

static gint
cell_toggle_max_width (ECellView *ecell_view,
                       gint model_col,
                       gint view_col)
{
	ECellToggle *self;
	gint max_width = 0;
	gint number_of_rows;
	gint row;

	cell_toggle_ensure_icons (E_CELL_TOGGLE (ecell_view->ecell), ecell_view);

	self = E_CELL_TOGGLE (ecell_view->ecell);

	number_of_rows = e_table_model_row_count (ecell_view->e_table_model);
	for (row = 0; row < number_of_rows; row++) {
		SurfaceData *sd;
		gpointer value;

		value = e_table_model_value_at (ecell_view->e_table_model, model_col, row);
		sd = g_ptr_array_index (self->priv->surfaces, GPOINTER_TO_INT (value));

		max_width = MAX (max_width, sd->width);
	}

	return max_width;
}

static gchar *
cell_toggle_get_bg_color (ECellView *ecell_view,
			  gint row)
{
	ECellToggle *toggle = E_CELL_TOGGLE (ecell_view->ecell);
	gchar *color_spec, *bg_color;

	if (toggle->priv->bg_color_column == -1)
		return NULL;

	color_spec = e_table_model_value_at (
		ecell_view->e_table_model,
		toggle->priv->bg_color_column, row);

	bg_color = g_strdup (color_spec);

	if (color_spec)
		e_table_model_free_value (ecell_view->e_table_model, toggle->priv->bg_color_column, color_spec);

	return bg_color;
}

static void
cell_toggle_set_property (GObject *object,
			  guint property_id,
			  const GValue *value,
			  GParamSpec *pspec)
{
	ECellToggle *toggle;

	toggle = E_CELL_TOGGLE (object);

	switch (property_id) {
	case PROP_BG_COLOR_COLUMN:
		toggle->priv->bg_color_column = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
cell_toggle_get_property (GObject *object,
			  guint property_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	ECellToggle *toggle;

	toggle = E_CELL_TOGGLE (object);

	switch (property_id) {
	case PROP_BG_COLOR_COLUMN:
		g_value_set_int (value, toggle->priv->bg_color_column);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_cell_toggle_class_init (ECellToggleClass *class)
{
	GObjectClass *object_class;
	ECellClass *cell_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = cell_toggle_dispose;
	object_class->finalize = cell_toggle_finalize;
	object_class->set_property = cell_toggle_set_property;
	object_class->get_property = cell_toggle_get_property;

	cell_class = E_CELL_CLASS (class);
	cell_class->new_view = cell_toggle_new_view;
	cell_class->kill_view = cell_toggle_kill_view;
	cell_class->draw = cell_toggle_draw;
	cell_class->event = cell_toggle_event;
	cell_class->height = cell_toggle_height;
	cell_class->print = cell_toggle_print;
	cell_class->print_height = cell_toggle_print_height;
	cell_class->max_width = cell_toggle_max_width;
	cell_class->get_bg_color = cell_toggle_get_bg_color;

	g_object_class_install_property (
		object_class,
		PROP_BG_COLOR_COLUMN,
		g_param_spec_int (
			"bg-color-column",
			"BG Color Column",
			NULL,
			-1, G_MAXINT, -1,
			G_PARAM_READWRITE));

	gal_a11y_e_cell_registry_add_cell_type (
		NULL, E_TYPE_CELL_TOGGLE, gal_a11y_e_cell_toggle_new);
}

static void
e_cell_toggle_init (ECellToggle *cell_toggle)
{
	GError *error = NULL;

	cell_toggle->priv = e_cell_toggle_get_instance_private (cell_toggle);
	cell_toggle->priv->surfaces = g_ptr_array_new_with_free_func (surface_data_free);
	cell_toggle->priv->surface_scale = 0;
	cell_toggle->priv->bg_color_column = -1;
	cell_toggle->priv->empty = gdk_pixbuf_new_from_resource ("/org.gnome.Evolution/empty.svg", &error);
	if (!cell_toggle->priv->empty)
		g_warning ("%s: Failed to load 'empty.svg': %s", G_STRFUNC, error ? error->message : "Unknown error");
	g_clear_error (&error);
}

/**
 * e_cell_toggle_construct:
 * @cell_toggle: a fresh ECellToggle object
 * @icon_names: array of icon names, some of which may be %NULL
 * @n_icon_names: length of the @icon_names array
 *
 * Constructs the @cell_toggle object with the @icon_names and @n_icon_names
 * arguments.
 */
void
e_cell_toggle_construct (ECellToggle *cell_toggle,
                         const gchar **icon_names,
                         guint n_icon_names)
{
	guint ii;

	g_return_if_fail (E_IS_CELL_TOGGLE (cell_toggle));
	g_return_if_fail (icon_names != NULL);
	g_return_if_fail (n_icon_names > 0);

	cell_toggle->priv->icon_names = g_new (gchar *, n_icon_names);
	cell_toggle->priv->n_icon_names = n_icon_names;

	for (ii = 0; ii < n_icon_names; ii++)
		cell_toggle->priv->icon_names[ii] = g_strdup (icon_names[ii]);

	g_ptr_array_set_size (cell_toggle->priv->surfaces, 0);
}

/**
 * e_cell_toggle_new:
 * @icon_names: array of icon names, some of which may be %NULL
 * @n_icon_names: length of the @icon_names array
 *
 * Creates a new ECell renderer that can be used to render toggle
 * buttons with the icons specified in @icon_names.  The value returned
 * by ETableModel::get_value is typecast into an integer and clamped
 * to the [0..n_icon_names) range.  That will select the image rendered.
 *
 * %NULL elements in @icon_names will show no icon for the corresponding
 * integer value.
 *
 * Returns: an ECell object that can be used to render multi-state
 * toggle cells.
 */
ECell *
e_cell_toggle_new (const gchar **icon_names,
                   guint n_icon_names)
{
	ECellToggle *cell_toggle;

	g_return_val_if_fail (icon_names != NULL, NULL);
	g_return_val_if_fail (n_icon_names > 0, NULL);

	cell_toggle = g_object_new (E_TYPE_CELL_TOGGLE, NULL);
	e_cell_toggle_construct (cell_toggle, icon_names, n_icon_names);

	return (ECell *) cell_toggle;
}

void
e_cell_toggle_set_icon_descriptions (ECellToggle *cell_toggle,
				     const gchar **descriptions,
				     gint n_descriptions)
{
	gint ii;
	gint n_icon_names;

	g_return_if_fail (E_IS_CELL_TOGGLE (cell_toggle));
	g_return_if_fail (cell_toggle->priv->icon_descriptions == NULL);
	g_return_if_fail (n_descriptions == cell_toggle->priv->n_icon_names);

	n_icon_names = cell_toggle->priv->n_icon_names;

	cell_toggle->priv->icon_descriptions = g_new (gchar *, n_icon_names);

	for (ii = 0; ii < n_icon_names; ii++)
		cell_toggle->priv->icon_descriptions[ii] = g_strdup (descriptions[ii]);
}

const gchar *
e_cell_toggle_get_icon_description (ECellToggle *cell_toggle,
				    gint n)
{
	if (n < 0 || n >= cell_toggle->priv->n_icon_names)
		return NULL;

	if (!cell_toggle->priv->icon_descriptions)
		return NULL;

	return cell_toggle->priv->icon_descriptions[n];
}
