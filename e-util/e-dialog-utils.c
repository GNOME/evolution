/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-dialog-utils.h
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Authors:
 *   Michael Meeks <michael@ximian.com>
 *   Ettore Perazzoli <ettore@ximian.com>
 */

#include "e-dialog-utils.h"

#include "widgets/misc/e-bonobo-widget.h"

#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdk.h>

#include <gtk/gtksignal.h>

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-property-bag.h>


#define TRANSIENT_DATA_ID "e-dialog:transient"


static void
transient_realize_callback (GtkWidget *widget)
{
	GdkWindow *window;

	window = gtk_object_get_data (GTK_OBJECT (widget), TRANSIENT_DATA_ID);
	g_assert (window != NULL);

	gdk_window_set_transient_for (GTK_WIDGET (widget)->window, window);
}

static void
transient_unrealize_callback (GtkWidget *widget)
{
	GdkWindow *window;

	window = gtk_object_get_data (GTK_OBJECT (widget), TRANSIENT_DATA_ID);
	g_assert (window != NULL);

	gdk_property_delete (window, gdk_atom_intern ("WM_TRANSIENT_FOR", FALSE));
}

static void
transient_destroy_callback (GtkWidget *widget)
{
	GdkWindow *window;
	
	window = gtk_object_get_data (GTK_OBJECT (widget), "transient");
	if (window != NULL)
		gdk_window_unref (window);
}

static void       
set_transient_for_gdk (GtkWindow *window, 
		       GdkWindow *parent)
{
	g_return_if_fail (window != NULL);
	g_return_if_fail (gtk_object_get_data (GTK_OBJECT (window), TRANSIENT_DATA_ID) == NULL);

	gdk_window_ref (parent); /* FIXME? */

	gtk_object_set_data (GTK_OBJECT (window), TRANSIENT_DATA_ID, parent);

	if (GTK_WIDGET_REALIZED (window))
		gdk_window_set_transient_for (GTK_WIDGET (window)->window, parent);

	gtk_signal_connect (GTK_OBJECT (window), "realize",
			    GTK_SIGNAL_FUNC (transient_realize_callback), NULL);

	gtk_signal_connect (GTK_OBJECT (window), "unrealize",
			    GTK_SIGNAL_FUNC (transient_unrealize_callback), NULL);
	
	gtk_signal_connect (GTK_OBJECT (window), "destroy",
			    GTK_SIGNAL_FUNC (transient_destroy_callback), NULL);
}


/**
 * e_set_dialog_parent:
 * @dialog: 
 * @parent_widget: 
 * 
 * This sets the parent for @dialog to be @parent_widget.  Unlike
 * gtk_window_set_parent(), this doesn't need @parent_widget to be the actual
 * toplevel, and also works if @parent_widget is been embedded as a Bonobo
 * control by an out-of-process container.
 **/
void
e_set_dialog_parent (GtkWindow *dialog,
		     GtkWidget *parent_widget)
{
	Bonobo_PropertyBag property_bag;
	GtkWidget *toplevel;
	GdkWindow *gdk_window;
	CORBA_char *id;
	guint32 xid;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GTK_IS_WINDOW (dialog));
	g_return_if_fail (parent_widget != NULL);
	g_return_if_fail (GTK_IS_WIDGET (parent_widget));

	toplevel = gtk_widget_get_toplevel (parent_widget);
	if (toplevel == NULL)
		return;

	if (! BONOBO_IS_CONTROL (toplevel)) {
		if (GTK_IS_WINDOW (toplevel))
			gtk_window_set_transient_for (dialog, GTK_WINDOW (toplevel));
		return;
	}

	property_bag = bonobo_control_get_ambient_properties (BONOBO_CONTROL (toplevel), NULL);
	if (property_bag == CORBA_OBJECT_NIL)
		return;

	id = bonobo_property_bag_client_get_value_string (property_bag, E_BONOBO_WIDGET_TOPLEVEL_PROPERTY_ID, NULL);
	if (id == NULL)
		return;

	xid = strtol (id, NULL, 10);

	g_warning ("Got id `%s' -> %x", id, xid);

	gdk_window = gdk_window_foreign_new (xid);
	set_transient_for_gdk (dialog, gdk_window);
}

void
e_set_transient_for_with_xid ()
