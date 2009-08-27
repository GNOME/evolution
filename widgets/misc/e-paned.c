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

	guint sync_position	: 1;
};

enum {
	PROP_0,
	PROP_HPOSITION,
	PROP_VPOSITION,
	PROP_VERTICAL_VIEW
};

static gpointer parent_class;

static void
paned_notify_orientation_cb (EPaned *paned)
{
	paned->priv->sync_position = TRUE;
	gtk_widget_queue_resize (GTK_WIDGET (paned));

	g_object_notify (G_OBJECT (paned), "vertical-view");
}

static void
paned_notify_position_cb (EPaned *paned)
{
	GtkAllocation *allocation;
	gint position;

	if (paned->priv->sync_position)
		return;

	allocation = &GTK_WIDGET (paned)->allocation;
	position = gtk_paned_get_position (GTK_PANED (paned));

	if (e_paned_get_vertical_view (paned)) {
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

		case PROP_VERTICAL_VIEW:
			e_paned_set_vertical_view (
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

		case PROP_VERTICAL_VIEW:
			g_value_set_boolean (
				value, e_paned_get_vertical_view (
				E_PANED (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
paned_size_allocate (GtkWidget *widget,
                     GtkAllocation *allocation)
{
	EPaned *paned = E_PANED (widget);
	gint allocated;
	gint position;

	/* Chain up to parent's size_allocate() method. */
	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

	if (!paned->priv->sync_position)
		return;

	if (e_paned_get_vertical_view (paned)) {
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

	g_object_class_install_property (
		object_class,
		PROP_VERTICAL_VIEW,
		g_param_spec_boolean (
			"vertical-view",
			_("Vertical View"),
			_("Whether vertical view is enabled"),
			FALSE,
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
	g_return_if_fail (E_IS_PANED (paned));

	if (hposition == paned->priv->hposition)
		return;

	paned->priv->hposition = hposition;

	g_object_notify (G_OBJECT (paned), "hposition");

	if (e_paned_get_vertical_view (paned)) {
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
	g_return_if_fail (E_IS_PANED (paned));

	if (vposition == paned->priv->vposition)
		return;

	paned->priv->vposition = vposition;

	g_object_notify (G_OBJECT (paned), "vposition");

	if (!e_paned_get_vertical_view (paned)) {
		paned->priv->sync_position = TRUE;
		gtk_widget_queue_resize (GTK_WIDGET (paned));
	}
}

gboolean
e_paned_get_vertical_view (EPaned *paned)
{
	GtkOrientable *orientable;
	GtkOrientation orientation;

	g_return_val_if_fail (E_IS_PANED (paned), FALSE);

	orientable = GTK_ORIENTABLE (paned);
	orientation = gtk_orientable_get_orientation (orientable);

	return (orientation == GTK_ORIENTATION_HORIZONTAL);
}

void
e_paned_set_vertical_view (EPaned *paned,
                           gboolean vertical_view)
{
	GtkOrientable *orientable;
	GtkOrientation orientation;

	g_return_if_fail (E_IS_PANED (paned));

	if (vertical_view)
		orientation = GTK_ORIENTATION_HORIZONTAL;
	else
		orientation = GTK_ORIENTATION_VERTICAL;

	orientable = GTK_ORIENTABLE (paned);
	gtk_orientable_set_orientation (orientable, orientation);
}
