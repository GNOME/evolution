/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "ea-cal-view.h"
#include "ea-calendar-helpers.h"
#include "e-day-view.h"
#include "e-week-view.h"
#include "e-cal-ops.h"
#include <glib/gi18n.h>

static AtkObject * ea_cal_view_get_parent (AtkObject *accessible);
static void ea_cal_view_real_initialize (AtkObject *accessible, gpointer data);

static void ea_cal_view_event_changed_cb (ECalendarView *cal_view,
                                          ECalendarViewEvent *event,
                                          gpointer data);
static void ea_cal_view_event_added_cb (ECalendarView *cal_view,
                                        ECalendarViewEvent *event,
                                        gpointer data);

static gboolean idle_dates_changed (gpointer data);
static void ea_cal_model_time_range_changed_cb (ECalModel *model, gint64 i64_start, gint64 i64_end, gpointer data);

static void atk_action_interface_init (AtkActionIface *iface);
static gboolean action_interface_do_action (AtkAction *action, gint i);
static gint action_interface_get_n_actions (AtkAction *action);
static const gchar *
		action_interface_get_description
						(AtkAction *action,
						 gint i);
static const gchar *
		action_interface_get_keybinding
						(AtkAction *action,
						 gint i);
static const gchar *
		action_interface_action_get_name
						(AtkAction *action,
						 gint i);

static gpointer parent_class = NULL;

G_DEFINE_TYPE_WITH_CODE (EaCalView, ea_cal_view, GTK_TYPE_CONTAINER_ACCESSIBLE,
	G_IMPLEMENT_INTERFACE (
		ATK_TYPE_ACTION, atk_action_interface_init))

static void
ea_cal_view_init (EaCalView *view)
{
}

static void
ea_cal_view_class_init (EaCalViewClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	class->get_parent = ea_cal_view_get_parent;
	class->initialize = ea_cal_view_real_initialize;
}

AtkObject *
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
ea_cal_view_real_initialize (AtkObject *accessible,
                             gpointer data)
{
	ECalendarView *cal_view;
	ECalModel *model;
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
	g_signal_connect (
		cal_view, "event_changed",
		G_CALLBACK (ea_cal_view_event_changed_cb), NULL);
	g_signal_connect (
		cal_view, "event_added",
		G_CALLBACK (ea_cal_view_event_added_cb), NULL);

	/* listen for date changes */
	model = e_calendar_view_get_model (cal_view);

	if (model)
		g_signal_connect_after (
			model, "time-range-changed",
			G_CALLBACK (ea_cal_model_time_range_changed_cb), accessible);
}

static AtkObject *
ea_cal_view_get_parent (AtkObject *accessible)
{
	ECalendarView *cal_view;
	GtkWidget *widget;

	g_return_val_if_fail (EA_IS_CAL_VIEW (accessible), NULL);

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (widget == NULL)
		return NULL;

	cal_view = E_CALENDAR_VIEW (widget);

	widget = gtk_widget_get_parent (GTK_WIDGET (cal_view));
	if (!widget)
		return NULL;

	return gtk_widget_get_accessible (widget);
}

static void
ea_cal_view_event_changed_cb (ECalendarView *cal_view,
                              ECalendarViewEvent *event,
                              gpointer data)
{
	AtkObject *atk_obj;
	AtkObject *event_atk_obj = NULL;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (cal_view));
	if (!EA_IS_CAL_VIEW (atk_obj))
		return;

	if ((E_IS_DAY_VIEW (cal_view)) && event && event->canvas_item) {
		event_atk_obj =
			ea_calendar_helpers_get_accessible_for (event->canvas_item);
	}
	else if ((E_IS_WEEK_VIEW (cal_view)) && event) {
		EWeekViewEventSpan *span;
		EWeekViewEvent *week_view_event = (EWeekViewEvent *) event;
		EWeekView *week_view = E_WEEK_VIEW (cal_view);
		/* get the first span of the event */
		span = &g_array_index (week_view->spans, EWeekViewEventSpan,
				       week_view_event->spans_index);
		if (span && span->text_item)
			event_atk_obj =
				ea_calendar_helpers_get_accessible_for (
				span->text_item);
	}
	if (event_atk_obj) {
#ifdef ACC_DEBUG
		printf ("AccDebug: event=%p changed\n", (gpointer) event);
#endif
		g_object_notify (G_OBJECT (event_atk_obj), "accessible-name");
		g_signal_emit_by_name (event_atk_obj, "visible_data_changed");
	}

}

