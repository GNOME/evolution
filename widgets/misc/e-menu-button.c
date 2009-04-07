/*
 * e-menu-button.c
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

#include "e-menu-button.h"

#define E_MENU_BUTTON_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MENU_BUTTON, EMenuButtonPrivate))

struct _EMenuButtonPrivate {
	GtkWidget *toggle_button;
	GtkMenu *menu;  /* not referenced */
};

enum {
	PROP_0,
	PROP_MENU
};

enum {
	SHOW_MENU,
	LAST_SIGNAL
};

static gpointer parent_class;
static gulong signals[LAST_SIGNAL];

static void
menu_button_detach (GtkWidget *widget,
                    GtkMenu *menu)
{
	EMenuButtonPrivate *priv;

	priv = E_MENU_BUTTON_GET_PRIVATE (widget);

	g_return_if_fail (priv->menu == menu);

	priv->menu = NULL;
}

static void
menu_button_deactivate_cb (EMenuButton *menu_button)
{
	GtkToggleButton *toggle_button;

	toggle_button = GTK_TOGGLE_BUTTON (menu_button->priv->toggle_button);
	gtk_toggle_button_set_active (toggle_button, FALSE);
}

static void
menu_button_menu_position (GtkMenu *menu,
                           gint *x,
                           gint *y,
                           gboolean *push_in,
                           EMenuButton *menu_button)
{
	GtkRequisition requisition;
	GtkTextDirection direction;
	GdkRectangle monitor;
	GdkScreen *screen;
	GdkWindow *window;
	GtkWidget *widget;
	GtkWidget *toggle_button;
	gint button_bottom;
	gint monitor_bottom;
	gint monitor_num;

	widget = GTK_WIDGET (menu_button);
	toggle_button = menu_button->priv->toggle_button;
	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

	window = gtk_widget_get_parent_window (widget);
	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	monitor_num = gdk_screen_get_monitor_at_window (screen, window);
	if (monitor_num < 0)
		monitor_num = 0;
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	gdk_window_get_origin (window, x, y);
	*x += widget->allocation.x;
	*y += widget->allocation.y;

	direction = gtk_widget_get_direction (widget);
	if (direction == GTK_TEXT_DIR_LTR)
		x += MAX (widget->allocation.width - requisition.width, 0);
	else if (requisition.width > widget->allocation.width)
		x -= requisition.width - widget->allocation.width;

	button_bottom = *y + toggle_button->allocation.height;
	monitor_bottom = monitor.y + monitor.height;

	if (button_bottom + requisition.height <= monitor_bottom)
		y += toggle_button->allocation.height;
	else if (*y - requisition.height >= monitor.y)
		y -= requisition.height;
	else if (monitor_bottom - button_bottom > *y)
		y += toggle_button->allocation.height;
	else
		y -= requisition.height;

	*push_in = FALSE;
}

static void
menu_button_show_popup_menu (EMenuButton *menu_button,
                             GdkEventButton *event)
{
	g_signal_emit (menu_button, signals[SHOW_MENU], 0);

	if (menu_button->priv->menu == NULL)
		return;

	if (event != NULL)
		gtk_menu_popup (
			menu_button->priv->menu, NULL, NULL,
			(GtkMenuPositionFunc) menu_button_menu_position,
			menu_button, event->button, event->time);
	else
		gtk_menu_popup (
			menu_button->priv->menu, NULL, NULL,
			(GtkMenuPositionFunc) menu_button_menu_position,
			menu_button, 0, gtk_get_current_event_time ());
}

static gboolean
menu_button_toggle_button_press_event_cb (EMenuButton *menu_button,
                                          GdkEventButton *event)
{
	if (event->button == 1) {
		menu_button_show_popup_menu (menu_button, event);
		return TRUE;
	}

	return FALSE;
}

