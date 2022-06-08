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

#include <glib/gi18n.h>

#include "e-util/gal-a11y-e-text.h"

#include "ea-cal-view-event.h"
#include "ea-calendar-helpers.h"
#include "ea-day-view.h"
#include "ea-week-view.h"

static void	atk_component_interface_init	(AtkComponentIface *iface);
static void	atk_action_interface_init	(AtkActionIface *iface);

G_DEFINE_TYPE_WITH_CODE (EaCalViewEvent, ea_cal_view_event, GAL_A11Y_TYPE_E_TEXT,
	G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init)
	G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init))

static void	ea_cal_view_event_dispose	(GObject *object);
static const gchar *
		ea_cal_view_event_get_name	(AtkObject *accessible);
static const gchar *
		ea_cal_view_event_get_description
						(AtkObject *accessible);
static AtkObject *
		ea_cal_view_event_get_parent	(AtkObject *accessible);
static gint	ea_cal_view_event_get_index_in_parent
						(AtkObject *accessible);
static AtkStateSet *
		ea_cal_view_event_ref_state_set	(AtkObject *accessible);

/* component interface */
static void	ea_cal_view_get_extents		(AtkComponent *component,
						 gint *x,
						 gint *y,
						 gint *width,
						 gint *height,
						 AtkCoordType coord_type);
/* action interface */
static gboolean	ea_cal_view_event_do_action	(AtkAction *action,
						 gint i);
static gint	ea_cal_view_event_get_n_actions	(AtkAction *action);
static const gchar *
		ea_cal_view_event_action_get_name
						(AtkAction *action,
						 gint i);

#ifdef ACC_DEBUG
static gint n_ea_cal_view_event_created = 0;
static gint n_ea_cal_view_event_destroyed = 0;
static void ea_cal_view_finalize (GObject *object);
#endif

static void
ea_cal_view_event_class_init (EaCalViewEventClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
#ifdef ACC_DEBUG
	gobject_class->finalize = ea_cal_view_finalize;
#endif

	gobject_class->dispose = ea_cal_view_event_dispose;

	class->get_name = ea_cal_view_event_get_name;
	class->get_description = ea_cal_view_event_get_description;
	class->get_parent = ea_cal_view_event_get_parent;
	class->get_index_in_parent = ea_cal_view_event_get_index_in_parent;
	class->ref_state_set = ea_cal_view_event_ref_state_set;

}

static void
ea_cal_view_event_init (EaCalViewEvent *a11y)
{
	a11y->state_set = atk_state_set_new ();
	atk_state_set_add_state (a11y->state_set, ATK_STATE_TRANSIENT);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_ENABLED);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_SENSITIVE);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_SELECTABLE);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_SHOWING);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_FOCUSABLE);
}

#ifdef ACC_DEBUG
static void ea_cal_view_finalize (GObject *object)
{
	G_OBJECT_CLASS (ea_cal_view_event_parent_class)->finalize (object);

	++n_ea_cal_view_event_destroyed;
	printf (
		"ACC_DEBUG: n_ea_cal_view_event_destroyed = %d\n",
		n_ea_cal_view_event_destroyed);
}
#endif

