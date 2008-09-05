/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-switcher.c
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

#include "e-shell-switcher.h"

#define E_SHELL_SWITCHER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_SWITCHER, EShellSwitcherPrivate))

#define H_PADDING 6
#define V_PADDING 6

struct _EShellSwitcherPrivate {
	GList *proxies;
	gboolean style_set;
	GtkToolbarStyle style;
	GtkSettings *settings;
	gulong settings_handler_id;
};

enum {
	PROP_0,
	PROP_TOOLBAR_STYLE
};

enum {
	STYLE_CHANGED,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

static void
switcher_layout_actions (EShellSwitcher *switcher)
{
	GtkAllocation *allocation = & GTK_WIDGET (switcher)->allocation;
	gboolean icons_only;
	int num_btns = g_list_length (switcher->priv->proxies), btns_per_row;
	GList **rows, *p;
	int row_number;
	int max_width = 0, max_height = 0;
	int row_last;
	int x, y;
	int i;

	if (num_btns == 0)
		return;

	icons_only = (switcher->priv->style == GTK_TOOLBAR_ICONS);

	/* Figure out the max width and height. */
	for (p = switcher->priv->proxies; p != NULL; p = p->next) {
		GtkWidget *widget = p->data;
		GtkRequisition requisition;

		gtk_widget_size_request (widget, &requisition);
		max_height = MAX (max_height, requisition.height);
		max_width = MAX (max_width, requisition.width);
	}

	/* Figure out how many rows and columns we'll use. */
	btns_per_row = MAX (1, allocation->width / (max_width + H_PADDING));
	if (!icons_only) {
		/* If using text buttons, we want to try to have a
		 * completely filled-in grid, but if we can't, we want
		 * the odd row to have just a single button. */
		while (num_btns % btns_per_row > 1)
			btns_per_row--;
	}

	/* Assign buttons to rows. */
	rows = g_new0 (GList *, num_btns / btns_per_row + 1);

	if (!icons_only && num_btns % btns_per_row != 0) {
		rows[0] = g_list_append (rows[0], switcher->priv->proxies->data);

		p = switcher->priv->proxies->next;
		row_number = p ? 1 : 0;
	} else {
		p = switcher->priv->proxies;
		row_number = 0;
	}

	for (; p != NULL; p = p->next) {
		GtkWidget *widget = p->data;

		if (g_list_length (rows[row_number]) == btns_per_row)
			row_number++;

		rows[row_number] = g_list_append (rows[row_number], widget);
	}

	row_last = row_number;

	/* Layout the buttons. */
	y = allocation->y;
	for (i = 0; i < row_last + 1; i++) {
		int len, extra_width;

		x = H_PADDING + allocation->x;
		len = g_list_length (rows[i]);
		if (!icons_only)
			extra_width = (allocation->width - (len * max_width ) - (len * H_PADDING)) / len;
		else
			extra_width = 0;
		for (p = rows [i]; p != NULL; p = p->next) {
			GtkAllocation child_allocation;

			child_allocation.x = x;
			child_allocation.y = y;
			child_allocation.width = max_width + extra_width;
			child_allocation.height = max_height;

			gtk_widget_size_allocate (GTK_WIDGET (p->data), &child_allocation);

			x += child_allocation.width + H_PADDING;
		}

		y += max_height + V_PADDING;
	}

	for (i = 0; i <= row_last; i ++)
		g_list_free (rows [i]);
	g_free (rows);
}

static void
switcher_toolbar_style_changed_cb (EShellSwitcher *switcher)
{
	if (!switcher->priv->style_set) {
		switcher->priv->style_set = TRUE;
		e_shell_switcher_unset_style (switcher);
	}
}

static void
switcher_set_property (GObject *object,
                      guint property_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TOOLBAR_STYLE:
			e_shell_switcher_set_style (
				E_SHELL_SWITCHER (object),
				g_value_get_enum (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
switcher_get_property (GObject *object,
                      guint property_id,
                      GValue *value,
                      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TOOLBAR_STYLE:
			g_value_set_enum (
				value, e_shell_switcher_get_style (
				E_SHELL_SWITCHER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
switcher_dispose (GObject *object)
{
	EShellSwitcherPrivate *priv;

	priv = E_SHELL_SWITCHER_GET_PRIVATE (object);

	while (priv->proxies != NULL) {
		GtkWidget *widget = priv->proxies->data;
		gtk_container_remove (GTK_CONTAINER (object), widget);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
switcher_size_request (GtkWidget *widget,
                      GtkRequisition *requisition)
{
	EShellSwitcherPrivate *priv;
	GList *iter;

	priv = E_SHELL_SWITCHER_GET_PRIVATE (widget);

	requisition->width = 0;
	requisition->height = 0;

	for (iter = priv->proxies; iter != NULL; iter = iter->next) {
		GtkWidget *widget = iter->data;
		GtkRequisition child_requisition;

		gtk_widget_size_request (widget, &child_requisition);

		child_requisition.width += H_PADDING;
		child_requisition.height += V_PADDING;

		requisition->width = MAX (
			requisition->width, child_requisition.width);
		requisition->height += child_requisition.height;
	}
}

static void
switcher_size_allocate (GtkWidget *widget,
                       GtkAllocation *allocation)
{
	EShellSwitcherPrivate *priv;

	priv = E_SHELL_SWITCHER_GET_PRIVATE (widget);

	widget->allocation = *allocation;

	switcher_layout_actions (E_SHELL_SWITCHER (widget));
}

static void
switcher_screen_changed (GtkWidget *widget,
                        GdkScreen *previous_screen)
{
	EShellSwitcherPrivate *priv;
	GtkSettings *settings;

	priv = E_SHELL_SWITCHER_GET_PRIVATE (widget);

	if (gtk_widget_has_screen (widget))
		settings = gtk_widget_get_settings (widget);
	else
		settings = NULL;

	if (settings == priv->settings)
		return;

	if (priv->settings != NULL) {
		g_signal_handler_disconnect (
			priv->settings, priv->settings_handler_id);
		g_object_unref (priv->settings);
	}

	if (settings != NULL) {
		priv->settings = g_object_ref (settings);
		priv->settings_handler_id = g_signal_connect_swapped (
			settings, "notify::gtk-toolbar-style",
			G_CALLBACK (switcher_toolbar_style_changed_cb), widget);
	} else
		priv->settings = NULL;

	switcher_toolbar_style_changed_cb (E_SHELL_SWITCHER (widget));
}

static void
switcher_remove (GtkContainer *container,
                GtkWidget *widget)
{
	EShellSwitcherPrivate *priv;
	GList *link;

	priv = E_SHELL_SWITCHER_GET_PRIVATE (container);

	/* Look in the internal widgets first. */

	link = g_list_find (priv->proxies, widget);
	if (link != NULL) {
		GtkWidget *widget = link->data;

		gtk_widget_unparent (widget);
		priv->proxies = g_list_delete_link (priv->proxies, link);
		gtk_widget_queue_resize (GTK_WIDGET (container));
		return;
	}

	/* Chain up to parent's remove() method. */
	GTK_CONTAINER_CLASS (parent_class)->remove (container, widget);
}

static void
switcher_forall (GtkContainer *container,
                gboolean include_internals,
                GtkCallback callback,
                gpointer callback_data)
{
	EShellSwitcherPrivate *priv;

	priv = E_SHELL_SWITCHER_GET_PRIVATE (container);

	if (include_internals)
		g_list_foreach (
			priv->proxies, (GFunc) callback, callback_data);

	/* Chain up to parent's forall() method. */
	GTK_CONTAINER_CLASS (parent_class)->forall (
		container, include_internals, callback, callback_data);
}

static void
switcher_style_changed (EShellSwitcher *switcher,
                       GtkToolbarStyle style)
{
	if (switcher->priv->style == style)
		return;

	switcher->priv->style = style;

	g_list_foreach (
		switcher->priv->proxies,
		(GFunc) gtk_tool_item_toolbar_reconfigured, NULL);

	gtk_widget_queue_resize (GTK_WIDGET (switcher));
	g_object_notify (G_OBJECT (switcher), "toolbar-style");
}

static GtkIconSize
switcher_get_icon_size (GtkToolShell *shell)
{
	return GTK_ICON_SIZE_LARGE_TOOLBAR;
}

static GtkOrientation
switcher_get_orientation (GtkToolShell *shell)
{
	return GTK_ORIENTATION_HORIZONTAL;
}

static GtkToolbarStyle
switcher_get_style (GtkToolShell *shell)
{
	return e_shell_switcher_get_style (E_SHELL_SWITCHER (shell));
}

static GtkReliefStyle
switcher_get_relief_style (GtkToolShell *shell)
{
	/* XXX GTK+ 2.13.6 discards this value.
	 *     http://bugzilla.gnome.org/show_bug.cgi?id=549943 */
	return GTK_RELIEF_NORMAL;
}

static void
switcher_class_init (EShellSwitcherClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellSwitcherPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = switcher_set_property;
	object_class->get_property = switcher_get_property;
	object_class->dispose = switcher_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->size_request = switcher_size_request;
	widget_class->size_allocate = switcher_size_allocate;
	widget_class->screen_changed = switcher_screen_changed;

	container_class = GTK_CONTAINER_CLASS (class);
	container_class->remove = switcher_remove;
	container_class->forall = switcher_forall;

	class->style_changed = switcher_style_changed;

	g_object_class_install_property (
		object_class,
		PROP_TOOLBAR_STYLE,
		g_param_spec_enum (
			"toolbar-style",
			NULL,
			NULL,
			GTK_TYPE_TOOLBAR_STYLE,
			E_SHELL_SWITCHER_DEFAULT_TOOLBAR_STYLE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	signals[STYLE_CHANGED] = g_signal_new (
		"style-changed",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellSwitcherClass, style_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__ENUM,
		G_TYPE_NONE, 1,
		GTK_TYPE_TOOLBAR_STYLE);
}

static void
switcher_init (EShellSwitcher *switcher)
{
	switcher->priv = E_SHELL_SWITCHER_GET_PRIVATE (switcher);

	GTK_WIDGET_SET_FLAGS (switcher, GTK_NO_WINDOW);
}

static void
switcher_tool_shell_iface_init (GtkToolShellIface *iface)
{
	iface->get_icon_size = switcher_get_icon_size;
	iface->get_orientation = switcher_get_orientation;
	iface->get_style = switcher_get_style;
	iface->get_relief_style = switcher_get_relief_style;
}

GType
e_shell_switcher_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EShellSwitcherClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) switcher_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellSwitcher),
			0,     /* n_preallocs */
			(GInstanceInitFunc) switcher_init,
			NULL   /* value_table */
		};

		static const GInterfaceInfo tool_shell_info = {
			(GInterfaceInitFunc) switcher_tool_shell_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL   /* interface_data */
		};

		type = g_type_register_static (
			GTK_TYPE_BIN, "EShellSwitcher", &type_info, 0);

		g_type_add_interface_static (
			type, GTK_TYPE_TOOL_SHELL, &tool_shell_info);
	}

	return type;
}

GtkWidget *
e_shell_switcher_new (void)
{
	return g_object_new (E_TYPE_SHELL_SWITCHER, NULL);
}

void
e_shell_switcher_add_action (EShellSwitcher *switcher,
                             GtkAction *action)
{
	GtkWidget *widget;

	g_return_if_fail (E_IS_SHELL_SWITCHER (switcher));
	g_return_if_fail (GTK_IS_ACTION (action));

	g_object_ref (action);
	widget = gtk_action_create_tool_item (action);
	gtk_tool_item_set_is_important (GTK_TOOL_ITEM (widget), TRUE);
	gtk_widget_show (widget);

	switcher->priv->proxies = g_list_append (
		switcher->priv->proxies, widget);

	gtk_widget_set_parent (widget, GTK_WIDGET (switcher));
	gtk_widget_queue_resize (GTK_WIDGET (switcher));
}

GtkToolbarStyle
e_shell_switcher_get_style (EShellSwitcher *switcher)
{
	g_return_val_if_fail (
		E_IS_SHELL_SWITCHER (switcher),
		E_SHELL_SWITCHER_DEFAULT_TOOLBAR_STYLE);

	return switcher->priv->style;
}

void
e_shell_switcher_set_style (EShellSwitcher *switcher,
                     GtkToolbarStyle style)
{
	g_return_if_fail (E_IS_SHELL_SWITCHER (switcher));

	switcher->priv->style_set = TRUE;
	g_signal_emit (switcher, signals[STYLE_CHANGED], 0, style);
}

void
e_shell_switcher_unset_style (EShellSwitcher *switcher)
{
	GtkSettings *settings;
	GtkToolbarStyle style;

	g_return_if_fail (E_IS_SHELL_SWITCHER (switcher));

	if (!switcher->priv->style_set)
		return;

	settings = switcher->priv->settings;
	if (settings != NULL)
		g_object_get (settings, "gtk-toolbar-style", &style, NULL);
	else
		style = E_SHELL_SWITCHER_DEFAULT_TOOLBAR_STYLE;

	if (style == GTK_TOOLBAR_BOTH)
		style = GTK_TOOLBAR_BOTH_HORIZ;

	if (style != switcher->priv->style)
		g_signal_emit (switcher, signals[STYLE_CHANGED], 0, style);

	switcher->priv->style_set = FALSE;
}
