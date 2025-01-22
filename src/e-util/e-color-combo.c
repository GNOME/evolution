/* e-color-combo.c
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "evolution-config.h"

#include "e-color-combo.h"
#include "e-color-chooser-widget.h"

#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include <cairo/cairo.h>

struct _EColorComboPrivate {
	GtkWidget *color_frame;		/* not referenced */

	GtkWidget *popover;
	GtkWidget *default_button;	/* not referenced */
	GtkWidget *chooser_widget;	/* not referenced */

	guint popup_shown	: 1;
	guint popup_in_progress : 1;

	guint default_transparent: 1;
	GdkRGBA *current_color;
	GdkRGBA *default_color;

	GList *palette;
};

enum {
	PROP_0,
	PROP_CURRENT_COLOR,
	PROP_DEFAULT_COLOR,
	PROP_DEFAULT_LABEL,
	PROP_DEFAULT_TRANSPARENT,
	PROP_PALETTE,
	PROP_POPUP_SHOWN
};

enum {
	ACTIVATED,
	POPUP,
	POPDOWN,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static GdkRGBA black = { 0, 0, 0, 1 };

G_DEFINE_TYPE_WITH_PRIVATE (EColorCombo, e_color_combo, GTK_TYPE_BUTTON);

static void
color_combo_popup (EColorCombo *combo)
{
	if (!gtk_widget_get_realized (GTK_WIDGET (combo)))
		return;

	if (combo->priv->popup_shown)
		return;

	/* Always make sure the editor-mode is OFF */
	g_object_set (
		G_OBJECT (combo->priv->chooser_widget),
		"show-editor", FALSE, NULL);

	/* Show the pop-up. */
	gtk_widget_show_all (combo->priv->popover);
	gtk_widget_grab_focus (combo->priv->chooser_widget);
}

static void
color_combo_popdown (EColorCombo *combo)
{
	if (!gtk_widget_get_realized (GTK_WIDGET (combo)))
		return;

	if (!combo->priv->popup_shown)
		return;

	gtk_widget_hide (combo->priv->popover);
}

static gboolean
color_combo_window_button_press_event_cb (EColorCombo *combo,
                                          GdkEvent *event,
                                          gpointer user_data)
{
	GtkWidget *event_widget;

	event_widget = gtk_get_event_widget ((GdkEvent *) event);

	if (event_widget == combo->priv->popover)
		return TRUE;

	if (combo->priv->popup_shown)
		return FALSE;

	combo->priv->popup_in_progress = TRUE;
	color_combo_popup (combo);

	return TRUE;
}

static gboolean
color_combo_window_button_release_event_cb (EColorCombo *combo,
                                            GdkEvent *event,
                                            gpointer user_data)
{
	gboolean popup_in_progress;

	popup_in_progress = combo->priv->popup_in_progress;
	combo->priv->popup_in_progress = FALSE;

	if (popup_in_progress)
		return FALSE;

	if (combo->priv->popup_shown)
		goto popdown;

	return FALSE;

popdown:
	color_combo_popdown (combo);

	return TRUE;
}

static void
color_combo_child_show_cb (EColorCombo *combo)
{
	combo->priv->popup_shown = TRUE;
	g_object_notify (G_OBJECT (combo), "popup-shown");
}

static void
color_combo_child_hide_cb (EColorCombo *combo)
{
	combo->priv->popup_shown = FALSE;
	g_object_notify (G_OBJECT (combo), "popup-shown");
}

static void
color_combo_get_preferred_width (GtkWidget *widget,
                                 gint *min_width,
                                 gint *natural_width)
{
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (e_color_combo_parent_class);
	widget_class->get_preferred_width (widget, min_width, natural_width);

	/* Make sure the box with color sample is always visible */
	if (min_width)
		*min_width += 20;

	if (natural_width)
		*natural_width += 20;
}

static gboolean
color_combo_button_press_event_cb (GtkWidget *widget,
                                   GdkEventButton *event)
{
	EColorCombo *combo = E_COLOR_COMBO (widget);
	GdkWindow *window;
	gint x, y, width, height;

	window = gtk_widget_get_window (combo->priv->color_frame);
	gdk_window_get_position (window, &x, &y);
	/* Width - only width of the frame with color box */
	width = gtk_widget_get_allocated_width (combo->priv->color_frame);

	/* Height - height of the entire button (widget) */
	height = gtk_widget_get_allocated_height (widget);

	/* Check whether user clicked on the color frame - in such case
	 * apply the color immediatelly without displaying the popup widget */
	if ((event->x_root >= x) && (event->x_root <= x + width) &&
	    (event->y_root >= y) && (event->y_root <= y + height)) {
		GdkRGBA color;

		e_color_combo_get_current_color (combo, &color);
		g_signal_emit (combo, signals[ACTIVATED], 0, &color);

		return TRUE;
	}

	/* Otherwise display the popup widget */
	if (combo->priv->popup_shown) {
		color_combo_popdown (combo);
	} else {
		combo->priv->popup_in_progress = TRUE;
		color_combo_popup (combo);
	}

	return FALSE;
}

static void
color_combo_swatch_color_changed (EColorCombo *combo,
                                  GdkRGBA *color,
                                  gpointer user_data)
{
	g_signal_emit (combo, signals[ACTIVATED], 0, color);

	e_color_combo_set_current_color (combo, color);

	color_combo_popdown (combo);
}

static void
draw_transparent_graphic (cairo_t *cr,
                          gint width,
                          gint height)
{
	gint ii, step, x_offset, y_offset;

	step = height / 2;
	x_offset = width % step;
	y_offset = height % step;

	for (ii = 0; ii < width; ii += step) {
		if (ii % 2)
			cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		else
			cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);

		if (ii + step >= width)
			cairo_rectangle (cr, ii, 0, step + x_offset, step);
		else
			cairo_rectangle (cr, ii, 0, step, step);

		cairo_fill (cr);

		if (ii % 2)
			cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);
		else
			cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);

		if (ii + step >= width)
			cairo_rectangle (cr, ii, step, step + x_offset, step + y_offset);
		else
			cairo_rectangle (cr, ii, step, step, step + y_offset);

		cairo_fill (cr);
	}
}

