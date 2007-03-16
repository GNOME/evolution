/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-gui-utils.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas-pixbuf.h>

#include "e-gui-utils.h"

void
e_auto_kill_popup_menu_on_selection_done (GtkMenu *menu)
{
	g_return_if_fail (GTK_IS_MENU (menu));
	
	g_signal_connect (menu, "selection_done", G_CALLBACK (gtk_widget_destroy), menu);
}

void
e_popup_menu (GtkMenu *menu, GdkEvent *event)
{
	g_return_if_fail (GTK_IS_MENU (menu));

	e_auto_kill_popup_menu_on_selection_done (menu);

	if (event) {
		if (event->type == GDK_KEY_PRESS)
			gtk_menu_popup (menu, NULL, NULL, 0, NULL, 0,
					event->key.time);
		else if ((event->type == GDK_BUTTON_PRESS) ||
			 (event->type == GDK_BUTTON_RELEASE) ||
			 (event->type == GDK_2BUTTON_PRESS) ||
			 (event->type == GDK_3BUTTON_PRESS)){
			gtk_menu_popup (menu, NULL, NULL, 0, NULL,
					event->button.button,
					event->button.time);
		}
	} else
		gtk_menu_popup (menu, NULL, NULL, 0, NULL, 0,
				GDK_CURRENT_TIME);
}

typedef struct {
	GtkCallback callback;
	gpointer closure;
} CallbackClosure;

static void
e_container_foreach_leaf_callback(GtkWidget *widget, CallbackClosure *callback_closure)
{
	if (GTK_IS_CONTAINER(widget)) {
		e_container_foreach_leaf(GTK_CONTAINER(widget), callback_closure->callback, callback_closure->closure);
	} else {
		(*callback_closure->callback) (widget, callback_closure->closure);
	}
}

void
e_container_foreach_leaf(GtkContainer *container,
			 GtkCallback callback,
			 gpointer closure)
{
	CallbackClosure callback_closure;
	callback_closure.callback = callback;
	callback_closure.closure = closure;
	gtk_container_foreach(container, (GtkCallback) e_container_foreach_leaf_callback, &callback_closure);
}

static void
e_container_change_tab_order_destroy_notify(gpointer data)
{
	GList *list = data;
	g_list_foreach(list, (GFunc) g_object_unref, NULL);
	g_list_free(list);
}


static gint
e_container_change_tab_order_callback(GtkContainer *container,
				      GtkDirectionType direction,
				      GList *children)
{
	GtkWidget *focus_child;
	GtkWidget *child;

	if (direction != GTK_DIR_TAB_FORWARD &&
	    direction != GTK_DIR_TAB_BACKWARD)
		return FALSE;

	focus_child = container->focus_child;

	if (focus_child == NULL)
		return FALSE;

	if (direction == GTK_DIR_TAB_BACKWARD) {
		children = g_list_last(children);
	}

	while (children) {
		child = children->data;
		if (direction == GTK_DIR_TAB_FORWARD)
			children = children->next;
		else
			children = children->prev;

		if (!child)
			continue;

		if (focus_child) {
			if (focus_child == child) {
				focus_child = NULL;

				if (GTK_WIDGET_DRAWABLE (child) &&
				    GTK_IS_CONTAINER (child) &&
				    !GTK_WIDGET_HAS_FOCUS (child))
					if (gtk_widget_child_focus (GTK_WIDGET (child), direction)) {
						g_signal_stop_emission_by_name (container, "focus");
						return TRUE;
					}
			}
		}
		else if (GTK_WIDGET_DRAWABLE (child)) {
			if (GTK_IS_CONTAINER (child)) {
				if (gtk_widget_child_focus (GTK_WIDGET (child), direction)) {
					g_signal_stop_emission_by_name (container, "focus");
					return TRUE;
				}
			}
			else if (GTK_WIDGET_CAN_FOCUS (child)) {
				gtk_widget_grab_focus (child);
				g_signal_stop_emission_by_name (container, "focus");
				return TRUE;
			}
		}
	}

	return FALSE;
}

gint
e_container_change_tab_order(GtkContainer *container, GList *widgets)
{
	GList *list;
	list = g_list_copy(widgets);
	g_list_foreach(list, (GFunc) g_object_ref, NULL);
	return gtk_signal_connect_full(GTK_OBJECT(container), "focus",
				       GTK_SIGNAL_FUNC(e_container_change_tab_order_callback),
				       NULL, list,
				       e_container_change_tab_order_destroy_notify,
				       FALSE, FALSE);
}

struct widgetandint {
	GtkWidget *widget;
	int count;
};

static void
nth_entry_callback(GtkWidget *widget, struct widgetandint *data)
{
	if (GTK_IS_ENTRY(widget)) {
		if (data->count > 1) {
			data->count --;
			data->widget = widget;
		} else if (data->count == 1) {
			data->count --;
			data->widget = NULL;
			gtk_widget_grab_focus(widget);
		}
	}
}

void
e_container_focus_nth_entry(GtkContainer *container, int n)
{
	struct widgetandint data;
	data.widget = NULL;
	data.count = n;
	e_container_foreach_leaf(container, (GtkCallback) nth_entry_callback, &data);
	if (data.widget)
		gtk_widget_grab_focus(data.widget);
}

gboolean
e_glade_xml_connect_widget (GladeXML *gui, char *name, char *signal, GCallback cb, gpointer closure)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget (gui, name);

	if (widget) {
		g_signal_connect (widget, signal,
				  cb, closure);
		return TRUE;
	}

	return FALSE;
}

gboolean
e_glade_xml_set_sensitive (GladeXML *gui, char *name, gboolean sensitive)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget (gui, name);

	if (widget) {
		gtk_widget_set_sensitive (widget, sensitive);
		return TRUE;
	}

	return FALSE;
}
