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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-color-combo.h"
#include "e-color-chooser-widget.h"

#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include <cairo/cairo.h>

#define E_COLOR_COMBO_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_COLOR_COMBO, EColorComboPrivate))

struct _EColorComboPrivate {
	GtkWidget *color_frame;		/* not referenced */
	GtkWidget *arrow;		/* not referenced */

	GtkWidget *window;
	GtkWidget *default_button;	/* not referenced */
	GtkWidget *chooser_widget;	/* not referenced */

	guint popup_shown	: 1;
	guint popup_in_progress : 1;

	GdkRGBA *current_color;
	GdkRGBA *default_color;
	gint default_transparent: 1;

	GList *palette;

	GdkDevice *grab_keyboard;
	GdkDevice *grab_mouse;
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

static struct {
	const gchar *color;
	const gchar *tooltip;
} default_colors[] = {

	{ "#000000", N_("black") },
	{ "#993300", N_("light brown") },
	{ "#333300", N_("brown gold") },
	{ "#003300", N_("dark green #2") },
	{ "#003366", N_("navy") },
	{ "#000080", N_("dark blue") },
	{ "#333399", N_("purple #2") },
	{ "#333333", N_("very dark gray") },

	{ "#800000", N_("dark red") },
	{ "#FF6600", N_("red-orange") },
	{ "#808000", N_("gold") },
	{ "#008000", N_("dark green") },
	{ "#008080", N_("dull blue") },
	{ "#0000FF", N_("blue") },
	{ "#666699", N_("dull purple") },
	{ "#808080", N_("dark grey") },

	{ "#FF0000", N_("red") },
	{ "#FF9900", N_("orange") },
	{ "#99CC00", N_("lime") },
	{ "#339966", N_("dull green") },
	{ "#33CCCC", N_("dull blue #2") },
	{ "#3366FF", N_("sky blue #2") },
	{ "#800080", N_("purple") },
	{ "#969696", N_("gray") },

	{ "#FF00FF", N_("magenta") },
	{ "#FFCC00", N_("bright orange") },
	{ "#FFFF00", N_("yellow") },
	{ "#00FF00", N_("green") },
	{ "#00FFFF", N_("cyan") },
	{ "#00CCFF", N_("bright blue") },
	{ "#993366", N_("red purple") },
	{ "#C0C0C0", N_("light grey") },

	{ "#FF99CC", N_("pink") },
	{ "#FFCC99", N_("light orange") },
	{ "#FFFF99", N_("light yellow") },
	{ "#CCFFCC", N_("light green") },
	{ "#CCFFFF", N_("light cyan") },
	{ "#99CCFF", N_("light blue") },
	{ "#CC99FF", N_("light purple") },
	{ "#FFFFFF", N_("white") }
};

G_DEFINE_TYPE (
	EColorCombo,
	e_color_combo,
	GTK_TYPE_BUTTON);

static void
color_combo_reposition_window (EColorCombo *combo)
{
	GdkScreen *screen;
	GdkWindow *window;
	GdkRectangle monitor;
	GtkAllocation allocation;
	gint monitor_num;
	gint x, y, width, height;

	screen = gtk_widget_get_screen (GTK_WIDGET (combo));
	window = gtk_widget_get_window (GTK_WIDGET (combo));
	monitor_num = gdk_screen_get_monitor_at_window (screen, window);
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	gdk_window_get_origin (window, &x, &y);

	if (!gtk_widget_get_has_window (GTK_WIDGET (combo))) {
		gtk_widget_get_allocation (GTK_WIDGET (combo), &allocation);
		x += allocation.x;
		y += allocation.y;
	}

	gtk_widget_get_allocation (combo->priv->window, &allocation);
	width = allocation.width;
	height = allocation.height;

	x = CLAMP (x, monitor.x, monitor.x + monitor.width - width);
	y = CLAMP (y, monitor.y, monitor.y + monitor.height - height);

	gtk_window_move (GTK_WINDOW (combo->priv->window), x, y);
}

static void
color_combo_popup (EColorCombo *combo)
{
	GdkWindow *window;
	GtkWidget *toplevel;
	gboolean grab_status;
	GdkDevice *device, *mouse, *keyboard;
	guint32 activate_time;

	device = gtk_get_current_event_device ();
	g_return_if_fail (device != NULL);

	if (!gtk_widget_get_realized (GTK_WIDGET (combo)))
		return;

	if (combo->priv->popup_shown)
		return;

	activate_time = gtk_get_current_event_time ();
	if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD) {
		keyboard = device;
		mouse = gdk_device_get_associated_device (device);
	} else {
		keyboard = gdk_device_get_associated_device (device);
		mouse = device;
	}

	/* Position the window over the button. */
	color_combo_reposition_window (combo);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (combo));
	if (GTK_IS_WINDOW (toplevel))
		gtk_window_set_transient_for (GTK_WINDOW (combo->priv->window), GTK_WINDOW (toplevel));

	/* Try to grab the pointer and keyboard. */
	window = gtk_widget_get_window (toplevel);
	grab_status =
		(keyboard == NULL) ||
		(gdk_device_grab (
			keyboard, window,
			GDK_OWNERSHIP_WINDOW, TRUE,
			GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
			NULL, activate_time) == GDK_GRAB_SUCCESS);
	if (grab_status) {
		grab_status =
			(mouse == NULL) ||
			(gdk_device_grab (
				mouse, window,
				GDK_OWNERSHIP_WINDOW, TRUE,
				GDK_BUTTON_PRESS_MASK |
				GDK_BUTTON_RELEASE_MASK |
				GDK_POINTER_MOTION_MASK,
				NULL, activate_time) == GDK_GRAB_SUCCESS);
		if (!grab_status && keyboard)
			gdk_device_ungrab (keyboard, activate_time);
	}

	if (grab_status) {
		gtk_device_grab_add (combo->priv->window, mouse, TRUE);
		combo->priv->grab_keyboard = keyboard;
		combo->priv->grab_mouse = mouse;
	} else {
		gtk_widget_hide (combo->priv->window);
	}

	/* Always make sure the editor-mode is OFF */
	g_object_set (
		G_OBJECT (combo->priv->chooser_widget),
		"show-editor", FALSE, NULL);

	/* Show the pop-up. */
	gtk_widget_show_all (combo->priv->window);
	gtk_widget_grab_focus (combo->priv->window);
}