static void
color_combo_draw_frame_cb (GtkWidget *widget,
                           cairo_t *cr,
                           gpointer user_data)
{
	EColorCombo *combo = user_data;
	GdkRGBA rgba;
	GtkAllocation allocation;
	gint height, width;

	e_color_combo_get_current_color (combo, &rgba);

	gtk_widget_get_allocation (widget, &allocation);
	width = allocation.width;
	height = allocation.height;

	if (rgba.alpha == 0) {
		draw_transparent_graphic (cr, width, height);
	} else {
		cairo_set_source_rgba (cr, rgba.red, rgba.green, rgba.blue, rgba.alpha);
		cairo_rectangle (cr, 0, 0, width, height);
		cairo_fill (cr);
	}
}

static void
color_combo_set_default_color_cb (EColorCombo *combo,
                                  gpointer user_data)
{
	GdkRGBA color;

	e_color_combo_get_default_color (combo, &color);
	e_color_combo_set_current_color (combo, &color);

	g_signal_emit (combo, signals[ACTIVATED], 0, &color);
}

static void
color_combo_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_COLOR:
			e_color_combo_set_current_color (
				E_COLOR_COMBO (object),
				g_value_get_boxed (value));
			return;

		case PROP_DEFAULT_COLOR:
			e_color_combo_set_default_color (
				E_COLOR_COMBO (object),
				g_value_get_boxed (value));
			return;

		case PROP_DEFAULT_LABEL:
			e_color_combo_set_default_label (
				E_COLOR_COMBO (object),
				g_value_get_string (value));
			return;

		case PROP_DEFAULT_TRANSPARENT:
			e_color_combo_set_default_transparent (
				E_COLOR_COMBO (object),
				g_value_get_boolean (value));
			return;

		case PROP_PALETTE:
			e_color_combo_set_palette (
				E_COLOR_COMBO (object),
				g_value_get_pointer (value));
			return;

		case PROP_POPUP_SHOWN:
			if (g_value_get_boolean (value))
				e_color_combo_popup (
					E_COLOR_COMBO (object));
			else
				e_color_combo_popdown (
					E_COLOR_COMBO (object));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
