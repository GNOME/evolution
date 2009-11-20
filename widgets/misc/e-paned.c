/*
 * e-paned.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-paned.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#define E_PANED_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_PANED, EPanedPrivate))

struct _EPanedPrivate {
	gint hposition;
	gint vposition;

	gulong wse_handler_id;

	guint sync_position	: 1;
	guint toplevel_ready	: 1;
};

enum {
	PROP_0,
	PROP_HPOSITION,
	PROP_VPOSITION
};

static gpointer parent_class;

static gboolean
paned_window_state_event_cb (EPaned *paned,
                             GdkEventWindowState *event,
                             GtkWidget *toplevel)
{
	/* Wait for WITHDRAWN to change from 1 to 0. */
	if (!(event->changed_mask & GDK_WINDOW_STATE_WITHDRAWN))
		return FALSE;

	/* The whole point of this hack is to trap a point where if
	 * the window were to be maximized initially, the maximized
	 * allocation would already be negotiated.  We're there now.
	 * Set a flag so we know it's safe to set GtkPaned position. */
	paned->priv->toplevel_ready = TRUE;

	paned->priv->sync_position = TRUE;
	gtk_widget_queue_resize (GTK_WIDGET (paned));

	/* We don't need to listen for window state events anymore. */
	g_signal_handler_disconnect (toplevel, paned->priv->wse_handler_id);
	paned->priv->wse_handler_id = 0;

	return FALSE;
}

static void
paned_notify_orientation_cb (EPaned *paned)
{
	/* Ignore the next "notify::position" emission. */
	paned->priv->sync_position = TRUE;
	gtk_widget_queue_resize (GTK_WIDGET (paned));
}

static void
paned_notify_position_cb (EPaned *paned)
{
	GtkAllocation *allocation;
	GtkOrientable *orientable;
	GtkOrientation orientation;
	gint position;

	if (paned->priv->sync_position)
		return;

	orientable = GTK_ORIENTABLE (paned);
	orientation = gtk_orientable_get_orientation (orientable);

	allocation = &GTK_WIDGET (paned)->allocation;
	position = gtk_paned_get_position (GTK_PANED (paned));

	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		position = MAX (0, allocation->width - position);
		e_paned_set_hposition (paned, position);
	} else {
		position = MAX (0, allocation->height - position);
		e_paned_set_vposition (paned, position);
	}
}

static void
paned_set_property (GObject *object,
                    guint property_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_HPOSITION:
			e_paned_set_hposition (
				E_PANED (object),
				g_value_get_int (value));
			return;

		case PROP_VPOSITION:
			e_paned_set_vposition (
				E_PANED (object),
				g_value_get_int (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
paned_get_property (GObject *object,
                    guint property_id,
                    GValue *value,
                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_HPOSITION:
			g_value_set_int (
				value, e_paned_get_hposition (
				E_PANED (object)));
			return;

		case PROP_VPOSITION:
			g_value_set_int (
				value, e_paned_get_vposition (
				E_PANED (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
paned_realize (GtkWidget *widget)
{
	EPanedPrivate *priv;
	GtkWidget *toplevel;
	GdkWindowState state;
	GdkWindow *window;

	priv = E_PANED_GET_PRIVATE (widget);

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (parent_class)->realize (widget);

	/* XXX This would be easier if we could be notified of
	 *     window state events directly, but I can't seem
	 *     to make that happen. */

	toplevel = gtk_widget_get_toplevel (widget);
	window = gtk_widget_get_window (toplevel);
	state = gdk_window_get_state (window);

	/* If the window is withdrawn, wait for it to be shown before
	 * setting the pane position.  If the window is already shown,
	 * it's safe to set the pane position immediately. */
	if (state & GDK_WINDOW_STATE_WITHDRAWN)
		priv->wse_handler_id = g_signal_connect_swapped (
			toplevel, "window-state-event",
			G_CALLBACK (paned_window_state_event_cb), widget);
	else
		priv->toplevel_ready = TRUE;
}

static void
paned_size_allocate (GtkWidget *widget,
                     GtkAllocation *allocation)
{
	EPaned *paned = E_PANED (widget);
	GtkOrientable *orientable;
	GtkOrientation orientation;
	gint allocated;
	gint position;

	/* Chain up to parent's size_allocate() method. */
	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

	if (!paned->priv->toplevel_ready)
		return;

	if (!paned->priv->sync_position)
		return;

	orientable = GTK_ORIENTABLE (paned);
	orientation = gtk_orientable_get_orientation (orientable);

	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		allocated = allocation->width;
		position = e_paned_get_hposition (paned);
	} else {
		allocated = allocation->height;
		position = e_paned_get_vposition (paned);
	}

	position = MAX (0, allocated - position);
	gtk_paned_set_position (GTK_PANED (paned), position);

	paned->priv->sync_position = FALSE;
}

static void
paned_class_init (EPanedClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EPanedPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = paned_set_property;
	object_class->get_property = paned_get_property;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = paned_realize;
	widget_class->size_allocate = paned_size_allocate;

	g_object_class_install_property (
		object_class,
		PROP_HPOSITION,
		g_param_spec_int (
			"hposition",
			_("Horizontal Position"),
			_("Pane position when oriented horizontally"),
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_VPOSITION,
		g_param_spec_int (
			"vposition",
			_("Vertical Position"),
			_("Pane position when oriented vertically"),
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));
}

static void
paned_init (EPaned *paned)
{
	paned->priv = E_PANED_GET_PRIVATE (paned);

	g_signal_connect (
		paned, "notify::orientation",
		G_CALLBACK (paned_notify_orientation_cb), NULL);

	g_signal_connect (
		paned, "notify::position",
		G_CALLBACK (paned_notify_position_cb), NULL);
}

GType
e_paned_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EPanedClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) paned_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EPaned),
			0,     /* n_preallocs */
			(GInstanceInitFunc) paned_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_PANED, "EPaned", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_paned_new (GtkOrientation orientation)
{
	return g_object_new (E_TYPE_PANED, "orientation", orientation, NULL);
}

gint
e_paned_get_hposition (EPaned *paned)
{
	g_return_val_if_fail (E_IS_PANED (paned), 0);

	return paned->priv->hposition;
}

void
e_paned_set_hposition (EPaned *paned,
                       gint hposition)
{
	GtkOrientable *orientable;
	GtkOrientation orientation;

	g_return_if_fail (E_IS_PANED (paned));

	if (hposition == paned->priv->hposition)
		return;

	paned->priv->hposition = hposition;

	g_object_notify (G_OBJECT (paned), "hposition");

	orientable = GTK_ORIENTABLE (paned);
	orientation = gtk_orientable_get_orientation (orientable);

	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		paned->priv->sync_position = TRUE;
		gtk_widget_queue_resize (GTK_WIDGET (paned));
	}
}

gint
e_paned_get_vposition (EPaned *paned)
{
	g_return_val_if_fail (E_IS_PANED (paned), 0);

	return paned->priv->vposition;
}

void
e_paned_set_vposition (EPaned *paned,
                       gint vposition)
{
	GtkOrientable *orientable;
	GtkOrientation orientation;

	g_return_if_fail (E_IS_PANED (paned));

	if (vposition == paned->priv->vposition)
		return;

	paned->priv->vposition = vposition;

	g_object_notify (G_OBJECT (paned), "vposition");

	orientable = GTK_ORIENTABLE (paned);
	orientation = gtk_orientable_get_orientation (orientable);

	if (orientation == GTK_ORIENTATION_VERTICAL) {
		paned->priv->sync_position = TRUE;
		gtk_widget_queue_resize (GTK_WIDGET (paned));
	}
}
