/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* control-factory.c
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

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-init.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <glade/glade.h>

#include <liboaf/liboaf.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-exception.h>

#include <gal/widgets/e-cursors.h>

#include "alarm-notify/alarm.h"
#include "calendar-commands.h"
#include "calendar-config.h"
#include "component-factory.h"
#include "e-comp-editor-registry.h"
#include "comp-editor-factory.h"
#include "control-factory.h"
#include "itip-control-factory.h"
#include "tasks-control-factory.h"

ECompEditorRegistry *comp_editor_registry = NULL;

/* The component editor factory */
static CompEditorFactory *comp_editor_factory = NULL;


static void
init_bonobo (int argc, char **argv)
{
	if (gnome_init_with_popt_table ("evolution-calendar", VERSION, argc, argv,
					oaf_popt_options, 0, NULL) != 0)
		g_error (_("Could not initialize GNOME"));

	oaf_init (argc, argv);

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));
}

/* Factory function for the calendar component factory; just creates and
 * references a singleton service object.
 */
static BonoboObject *
comp_editor_factory_fn (BonoboGenericFactory *factory, void *data)
{
	if (!comp_editor_factory) {
		comp_editor_factory = comp_editor_factory_new ();
		if (!comp_editor_factory)
			return NULL;
	}

	bonobo_object_ref (BONOBO_OBJECT (comp_editor_factory));
	return BONOBO_OBJECT (comp_editor_factory);
}

/* Creates and registers the component editor factory */
static void
component_editor_factory_init (void)
{
	BonoboGenericFactory *factory;

	factory = bonobo_generic_factory_new (
		"OAFIID:GNOME_Evolution_Calendar_CompEditorFactory_Factory",
		comp_editor_factory_fn, NULL);
	if (!factory)
		g_error (_("Could not create the component editor factory"));
}

/* Does a simple activation and unreffing of the alarm notification service so
 * that the daemon will be launched if it is not running yet.
 */
static void
launch_alarm_daemon (void)
{
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_AlarmNotify an;

	CORBA_exception_init (&ev);
	an = oaf_activate_from_id ("OAFIID:GNOME_Evolution_Calendar_AlarmNotify", 0, NULL, &ev);

	if (BONOBO_EX (&ev)) {
		g_message ("add_alarms(): Could not activate the alarm notification service");
		CORBA_exception_free (&ev);
		return;
	}
	CORBA_exception_free (&ev);

	/* Just get rid of it; what we are interested in is that it gets launched */

	CORBA_exception_init (&ev);
	bonobo_object_release_unref (an, &ev);
	if (BONOBO_EX (&ev))
		g_message ("add_alarms(): Could not unref the alarm notification service");

	CORBA_exception_free (&ev);
}

int
main (int argc, char **argv)
{
	free (malloc (8));

	bindtextdomain(PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain(PACKAGE);

	init_bonobo (argc, argv);

	if (!gnome_vfs_init ())
		g_error (_("Could not initialize gnome-vfs"));

	glade_gnome_init ();
	e_cursors_init ();

#if 0
	//g_log_set_always_fatal ((GLogLevelFlags) 0xFFFF);
	g_log_set_always_fatal (G_LOG_LEVEL_ERROR |
				G_LOG_LEVEL_CRITICAL |
				G_LOG_LEVEL_WARNING);
#endif

	comp_editor_registry = E_COMP_EDITOR_REGISTRY (e_comp_editor_registry_new ());
	
	calendar_config_init ();

	control_factory_init ();
	component_factory_init ();
	itip_control_factory_init ();
	tasks_control_factory_init ();
	component_editor_factory_init ();

	launch_alarm_daemon ();

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	bonobo_main ();

	alarm_done ();
	calendar_config_write_on_exit ();

	gnome_vfs_shutdown ();

	return 0;
}