color_combo_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	EColorCombo *self = E_COLOR_COMBO (object);
	GdkRGBA color;

	switch (property_id) {
		case PROP_CURRENT_COLOR:
			e_color_combo_get_current_color (self, &color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_DEFAULT_COLOR:
			e_color_combo_get_default_color (self, &color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_DEFAULT_LABEL:
			g_value_set_string (value, e_color_combo_get_default_label (self));
			return;

		case PROP_DEFAULT_TRANSPARENT:
			g_value_set_boolean (value, e_color_combo_get_default_transparent (self));
			return;

		case PROP_PALETTE:
			g_value_set_pointer (value, e_color_combo_get_palette (self));
			return;

		case PROP_POPUP_SHOWN:
			g_value_set_boolean (value, self->priv->popup_shown);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
color_combo_dispose (GObject *object)
{
	EColorCombo *self = E_COLOR_COMBO (object);

	g_clear_pointer (&self->priv->popover, gtk_widget_destroy);
	g_clear_pointer (&self->priv->current_color, gdk_rgba_free);
	g_clear_pointer (&self->priv->default_color, gdk_rgba_free);

	g_list_free_full (self->priv->palette, (GDestroyNotify) gdk_rgba_free);
	self->priv->palette = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_color_combo_parent_class)->dispose (object);
}

static void
e_color_combo_class_init (EColorComboClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = color_combo_set_property;
	object_class->get_property = color_combo_get_property;
	object_class->dispose = color_combo_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_preferred_width = color_combo_get_preferred_width;
	widget_class->button_press_event = color_combo_button_press_event_cb;

	class->popup = color_combo_popup;
	class->popdown = color_combo_popdown;

	g_object_class_install_property (
		object_class,
		PROP_CURRENT_COLOR,
		g_param_spec_boxed (
			"current-color",
			"Current color",
			"The currently selected color",
			GDK_TYPE_RGBA,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_COLOR,
		g_param_spec_boxed (
			"default-color",
			"Default color",
			"The color associated with the default button",
			GDK_TYPE_RGBA,
			G_PARAM_CONSTRUCT |
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_LABEL,
		g_param_spec_string (
			"default-label",
			"Default label",
			"The label for the default button",
			_("Default"),
			G_PARAM_CONSTRUCT |
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_TRANSPARENT,
		g_param_spec_boolean (
			"default-transparent",
			"Default is transparent",
			"Whether the default color is transparent",
			FALSE,
			G_PARAM_CONSTRUCT |
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PALETTE,
		g_param_spec_pointer (
			"palette",
			"Color palette",
			"Custom color palette",
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_POPUP_SHOWN,
		g_param_spec_boolean (
			"popup-shown",
			"Popup shown",
			"Whether the combo's dropdown is shown",
			FALSE,
			G_PARAM_READWRITE));

	signals[ACTIVATED] = g_signal_new (
		"activated",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EColorComboClass, activated),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[POPUP] = g_signal_new (
		"popup",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EColorComboClass, popup),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[POPDOWN] = g_signal_new (
		"popdown",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EColorComboClass, popdown),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_KEY_Down, GDK_MOD1_MASK, "popup", 0);
	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_KEY_KP_Down, GDK_MOD1_MASK, "popup", 0);

	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_KEY_Up, GDK_MOD1_MASK, "popdown", 0);
	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_KEY_KP_Up, GDK_MOD1_MASK, "popdown", 0);
	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_KEY_Escape, 0, "popdown", 0);
}

static void
e_color_combo_init (EColorCombo *combo)
{
	GtkWidget *container;
	GtkWidget *widget;

	combo->priv = e_color_combo_get_instance_private (combo);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_add (GTK_CONTAINER (combo), widget);

	container = widget;

	/* Build the combo button. */
	widget = gtk_frame_new (NULL);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	g_signal_connect (
		widget, "draw",
		G_CALLBACK (color_combo_draw_frame_cb), combo);
	combo->priv->color_frame = widget;  /* do not reference */

	widget = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);

	widget = gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);

	gtk_widget_show_all (container);

	/* Build the drop-down menu */
	widget = gtk_popover_new (GTK_WIDGET (combo));
	gtk_popover_set_position (GTK_POPOVER (widget), GTK_POS_BOTTOM);
	gtk_popover_set_modal (GTK_POPOVER (widget), TRUE);
	combo->priv->popover = g_object_ref_sink (widget);

	g_signal_connect_swapped (
		widget, "show",
		G_CALLBACK (color_combo_child_show_cb), combo);
	g_signal_connect_swapped (
		widget, "hide",
		G_CALLBACK (color_combo_child_hide_cb), combo);
	g_signal_connect_swapped (
		widget, "button-press-event",
		G_CALLBACK (color_combo_window_button_press_event_cb), combo);
	g_signal_connect_swapped (
		widget, "button-release-event",
		G_CALLBACK (color_combo_window_button_release_event_cb), combo);

	container = widget;

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 5);
	gtk_container_add (GTK_CONTAINER (container), widget);

	container = widget;

	widget = gtk_button_new ();
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	combo->priv->default_button = widget;  /* do not reference */

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (color_combo_set_default_color_cb), combo);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (color_combo_popdown), combo);

	widget = e_color_chooser_widget_new ();
	g_object_set_data (G_OBJECT (widget), "window", combo->priv->popover);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	combo->priv->chooser_widget = widget;  /* do not reference */

	g_signal_connect_swapped (
		widget, "color-activated",
		G_CALLBACK (color_combo_swatch_color_changed), combo);
	g_signal_connect_swapped (
		widget, "editor-activated",
		G_CALLBACK (color_combo_popdown), combo);

	combo->priv->current_color = gdk_rgba_copy (&black);
	combo->priv->default_color = gdk_rgba_copy (&black);
}

