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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "calendar-component.h"
#include "calendar-commands.h"
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
	GSList *source_selection;
	
	GnomeCalendar *calendar;
};


/* Utility functions.  */

static void
add_uri_for_source (ESource *source, GnomeCalendar *calendar)
{
	char *uri = e_source_get_uri (source);

	gnome_calendar_add_event_uri (calendar, uri);
	g_free (uri);
}

static void
remove_uri_for_source (ESource *source, GnomeCalendar *calendar)
{
	char *uri = e_source_get_uri (source);

	gnome_calendar_remove_event_uri (calendar, uri);
	g_free (uri);
}

static gboolean
is_in_selection (GSList *selection, ESource *source)
{
	GSList *l;
	
	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;
		
		if (!strcmp (e_source_peek_uid (selected_source), e_source_peek_uid (source)))
			return TRUE;
	}

	return FALSE;
}

static void
update_uris_for_selection (ESourceSelector *selector, CalendarComponent *calendar_component)
{
	CalendarComponentPrivate *priv;
	GSList *selection, *l;
	
	selection = e_source_selector_get_selection (selector);

	priv = calendar_component->priv;
	
	for (l = priv->source_selection; l; l = l->next) {
		ESource *old_selected_source = l->data;

		if (!is_in_selection (selection, old_selected_source))
			remove_uri_for_source (old_selected_source, priv->calendar);
	}	
	
	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;
		
		add_uri_for_source (selected_source, priv->calendar);
	}

	e_source_selector_free_selection (priv->source_selection);
	priv->source_selection = selection;
}

/* Callbacks.  */
static void
source_selection_changed_callback (ESourceSelector *selector, 
				   CalendarComponent *calendar_component)
{
	update_uris_for_selection (selector, calendar_component);
}

static void
primary_source_selection_changed_callback (ESourceSelector *selector,
					   CalendarComponent *calendar_component)
{
	CalendarComponentPrivate *priv;
	ESource *source;
	char *uri;

	priv = calendar_component->priv;
	
	source = e_source_selector_peek_primary_selection (selector);
	if (!source)
		return;

	/* Set the default */
	uri = e_source_get_uri (source);
	gnome_calendar_set_default_uri (priv->calendar, uri);
	g_free (uri);

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

	if (priv->source_selection != NULL) {
		e_source_selector_free_selection (priv->source_selection);
		priv->source_selection = NULL;
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
control_activate_cb (BonoboControl *control, gboolean activate, gpointer data)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	if (activate)
		calendar_control_activate (control, gcal);
	else
		calendar_control_deactivate (control, gcal);
}

static void
impl_createControls (PortableServer_Servant servant,
		     Bonobo_Control *corba_sidebar_control,
		     Bonobo_Control *corba_view_control,
		     CORBA_Environment *ev)
{
	CalendarComponent *calendar_component = CALENDAR_COMPONENT (bonobo_object_from_servant (servant));
	CalendarComponentPrivate *priv;
	GtkWidget *selector;
	GtkWidget *selector_scrolled_window;
	BonoboControl *sidebar_control;
	BonoboControl *view_control;

	priv = calendar_component->priv;
	
	/* Create sidebar selector */
	selector = e_source_selector_new (calendar_component->priv->source_list);
	gtk_widget_show (selector);

	selector_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (selector_scrolled_window), selector);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					     GTK_SHADOW_IN);
	gtk_widget_show (selector_scrolled_window);

	sidebar_control = bonobo_control_new (selector_scrolled_window);

	/* Create main calendar view */
	/* FIXME Instead of returning, we should make a control with a
	 * label describing the problem */
	priv->calendar = new_calendar ();
	if (!priv->calendar) {
		g_warning (G_STRLOC ": could not create the calendar widget!");
		return;
	}
	
	gtk_widget_show (GTK_WIDGET (priv->calendar));

	view_control = bonobo_control_new (GTK_WIDGET (priv->calendar));
	if (!view_control) {
		g_warning (G_STRLOC ": could not create the control!");
		return;
	}
	g_object_set_data (G_OBJECT (priv->calendar), "control", view_control);

	g_signal_connect (view_control, "activate", G_CALLBACK (control_activate_cb), priv->calendar);

	g_signal_connect_object (selector, "selection_changed",
				 G_CALLBACK (source_selection_changed_callback), 
				 G_OBJECT (calendar_component), 0);
	g_signal_connect_object (selector, "primary_selection_changed",
				 G_CALLBACK (primary_source_selection_changed_callback), 
				 G_OBJECT (calendar_component), 0);

	update_uris_for_selection (E_SOURCE_SELECTOR (selector), calendar_component);

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
			g_warning (G_STRLOC, ": Cannot create directory %s: %s",
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
