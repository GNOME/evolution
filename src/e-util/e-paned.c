/*
 * e-paned.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-paned.h"

#include <glib/gi18n-lib.h>

#include "e-misc-utils.h"

#define SYNC_REQUEST_NONE		0
#define SYNC_REQUEST_POSITION		1
#define SYNC_REQUEST_PROPORTION		2

struct _EPanedPrivate {
	gint hposition;
	gint vposition;
	gdouble proportion;

	gulong wse_handler_id;

	guint fixed_resize : 1;
	guint sync_request : 2;
	guint toplevel_ready : 1;
};

enum {
	PROP_0,
	PROP_HPOSITION,
	PROP_VPOSITION,
	PROP_PROPORTION,
	PROP_FIXED_RESIZE
};

G_DEFINE_TYPE_WITH_PRIVATE (EPaned, e_paned, GTK_TYPE_PANED)

static gboolean
paned_queue_resize_on_idle (GtkWidget *paned)
{
	gtk_widget_queue_resize_no_redraw (paned);

	return FALSE;
}

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

	if (paned->priv->sync_request != SYNC_REQUEST_NONE)
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
	if (e_paned_get_fixed_resize (paned))
		paned->priv->sync_request = SYNC_REQUEST_POSITION;
	else
		paned->priv->sync_request = SYNC_REQUEST_PROPORTION;
	gtk_widget_queue_resize (GTK_WIDGET (paned));
}

static void
paned_recalc_positions (EPaned *paned,
			gboolean with_proportion)
{
	GtkAllocation allocation;
	GtkOrientation orientation;
	gdouble proportion;
	gint position;

	orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (paned));

	gtk_widget_get_allocation (GTK_WIDGET (paned), &allocation);
	position = gtk_paned_get_position (GTK_PANED (paned));

	g_object_freeze_notify (G_OBJECT (paned));

	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		position = MAX (0, allocation.width - position);
		proportion = (gdouble) position / allocation.width;

		if (paned->priv->hposition != position) {
			paned->priv->hposition = position;
			g_object_notify (G_OBJECT (paned), "hposition");
		}
	} else {
		position = MAX (0, allocation.height - position);
		proportion = (gdouble) position / allocation.height;

		if (paned->priv->vposition != position) {
			paned->priv->vposition = position;
			g_object_notify (G_OBJECT (paned), "vposition");
		}
	}

	if (with_proportion && paned->priv->proportion != proportion) {
		paned->priv->proportion = proportion;
		g_object_notify (G_OBJECT (paned), "proportion");
	}

	g_object_thaw_notify (G_OBJECT (paned));
}

static void
paned_notify_position_cb (EPaned *paned)
{
	/* If a sync has already been requested, do nothing. */
	if (paned->priv->sync_request != SYNC_REQUEST_NONE)
		return;

	g_object_freeze_notify (G_OBJECT (paned));

	paned_recalc_positions (paned, TRUE);

	if (e_paned_get_fixed_resize (paned))
		paned->priv->sync_request = SYNC_REQUEST_POSITION;
	else
		paned->priv->sync_request = SYNC_REQUEST_PROPORTION;

	g_object_thaw_notify (G_OBJECT (paned));
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

		case PROP_PROPORTION:
			e_paned_set_proportion (
				E_PANED (object),
				g_value_get_double (value));
			return;

		case PROP_FIXED_RESIZE:
			e_paned_set_fixed_resize (
				E_PANED (object),
				g_value_get_boolean (value));
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

		case PROP_PROPORTION:
			g_value_set_double (
				value, e_paned_get_proportion (
				E_PANED (object)));
			return;

		case PROP_FIXED_RESIZE:
			g_value_set_boolean (
				value, e_paned_get_fixed_resize (
				E_PANED (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
paned_realize (GtkWidget *widget)
{
	EPaned *self;
	GtkWidget *toplevel;
	GdkWindowState state;
	GdkWindow *window;

	self = E_PANED (widget);

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (e_paned_parent_class)->realize (widget);

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
		self->priv->wse_handler_id = g_signal_connect_swapped (
			toplevel, "window-state-event",
			G_CALLBACK (paned_window_state_event_cb), widget);
	else
		self->priv->toplevel_ready = TRUE;
}

static void
paned_size_allocate (GtkWidget *widget,
                     GtkAllocation *allocation)
{
	EPaned *paned = E_PANED (widget);
	GtkOrientable *orientable;
	GtkOrientation orientation;
	gdouble proportion, old_proportion = -1.0;
	gint allocated;
	gint position, min_position = -1, max_position = -1, clamp_position;
	gboolean corrected_portion = FALSE, notify_proportion_change = FALSE;

	if (!e_paned_get_fixed_resize (paned))
		old_proportion = e_paned_get_proportion (paned);

	/* Chain up to parent's size_allocate() method. */
	GTK_WIDGET_CLASS (e_paned_parent_class)->size_allocate (widget, allocation);

	if (paned->priv->sync_request == SYNC_REQUEST_PROPORTION &&
	    old_proportion != e_paned_get_proportion (paned) && old_proportion > 0.0) {
		paned->priv->proportion = old_proportion;
		notify_proportion_change = TRUE;
		corrected_portion = TRUE;
	}

	if (!paned->priv->toplevel_ready) {
		if (notify_proportion_change)
			g_object_notify (G_OBJECT (paned), "proportion");

		return;
	}

	if (paned->priv->sync_request == SYNC_REQUEST_NONE) {
		paned_recalc_positions (paned, FALSE);

		if (notify_proportion_change)
			g_object_notify (G_OBJECT (paned), "proportion");

		return;
	}

	orientable = GTK_ORIENTABLE (paned);
	orientation = gtk_orientable_get_orientation (orientable);

	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		allocated = allocation->width;
		position = e_paned_get_hposition (paned);
	} else {
		allocated = allocation->height;
		position = e_paned_get_vposition (paned);
	}

	proportion = e_paned_get_proportion (paned);

	if (paned->priv->sync_request == SYNC_REQUEST_POSITION) {
		position = MAX (0, allocated - position);

		if (!e_paned_get_fixed_resize (paned) && allocated > 0) {
			proportion = 1.0 - ((gdouble) position / allocated);
			paned->priv->proportion = proportion;
			notify_proportion_change = TRUE;
		}
	} else {
		position = (1.0 - proportion) * allocated;
	}

	g_object_get (G_OBJECT (paned),
		"min-position", &min_position,
		"max-position", &max_position,
		NULL);

	clamp_position = MAX (0, CLAMP (position, min_position, max_position));

	if (clamp_position != gtk_paned_get_position (GTK_PANED (paned)))
		gtk_paned_set_position (GTK_PANED (paned), clamp_position);

	if (clamp_position != position && allocated > 0) {
		proportion = 1.0 - ((gdouble) clamp_position / allocated);
		paned->priv->proportion = proportion;
		notify_proportion_change = TRUE;

		corrected_portion = TRUE;
	}

	if (corrected_portion) {
		position = MAX (0, allocated - clamp_position);

		if (position > 0) {
			if (orientation == GTK_ORIENTATION_HORIZONTAL) {
				paned->priv->hposition = position;
				g_object_notify (G_OBJECT (paned), "hposition");
			} else {
				paned->priv->vposition = position;
				g_object_notify (G_OBJECT (paned), "vposition");
			}
		}
	}

	if (notify_proportion_change)
		g_object_notify (G_OBJECT (paned), "proportion");

	paned->priv->sync_request = SYNC_REQUEST_NONE;

	/* gtk_paned_set_position() calls queue_resize, which cannot
	 * be called from size_allocate, so schedule it from an idle
	 * callback so the change takes effect. */
	g_idle_add_full (
		G_PRIORITY_DEFAULT_IDLE,
		(GSourceFunc) paned_queue_resize_on_idle,
		g_object_ref (paned),
		(GDestroyNotify) g_object_unref);
}

static void
e_paned_class_init (EPanedClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

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
			"Horizontal Position",
			"Pane position when oriented horizontally",
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_VPOSITION,
		g_param_spec_int (
			"vposition",
			"Vertical Position",
			"Pane position when oriented vertically",
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PROPORTION,
		g_param_spec_double (
			"proportion",
			"Proportion",
			"Proportion of the 2nd pane size",
			0.0,
			1.0,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FIXED_RESIZE,
		g_param_spec_boolean (
			"fixed-resize",
			"Fixed Resize",
			"Keep the 2nd pane fixed during resize",
			TRUE,
			G_PARAM_READWRITE));
}

static void
e_paned_init (EPaned *paned)
{
	paned->priv = e_paned_get_instance_private (paned);

	paned->priv->proportion = 0.5;
	paned->priv->fixed_resize = TRUE;

	e_signal_connect_notify (
		paned, "notify::orientation",
		G_CALLBACK (paned_notify_orientation_cb), NULL);

	e_signal_connect_notify (
		paned, "notify::position",
		G_CALLBACK (paned_notify_position_cb), NULL);
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
		paned->priv->sync_request = SYNC_REQUEST_POSITION;
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
		paned->priv->sync_request = SYNC_REQUEST_POSITION;
		gtk_widget_queue_resize (GTK_WIDGET (paned));
	}
}

gdouble
e_paned_get_proportion (EPaned *paned)
{
	g_return_val_if_fail (E_IS_PANED (paned), 0.5);

	return paned->priv->proportion;
}

void
e_paned_set_proportion (EPaned *paned,
                        gdouble proportion)
{
	g_return_if_fail (E_IS_PANED (paned));
	g_return_if_fail (CLAMP (proportion, 0.0, 1.0) == proportion);

	if (paned->priv->proportion == proportion)
		return;

	paned->priv->proportion = proportion;

	paned->priv->sync_request = SYNC_REQUEST_PROPORTION;
	gtk_widget_queue_resize (GTK_WIDGET (paned));

	g_object_notify (G_OBJECT (paned), "proportion");
}

gboolean
e_paned_get_fixed_resize (EPaned *paned)
{
	g_return_val_if_fail (E_IS_PANED (paned), FALSE);

	return paned->priv->fixed_resize;
}

void
e_paned_set_fixed_resize (EPaned *paned,
                          gboolean fixed_resize)
{
	g_return_if_fail (E_IS_PANED (paned));

	if (fixed_resize == paned->priv->fixed_resize)
		return;

	paned->priv->fixed_resize = fixed_resize;

	g_object_notify (G_OBJECT (paned), "fixed-resize");
}