GtkWidget *
e_color_combo_new (void)
{
	return g_object_new (E_TYPE_COLOR_COMBO, NULL);
}

GtkWidget *
e_color_combo_new_defaults (GdkRGBA *default_color,
                            const gchar *default_label)
{
	g_return_val_if_fail (default_color != NULL, NULL);
	g_return_val_if_fail (default_label != NULL, NULL);

	return g_object_new (
		E_TYPE_COLOR_COMBO,
		"default-color", default_color,
		"default-label", default_label,
		NULL);
}

void
e_color_combo_popup (EColorCombo *combo)
{
	g_return_if_fail (E_IS_COLOR_COMBO (combo));

	g_signal_emit (combo, signals[POPUP], 0);
}

void
e_color_combo_popdown (EColorCombo *combo)
{
	g_return_if_fail (E_IS_COLOR_COMBO (combo));

	g_signal_emit (combo, signals[POPDOWN], 0);
}

void
e_color_combo_get_current_color (EColorCombo *combo,
                                 GdkRGBA *color)
{
	g_return_if_fail (E_IS_COLOR_COMBO (combo));
	g_return_if_fail (color != NULL);

	color->red = combo->priv->current_color->red;
	color->green = combo->priv->current_color->green;
	color->blue = combo->priv->current_color->blue;
	color->alpha = combo->priv->current_color->alpha;
}

void
e_color_combo_set_current_color (EColorCombo *combo,
                                 const GdkRGBA *color)
{
	g_return_if_fail (E_IS_COLOR_COMBO (combo));

	if (color == NULL)
		color = &black;

	if (combo->priv->current_color) {

		if (gdk_rgba_equal (color, combo->priv->current_color)) {
			return;
		}

		gdk_rgba_free (combo->priv->current_color);
	}

	combo->priv->current_color = gdk_rgba_copy (color);

	gtk_color_chooser_set_rgba (
		GTK_COLOR_CHOOSER (combo->priv->chooser_widget), color);
	gtk_widget_queue_draw (combo->priv->color_frame);

	g_object_notify (G_OBJECT (combo), "current-color");
}

