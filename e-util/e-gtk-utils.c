/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-gtk-utils.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtklayout.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkalignment.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#endif

#include "e-gtk-utils.h"


void
e_signal_connect_while_alive (void *instance,
			      const char *name,
			      GCallback callback,
			      void *callback_data,
			      void *alive_instance)
{
	GClosure *closure;

	g_return_if_fail (GTK_IS_OBJECT (instance));

	closure = g_cclosure_new (callback, callback_data, NULL);
	g_object_watch_closure (alive_instance, closure);
	g_signal_connect_closure_by_id (instance, g_signal_lookup (name, G_OBJECT_TYPE (instance)), 0,
					closure, FALSE);
}


/* (Cut and pasted from Gtk.)  */

typedef struct DisconnectInfo {
	unsigned int signal_handler;

	GtkObject *object1;
	unsigned int disconnect_handler1;

	GtkObject *object2;
	unsigned int disconnect_handler2;
} DisconnectInfo;

static unsigned int
alive_disconnecter (GtkObject *object,
		    DisconnectInfo *info)
{
	g_assert (info != NULL);
	
	g_signal_handler_disconnect (info->object1, info->disconnect_handler1);
	g_signal_handler_disconnect (info->object1, info->signal_handler);
	g_signal_handler_disconnect (info->object2, info->disconnect_handler2);
	
	g_free (info);
	
	return 0;
}

/**
 * e_gtk_signal_connect_full_while_alive:
 * @object: 
 * @name: 
 * @func: 
 * @marshal: 
 * @data: 
 * @destroy_func: 
 * @object_signal: 
 * @after: 
 * @alive_object: 
 * 
 * Connect a signal like `gtk_signal_connect_while_alive()', but with full
 * params like `gtk_signal_connect_full()'.
 **/
void
e_signal_connect_full_while_alive (void *instance,
				   const char *name,
				   GtkSignalFunc func,
				   GtkCallbackMarshal marshal,
				   void *data,
				   GtkDestroyNotify destroy_func,
				   gboolean instance_signal,
				   gboolean after,
				   void *alive_instance)
{
	DisconnectInfo *info;
	
	g_return_if_fail (GTK_IS_OBJECT (instance));
	g_return_if_fail (name != NULL);
	g_return_if_fail (func != NULL);
	g_return_if_fail (GTK_IS_OBJECT (alive_instance));
	
	info = g_new (DisconnectInfo, 1);

	info->signal_handler = gtk_signal_connect_full (instance, name,
							func, marshal, data,
							destroy_func,
							instance_signal, after);

	info->object1 = instance;
	info->disconnect_handler1 = g_signal_connect (instance, "destroy",
						      G_CALLBACK (alive_disconnecter), info);

	info->object2 = alive_instance;
	info->disconnect_handler2 = g_signal_connect (alive_instance, "destroy",
						      G_CALLBACK (alive_disconnecter), info);
}


#ifdef GDK_WINDOWING_X11
/* BackingStore support.  */

static void
widget_realize_callback_for_backing_store (GtkWidget *widget,
					   void *data)
{
	XSetWindowAttributes attributes;
	GdkWindow *window;

	if (GTK_IS_LAYOUT (widget))
		window = GTK_LAYOUT (widget)->bin_window;
	else
		window = widget->window;

	attributes.backing_store = Always;
	XChangeWindowAttributes (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XWINDOW (window),
				 CWBackingStore, &attributes);
}

#endif

/**
 * e_make_widget_backing_stored:
 * @widget: A GtkWidget
 * 
 * Make sure that the window for @widget has the BackingStore attribute set to
 * Always when realized.  This will allow the widget to be refreshed by the X
 * server even if the application is currently not responding to X events (this
 * is e.g. very useful for the splash screen).
 *
 * Notice that this will not work 100% in all cases as the server might not
 * support that or just refuse to do so.
 **/
void
e_make_widget_backing_stored  (GtkWidget *widget)
{
#ifdef GDK_WINDOWING_X11
	g_signal_connect (widget, "realize", G_CALLBACK (widget_realize_callback_for_backing_store), NULL);
#endif
}


/**
 * e_gtk_button_new_with_icon:
 * @text: The mnemonic text for the label.
 * @stock: The name of the stock item to get the icon from.
 * 
 * Create a gtk button with a custom label and a stock icon.
 *
 * 
 * Return value: The widget.
 **/
GtkWidget *
e_gtk_button_new_with_icon(const char *text, const char *stock)
{
	GtkWidget *button, *label;
	GtkStockItem item;

	button = gtk_button_new();
	label = gtk_label_new_with_mnemonic(text);
	gtk_label_set_mnemonic_widget((GtkLabel *)label, button);

	if (gtk_stock_lookup(stock, &item)) {
		GtkWidget *image, *hbox, *align;

		image = gtk_image_new_from_stock(stock, GTK_ICON_SIZE_BUTTON);
		hbox = gtk_hbox_new(FALSE, 2);
		align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
		gtk_box_pack_start((GtkBox *)hbox, image, FALSE, FALSE, 0);
		gtk_box_pack_end((GtkBox *)hbox, label, FALSE, FALSE, 0);
		gtk_container_add((GtkContainer *)align, hbox);
		gtk_container_add((GtkContainer *)button, align);
		gtk_widget_show_all(align);
	} else {
		gtk_misc_set_alignment((GtkMisc *)label, 0.5, 0.5);
		gtk_container_add((GtkContainer *)button, label);
		gtk_widget_show(label);
	}

	return button;
}
