/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-test-component.c
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
 * Author: Ettore Perazzoli
 */

/* Simple test component for the Evolution shell.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "evolution-shell-component.h"

#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>

#include <gdk-pixbuf/gdk-pixbuf.h>


#define COMPONENT_FACTORY_ID "OAFIID:GNOME_Evolution_TestComponent_ShellComponentFactory"
#define COMPONENT_ID         "OAFIID:GNOME_Evolution_TestComponent_ShellComponent"

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "test", "/usr/share/pixmaps/gnome-money.png", NULL, NULL },
	{ NULL }
};


static EvolutionShellClient *parent_shell = NULL;
static GNOME_Evolution_Activity activity_interface = CORBA_OBJECT_NIL;

static CORBA_long activity_id = 0;

static BonoboListener *task_bar_event_listener;

static int timeout_id = 0;
static int progress = -1;


static void
create_icon_from_pixbuf (GdkPixbuf *pixbuf,
			 GNOME_Evolution_Icon *frame_return)
{
	const char *sp;
	CORBA_octet *dp;
	int width, height, total_width, rowstride;
	int i, j;
	gboolean has_alpha;

	width     = gdk_pixbuf_get_width (pixbuf);
	height    = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

	if (has_alpha)
		total_width = 4 * width;
	else
		total_width = 3 * width;

	frame_return->width = width;
	frame_return->height = height;
	frame_return->hasAlpha = has_alpha;

	frame_return->rgba_data._length = frame_return->height * total_width;
	frame_return->rgba_data._maximum = frame_return->rgba_data._length;
	frame_return->rgba_data._buffer = CORBA_sequence_CORBA_octet_allocbuf (frame_return->rgba_data._maximum);

	sp = gdk_pixbuf_get_pixels (pixbuf);
	dp = frame_return->rgba_data._buffer;
	for (i = 0; i < height; i ++) {
		for (j = 0; j < total_width; j++) {
			*(dp ++) = sp[j];
		}
		sp += rowstride;
	}
}

static GNOME_Evolution_AnimatedIcon *
create_animated_icon (void)
{
	GNOME_Evolution_AnimatedIcon *animated_icon;
	GdkPixbuf *pixbuf;

	animated_icon = GNOME_Evolution_AnimatedIcon__alloc ();

	animated_icon->_length = 1;
	animated_icon->_maximum = 1;
	animated_icon->_buffer = CORBA_sequence_GNOME_Evolution_Icon_allocbuf (animated_icon->_maximum);

	pixbuf = gdk_pixbuf_new_from_file (gnome_pixmap_file ("gnome-money.png"));
	create_icon_from_pixbuf (pixbuf, &animated_icon->_buffer[0]);
	gdk_pixbuf_unref (pixbuf);

	CORBA_sequence_set_release (animated_icon, TRUE);

	return animated_icon;
}


static void
task_bar_event_listener_callback (BonoboListener *listener,
				  char *event_name,
				  CORBA_any *any,
				  CORBA_Environment *ev,
				  void *data)
{
	g_print ("Taskbar event -- %s\n", event_name);
}

/* Timeout #3: We are done.  */
static int
timeout_callback_3 (void *data)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Activity_operationFinished (activity_interface,
						    activity_id,
						    &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot report operation as finished; exception returned -- %s\n",
			   ev._repo_id);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	return FALSE;
}

/* Timeout #2: Update the progress until it reaches 100%.  */
static int
timeout_callback_2 (void *data)
{
	CORBA_Environment ev;

	if (progress < 0)
		progress = 0;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Activity_operationProgressing (activity_interface,
						       activity_id,
						       "Operation Foo in progress",
						       (CORBA_float) progress / 100.0,
						       &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot update operation; exception returned -- %s\n",
			   ev._repo_id);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	progress += 10;
	if (progress > 100) {
		gtk_timeout_add (1000, timeout_callback_3, NULL);
		return FALSE;
	}

	return TRUE;
}