void
e_color_combo_get_default_color (EColorCombo *combo,
                                 GdkRGBA *color)
{
	g_return_if_fail (E_IS_COLOR_COMBO (combo));
	g_return_if_fail (color != NULL);

	color->red = combo->priv->default_color->red;
	color->green = combo->priv->default_color->green;
	color->blue = combo->priv->default_color->blue;
	color->alpha = combo->priv->default_color->alpha;
}

void
e_color_combo_set_default_color (EColorCombo *combo,
                                 const GdkRGBA *color)
{
	g_return_if_fail (E_IS_COLOR_COMBO (combo));

	if (color == NULL)
		color = &black;

	if (combo->priv->default_color) {
		if (gdk_rgba_equal (color, combo->priv->default_color))
			return;

		gdk_rgba_free (combo->priv->default_color);
	}
	combo->priv->default_color = gdk_rgba_copy (color);

	gtk_color_chooser_set_rgba (
		GTK_COLOR_CHOOSER (combo->priv->chooser_widget), color);

	g_object_notify (G_OBJECT (combo), "default-color");
}

const gchar *
e_color_combo_get_default_label (EColorCombo *combo)
{
	g_return_val_if_fail (E_IS_COLOR_COMBO (combo), NULL);

	return gtk_button_get_label (GTK_BUTTON (combo->priv->default_button));
}

void
e_color_combo_set_default_label (EColorCombo *combo,
                                 const gchar *text)
{
	g_return_if_fail (E_IS_COLOR_COMBO (combo));

	gtk_button_set_label (GTK_BUTTON (combo->priv->default_button), text);

	g_object_notify (G_OBJECT (combo), "default-label");
}

gboolean
e_color_combo_get_default_transparent (EColorCombo *combo)
{
	g_return_val_if_fail (E_IS_COLOR_COMBO (combo), FALSE);

	return combo->priv->default_transparent;
}

void
e_color_combo_set_default_transparent (EColorCombo *combo,
                                       gboolean transparent)
{
	g_return_if_fail (E_IS_COLOR_COMBO (combo));

	combo->priv->default_transparent = transparent;
	if (transparent)
		combo->priv->default_color->alpha = 0;

	g_object_notify (G_OBJECT (combo), "default-transparent");
}

GList *
e_color_combo_get_palette (EColorCombo *combo)
{
	g_return_val_if_fail (E_IS_COLOR_COMBO (combo), NULL);

	return g_list_copy (combo->priv->palette);
}

void
e_color_combo_set_palette (EColorCombo *combo,
                           GList *palette)
{
	gint ii, count, colors_per_line;
	GList *iter;
	GdkRGBA *colors;

	g_return_if_fail (E_IS_COLOR_COMBO (combo));

	count = g_list_length (palette);
	colors_per_line = (count % 10 == 0) ? 10 : 9;

	colors = g_malloc_n (count, sizeof (GdkRGBA));
	g_list_free_full (combo->priv->palette, (GDestroyNotify) gdk_rgba_free);
	ii = 0;
	combo->priv->palette = NULL;
	for (iter = palette; iter; iter = g_list_next (iter)) {
		combo->priv->palette = g_list_prepend (
			combo->priv->palette, gdk_rgba_copy (iter->data));

		colors[ii] = *((GdkRGBA *) iter->data);
		ii++;
	}
	combo->priv->palette = g_list_reverse (combo->priv->palette);

	gtk_color_chooser_add_palette (
		GTK_COLOR_CHOOSER (combo->priv->chooser_widget),
		GTK_ORIENTATION_HORIZONTAL, 0, 0, NULL);
	gtk_color_chooser_add_palette (
		GTK_COLOR_CHOOSER (combo->priv->chooser_widget),
		GTK_ORIENTATION_HORIZONTAL, colors_per_line, count, colors);
	g_free (colors);
}
