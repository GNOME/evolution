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


static void
control_deactivate (BonoboControl *control, BonoboUIHandler *uih)
{
	/* how to remove a menu item */
	bonobo_ui_handler_menu_remove (uih, "/Actions/New Contact"); 

	/* remove our toolbar */
	bonobo_ui_handler_dock_remove (uih, "/Toolbar");
}

static void
do_nothing_cb (BonoboUIHandler *uih, void *user_data, const char *path)
{
	printf ("Yow! I am called back!\n");
}

static GnomeUIInfo gnome_toolbar [] = {
	GNOMEUIINFO_ITEM_STOCK (N_("New"), N_("Create a new contact"), do_nothing_cb, GNOME_STOCK_PIXMAP_NEW),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_STOCK (N_("Find"), N_("Find a contact"), do_nothing_cb, GNOME_STOCK_PIXMAP_SEARCH),
	GNOMEUIINFO_ITEM_STOCK (N_("Print"), N_("Print contacts"), do_nothing_cb, GNOME_STOCK_PIXMAP_PRINT),
	GNOMEUIINFO_ITEM_STOCK (N_("Delete"), N_("Delete a contact"), do_nothing_cb, GNOME_STOCK_PIXMAP_TRASH),

	GNOMEUIINFO_END
};




static void
control_activate (BonoboControl *control, BonoboUIHandler *uih)
{
	Bonobo_UIHandler  remote_uih;
	GtkWidget *toolbar;
	BonoboControl *toolbar_control;
	
	remote_uih = bonobo_control_get_remote_ui_handler (control);
	bonobo_ui_handler_set_container (uih, remote_uih);		

	bonobo_ui_handler_menu_new_item (uih, "/Actions/New Contact", N_("_New Contact"),       
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, do_nothing_cb, NULL);

	toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL,
				   GTK_TOOLBAR_BOTH);

	gnome_app_fill_toolbar (GTK_TOOLBAR (toolbar),
				gnome_toolbar, 
				NULL);
	
	gtk_widget_show_all (toolbar);

	toolbar_control = bonobo_control_new (toolbar);
	bonobo_ui_handler_dock_add (
		uih, "/Toolbar",
		bonobo_object_corba_objref (BONOBO_OBJECT (toolbar_control)),
		GNOME_DOCK_ITEM_BEH_LOCKED |
		GNOME_DOCK_ITEM_BEH_EXCLUSIVE,
		GNOME_DOCK_TOP,
		1, 1, 0);
}

static void
control_activate_cb (BonoboControl *control, 
		     gboolean activate, 
		     gpointer user_data)
{
	BonoboUIHandler  *uih;

	uih = bonobo_control_get_ui_handler (control);
	g_assert (uih);
	
	if (activate)
		control_activate (control, uih);
	else
		control_deactivate (control, uih);
}


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

	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    control_activate_cb, NULL);	
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