static void
ea_cal_view_event_added_cb (ECalendarView *cal_view,
                            ECalendarViewEvent *event,
                            gpointer data)
{
	AtkObject *atk_obj;
	AtkObject *event_atk_obj = NULL;
	gint index;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (cal_view));
	if (!EA_IS_CAL_VIEW (atk_obj))
		return;

	if ((E_IS_DAY_VIEW (cal_view)) && event && event->canvas_item) {
		event_atk_obj =
			ea_calendar_helpers_get_accessible_for (event->canvas_item);
	}
	else if ((E_IS_WEEK_VIEW (cal_view)) && event) {
		EWeekViewEventSpan *span;
		EWeekViewEvent *week_view_event = (EWeekViewEvent *) event;
		EWeekView *week_view = E_WEEK_VIEW (cal_view);
		/* get the first span of the event */
		span = &g_array_index (
			week_view->spans, EWeekViewEventSpan,
			week_view_event->spans_index);
		if (span && span->text_item)
			event_atk_obj =
				ea_calendar_helpers_get_accessible_for (
				span->text_item);

	}
	if (event_atk_obj) {
		index = atk_object_get_index_in_parent (event_atk_obj);
		if (index < 0)
			return;
#ifdef ACC_DEBUG
		printf ("AccDebug: event=%p added\n", (gpointer) event);
#endif
		g_signal_emit_by_name (
			atk_obj, "children_changed::add",
			index, event_atk_obj, NULL);
	}
}

static gboolean
idle_dates_changed (gpointer data)
{
	AtkObject *ea_cal_view;

	g_return_val_if_fail (data, FALSE);
	g_return_val_if_fail (EA_IS_CAL_VIEW (data), FALSE);

	ea_cal_view = ATK_OBJECT (data);
	g_clear_pointer (&ea_cal_view->name, g_free);
	g_object_notify (G_OBJECT (ea_cal_view), "accessible-name");
	g_signal_emit_by_name (ea_cal_view, "visible_data_changed");
	g_signal_emit_by_name (ea_cal_view, "children_changed", NULL, NULL, NULL);
#ifdef ACC_DEBUG
	printf ("AccDebug: cal view date changed\n");
#endif

	return FALSE;
}

static void
ea_cal_model_time_range_changed_cb (ECalModel *model,
				    gint64 i64_start,
				    gint64 i64_end,
				    gpointer data)
{
	g_idle_add (idle_dates_changed, data);
}

/* atk action interface */

#define CAL_VIEW_ACTION_NUM 5

static const gchar * action_name[CAL_VIEW_ACTION_NUM] = {
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
action_interface_do_action (AtkAction *action,
                            gint index)
{
	GtkWidget *widget;
	gboolean return_value = TRUE;
	time_t dtstart, dtend;
	ECalendarView *cal_view;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
	if (widget == NULL)
		/*
		 * State is defunct
		 */
		return FALSE;

	if (!gtk_widget_get_sensitive (widget) || !gtk_widget_is_visible (widget))
		return FALSE;

	cal_view = E_CALENDAR_VIEW (widget);
	switch (index) {
	case 0:
		/* New Appointment */
		e_calendar_view_new_appointment (cal_view, E_NEW_APPOINTMENT_FLAG_NONE);
		break;
	case 1:
		/* New All Day Event */
		g_warn_if_fail (e_calendar_view_get_selected_time_range (cal_view, &dtstart, &dtend));
		e_cal_ops_new_component_editor_from_model (
			e_calendar_view_get_model (cal_view), NULL,
			dtstart, dtend, FALSE, TRUE);
		break;
	case 2:
		/* New Meeting */
		g_warn_if_fail (e_calendar_view_get_selected_time_range (cal_view, &dtstart, &dtend));
		e_cal_ops_new_component_editor_from_model (
			e_calendar_view_get_model (cal_view), NULL,
			dtstart, dtend, TRUE, FALSE);
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

static const gchar *
action_interface_get_description (AtkAction *action,
                                  gint index)
{
	return action_interface_action_get_name (action, index);
}

static const gchar *
action_interface_get_keybinding (AtkAction *action,
                                 gint index)
{
	GtkWidget *widget;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
	if (widget == NULL)
		/*
		 * State is defunct
		 */
		return NULL;

	if (!gtk_widget_get_sensitive (widget) || !gtk_widget_is_visible (widget))
		return NULL;

	 switch (index) {
	 case 0:
		 /* New Appointment */
		 return "<Alt>fna;<Control>n";
	 case 1:
		 /* New Event */
		 return "<Alt>fnd;<Shift><Control>d";
	 case 2:
		 /* New Meeting */
		 return "<Alt>fne;<Shift><Control>e";
	 case 3:
		 /* Go to today */
		 return "<Alt>vt;<Alt><Control>t";
	 case 4:
		 /* Go to date */
		 return "<Alt>vd;<Alt><Control>g";
	 default:
		 break;
	 }
	 return NULL;
}

static const gchar *
action_interface_action_get_name (AtkAction *action,
                                  gint i)
{
	if (i >= 0 && i < CAL_VIEW_ACTION_NUM)
		return action_name[i];
	return NULL;
}
