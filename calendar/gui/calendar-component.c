/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* calendar-component.c
 *
 * Copyright (C) 2003  Ettore Perazzoli
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef CONFIG_H
#include <config.h>
#endif

#include "calendar-component.h"
#include "control-factory.h"
#include "gnome-cal.h"
#include "migration.h"

#include "widgets/misc/e-source-selector.h"

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-i18n.h>
#include <gal/util/e-util.h>

#include <errno.h>


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;


struct _CalendarComponentPrivate {
	char *config_directory;

	GConfClient *gconf_client;
	ESourceList *source_list;
};


/* Utility functions.  */

static void
load_uri_for_source (ESource *source, BonoboControl *view_control)
{
	GnomeCalendar *gcal;
	char *uri = e_source_get_uri (source);

	gcal = (GnomeCalendar *) bonobo_control_get_widget (view_control);
	gnome_calendar_add_event_uri (gcal, uri);
	g_free (uri);
}

static void
load_uri_for_selection (ESourceSelector *selector, BonoboControl *view_control)
{
	GSList *selection, *l;
	
	selection = e_source_selector_get_selection (selector);
	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;
		
		load_uri_for_source (selected_source, view_control);
	}	
}

/* Callbacks.  */
static void
source_selection_changed_callback (ESourceSelector *selector,
				   BonoboControl *view_control)
{
	
	load_uri_for_selection (selector, view_control);
}

static void
primary_source_selection_changed_callback (ESourceSelector *selector,
					   BonoboControl *view_control)
{
	ESource *source;
	GnomeCalendar *gcal;
	ECalModel *model;
	CalClient *client;

	source = e_source_selector_peek_primary_selection (selector);
	if (!source)
		return;

	/* set the default client on the GnomeCalendar */
	gcal = (GnomeCalendar *) bonobo_control_get_widget (view_control);
	if (!GNOME_IS_CALENDAR (gcal))
		return;

	model = gnome_calendar_get_calendar_model (gcal);
	client = e_cal_model_get_client_for_uri (model, e_source_get_uri (source));
	if (client)
		gnome_calendar_set_default_client (gcal, client);
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	CalendarComponentPrivate *priv = CALENDAR_COMPONENT (object)->priv;

	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}

	if (priv->gconf_client != NULL) {
		g_object_unref (priv->gconf_client);
		priv->gconf_client = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	CalendarComponentPrivate *priv = CALENDAR_COMPONENT (object)->priv;

	g_free (priv->config_directory);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Evolution::Component CORBA methods.  */

static void
impl_createControls (PortableServer_Servant servant,
		     Bonobo_Control *corba_sidebar_control,
		     Bonobo_Control *corba_view_control,
		     CORBA_Environment *ev)
{
	CalendarComponent *calendar_component = CALENDAR_COMPONENT (bonobo_object_from_servant (servant));
	GtkWidget *selector;
	GtkWidget *selector_scrolled_window;
	BonoboControl *sidebar_control;
	BonoboControl *view_control;

	selector = e_source_selector_new (calendar_component->priv->source_list);
	gtk_widget_show (selector);

	selector_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (selector_scrolled_window), selector);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (selector_scrolled_window);

	sidebar_control = bonobo_control_new (selector_scrolled_window);

	view_control = control_factory_new_control ();

	g_signal_connect_object (selector, "selection_changed",
				 G_CALLBACK (source_selection_changed_callback), 
				 G_OBJECT (view_control), 0);
	g_signal_connect_object (selector, "primary_selection_changed",
				 G_CALLBACK (primary_source_selection_changed_callback), 
				 G_OBJECT (view_control), 0);

	load_uri_for_selection (E_SOURCE_SELECTOR (selector), view_control);

	*corba_sidebar_control = CORBA_Object_duplicate (BONOBO_OBJREF (sidebar_control), ev);
	*corba_view_control = CORBA_Object_duplicate (BONOBO_OBJREF (view_control), ev);
}


/* Initialization.  */

static void
calendar_component_class_init (CalendarComponentClass *class)
{
	POA_GNOME_Evolution_Component__epv *epv = &class->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	epv->createControls = impl_createControls;

	object_class->dispose = impl_dispose;
	object_class->finalize = impl_finalize;

	epv->createControls = impl_createControls;
}

static void
calendar_component_init (CalendarComponent *component)
{
	CalendarComponentPrivate *priv;
	GSList *groups;

	priv = g_new0 (CalendarComponentPrivate, 1);

	priv->config_directory = g_build_filename (g_get_home_dir (),
						   ".evolution", "calendar", "config",
						   NULL);

	/* EPFIXME: Should use a custom one instead?  Also we should add
	 * calendar_component_peek_gconf_client().  */
	priv->gconf_client = gconf_client_get_default ();

	priv->source_list = e_source_list_new_for_gconf (priv->gconf_client,
							 "/apps/evolution/calendar/sources");

	/* create default calendars if there are no groups */
	groups = e_source_list_peek_groups (priv->source_list);
	if (!groups) {
		ESourceGroup *group;
		ESource *source;
		char *base_uri, *new_dir;

		/* create the source group */
		base_uri = g_build_filename (g_get_home_dir (),
					     "/.evolution/calendar/local/OnThisComputer/",
					     NULL);
		group = e_source_group_new (_("On This Computer"), base_uri);
		e_source_list_add_group (priv->source_list, group, -1);

		/* migrate calendars from older setup */
		if (!migrate_old_calendars (group)) {
			/* create default calendars */
			new_dir = g_build_filename (base_uri, "Personal/", NULL);
			if (!e_mkdir_hier (new_dir, 0700)) {
				source = e_source_new (_("Personal"), "Personal");
				e_source_group_add_source (group, source, -1);
			}
			g_free (new_dir);

			new_dir = g_build_filename (base_uri, "Work/", NULL);
			if (!e_mkdir_hier (new_dir, 0700)) {
				source = e_source_new (_("Work"), "Work");
				e_source_group_add_source (group, source, -1);
			}
			g_free (new_dir);
		}

		g_free (base_uri);
	}

	component->priv = priv;
}


/* Public API.  */

CalendarComponent *
calendar_component_peek (void)
{
	static CalendarComponent *component = NULL;

	if (component == NULL) {
		component = g_object_new (calendar_component_get_type (), NULL);

		if (e_mkdir_hier (calendar_component_peek_config_directory (component), 0777) != 0) {
			g_warning ("Cannot create directory %s: %s",
				   calendar_component_peek_config_directory (component),
				   g_strerror (errno));
			g_object_unref (component);
			component = NULL;
		}
	}

	return component;
}

const char *
calendar_component_peek_config_directory (CalendarComponent *component)
{
	return component->priv->config_directory;
}


BONOBO_TYPE_FUNC_FULL (CalendarComponent, GNOME_Evolution_Component, PARENT_TYPE, calendar_component)
