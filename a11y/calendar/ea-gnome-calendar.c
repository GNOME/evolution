/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-gnome-calendar.c
 *
 * Copyright (C) 2003 Ximian, Inc.
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
 * Author: Bolian Yin <bolian.yin@sun.com> Sun Microsystem Inc., 2003
 *
 */

#include "ea-gnome-calendar.h"
#include "calendar-commands.h"
#include <gtk/gtknotebook.h>
#include <libgnome/gnome-i18n.h>

static void ea_gnome_calendar_class_init (EaGnomeCalendarClass *klass);

static G_CONST_RETURN gchar* ea_gnome_calendar_get_name (AtkObject *accessible);
static G_CONST_RETURN gchar* ea_gnome_calendar_get_description (AtkObject *accessible);
static gint ea_gnome_calendar_get_n_children (AtkObject* obj);
static AtkObject * ea_gnome_calendar_ref_child (AtkObject *obj, gint i);

static void ea_gcal_switch_view_cb (GtkNotebook *widget, GtkNotebookPage *page,
				    guint index, gpointer data);
static void ea_gcal_dates_change_cb (GnomeCalendar *gcal, gpointer data);

static gpointer parent_class = NULL;

GType
ea_gnome_calendar_get_type (void)
{
	static GType type = 0;
	AtkObjectFactory *factory;
	GTypeQuery query;
	GType derived_atk_type;

	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (EaGnomeCalendarClass),
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) ea_gnome_calendar_class_init, /* class init */
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EaGnomeCalendar), /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL /* value table */
		};

		/*
		 * Figure out the size of the class and instance
		 * we are run-time deriving from (GailWidget, in this case)
		 */

		factory = atk_registry_get_factory (atk_get_default_registry (),
						    GTK_TYPE_WIDGET);
		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);
		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		type = g_type_register_static (derived_atk_type,
					       "EaGnomeCalendar", &tinfo, 0);

	}

	return type;
}

static void
ea_gnome_calendar_class_init (EaGnomeCalendarClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_gnome_calendar_get_name;
	class->get_description = ea_gnome_calendar_get_description;

	class->get_n_children = ea_gnome_calendar_get_n_children;
	class->ref_child = ea_gnome_calendar_ref_child;
}

AtkObject* 
ea_gnome_calendar_new (GtkWidget *widget)
{
	GObject *object;
	AtkObject *accessible;
	GnomeCalendar *gcal;
	GtkWidget *notebook;

	g_return_val_if_fail (GNOME_IS_CALENDAR (widget), NULL);

	object = g_object_new (EA_TYPE_GNOME_CALENDAR, NULL);

	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, widget);

	accessible->role = ATK_ROLE_FILLER;

	gcal = GNOME_CALENDAR (widget);

	/* listen on view type change
	 */
	g_signal_connect (widget, "dates_shown_changed",
			  G_CALLBACK (ea_gcal_dates_change_cb),
			  accessible);
	notebook = gnome_calendar_get_view_notebook_widget (gcal);
	if (notebook) {
		g_signal_connect (notebook, "switch_page",
				  G_CALLBACK (ea_gcal_switch_view_cb),
				  accessible);
	}

#ifdef ACC_DEBUG
	printf ("EvoAcc: ea-gnome-calendar created: %p\n", (void *)accessible);
#endif

	return accessible;
}

static G_CONST_RETURN gchar*
ea_gnome_calendar_get_name (AtkObject *accessible)
{
	if (accessible->name)
		return accessible->name;
	return _("Gnome Calendar");
}

static G_CONST_RETURN gchar*
ea_gnome_calendar_get_description (AtkObject *accessible)
{
	if (accessible->description)
		return accessible->description;
	return _("Gnome Calendar");
}

static gint
ea_gnome_calendar_get_n_children (AtkObject* obj)
{
	g_return_val_if_fail (EA_IS_GNOME_CALENDAR (obj), 0);

	if (!GTK_ACCESSIBLE (obj)->widget)
		return -1;
	return 4;
}

static AtkObject *
ea_gnome_calendar_ref_child (AtkObject *obj, gint i)
{
	AtkObject * child = NULL;
	GnomeCalendar * calendarWidget;
	GtkWidget *childWidget;

	g_return_val_if_fail (EA_IS_GNOME_CALENDAR (obj), NULL);
	/* valid child index range is [0-3] */
	if (i < 0 || i >3 )
		return NULL;

	if (!GTK_ACCESSIBLE (obj)->widget)
		return NULL;
	calendarWidget = GNOME_CALENDAR (GTK_ACCESSIBLE (obj)->widget);

	switch (i) {
	case 0:
		/* for the search bar */
		childWidget = gnome_calendar_get_search_bar_widget (calendarWidget);
		child = gtk_widget_get_accessible (childWidget);
		atk_object_set_parent (child, obj);
		atk_object_set_name (child, _("search bar"));
		atk_object_set_description (child, _("evolution calendar search bar"));
		break;
	case 1:
		/* for the day/week view */
		childWidget = gnome_calendar_get_current_view_widget (calendarWidget);
		child = gtk_widget_get_accessible (childWidget);
		atk_object_set_parent (child, obj);
		break;
	case 2:
		/* for calendar */
		childWidget = gnome_calendar_get_e_calendar_widget (calendarWidget);
		child = gtk_widget_get_accessible (childWidget);
		break;
	case 3:
		/* for todo list */
		childWidget = GTK_WIDGET (gnome_calendar_get_task_pad (calendarWidget));
		child = gtk_widget_get_accessible (childWidget);
		break;
	default:
		break;
	}
	if (child)
		g_object_ref(child);
	return child;
}

static void
ea_gcal_switch_view_cb (GtkNotebook *widget, GtkNotebookPage *page,
			guint index, gpointer data)
{
	GtkWidget *new_widget;

	new_widget = gtk_notebook_get_nth_page (widget, index);

	/* views are always the second child in gnome calendar
	 */
	if (new_widget)
		g_signal_emit_by_name (G_OBJECT(data), "children_changed::add",
				       1, gtk_widget_get_accessible (new_widget), NULL);

#ifdef ACC_DEBUG
	printf ("AccDebug: view switch to widget %p (index=%d) \n",
		(void *)new_widget, index);
#endif
}

static void
ea_gcal_dates_change_cb (GnomeCalendar *gcal, gpointer data)
{
	const gchar *new_name;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (data);
	g_return_if_fail (EA_IS_GNOME_CALENDAR (data));

	new_name = calendar_get_text_for_folder_bar_label (gcal);
	atk_object_set_name (ATK_OBJECT(data), new_name);
	g_signal_emit_by_name (data, "visible_data_changed");

#ifdef ACC_DEBUG
	printf ("AccDebug: calendar dates changed, label=%s\n", new_name);
#endif
}