AtkObject *
ea_cal_view_event_new (GObject *obj)
{
	AtkObject *atk_obj = NULL;
	GObject *target_obj;
	ECalendarView *cal_view;

	g_return_val_if_fail (E_IS_TEXT (obj), NULL);
	cal_view = ea_calendar_helpers_get_cal_view_from (GNOME_CANVAS_ITEM (obj));
	if (!cal_view)
		return NULL;

	if (E_IS_WEEK_VIEW (cal_view)) {
		gint event_num, span_num;
		EWeekViewEvent *week_view_event;
		EWeekViewEventSpan *event_span;
		EWeekView *week_view = E_WEEK_VIEW (cal_view);

		/* for week view, we need to check if a atkobject exists for
		 * the first span of the same event
		 */
		if (!e_week_view_find_event_from_item (week_view,
						       GNOME_CANVAS_ITEM (obj),
						       &event_num,
						       &span_num))
			return NULL;

		if (!is_array_index_in_bounds (week_view->events, event_num))
			return NULL;

		week_view_event = &g_array_index (week_view->events,
						  EWeekViewEvent,
						  event_num);

		if (!is_array_index_in_bounds (
			week_view->spans, week_view_event->spans_index))
			return NULL;

		/* get the first span */
		event_span = &g_array_index (week_view->spans,
					     EWeekViewEventSpan,
					     week_view_event->spans_index);
		target_obj = G_OBJECT (event_span->text_item);
		atk_obj = g_object_get_data (target_obj, "accessible-object");

	}
	else
		target_obj = obj;

	if (!atk_obj) {
		static AtkRole event_role = ATK_ROLE_INVALID;
		atk_obj = ATK_OBJECT (
			g_object_new (EA_TYPE_CAL_VIEW_EVENT,
			NULL));
		atk_object_initialize (atk_obj, target_obj);
		if (event_role == ATK_ROLE_INVALID)
			event_role = atk_role_register ("Calendar Event");
		atk_obj->role = event_role;
#ifdef ACC_DEBUG
		++n_ea_cal_view_event_created;
		printf (
			"ACC_DEBUG: n_ea_cal_view_event_created = %d\n",
			n_ea_cal_view_event_created);
#endif
	}

	/* the registered factory for E_TEXT is cannot create a EaCalViewEvent,
	 * we should save the EaCalViewEvent object in it.
	 */
	g_object_set_data (obj, "accessible-object", atk_obj);

	return atk_obj;
}

static void
ea_cal_view_event_dispose (GObject *object)
{
	EaCalViewEvent *a11y = EA_CAL_VIEW_EVENT (object);

	g_clear_object (&a11y->state_set);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (ea_cal_view_event_parent_class)->dispose (object);
}

static const gchar *
ea_cal_view_event_get_name (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarViewEvent *event;
	gchar *name_string;
	const gchar *alarm_string;
	const gchar *recur_string;
	const gchar *meeting_string;
	gchar *summary_string = NULL;
	const gchar *summary;

	g_return_val_if_fail (EA_IS_CAL_VIEW_EVENT (accessible), NULL);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj || !E_IS_TEXT (g_obj))
		return NULL;
	event = ea_calendar_helpers_get_cal_view_event_from (GNOME_CANVAS_ITEM (g_obj));
	if (!is_comp_data_valid (event))
		return NULL;

	alarm_string = recur_string = meeting_string = "";
	if (event && event->comp_data) {
		ICalProperty *prop;

		if (e_cal_util_component_has_alarms (event->comp_data->icalcomp))
			alarm_string = _("It has reminders.");

		if (e_cal_util_component_has_recurrences (event->comp_data->icalcomp))
			recur_string = _("It has recurrences.");

		if (e_cal_util_component_has_organizer (event->comp_data->icalcomp))
			meeting_string = _("It is a meeting.");

		prop = e_cal_util_component_find_property_for_locale (event->comp_data->icalcomp, I_CAL_SUMMARY_PROPERTY, NULL);
		summary = prop ? i_cal_property_get_summary (prop) : NULL;
		if (summary)
			summary_string = g_strdup_printf (_("Calendar Event: Summary is %s."), summary);
		g_clear_object (&prop);
	}

	if (!summary_string)
		summary_string = g_strdup (_("Calendar Event: It has no summary."));

	name_string = g_strdup_printf (
		"%s %s %s %s", summary_string,
		alarm_string, recur_string, meeting_string);
	g_free (summary_string);

	ATK_OBJECT_CLASS (ea_cal_view_event_parent_class)->set_name (accessible, name_string);
#ifdef ACC_DEBUG
	printf (
		"EvoAcc:  name for event accobj=%p, is %s\n",
		(gpointer) accessible, new_name);
#endif
	g_free (name_string);
	return accessible->name;
}

static const gchar *
ea_cal_view_event_get_description (AtkObject *accessible)
{
	if (accessible->description)
		return accessible->description;

	return _("calendar view event");
}

