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
#include "goto.h"
#include <glib/gstrfuncs.h>
#include <libgnome/gnome-i18n.h>

static void ea_cal_view_class_init (EaCalViewClass *klass);

static AtkObject* ea_cal_view_get_parent (AtkObject *accessible);
static void ea_cal_view_real_initialize (AtkObject *accessible, gpointer data);

static void ea_cal_view_event_changed_cb (ECalendarView *cal_view,
                                          ECalendarViewEvent *event, gpointer data);
static void ea_cal_view_event_added_cb (ECalendarView *cal_view,
                                        ECalendarViewEvent *event, gpointer data);

static gboolean idle_dates_changed (gpointer data);
static void ea_cal_view_dates_change_cb (GnomeCalendar *gcal, gpointer data);

static void atk_action_interface_init (AtkActionIface *iface);
static gboolean action_interface_do_action (AtkAction *action, gint i);
static gint action_interface_get_n_actions (AtkAction *action);
static G_CONST_RETURN gchar*
action_interface_get_description(AtkAction *action, gint i);
static G_CONST_RETURN gchar*
action_interface_get_keybinding (AtkAction *action, gint i);
static G_CONST_RETURN gchar*
action_interface_action_get_name(AtkAction *action, gint i);

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

		static const GInterfaceInfo atk_action_info = {
			(GInterfaceInitFunc) atk_action_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
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
		g_type_add_interface_static (type, ATK_TYPE_ACTION,
					     &atk_action_info);
	}

	return type;
}

static void
ea_cal_view_class_init (EaCalViewClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	class->get_parent = ea_cal_view_get_parent;
	class->initialize = ea_cal_view_real_initialize;
}

AtkObject* 
ea_cal_view_new (GtkWidget *widget)
{
	GObject *object;
	AtkObject *accessible;

	g_return_val_if_fail (E_IS_CALENDAR_VIEW (widget), NULL);

	object = g_object_new (EA_TYPE_CAL_VIEW, NULL);

	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, widget);

	return accessible;
}

static void
ea_cal_view_real_initialize (AtkObject *accessible, gpointer data)
{
	ECalendarView *cal_view;
	GnomeCalendar *gcal;
	static AtkRole role = ATK_ROLE_INVALID;

	g_return_if_fail (EA_IS_CAL_VIEW (accessible));
	g_return_if_fail (E_IS_CALENDAR_VIEW (data));

        ATK_OBJECT_CLASS (parent_class)->initialize (accessible, data);
	if (role == ATK_ROLE_INVALID)
		role = atk_role_register ("Calendar View");
	accessible->role = role;
	cal_view = E_CALENDAR_VIEW (data);

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
	gcal = e_calendar_view_get_calendar (cal_view);

	if (gcal)
		g_signal_connect (gcal, "dates_shown_changed",
				  G_CALLBACK (ea_cal_view_dates_change_cb),
				  accessible);
}

static AtkObject* 
ea_cal_view_get_parent (AtkObject *accessible)
{
	ECalendarView *cal_view;
	GnomeCalendar *gnomeCalendar;

	g_return_val_if_fail (EA_IS_CAL_VIEW (accessible), NULL);

	if (!GTK_ACCESSIBLE (accessible)->widget)
		return NULL;
	cal_view = E_CALENDAR_VIEW (GTK_ACCESSIBLE (accessible)->widget);

	gnomeCalendar = e_calendar_view_get_calendar (cal_view);

	return gtk_widget_get_accessible (GTK_WIDGET(gnomeCalendar));
}

static void
ea_cal_view_event_changed_cb (ECalendarView *cal_view, ECalendarViewEvent *event,
                              gpointer data)
{
	AtkObject *atk_obj;
	EaCalView *ea_cal_view;
	AtkObject *event_atk_obj = NULL;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

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
		printf ("AccDebug: event=%p changed\n", (void *)event);
#endif
		g_object_notify (G_OBJECT(event_atk_obj), "accessible-name");
		g_signal_emit_by_name (event_atk_obj, "visible_data_changed");
	}

}

static void
ea_cal_view_event_added_cb (ECalendarView *cal_view, ECalendarViewEvent *event,
                            gpointer data)
{
	AtkObject *atk_obj;
	EaCalView *ea_cal_view;
	AtkObject *event_atk_obj = NULL;
	gint index;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

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
		printf ("AccDebug: event=%p added\n", (void *)event);
#endif
		g_signal_emit_by_name (atk_obj, "children_changed::add",
				       index, event_atk_obj, NULL);
	}
}

