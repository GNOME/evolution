/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-cal-view.c
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

#include "ea-cal-view.h"
#include "ea-calendar-helpers.h"
#include "e-day-view.h"
#include "e-week-view.h"
#include "calendar-commands.h"
#include <glib/gstrfuncs.h>

static void ea_cal_view_class_init (EaCalViewClass *klass);

static AtkObject* ea_cal_view_get_parent (AtkObject *accessible);
static gint ea_cal_view_get_index_in_parent (AtkObject *accessible);
static void ea_cal_view_real_initialize (AtkObject *accessible, gpointer data);

static void ea_cal_view_event_changed_cb (ECalView *cal_view,
                                          ECalViewEvent *event, gpointer data);
static void ea_cal_view_event_added_cb (ECalView *cal_view,
                                        ECalViewEvent *event, gpointer data);

static void ea_cal_view_dates_change_cb (GnomeCalendar *gcal, gpointer data);

static gpointer parent_class = NULL;

GType
ea_cal_view_get_type (void)
{
	static GType type = 0;
	AtkObjectFactory *factory;
	GTypeQuery query;
	GType derived_atk_type;

	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (EaCalViewClass),
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) ea_cal_view_class_init, /* class init */
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EaCalView), /* instance size */
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
					       "EaCalView", &tinfo, 0);
	}

	return type;
}

static void
ea_cal_view_class_init (EaCalViewClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	class->get_parent = ea_cal_view_get_parent;
	class->get_index_in_parent = ea_cal_view_get_index_in_parent;
	class->initialize = ea_cal_view_real_initialize;
}

AtkObject* 
ea_cal_view_new (GtkWidget *widget)
{
	GObject *object;
	AtkObject *accessible;

	g_return_val_if_fail (E_IS_CAL_VIEW (widget), NULL);

	object = g_object_new (EA_TYPE_CAL_VIEW, NULL);

	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, widget);

	return accessible;
}

static void
ea_cal_view_real_initialize (AtkObject *accessible, gpointer data)
{
	ECalView *cal_view;
	GnomeCalendar *gcal;

	g_return_if_fail (EA_IS_CAL_VIEW (accessible));
	g_return_if_fail (E_IS_CAL_VIEW (data));

        ATK_OBJECT_CLASS (parent_class)->initialize (accessible, data);
	accessible->role = ATK_ROLE_CANVAS;
	cal_view = E_CAL_VIEW (data);

	/* add listener for event_changed, event_added
	 * we don't need to listen on event_removed. When the e_text
	 * of the event is removed, the cal_view_event will go to the state
	 * of "defunct" (changed by weak ref callback of atkgobjectaccessible
	 */
	g_signal_connect (G_OBJECT(cal_view), "event_changed",
			  G_CALLBACK (ea_cal_view_event_changed_cb), NULL);
	g_signal_connect (G_OBJECT(cal_view), "event_added",
			  G_CALLBACK (ea_cal_view_event_added_cb), NULL);

	/* listen for date changes of calendar */
	gcal = e_cal_view_get_calendar (cal_view);

	if (gcal)
		g_signal_connect (gcal, "dates_shown_changed",
				  G_CALLBACK (ea_cal_view_dates_change_cb),
				  accessible);
}

static AtkObject* 
ea_cal_view_get_parent (AtkObject *accessible)
{
	ECalView *cal_view;
	GnomeCalendar *gnomeCalendar;

	g_return_val_if_fail (EA_IS_CAL_VIEW (accessible), NULL);

	if (!GTK_ACCESSIBLE (accessible)->widget)
		return NULL;
	cal_view = E_CAL_VIEW (GTK_ACCESSIBLE (accessible)->widget);

	gnomeCalendar = e_cal_view_get_calendar (cal_view);

	return gtk_widget_get_accessible (GTK_WIDGET(gnomeCalendar));
}

static gint
ea_cal_view_get_index_in_parent (AtkObject *accessible)
{
	return 1;
}

static void
ea_cal_view_event_changed_cb (ECalView *cal_view, ECalViewEvent *event,
                              gpointer data)
{
	AtkObject *atk_obj;
	EaCalView *ea_cal_view;
	AtkObject *event_atk_obj = NULL;

	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET(cal_view));
	if (!EA_IS_CAL_VIEW (atk_obj))
		return;
	ea_cal_view = EA_CAL_VIEW (atk_obj);

	if ((E_IS_DAY_VIEW (cal_view)) && event && event->canvas_item) {
		event_atk_obj =
			ea_calendar_helpers_get_accessible_for (event->canvas_item);
	}
	else if ((E_IS_WEEK_VIEW (cal_view)) && event) {
		EWeekViewEventSpan *span;
		EWeekViewEvent *week_view_event = (EWeekViewEvent *)event;
		EWeekView *week_view = E_WEEK_VIEW (cal_view);
		/* get the first span of the event */
		span = &g_array_index (week_view->spans, EWeekViewEventSpan,
				       week_view_event->spans_index);
		if (span && span->text_item)
			event_atk_obj = ea_calendar_helpers_get_accessible_for (span->text_item);
	}
	if (event_atk_obj) {
#ifdef ACC_DEBUG
		printf ("AccDebug: event=%p changed\n", event);
#endif
		g_object_notify (G_OBJECT(event_atk_obj), "accessible-name");
		g_signal_emit_by_name (event_atk_obj, "visible_data_changed");
	}

}

static void
ea_cal_view_event_added_cb (ECalView *cal_view, ECalViewEvent *event,
                            gpointer data)
{
	AtkObject *atk_obj;
	EaCalView *ea_cal_view;
	AtkObject *event_atk_obj = NULL;
	gint index;

	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET(cal_view));
	if (!EA_IS_CAL_VIEW (atk_obj))
		return;
	ea_cal_view = EA_CAL_VIEW (atk_obj);

	if ((E_IS_DAY_VIEW (cal_view)) && event && event->canvas_item) {
		event_atk_obj =
			ea_calendar_helpers_get_accessible_for (event->canvas_item);
	}
	else if ((E_IS_WEEK_VIEW (cal_view)) && event) {
		EWeekViewEventSpan *span;
		EWeekViewEvent *week_view_event = (EWeekViewEvent *)event;
		EWeekView *week_view = E_WEEK_VIEW (cal_view);
		/* get the first span of the event */
		span = &g_array_index (week_view->spans, EWeekViewEventSpan,
				       week_view_event->spans_index);
		if (span && span->text_item)
			event_atk_obj = ea_calendar_helpers_get_accessible_for (span->text_item);

	}
	if (event_atk_obj) {
		index = atk_object_get_index_in_parent (event_atk_obj);
		if (index < 0)
			return;
#ifdef ACC_DEBUG
		printf ("AccDebug: event=%p added\n", event);
#endif
		g_signal_emit_by_name (atk_obj, "children_changed::add",
				       index, event_atk_obj, NULL);
	}
}

static void
ea_cal_view_dates_change_cb (GnomeCalendar *gcal, gpointer data)
{
	AtkObject *atk_obj;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (data);
	g_return_if_fail (EA_IS_CAL_VIEW (data));

	atk_obj = ATK_OBJECT(data);
	if (atk_obj->name) {
		g_free (atk_obj->name);
		atk_obj->name = NULL;
	}
	g_object_notify (G_OBJECT (data), "accessible-name");
	g_signal_emit_by_name (data, "visible_data_changed");
}
