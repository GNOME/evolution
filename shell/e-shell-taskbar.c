/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-taskbar.c
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

#include "e-shell-taskbar.h"

#include <e-shell-view.h>

#define E_SHELL_TASKBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_TASKBAR, EShellTaskbarPrivate))

struct _EShellTaskbarPrivate {

	gpointer shell_view;  /* weak pointer */

	GtkWidget *label;
	GtkWidget *hbox;
};

enum {
	PROP_0,
	PROP_SHELL_VIEW
};

static gpointer parent_class;

static void
shell_taskbar_set_shell_view (EShellTaskbar *shell_taskbar,
                              EShellView *shell_view)
{
	g_return_if_fail (shell_taskbar->priv->shell_view == NULL);

	shell_taskbar->priv->shell_view = shell_view;

	g_object_add_weak_pointer (
		G_OBJECT (shell_view),
		&shell_taskbar->priv->shell_view);
}

static void
shell_taskbar_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_VIEW:
			shell_taskbar_set_shell_view (
				E_SHELL_TASKBAR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_taskbar_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_VIEW:
			g_value_set_object (
				value, e_shell_taskbar_get_shell_view (
				E_SHELL_TASKBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_taskbar_dispose (GObject *object)
{
	EShellTaskbarPrivate *priv;

	priv = E_SHELL_TASKBAR_GET_PRIVATE (object);

	if (priv->shell_view != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->shell_view), &priv->shell_view);
		priv->shell_view = NULL;
	}

	if (priv->label != NULL) {
		g_object_unref (priv->label);
		priv->label = NULL;
	}

	if (priv->hbox != NULL) {
		g_object_unref (priv->hbox);
		priv->hbox = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_taskbar_class_init (EShellTaskbarClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellTaskbarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_taskbar_set_property;
	object_class->get_property = shell_taskbar_get_property;
	object_class->dispose = shell_taskbar_dispose;

	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-view",
			NULL,
			NULL,
			E_TYPE_SHELL_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
shell_taskbar_init (EShellTaskbar *shell_taskbar)
{
	GtkWidget *widget;
	gint height;

	shell_taskbar->priv = E_SHELL_TASKBAR_GET_PRIVATE (shell_taskbar);

	gtk_box_set_spacing (GTK_BOX (shell_taskbar), 12);

	widget = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start (GTK_BOX (shell_taskbar), widget, TRUE, TRUE, 0);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	shell_taskbar->priv->label = g_object_ref (widget);
	gtk_widget_hide (widget);

	widget = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (shell_taskbar), widget, TRUE, TRUE, 0);
	shell_taskbar->priv->hbox = g_object_ref (widget);
	gtk_widget_hide (widget);

	/* Make the taskbar large enough to accomodate a small icon.
	 * XXX The "* 2" is a fudge factor to allow for some padding
	 *     The true value is probably buried in a style property. */
	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, NULL, &height);
	gtk_widget_set_size_request (
		GTK_WIDGET (shell_taskbar), -1, height * 2);
}

GType
e_shell_taskbar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EShellTaskbarClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) shell_taskbar_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellTaskbar),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_taskbar_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_HBOX, "EShellTaskbar", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_shell_taskbar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_SHELL_TASKBAR, "shell-view", shell_view, NULL);
}

EShellView *
e_shell_taskbar_get_shell_view (EShellTaskbar *shell_taskbar)
{
	g_return_val_if_fail (E_IS_SHELL_TASKBAR (shell_taskbar), NULL);

	return shell_taskbar->priv->shell_view;
}

void
e_shell_taskbar_set_message (EShellTaskbar *shell_taskbar,
                             const gchar *message)
{
	GtkWidget *label;

	g_return_if_fail (E_IS_SHELL_TASKBAR (shell_taskbar));

	label = shell_taskbar->priv->label;
	message = (message == NULL) ? message : "";
	gtk_label_set_text (GTK_LABEL (label), message);

	if (*message != '\0')
		gtk_widget_show (label);
	else
		gtk_widget_hide (label);
}

void
e_shell_taskbar_unset_message (EShellTaskbar *shell_taskbar)
{
	g_return_if_fail (E_IS_SHELL_TASKBAR (shell_taskbar));

	e_shell_taskbar_set_message (shell_taskbar, NULL);
}

void
e_shell_taskbar_prepend_task (EShellTaskbar *shell_taskbar,
                              ETaskWidget *task_widget)
{
	GtkBox *box;
	GtkWidget *child;

	g_return_if_fail (E_IS_SHELL_TASKBAR (shell_taskbar));
	g_return_if_fail (E_IS_TASK_WIDGET (task_widget));

	child = GTK_WIDGET (task_widget);
	box = GTK_BOX (shell_taskbar->priv->hbox);
	gtk_box_pack_start (box, child, TRUE, TRUE, 0);
	gtk_box_reorder_child (box, child, 0);
	gtk_widget_show (GTK_WIDGET (box));
}

void
e_shell_taskbar_remove_task (EShellTaskbar *shell_taskbar,
                             gint position)
{
	ETaskWidget *task_widget;
	GtkBox *box;

	g_return_if_fail (E_IS_SHELL_TASKBAR (shell_taskbar));
	g_return_if_fail (position >= 0);

	task_widget = e_shell_taskbar_get_task_widget (
		shell_taskbar, position);
	gtk_widget_destroy (GTK_WIDGET (task_widget));

	box = GTK_BOX (shell_taskbar->priv->hbox);
	if (box->children == NULL)
		gtk_widget_hide (GTK_WIDGET (box));
}

ETaskWidget *
e_shell_taskbar_get_task_widget_from_id (EShellTaskbar *shell_taskbar,
                                         guint task_id)
{
	GtkBox *box;
	GList *iter;

	g_return_val_if_fail (E_IS_SHELL_TASKBAR (shell_taskbar), NULL);

	box = GTK_BOX (shell_taskbar->priv->hbox);

	for (iter = box->children; iter != NULL; iter = iter->next) {
		GtkBoxChild *child_info = iter->data;
		ETaskWidget *task_widget;

		task_widget = E_TASK_WIDGET (child_info->widget);

		if (task_widget->id == task_id)
			return task_widget;
	}

	return NULL;
}

void
e_shell_taskbar_remove_task_from_id (EShellTaskbar *shell_taskbar,
                                     guint task_id)
{
	ETaskWidget *task_widget;
	GtkBox *box;

	g_return_if_fail (E_IS_SHELL_TASKBAR (shell_taskbar));

	task_widget = e_shell_taskbar_get_task_widget_from_id (
		shell_taskbar, task_id);
	g_return_if_fail (task_widget != NULL);

	gtk_widget_destroy (GTK_WIDGET (task_widget));

	box = GTK_BOX (shell_taskbar->priv->hbox);
	if (box->children == NULL)
		gtk_widget_hide (GTK_WIDGET (box));
}

ETaskWidget *
e_shell_taskbar_get_task_widget (EShellTaskbar *shell_taskbar,
                                 gint position)
{
	GtkBoxChild *child_info;
	GtkBox *box;

	g_return_val_if_fail (E_IS_SHELL_TASKBAR (shell_taskbar), NULL);

	box = GTK_BOX (shell_taskbar->priv->hbox);
	child_info = g_list_nth (box->children, position)->data;

	return E_TASK_WIDGET (child_info->widget);
}
