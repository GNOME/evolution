/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-bonobo-widget.c
 *
 * Copyright (C) 2001  Ximian, Inc.
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
 * Authors:
 *   Michael Meeks <michael@ximian.com>
 *   Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-bonobo-widget.h"

#include <bonobo/bonobo-control.h>

#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#include <gdk/gdktypes.h>


#define PARENT_TYPE bonobo_widget_get_type ()
static BonoboWidgetClass *parent_class = NULL;


static void
class_init (GtkObjectClass *object_class)
{
	parent_class = g_type_class_ref(PARENT_TYPE);

	/* No method to override.  */
}

static void
init (EBonoboWidget *bonobo_widget)
{
	/* Nothing to initialize.  */
}


static void
evolution_toplevel_property_get_fn (BonoboPropertyBag *bag,
				    BonoboArg *arg,
				    unsigned int arg_id,
				    CORBA_Environment *ev,
				    void *data)
{
	BonoboControlFrame *frame;
	GtkWidget *toplevel;
	char *id;

	frame = BONOBO_CONTROL_FRAME (data);

	toplevel = bonobo_control_frame_get_widget (frame);
	while (toplevel->parent != NULL)
		toplevel = toplevel->parent;

	if (BONOBO_IS_CONTROL (toplevel)) {
		Bonobo_PropertyBag toplevel_property_bag;

		toplevel_property_bag = bonobo_control_get_ambient_properties (BONOBO_CONTROL (toplevel), NULL);
		if (toplevel_property_bag == CORBA_OBJECT_NIL)
			goto error;
		
		id = bonobo_property_bag_client_get_value_string (toplevel_property_bag,
								  E_BONOBO_WIDGET_TOPLEVEL_PROPERTY_ID,
								  NULL);
		if (id == NULL)
			goto error;
			
		*(char **)arg->_value = id;
		return;
	}

	id = bonobo_control_windowid_from_x11 (GDK_WINDOW_XWINDOW (toplevel->window));
	*(char **)arg->_value = CORBA_string_dup (id);
	g_free (id);

	return;

 error:
	/* FIXME: exception? */
	*(char **)arg->_value = CORBA_string_dup ("");
}

static void
setup_toplevel_property (EBonoboWidget *widget)
{
	BonoboPropertyBag *property_bag;
	BonoboControlFrame *control_frame;

	control_frame = bonobo_widget_get_control_frame (BONOBO_WIDGET (widget));

	if (!(property_bag = bonobo_control_frame_get_propbag (control_frame))) {
		property_bag = bonobo_property_bag_new (NULL, NULL, NULL);
		bonobo_control_frame_set_propbag (control_frame, property_bag);
	}

	/* FIXME: Kill property bag when the frame dies.  */

	bonobo_property_bag_add_full (property_bag, E_BONOBO_WIDGET_TOPLEVEL_PROPERTY_ID, 0,
				      TC_Bonobo_Control_windowId,
				      NULL, "Toplevel Window ID",
				      BONOBO_PROPERTY_READABLE,
				      evolution_toplevel_property_get_fn, NULL,
				      control_frame);
}


EBonoboWidget *
e_bonobo_widget_construct_control_from_objref (EBonoboWidget *widget,
					       Bonobo_Control control,
					       Bonobo_UIContainer uic)
{
	g_return_val_if_fail (widget != NULL, NULL);
	g_return_val_if_fail (E_IS_BONOBO_WIDGET (widget), NULL);

	if (bonobo_widget_construct_control_from_objref (BONOBO_WIDGET (widget), control, uic) == NULL)
		return NULL;

	setup_toplevel_property (widget);
	return widget;
}

EBonoboWidget *
e_bonobo_widget_construct_control (EBonoboWidget *widget,
				   const char *moniker,
				   Bonobo_UIContainer uic)
{
	g_return_val_if_fail (widget != NULL, NULL);
	g_return_val_if_fail (E_IS_BONOBO_WIDGET (widget), NULL);
	g_return_val_if_fail (moniker != NULL, NULL);

	if (bonobo_widget_construct_control (BONOBO_WIDGET (widget), moniker, uic) == NULL)
		return NULL;

	setup_toplevel_property (widget);

	return widget;
}


GtkWidget *
e_bonobo_widget_new_control (const char *moniker,
			     Bonobo_UIContainer uic)
{
	EBonoboWidget *widget;

	g_return_val_if_fail (moniker != NULL, NULL);

	widget = gtk_type_new (e_bonobo_widget_get_type ());
	widget = e_bonobo_widget_construct_control (widget, moniker, uic);

	if (widget == NULL)
		return NULL;

	return GTK_WIDGET (widget);
}

GtkWidget *
e_bonobo_widget_new_control_from_objref (Bonobo_Control control,
					 Bonobo_UIContainer  uic)
{
	EBonoboWidget *widget;

	g_return_val_if_fail (control != CORBA_OBJECT_NIL, NULL);

	widget = gtk_type_new (E_TYPE_BONOBO_WIDGET);

	widget = e_bonobo_widget_construct_control_from_objref (widget, control, uic);
	if (widget == NULL)
		return NULL;

	return GTK_WIDGET (widget);
}


E_MAKE_TYPE (e_bonobo_widget, "EBonoboWidget", EBonoboWidget, class_init, init, PARENT_TYPE)