static void
color_combo_popdown (EColorCombo *combo)
{
	if (!gtk_widget_get_realized (GTK_WIDGET (combo)))
		return;

	if (!combo->priv->popup_shown)
		return;

	/* Hide the pop-up. */
	gtk_device_grab_remove (combo->priv->window, combo->priv->grab_mouse);
	gtk_widget_hide (combo->priv->window);

	if (combo->priv->grab_keyboard)
		gdk_device_ungrab (combo->priv->grab_keyboard, GDK_CURRENT_TIME);
	if (combo->priv->grab_mouse)
		gdk_device_ungrab (combo->priv->grab_mouse, GDK_CURRENT_TIME);

	combo->priv->grab_keyboard = NULL;
	combo->priv->grab_mouse = NULL;
}

static gboolean
color_combo_window_button_press_event_cb (EColorCombo *combo,
                                          GdkEvent *event,
                                          gpointer user_data)
{
	GtkWidget *event_widget;

	event_widget = gtk_get_event_widget ((GdkEvent *) event);

	if (event_widget == combo->priv->window)
		return TRUE;

	if (combo->priv->popup_shown == TRUE)
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
		cairo_set_source_rgb (cr, rgba.red, rgba.green, rgba.blue);
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
	e_color_combo_set_default_transparent (combo, (color.alpha == 0));

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
				g_value_get_object (value));
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
	EColorComboPrivate *priv;
	GdkRGBA color;

	priv = E_COLOR_COMBO_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_CURRENT_COLOR:
			e_color_combo_get_current_color (
				E_COLOR_COMBO (object), &color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_DEFAULT_COLOR:
			e_color_combo_get_default_color (
				E_COLOR_COMBO (object), &color);
			g_value_set_boxed (value, &color);
			return;

		case PROP_DEFAULT_LABEL:
			g_value_set_string (
				value, e_color_combo_get_default_label (
				E_COLOR_COMBO (object)));
			return;

		case PROP_DEFAULT_TRANSPARENT:
			g_value_set_boolean (
				value,
				e_color_combo_get_default_transparent (
				E_COLOR_COMBO (object)));
			return;

		case PROP_PALETTE:
			g_value_set_object (
				value, e_color_combo_get_palette (
				E_COLOR_COMBO (object)));
			return;

		case PROP_POPUP_SHOWN:
			g_value_set_boolean (value, priv->popup_shown);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
color_combo_dispose (GObject *object)
{
	EColorComboPrivate *priv;

	priv = E_COLOR_COMBO_GET_PRIVATE (object);

	if (priv->window != NULL) {
		gtk_widget_destroy (priv->window);
		priv->window = NULL;
	}

	if (priv->current_color != NULL) {
		gdk_rgba_free (priv->current_color);
		priv->current_color = NULL;
	}

	if (priv->default_color != NULL) {
		gdk_rgba_free (priv->default_color);
		priv->default_color = NULL;
	}

	g_list_free_full (priv->palette, (GDestroyNotify) gdk_rgba_free);
	priv->palette = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_color_combo_parent_class)->dispose (object);
}

static void
e_color_combo_class_init (EColorComboClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EColorComboPrivate));

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
	GtkWidget *toplevel;
	GtkWidget *widget;
	GList *palette;
	guint ii;

	combo->priv = E_COLOR_COMBO_GET_PRIVATE (combo);

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

	widget = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	combo->priv->arrow = widget;  /* do not reference */

	/* Build the drop-down menu */
	widget = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	gtk_window_set_resizable (GTK_WINDOW (widget), FALSE);
	gtk_window_set_type_hint (
		GTK_WINDOW (widget), GDK_WINDOW_TYPE_HINT_COMBO);
	combo->priv->window = g_object_ref_sink (widget);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (combo));
	if (GTK_IS_WINDOW (toplevel)) {
		gtk_window_group_add_window (
			gtk_window_get_group (GTK_WINDOW (toplevel)),
			GTK_WINDOW (widget));
		gtk_window_set_transient_for (
			GTK_WINDOW (widget), GTK_WINDOW (toplevel));
	}

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
	g_object_set_data (G_OBJECT (widget), "window", combo->priv->window);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	combo->priv->chooser_widget = widget;  /* do not reference */

	g_signal_connect_swapped (
		widget, "color-activated",
		G_CALLBACK (color_combo_swatch_color_changed), combo);
	g_signal_connect_swapped (
		widget, "editor-activated",
		G_CALLBACK (color_combo_popdown), combo);

	palette = NULL;
	for (ii = 0; ii < G_N_ELEMENTS (default_colors); ii++) {
		GdkRGBA *color = g_new0 (GdkRGBA, 1);
		gdk_rgba_parse (color, default_colors[ii].color);

		palette = g_list_prepend (palette, color);
	}
	palette = g_list_reverse (palette);
	e_color_combo_set_palette (combo, palette);
	g_list_free_full (palette, (GDestroyNotify) g_free);

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

	e_color_combo_set_default_transparent (combo, (color->alpha == 0));

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