static AtkObject *
ea_cal_view_event_get_parent (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	GnomeCanvasItem *canvas_item;
	ECalendarView *cal_view;

	g_return_val_if_fail (EA_IS_CAL_VIEW_EVENT (accessible), NULL);
	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);

	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (g_obj == NULL)
		/* Object is defunct */
		return NULL;
	canvas_item = GNOME_CANVAS_ITEM (g_obj);

	cal_view = ea_calendar_helpers_get_cal_view_from (canvas_item);

	if (!cal_view)
		return NULL;

	return gtk_widget_get_accessible (GTK_WIDGET (cal_view));
}

static gint
ea_cal_view_event_get_index_in_parent (AtkObject *accessible)
{
	GObject *g_obj;
	GnomeCanvasItem *canvas_item;
	ECalendarView *cal_view;
	ECalendarViewEvent *cal_view_event;

	g_return_val_if_fail (EA_IS_CAL_VIEW_EVENT (accessible), -1);
	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible));
	if (!g_obj)
		/* defunct object*/
		return -1;

	canvas_item = GNOME_CANVAS_ITEM (g_obj);
	cal_view = ea_calendar_helpers_get_cal_view_from (canvas_item);
	if (!cal_view)
		return -1;

	cal_view_event = ea_calendar_helpers_get_cal_view_event_from (canvas_item);
	if (!cal_view_event)
		return -1;

	if (E_IS_DAY_VIEW (cal_view)) {
		EDayView *day_view = E_DAY_VIEW (cal_view);
		EDayViewEvent *day_view_event;
		gint day, event_num, num_before;
		gint days_shown;

		days_shown = e_day_view_get_days_shown (day_view);

		/* the long event comes first in the order */
		for (event_num = day_view->long_events->len - 1; event_num >= 0;
		     --event_num) {
			day_view_event = &g_array_index (day_view->long_events,
							 EDayViewEvent, event_num);
			if (cal_view_event == (ECalendarViewEvent *) day_view_event)
				return event_num;

		}
		num_before = day_view->long_events->len;

		for (day = 0; day < days_shown; ++day) {
			for (event_num = day_view->events[day]->len - 1; event_num >= 0;
			     --event_num) {
				day_view_event = &g_array_index (day_view->events[day],
							EDayViewEvent, event_num);
				if (cal_view_event == (ECalendarViewEvent *) day_view_event)
					return num_before + event_num;
			}
			num_before += day_view->events[day]->len;
		}
	}
	else if (E_IS_WEEK_VIEW (cal_view)) {
		AtkObject *atk_parent, *atk_child;
		gint index = 0;

		atk_parent = atk_object_get_parent (accessible);
		while ((atk_child = atk_object_ref_accessible_child (atk_parent,
								     index)) != NULL) {
			if (atk_child == accessible) {
				g_object_unref (atk_child);
				return index;
			}
			g_object_unref (atk_child);
			++index;
		}
	}
	else {
		g_return_val_if_reached (-1);
	}
	return -1;
}

static AtkStateSet *
ea_cal_view_event_ref_state_set (AtkObject *accessible)
{
	EaCalViewEvent *atk_event = EA_CAL_VIEW_EVENT (accessible);

	g_return_val_if_fail (atk_event->state_set, NULL);

	g_object_ref (atk_event->state_set);

	return atk_event->state_set;
}

/* Atk Component Interface */

static void
atk_component_interface_init (AtkComponentIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->get_extents = ea_cal_view_get_extents;
}