static gboolean
idle_dates_changed (gpointer data)
{
	AtkObject *ea_cal_view;

	g_return_val_if_fail (data, FALSE);
	g_return_val_if_fail (EA_IS_CAL_VIEW (data), FALSE);

	ea_cal_view = ATK_OBJECT(data);

	if (ea_cal_view->name) {
		g_free (ea_cal_view->name);
		ea_cal_view->name = NULL;
	}
	g_object_notify (G_OBJECT (ea_cal_view), "accessible-name");
	g_signal_emit_by_name (ea_cal_view, "visible_data_changed");
	g_signal_emit_by_name (ea_cal_view, "children_changed", NULL);
#ifdef ACC_DEBUG
	printf ("AccDebug: cal view date changed\n");
#endif

	return FALSE;
}

static void
ea_cal_view_dates_change_cb (GnomeCalendar *gcal, gpointer data)
{
	g_idle_add (idle_dates_changed, data);
}

/* atk action interface */

#define CAL_VIEW_ACTION_NUM 5

static const char * action_name [CAL_VIEW_ACTION_NUM] = {
	N_("New Appointment"),
	N_("New All Day Event"),
	N_("New Meeting"),
	N_("Go to Today"),
	N_("Go to Date")
};

static void
atk_action_interface_init (AtkActionIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->do_action = action_interface_do_action;
	iface->get_n_actions = action_interface_get_n_actions;
	iface->get_description = action_interface_get_description;
	iface->get_keybinding = action_interface_get_keybinding;
	iface->get_name = action_interface_action_get_name;
}

static gboolean
action_interface_do_action (AtkAction *action, gint index)
{
	GtkWidget *widget;
	gboolean return_value = TRUE;
	time_t dtstart, dtend;
	ECalendarView *cal_view;

	widget = GTK_ACCESSIBLE (action)->widget;
	if (widget == NULL)
		/*
		 * State is defunct
		 */
		return FALSE;

	if (!GTK_WIDGET_IS_SENSITIVE (widget) || !GTK_WIDGET_VISIBLE (widget))
		return FALSE;

	cal_view = E_CALENDAR_VIEW (widget);
	 switch (index) {
	 case 0:
		 /* New Appointment */
		 e_calendar_view_new_appointment (cal_view);
		 break;
	 case 1:
		 /* New All Day Event */
		 e_calendar_view_get_selected_time_range (cal_view,
						     &dtstart, &dtend);
		 e_calendar_view_new_appointment_for (cal_view,
						 dtstart, dtend, TRUE, FALSE);
		 break;
	 case 2:
		 /* New Meeting */
		 e_calendar_view_get_selected_time_range (cal_view,
						     &dtstart, &dtend);
		 e_calendar_view_new_appointment_for (cal_view,
						 dtstart, dtend, FALSE, TRUE);
		 break;
	 case 3:
		 /* Go to today */
		 break;
		 calendar_goto_today (e_calendar_view_get_calendar (cal_view));
	 case 4:
		 /* Go to date */
		 goto_dialog (e_calendar_view_get_calendar (cal_view));
		 break;
	 default:
		 return_value = FALSE;
		 break;
	 }
	 return return_value;
}

static gint
action_interface_get_n_actions (AtkAction *action)
{
	return CAL_VIEW_ACTION_NUM;
}

static G_CONST_RETURN gchar*
action_interface_get_description(AtkAction *action, gint index)
{
	return action_interface_action_get_name (action, index);
}

static G_CONST_RETURN gchar*
action_interface_get_keybinding (AtkAction *action, gint index)
{
	GtkWidget *widget;
	EaCalView *ea_cal_view;

	widget = GTK_ACCESSIBLE (action)->widget;
	if (widget == NULL)
		/*
		 * State is defunct
		 */
		return NULL;

	if (!GTK_WIDGET_IS_SENSITIVE (widget) || !GTK_WIDGET_VISIBLE (widget))
		return FALSE;

	 ea_cal_view = EA_CAL_VIEW (action);

	 switch (index) {
	 case 0:
		 /* New Appointment */
		 return "<Alt>fna;<Control>n";
		 break;
	 case 1:
		 /* New Event */
		 return "<Alt>fnd;<Shift><Control>d";
		 break;
	 case 2:
		 /* New Meeting */
		 return "<Alt>fne;<Shift><Control>e";
		 break;
	 case 3:
		 /* Go to today */
		 return "<Alt>vt;<Alt><Control>t";
		 break;
	 case 4:
		 /* Go to date */
		 return "<Alt>vd;<Alt><Control>g";
		 break;
	 default:
		 break;
	 }
	 return NULL;
}

static G_CONST_RETURN gchar*
action_interface_action_get_name(AtkAction *action, gint i)
{
	if (i >= 0 && i < CAL_VIEW_ACTION_NUM)
		return action_name [i];
	return NULL;
}
