/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * folder-browser-factory.c: A Bonobo Control factory for Folder Browsers
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
/*
 * bonobo-clock-control.c
 *
 * Copyright 1999, Helix Code, Inc.
 *
 * Author:
 *   Nat Friedman (nat@nat.org)
 */

#include <config.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo.h>

#include <libgnomeui/gtk-clock.h>

#include "addressbook-widget.h"
#include "addressbook.h"

#if 0
static void
bonobo_clock_control_prop_value_changed_cb (BonoboPropertyBag *pb, char *name, char *type,
					    gpointer old_value, gpointer new_value,
					    gpointer user_data)
{
	GtkClock *clock = user_data;

	if (! strcmp (name, "running")) {
		gboolean *b = new_value;

		if (*b)
			gtk_clock_start (clock);
		else
			gtk_clock_stop (clock);
	}
}

/*
 * Callback routine used to release any values we associated with the control
 * dynamically.
 */
static void
release_data (GtkObject *object, void *data)
{
	g_free (data);
}
#endif

static BonoboObject *
addressbook_factory (BonoboGenericFactory *Factory, void *closure)
{
#if 0
	BonoboPropertyBag  *pb;
	CORBA_boolean	  *running;
#endif
	BonoboControl      *control;
	View               *view;

	/* Create the control. */
	view = create_view();
	control = bonobo_control_new (view->widget);
#if 0
	/* Create the properties. */
	pb = bonobo_property_bag_new ();
	bonobo_control_set_property_bag (control, pb);

	gtk_signal_connect (GTK_OBJECT (pb), "value_changed",
			    bonobo_clock_control_prop_value_changed_cb,
			    clock);

	running = g_new0 (CORBA_boolean, 1);
	*running = TRUE;
	bonobo_property_bag_add (pb, "running", "boolean",
				(gpointer) running,
				NULL, "Whether or not the clock is running", 0);

	/*
	 * Release "running" when the object is destroyed
	 */
	gtk_signal_connect (GTK_OBJECT (pb), "destroy", GTK_SIGNAL_FUNC (release_data), running);
#endif

	return BONOBO_OBJECT (control);
}

void
addressbook_factory_init (void)
{
	static BonoboGenericFactory *addressbook_control_factory = NULL;

	if (addressbook_control_factory != NULL)
		return;

	addressbook_control_factory =
		bonobo_generic_factory_new (
			"control-factory:addressbook",
			addressbook_factory, NULL);

	if (addressbook_control_factory == NULL) {
		g_error ("I could not register a Addressbook factory.");
	}
}