static void
menu_button_toggle_toggled_cb (EMenuButton *menu_button)
{
	GtkMenuShell *menu_shell;
	GtkToggleButton *toggle_button;

	menu_shell = GTK_MENU_SHELL (menu_button->priv->menu);
	toggle_button = GTK_TOGGLE_BUTTON (menu_button->priv->toggle_button);

	if (!gtk_toggle_button_get_active (toggle_button))
		return;

	if (GTK_WIDGET_VISIBLE (menu_shell))
		return;

	/* We get here only when the menu is activated by a key
	 * press, so that we can select the first menu item. */
	menu_button_show_popup_menu (menu_button, NULL);
	gtk_menu_shell_select_first (menu_shell, FALSE);
}

static void
menu_button_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MENU:
			e_menu_button_set_menu (
				E_MENU_BUTTON (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
menu_button_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MENU:
			g_value_set_object (
				value, e_menu_button_get_menu (
				E_MENU_BUTTON (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
menu_button_dispose (GObject *object)
{
	EMenuButtonPrivate *priv;

	priv = E_MENU_BUTTON_GET_PRIVATE (object);

	if (priv->toggle_button != NULL) {
		g_object_unref (priv->toggle_button);
		priv->toggle_button = NULL;
	}

	if (priv->menu != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->menu, menu_button_deactivate_cb, object);
		gtk_menu_detach (priv->menu);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
menu_button_size_request (GtkWidget *widget,
                          GtkRequisition *requisition)
{
	EMenuButtonPrivate *priv;
	GtkRequisition child_requisition;
	GtkWidget *child;

	priv = E_MENU_BUTTON_GET_PRIVATE (widget);

	/* Chain up to parent's size_request() method. */
	GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

	child = priv->toggle_button;
	gtk_widget_size_request (child, &child_requisition);
	requisition->width += child_requisition.width;
}

static void
menu_button_size_allocate (GtkWidget *widget,
                           GtkAllocation *allocation)
{
	EMenuButtonPrivate *priv;
	GtkAllocation child_allocation;
	GtkRequisition child_requisition;
	GtkWidget *child;
	gint toggle_x;

	priv = E_MENU_BUTTON_GET_PRIVATE (widget);

	widget->allocation = *allocation;

	child = priv->toggle_button;
	gtk_widget_size_request (child, &child_requisition);

	toggle_x = allocation->x + allocation->width - child_requisition.width;

	child_allocation.x = allocation->x;
	child_allocation.y = allocation->y;
	child_allocation.width = toggle_x - allocation->x;
	child_allocation.height = allocation->height;

	/* Chain up to parent's size_allocate() method. */
	GTK_WIDGET_CLASS (parent_class)->
		size_allocate (widget, &child_allocation);

	child_allocation.x = toggle_x;
	child_allocation.y = allocation->y;
	child_allocation.width = child_requisition.width;
	child_allocation.height = allocation->height;

	gtk_widget_size_allocate (child, &child_allocation);
}

static void
menu_button_state_changed (GtkWidget *widget,
                           GtkStateType previous_state)
{
	EMenuButtonPrivate *priv;

	priv = E_MENU_BUTTON_GET_PRIVATE (widget);

	if (!GTK_WIDGET_IS_SENSITIVE (widget) && priv->menu != NULL)
		gtk_menu_shell_deactivate (GTK_MENU_SHELL (priv->menu));

	/* Chain up to parent's state_changed() method. */
	GTK_WIDGET_CLASS (parent_class)->
		state_changed (widget, previous_state);
}

static void
menu_button_remove (GtkContainer *container,
                    GtkWidget *widget)
{
	EMenuButtonPrivate *priv;

	priv = E_MENU_BUTTON_GET_PRIVATE (container);

	/* Look in the internal widgets first. */

	if (widget == priv->toggle_button) {
		gtk_widget_unparent (priv->toggle_button);
		gtk_widget_queue_resize (GTK_WIDGET (container));
		return;
	}

	/* Chain up to parent's remove() method. */
	GTK_CONTAINER_CLASS (parent_class)->remove (container, widget);
}

static void
menu_button_forall (GtkContainer *container,
                    gboolean include_internals,
                    GtkCallback callback,
                    gpointer callback_data)
{
	EMenuButtonPrivate *priv;

	priv = E_MENU_BUTTON_GET_PRIVATE (container);

	if (include_internals)
		callback (priv->toggle_button, callback_data);

	/* Chain up to parent's forall() method. */
	GTK_CONTAINER_CLASS (parent_class)->forall (
		container, include_internals, callback, callback_data);
}

static void
menu_button_class_init (EMenuButtonClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMenuButtonPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = menu_button_set_property;
	object_class->get_property = menu_button_get_property;
	object_class->dispose = menu_button_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->size_request = menu_button_size_request;
	widget_class->size_allocate = menu_button_size_allocate;
	widget_class->state_changed = menu_button_state_changed;

	container_class = GTK_CONTAINER_CLASS (class);
	container_class->remove = menu_button_remove;
	container_class->forall = menu_button_forall;

	g_object_class_install_property (
		object_class,
		PROP_MENU,
		g_param_spec_object (
			"menu",
			"Menu",
			NULL,
			GTK_TYPE_MENU,
			G_PARAM_READWRITE));

	signals[SHOW_MENU] = g_signal_new (
		"show-menu",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMenuButtonClass, show_menu),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
menu_button_init (EMenuButton *menu_button)
{
	GtkWidget *container;
	GtkWidget *widget;

	menu_button->priv = E_MENU_BUTTON_GET_PRIVATE (menu_button);

	container = GTK_WIDGET (menu_button);

	widget = gtk_toggle_button_new ();
	gtk_widget_set_sensitive (widget, FALSE);
	gtk_widget_set_parent (widget, container);
	menu_button->priv->toggle_button = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		menu_button->priv->toggle_button, "button-press-event",
		G_CALLBACK (menu_button_toggle_button_press_event_cb),
		menu_button);

	g_signal_connect_swapped (
		menu_button->priv->toggle_button, "toggled",
		G_CALLBACK (menu_button_toggle_toggled_cb), menu_button);
}

GType
e_menu_button_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMenuButtonClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) menu_button_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_init */
			sizeof (EMenuButton),
			0,     /* n_preallocs */
			(GInstanceInitFunc) menu_button_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_BUTTON, "EMenuButton", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_menu_button_new (void)
{
	return g_object_new (E_TYPE_MENU_BUTTON, NULL);
}

GtkWidget *
e_menu_button_get_menu (EMenuButton *menu_button)
{
	g_return_val_if_fail (E_IS_MENU_BUTTON (menu_button), NULL);

	return GTK_WIDGET (menu_button->priv->menu);
}

void
e_menu_button_set_menu (EMenuButton *menu_button,
                        GtkWidget *menu)
{
	g_return_if_fail (E_IS_MENU_BUTTON (menu_button));
	g_return_if_fail (GTK_IS_MENU (menu) || menu == NULL);

	if (menu_button->priv->menu == GTK_MENU (menu))
		goto exit;

	if (menu_button->priv->menu != NULL) {
		GtkMenuShell *menu_shell;

		menu_shell = GTK_MENU_SHELL (menu_button->priv->menu);

		if (GTK_WIDGET_VISIBLE (menu_shell))
			gtk_menu_shell_deactivate (menu_shell);

		g_signal_handlers_disconnect_by_func (
			menu_shell, menu_button_deactivate_cb, menu_button);

		gtk_menu_detach (menu_button->priv->menu);
	}

	menu_button->priv->menu = GTK_MENU (menu);

	if (menu != NULL) {
		gtk_menu_attach_to_widget (
			GTK_MENU (menu), GTK_WIDGET (menu_button),
			(GtkMenuDetachFunc) menu_button_detach);

		g_signal_connect_swapped (
			menu, "deactivate",
			G_CALLBACK (menu_button_deactivate_cb), menu_button);
	}

	gtk_widget_set_sensitive (
		menu_button->priv->toggle_button, menu != NULL);

exit:
	g_object_notify (G_OBJECT (menu_button), "menu");
}