static void
ea_cal_view_get_extents (AtkComponent *component,
                         gint *x,
                         gint *y,
                         gint *width,
                         gint *height,
                         AtkCoordType coord_type)
{
	GObject *g_obj;
	GnomeCanvasItem *canvas_item;
	gint x_window, y_window;
	gint scroll_x, scroll_y;
	ECalendarView *cal_view;
	gint item_x, item_y, item_w, item_h;
	GtkWidget *canvas = NULL;
	GdkWindow *window;

	g_return_if_fail (EA_IS_CAL_VIEW_EVENT (component));

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (component));
	if (!g_obj)
		/* defunct object*/
		return;
	g_return_if_fail (E_IS_TEXT (g_obj));

	canvas_item = GNOME_CANVAS_ITEM (g_obj);
	cal_view = ea_calendar_helpers_get_cal_view_from (canvas_item);
	if (!cal_view)
		return;

	if (E_IS_DAY_VIEW (cal_view)) {
		gint day, event_num;

		if (!e_day_view_find_event_from_item (E_DAY_VIEW (cal_view),
						      canvas_item,
						      &day, &event_num))
			return;
		if (day == E_DAY_VIEW_LONG_EVENT) {
			gint start_day, end_day;
			if (!e_day_view_get_long_event_position (E_DAY_VIEW (cal_view),
								 event_num,
								 &start_day,
								 &end_day,
								 &item_x,
								 &item_y,
								 &item_w,
								 &item_h))
				return;
			canvas = E_DAY_VIEW (cal_view)->top_canvas;
		}
		else {
			if (!e_day_view_get_event_position (E_DAY_VIEW (cal_view), day,
							    event_num,
							    &item_x, &item_y,
							    &item_w, &item_h))

				return;
			canvas = E_DAY_VIEW (cal_view)->main_canvas;
		}
	}
	else if (E_IS_WEEK_VIEW (cal_view)) {
		gint event_num, span_num;
		if (!e_week_view_find_event_from_item (E_WEEK_VIEW (cal_view),
						       canvas_item, &event_num,
						       &span_num))
			return;

		if (!e_week_view_get_span_position (E_WEEK_VIEW (cal_view),
						    event_num, span_num,
						    &item_x, &item_y, &item_w))
			return;
		item_h = E_WEEK_VIEW_ICON_HEIGHT;
		canvas = E_WEEK_VIEW (cal_view)->main_canvas;
	}
	else
		return;

	if (!canvas)
		return;

	window = gtk_widget_get_window (canvas);
	gdk_window_get_origin (window, &x_window, &y_window);
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (canvas), &scroll_x, &scroll_y);

	*x = item_x + x_window - scroll_x;
	*y = item_y + y_window - scroll_y;
	*width = item_w;
	*height = item_h;

	if (coord_type == ATK_XY_WINDOW) {
		gint x_toplevel, y_toplevel;

		window = gtk_widget_get_window (GTK_WIDGET (cal_view));
		window = gdk_window_get_toplevel (window);
		gdk_window_get_origin (window, &x_toplevel, &y_toplevel);

		*x -= x_toplevel;
		*y -= y_toplevel;
	}

#ifdef ACC_DEBUG
	printf ("Event Bounds (%d, %d, %d, %d)\n", *x, *y, *width, *height);
#endif
}

#define CAL_VIEW_EVENT_ACTION_NUM 1

static const gchar * action_name[CAL_VIEW_EVENT_ACTION_NUM] = {
        N_("Grab Focus")
};

static void
atk_action_interface_init (AtkActionIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->do_action = ea_cal_view_event_do_action;
	iface->get_n_actions = ea_cal_view_event_get_n_actions;
	iface->get_name = ea_cal_view_event_action_get_name;
}

static gboolean
ea_cal_view_event_do_action (AtkAction *action,
                             gint i)
{
	AtkGObjectAccessible *atk_gobj;
	AtkComponent *atk_comp;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (action);

	if (i == 0) {
		atk_comp = (AtkComponent *) atk_gobj;
		return atk_component_grab_focus (atk_comp);
	}

	return FALSE;

}

static gint
ea_cal_view_event_get_n_actions (AtkAction *action)
{
	return CAL_VIEW_EVENT_ACTION_NUM;
}

static const gchar *
ea_cal_view_event_action_get_name (AtkAction *action,
                                   gint i)
{
	if (i >= 0 && i < CAL_VIEW_EVENT_ACTION_NUM)
		return action_name[i];
	return NULL;
}

