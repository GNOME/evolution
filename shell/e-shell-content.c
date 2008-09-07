/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-shell_content.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-shell-content.h"

#include <e-search-bar.h>

#define E_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_CONTENT, EShellContentPrivate))

struct _EShellContentPrivate {
	GtkWidget *search_bar;
};

static gpointer parent_class;

static void
shell_content_dispose (GObject *object)
{
	EShellContentPrivate *priv;

	priv = E_SHELL_CONTENT_GET_PRIVATE (object);

	if (priv->search_bar != NULL) {
		g_object_unref (priv->search_bar);
		priv->search_bar = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_content_size_request (GtkWidget *widget,
                            GtkRequisition *requisition)
{
	EShellContentPrivate *priv;
	GtkRequisition child_requisition;
	GtkWidget *child;

	priv = E_SHELL_CONTENT_GET_PRIVATE (widget);

	requisition->width = 0;
	requisition->height = 0;

	child = gtk_bin_get_child (GTK_BIN (widget));
	gtk_widget_size_request (child, requisition);

	child = priv->search_bar;
	gtk_widget_size_request (child, &child_requisition);
	requisition->width = MAX (requisition->width, child_requisition.width);
	requisition->height += child_requisition.height;
}

static void
shell_content_size_allocate (GtkWidget *widget,
                       GtkAllocation *allocation)
{
	EShellContentPrivate *priv;
	GtkAllocation child_allocation;
	GtkRequisition child_requisition;
	GtkWidget *child;

	priv = E_SHELL_CONTENT_GET_PRIVATE (widget);

	widget->allocation = *allocation;

	child = priv->search_bar;
	gtk_widget_size_request (child, &child_requisition);

	child_allocation.x = allocation->x;
	child_allocation.y = allocation->y;
	child_allocation.width = allocation->width;
	child_allocation.height = child_requisition.height;

	gtk_widget_size_allocate (child, &child_allocation);

	child_allocation.y += child_requisition.height;
	child_allocation.height =
		allocation->height - child_requisition.height;

	child = gtk_bin_get_child (GTK_BIN (widget));
	if (child != NULL)
		gtk_widget_size_allocate (child, &child_allocation);
}

static void
shell_content_remove (GtkContainer *container,
                GtkWidget *widget)
{
	EShellContentPrivate *priv;

	priv = E_SHELL_CONTENT_GET_PRIVATE (container);

	/* Look in the internal widgets first. */

	if (widget == priv->search_bar) {
		gtk_widget_unparent (priv->search_bar);
		gtk_widget_queue_resize (GTK_WIDGET (container));
		return;
	}

	/* Chain up to parent's remove() method. */
	GTK_CONTAINER_CLASS (parent_class)->remove (container, widget);
}

static void
shell_content_forall (GtkContainer *container,
                gboolean include_internals,
                GtkCallback callback,
                gpointer callback_data)
{
	EShellContentPrivate *priv;

	priv = E_SHELL_CONTENT_GET_PRIVATE (container);

	if (include_internals)
		callback (priv->search_bar, callback_data);

	/* Chain up to parent's forall() method. */
	GTK_CONTAINER_CLASS (parent_class)->forall (
		container, include_internals, callback, callback_data);
}

static void
shell_content_class_init (EShellContentClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = shell_content_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->size_request = shell_content_size_request;
	widget_class->size_allocate = shell_content_size_allocate;

	container_class = GTK_CONTAINER_CLASS (class);
	container_class->remove = shell_content_remove;
	container_class->forall = shell_content_forall;
}

static void
shell_content_init (EShellContent *shell_content)
{
	GtkWidget *widget;

	shell_content->priv = E_SHELL_CONTENT_GET_PRIVATE (shell_content);

	GTK_WIDGET_SET_FLAGS (shell_content, GTK_NO_WINDOW);

	widget = e_search_bar_new ();
	gtk_widget_set_parent (widget, GTK_WIDGET (shell_content));
	shell_content->priv->search_bar = g_object_ref (widget);
	gtk_widget_show (widget);
}

GType
e_shell_content_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EShellContentClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) shell_content_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellContent),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_content_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_BIN, "EShellContent", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_shell_content_new (void)
{
	return g_object_new (E_TYPE_SHELL_CONTENT, NULL);
}

GtkWidget *
e_shell_content_get_search_bar (EShellContent *shell_content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	return shell_content->priv->search_bar;
}
