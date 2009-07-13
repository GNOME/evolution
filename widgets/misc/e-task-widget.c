/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-task-widget.h"
#include "e-spinner.h"

#include <glib/gi18n.h>

#define SPACING 2

struct _ETaskWidgetPrivate {
	gchar *component_id;

	GtkWidget *label;
	GtkWidget *box;
	GtkWidget *image;

	void (*cancel_func) (gpointer data);
	gpointer data;
};

G_DEFINE_TYPE (ETaskWidget, e_task_widget, GTK_TYPE_EVENT_BOX)

/* GObject methods.  */

static void
impl_finalize (GObject *object)
{
	ETaskWidget *task_widget;
	ETaskWidgetPrivate *priv;

	task_widget = E_TASK_WIDGET (object);
	priv = task_widget->priv;

	g_free (priv->component_id);
	g_free (priv);

	(* G_OBJECT_CLASS (e_task_widget_parent_class)->finalize) (object);
}


static void
e_task_widget_class_init (ETaskWidgetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = impl_finalize;
}

static void
e_task_widget_init (ETaskWidget *task_widget)
{
	ETaskWidgetPrivate *priv;

	priv = g_new (ETaskWidgetPrivate, 1);

	priv->component_id = NULL;
	priv->label        = NULL;
	priv->image        = NULL;
	priv->box	   = NULL;

	task_widget->priv = priv;
	task_widget->id = 0;
}

static gboolean
button_press_event_cb (GtkWidget *w, gpointer data)
{
	ETaskWidget *tw = (ETaskWidget *) data;
	ETaskWidgetPrivate *priv = tw->priv;

	priv->cancel_func (priv->data);

	return TRUE;
}

static gboolean
prepare_popup (ETaskWidget *widget, GdkEventButton *event)
{
	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	if (event->button != 3)
		return FALSE;

	/* FIXME: Implement Cancel */

	return TRUE;
}


void
e_task_widget_construct (ETaskWidget *task_widget,
			 const gchar *component_id,
			 const gchar *information,
			 void (*cancel_func) (gpointer data),
			 gpointer data)
{
	ETaskWidgetPrivate *priv;
	GtkWidget *box;
	GtkWidget *frame;

	g_return_if_fail (task_widget != NULL);
	g_return_if_fail (E_IS_TASK_WIDGET (task_widget));
	g_return_if_fail (component_id != NULL);
	g_return_if_fail (information != NULL);

	priv = task_widget->priv;

	priv->component_id = g_strdup (component_id);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (task_widget), frame);
	gtk_widget_show (frame);

	box = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_widget_show (box);

	gtk_widget_set_size_request (box, 1, -1);

	priv->box = gtk_hbox_new (FALSE, 0);
	priv->image = e_spinner_new_spinning_small_shown ();
	gtk_widget_show (priv->box);
	gtk_box_pack_start (GTK_BOX (priv->box), priv->image, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box), priv->box, FALSE, TRUE, 0);
	priv->label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.5);
	gtk_widget_show (priv->label);
	gtk_box_pack_start (GTK_BOX (box), priv->label, TRUE, TRUE, 0);
	if (cancel_func) {
		GdkPixbuf *pixbuf;
		GtkWidget *image;
		GtkWidget *tool;

		pixbuf = gtk_icon_theme_load_icon (
			gtk_icon_theme_get_default (),
			"gtk-stop", 16, 0, NULL);
		image = gtk_image_new_from_pixbuf (pixbuf);
		g_object_unref (pixbuf);

		tool = (GtkWidget *) gtk_tool_button_new (image, NULL);
		gtk_box_pack_end (GTK_BOX (box), tool, FALSE, TRUE, 0);
		gtk_widget_show_all (tool);

		gtk_widget_set_sensitive (tool, cancel_func != NULL);
		priv->cancel_func = cancel_func;
		priv->data = data;
		g_signal_connect (tool, "clicked",  G_CALLBACK (button_press_event_cb), task_widget);
		g_signal_connect (task_widget, "button-press-event", G_CALLBACK (prepare_popup), task_widget);

	}

	e_task_widget_update (task_widget, information, -1.0);
}

GtkWidget *
e_task_widget_new_with_cancel (const gchar *component_id,
                               const gchar *information,
                               void (*cancel_func) (gpointer data),
                               gpointer data)
{
	ETaskWidget *task_widget;

	g_return_val_if_fail (information != NULL, NULL);

	task_widget = g_object_new (e_task_widget_get_type (), NULL);
	e_task_widget_construct (task_widget, component_id, information, cancel_func, data);

	return GTK_WIDGET (task_widget);
}

GtkWidget *
e_task_widget_new (const gchar *component_id,
		   const gchar *information)
{
	ETaskWidget *task_widget;

	g_return_val_if_fail (information != NULL, NULL);

	task_widget = g_object_new (e_task_widget_get_type (), NULL);
	e_task_widget_construct (task_widget, component_id, information, NULL, NULL);

	return GTK_WIDGET (task_widget);
}

GtkWidget *
e_task_widget_update_image (ETaskWidget *task_widget,
			    const gchar *stock, const gchar *text)
{
	GtkWidget *image, *tool;
        GdkPixbuf *pixbuf;

        pixbuf = gtk_icon_theme_load_icon (
                gtk_icon_theme_get_default (),
                stock, 16, 0, NULL);
        image = gtk_image_new_from_pixbuf (pixbuf);
        g_object_unref (pixbuf);

	tool = (GtkWidget *) gtk_tool_button_new (image, NULL);
	gtk_box_pack_start (GTK_BOX(task_widget->priv->box), tool, FALSE, TRUE, 0);
	gtk_widget_show_all (task_widget->priv->box);
	gtk_widget_hide (task_widget->priv->image);
	task_widget->priv->image = image;
	gtk_label_set_text (GTK_LABEL (task_widget->priv->label), text);

	return tool;
}


void
e_task_widget_update (ETaskWidget *task_widget,
		      const gchar *information,
		      double completion)
{
	ETaskWidgetPrivate *priv;
	gchar *text;

	g_return_if_fail (task_widget != NULL);
	g_return_if_fail (E_IS_TASK_WIDGET (task_widget));
	g_return_if_fail (information != NULL);

	priv = task_widget->priv;

	if (completion < 0.0) {
		/* For Translator only: %s is status message that is displayed (eg "moving items", "updating objects") */
		text = g_strdup_printf (_("%s (...)"), information);
	} else {
		gint percent_complete;
		percent_complete = (gint) (completion * 100.0 + .5);
		/* For Translator only: %s is status message that is displayed (eg "moving items", "updating objects");
		   %d is a number between 0 and 100, describing the percentage of operation complete */
		text = g_strdup_printf (_("%s (%d%% complete)"), information, percent_complete);
	}

	gtk_label_set_text (GTK_LABEL (priv->label), text);

	gtk_widget_set_tooltip_text (GTK_WIDGET (task_widget), text);

	g_free (text);
}

void
e_task_wiget_alert (ETaskWidget *task_widget)
{
	g_return_if_fail (task_widget != NULL);
	g_return_if_fail (E_IS_TASK_WIDGET (task_widget));
}

void
e_task_wiget_unalert (ETaskWidget *task_widget)
{
	g_return_if_fail (task_widget != NULL);
	g_return_if_fail (E_IS_TASK_WIDGET (task_widget));
}


const gchar *
e_task_widget_get_component_id  (ETaskWidget *task_widget)
{
	g_return_val_if_fail (task_widget != NULL, NULL);
	g_return_val_if_fail (E_IS_TASK_WIDGET (task_widget), NULL);

	return task_widget->priv->component_id;
}

