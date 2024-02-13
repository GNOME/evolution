
/* e-color-chooser-widget.c
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

#include "e-color-chooser-widget.h"

#include <glib/gi18n-lib.h>

/**
 * EColorChooserWidget:
 *
 * This widget wrapps around #GtkColorChooserWidget and allows the widget to be
 * used as a delegate for #GtkComboBox for instance.
 */

struct _EColorChooserWidgetPrivate {
	gboolean showing_editor;
};

enum {
	SIGNAL_EDITOR_ACTIVATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EColorChooserWidget, e_color_chooser_widget, GTK_TYPE_COLOR_CHOOSER_WIDGET);

static gboolean (* origin_swatch_button_press_event) (GtkWidget *widget, GdkEventButton *event);

/* UGLY UGLY UGLY!
 * GtkColorChooserWidget sends "color-activated" signal
 * only when user double-clicks the color. This is totally stupid
 * and since we want to use it in a combobox-like widget, we need
 * to be notified upon single click (which by default only selects the color).
 *
 * Unfortunatelly the GtkColorSwatch widget, which handles the button-press
 * event is a non-public widget embedded within the GtkColorChooserWidget,
 * so we can't just subclass it and fix the behavior.
 *
 * Here we override button_press_event of the GtkColorSwatch and manually
 * emit the 'activate' signal on single click. This is stupid, ugly and I
 * want to punch someone for such a stupid design...
 */
static gboolean
color_chooser_widget_button_press_event (GtkWidget *widget,
                                         GdkEventButton *event)
{
	GtkWidget *parent;

	g_return_val_if_fail (origin_swatch_button_press_event != NULL, FALSE);

	/* Override the behaviour only for GtkColorSwatch which is part of the EColorChooserWidget */
	parent = widget;
	while (parent && !E_IS_COLOR_CHOOSER_WIDGET (parent))
		parent = gtk_widget_get_parent (parent);

	if (parent &&
	    event->type == GDK_BUTTON_PRESS &&
	    event->button == GDK_BUTTON_PRIMARY) {
		g_signal_emit_by_name (widget, "activate");
		return TRUE;
	}

	return origin_swatch_button_press_event (widget, event);
}

static void
color_chooser_widget_color_activated (GtkColorChooser *chooser,
                                      GdkRGBA *color,
                                      gpointer user_data)
{
	/* Because we are simulating the double-click by accepting only
	 * single click, the color in the swatch is actually not selected,
	 * so we must do it manually */
	gtk_color_chooser_set_rgba (chooser, color);
}

static gboolean
run_color_chooser_dialog (gpointer user_data)
{
	EColorChooserWidget *self;
	GtkWidget *parent_window;
	GtkWidget *parent_chooser;
	GtkWidget *dialog;
	GtkWidget *chooser;

	parent_chooser = user_data;

	g_object_set (
		G_OBJECT (parent_chooser), "show-editor", FALSE, NULL);

	parent_window = g_object_get_data (G_OBJECT (parent_chooser), "window");
	if (!GTK_IS_WINDOW (parent_window))
		parent_window = gtk_widget_get_toplevel (parent_chooser);
	dialog = gtk_dialog_new_with_buttons (
		N_("Choose custom color"),
		GTK_WINDOW (parent_window),
		GTK_DIALOG_MODAL,
		_("_Cancel"), GTK_RESPONSE_REJECT,
		_("_OK"), GTK_RESPONSE_ACCEPT, NULL);

	chooser = gtk_color_chooser_widget_new ();
	g_object_set (G_OBJECT (chooser), "show-editor", TRUE, NULL);
	gtk_box_pack_start (
		GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
		chooser, TRUE, TRUE, 5);

	gtk_widget_show_all (chooser);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		GdkRGBA color;

		gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (chooser), &color);
		gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (parent_chooser), &color);

		g_signal_emit_by_name (parent_chooser, "color-activated", &color);
	}

	gtk_widget_destroy (dialog);

	self = E_COLOR_CHOOSER_WIDGET (parent_chooser);
	self->priv->showing_editor = FALSE;

	return FALSE;
}

static void
color_chooser_show_editor_notify_cb (EColorChooserWidget *chooser,
                                     GParamSpec *pspec,
                                     gpointer user_data)
{
	gboolean show_editor;

	g_object_get (G_OBJECT (chooser), "show-editor", &show_editor, NULL);

	/* Nothing to do here... */
	if ((show_editor == FALSE) || (chooser->priv->showing_editor == TRUE))
		return;

	chooser->priv->showing_editor = TRUE;

	/* Hide the editor - we don't want to display the single-color editor
	 * within this widget. We rather create a dialog window with the editor
	 * (we can't do it from this callback as Gtk would stop it in order to
	 * prevent endless recursion probably) */
	g_idle_add (run_color_chooser_dialog, chooser);
	g_signal_emit (chooser, signals[SIGNAL_EDITOR_ACTIVATED], 0);
}

void
e_color_chooser_widget_class_init (EColorChooserWidgetClass *class)
{
	signals[SIGNAL_EDITOR_ACTIVATED] = g_signal_new (
		"editor-activated",
		E_TYPE_COLOR_CHOOSER_WIDGET,
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EColorChooserWidgetClass, editor_activated),
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

/* Recursively go through GtkContainers within the GtkColorChooserWidget
 * and try to find GtkColorSwatch widget. */
static GtkWidget *
find_swatch (GtkContainer *container)
{
	GList *children, *child;

	children = gtk_container_get_children (container);
	for (child = children; child; child = g_list_next (child)) {
		GtkWidget *widget = child->data;
		GtkWidget *swatch;

		if (!widget)
			continue;

		if (GTK_IS_CONTAINER (widget)) {
			swatch = find_swatch (GTK_CONTAINER (widget));

			if (swatch != NULL) {
				g_list_free (children);
				return swatch;
			}
		}

		if (g_strcmp0 (G_OBJECT_TYPE_NAME (widget), "GtkColorSwatch") == 0) {
			g_list_free (children);
			return widget;
		}
	}

	g_list_free (children);

	return NULL;
}

void
e_color_chooser_widget_init (EColorChooserWidget *widget)
{
	GtkWidget *swatch;

	widget->priv = e_color_chooser_widget_get_instance_private (widget);
	widget->priv->showing_editor = FALSE;

	swatch = find_swatch (GTK_CONTAINER (widget));

	/* If swatch is NULL then GTK changed something and this widget
	 * becomes broken... */
	g_return_if_fail (swatch != NULL);

	if (swatch) {
		GtkWidgetClass *swatch_class;
		swatch_class = GTK_WIDGET_GET_CLASS (swatch);
		if (swatch_class->button_press_event != color_chooser_widget_button_press_event) {
			origin_swatch_button_press_event = swatch_class->button_press_event;
			swatch_class->button_press_event = color_chooser_widget_button_press_event;
		}
	}

	g_signal_connect (
		widget, "color-activated",
		G_CALLBACK (color_chooser_widget_color_activated), NULL);

	g_signal_connect (
		widget, "notify::show-editor",
		G_CALLBACK (color_chooser_show_editor_notify_cb), NULL);
}

GtkWidget *
e_color_chooser_widget_new (void)
{
	return g_object_new (
		E_TYPE_COLOR_CHOOSER_WIDGET,
		"show-editor", FALSE,
		"use-alpha", FALSE,
		NULL);
}