/* Timeout #1: Set busy.  */
static int
timeout_callback_1 (void *data)
{
	CORBA_boolean suggest_display;
	CORBA_Environment ev;
	GNOME_Evolution_AnimatedIcon *animated_icon;

	CORBA_exception_init (&ev);

	if (CORBA_Object_is_nil (activity_interface, &ev)) {
		CORBA_exception_free (&ev);
		return FALSE;
	}

	g_print ("Component becoming busy -- %s\n", COMPONENT_ID);

	task_bar_event_listener = bonobo_listener_new (task_bar_event_listener_callback, NULL);

	animated_icon = create_animated_icon ();

	GNOME_Evolution_Activity_operationStarted (activity_interface,
						   COMPONENT_ID,
						   animated_icon,
						   "Operation Foo started!",
						   FALSE,
						   bonobo_object_corba_objref (BONOBO_OBJECT (task_bar_event_listener)),
						   &activity_id,
						   &suggest_display,
						   &ev);

	CORBA_free (animated_icon);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot start an operation; exception returned -- %s\n",
			   ev._repo_id);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	g_print (" --> Activity ID: %ld\n", (long) activity_id);

	if (suggest_display)
		g_print (" --> Could display dialog box.\n");

	CORBA_exception_free (&ev);

	gtk_timeout_add (3000, timeout_callback_2, NULL);

	return FALSE;
}


static EvolutionShellComponentResult
create_view_fn (EvolutionShellComponent *shell_component,
		const char *physical_uri,
		const char *folder_type,
		BonoboControl **control_return,
		void *closure)
{
	GtkWidget *vbox;
	GtkWidget *label_1, *label_2;
	GtkWidget *event_box_1, *event_box_2;

	label_1 = gtk_label_new ("This is just a test component, displaying the following URI:");
	label_2 = gtk_label_new (physical_uri);

	event_box_1 = gtk_event_box_new ();
	event_box_2 = gtk_event_box_new ();

	vbox = gtk_vbox_new (FALSE, 5);

	gtk_box_pack_start (GTK_BOX (vbox), event_box_1, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label_1, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label_2, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), event_box_2, TRUE, TRUE, 0);

	gtk_widget_show (label_1);
	gtk_widget_show (label_2);
	gtk_widget_show (event_box_1);
	gtk_widget_show (event_box_2);

	gtk_widget_show (vbox);

	*control_return = bonobo_control_new (vbox);

	g_assert (timeout_id == 0);
	timeout_id = gtk_timeout_add (2000, timeout_callback_1, NULL);

	return EVOLUTION_SHELL_COMPONENT_OK;
}


/* Callbacks.  */

static void
owner_set_callback (EvolutionShellComponent *shell_component,
		    EvolutionShellClient *shell_client,
		    const char *evolution_homedir)
{
	CORBA_Environment ev;

	g_assert (parent_shell == NULL);

	g_print ("We have an owner -- home directory is `%s'\n", evolution_homedir);

	parent_shell = shell_client;

	CORBA_exception_init (&ev);

	activity_interface = Bonobo_Unknown_queryInterface (bonobo_object_corba_objref (BONOBO_OBJECT (shell_client)),
							    "IDL:GNOME/Evolution/Activity:1.0",
							    &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		activity_interface = CORBA_OBJECT_NIL;

	if (CORBA_Object_is_nil (activity_interface, &ev))
		g_warning ("Shell doesn't have a ::Activity interface -- weird!");

	CORBA_exception_free (&ev);
}

static int
owner_unset_idle_callback (void *data)
{
	gtk_main_quit ();
	return FALSE;
}

static void
owner_unset_callback (EvolutionShellComponent *shell_component,
		      void *data)
{
	g_idle_add_full (G_PRIORITY_LOW, owner_unset_idle_callback, NULL, NULL);
}


static BonoboObject *
factory_fn (BonoboGenericFactory *factory,
	    void *closure)
{
	EvolutionShellComponent *shell_component;

	shell_component = evolution_shell_component_new (folder_types,
							 create_view_fn,
							 NULL, NULL, NULL, NULL, NULL, NULL);

	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
			    GTK_SIGNAL_FUNC (owner_unset_callback), NULL);

	return BONOBO_OBJECT (shell_component);
}

static void
component_factory_init (void)
{
	BonoboGenericFactory *factory;

	factory = bonobo_generic_factory_new (COMPONENT_FACTORY_ID, factory_fn, NULL);

	if (factory == NULL)
		g_error ("Cannot initialize test component.");
}


int
main (int argc, char **argv)
{
	CORBA_ORB orb;

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	gnome_init_with_popt_table ("evolution-test-component", VERSION,
				    argc, argv, oaf_popt_options, 0, NULL);

	orb = oaf_init (argc, argv);

	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error ("Cannot initialize the test component.");

	component_factory_init ();

	bonobo_main ();

	return 0;
}
