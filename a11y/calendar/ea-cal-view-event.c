/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-cal-view-event.c
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

#include "ea-cal-view-event.h"
#include "ea-calendar-helpers.h"
#include "ea-day-view.h"
#include "ea-week-view.h"
#include <gal/e-text/e-text.h>
#include <libgnome/gnome-i18n.h>

static void ea_cal_view_event_class_init (EaCalViewEventClass *klass);

static G_CONST_RETURN gchar* ea_cal_view_event_get_name (AtkObject *accessible);
static G_CONST_RETURN gchar* ea_cal_view_event_get_description (AtkObject *accessible);
static AtkObject* ea_cal_view_event_get_parent (AtkObject *accessible);
static gint ea_cal_view_event_get_index_in_parent (AtkObject *accessible);

/* component interface */
static void atk_component_interface_init (AtkComponentIface *iface);
static void ea_cal_view_get_extents (AtkComponent *component,
				     gint *x, gint *y, gint *width, gint *height,
				     AtkCoordType coord_type);
/* action interface */
static void atk_action_interface_init (AtkActionIface *iface);
static gboolean ea_cal_view_event_do_action (AtkAction *action, gint i);
static gint ea_cal_view_event_get_n_actions (AtkAction *action);
static G_CONST_RETURN gchar* ea_cal_view_event_action_get_name (AtkAction *action, gint i);


#ifdef ACC_DEBUG
static gint n_ea_cal_view_event_created = 0, n_ea_cal_view_event_destroyed = 0;
static void ea_cal_view_finalize (GObject *object);
#endif

static gpointer parent_class = NULL;

GType
ea_cal_view_event_get_type (void)
{
	static GType type = 0;
	AtkObjectFactory *factory;
	GTypeQuery query;
	GType derived_atk_type;


	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (EaCalViewEventClass),
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) ea_cal_view_event_class_init, /* class init */
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EaCalViewEvent), /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL /* value table */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) atk_component_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

                static const GInterfaceInfo atk_action_info = {
                        (GInterfaceInitFunc) atk_action_interface_init,
                        (GInterfaceFinalizeFunc) NULL,
                        NULL
                };


		/*
		 * Figure out the size of the class and instance
		 * we are run-time deriving from (atk object for E_TEXT, in this case)
		 */

		factory = atk_registry_get_factory (atk_get_default_registry (),
						    E_TYPE_TEXT);
		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);

		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		/* we inherit the component, text and other interfaces from E_TEXT */
		type = g_type_register_static (derived_atk_type,
					       "EaCalViewEvent", &tinfo, 0);
		g_type_add_interface_static (type, ATK_TYPE_COMPONENT,
					     &atk_component_info);
                g_type_add_interface_static (type, ATK_TYPE_ACTION,
                                             &atk_action_info);

                
	}

	return type;
}

static void
ea_cal_view_event_class_init (EaCalViewEventClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
#ifdef ACC_DEBUG
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = ea_cal_view_finalize;
#endif

	parent_class = g_type_class_peek_parent (klass);


	class->get_name = ea_cal_view_event_get_name;
	class->get_description = ea_cal_view_event_get_description;
	class->get_parent = ea_cal_view_event_get_parent;
	class->get_index_in_parent = ea_cal_view_event_get_index_in_parent;

}

#ifdef ACC_DEBUG
static void ea_cal_view_finalize (GObject *object)
{
	++n_ea_cal_view_event_destroyed;
	printf ("ACC_DEBUG: n_ea_cal_view_event_destroyed = %d\n",
		n_ea_cal_view_event_destroyed);
}
#endif

AtkObject* 
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
		week_view_event = &g_array_index (week_view->events,
						  EWeekViewEvent,
						  event_num);
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
		atk_obj = ATK_OBJECT (g_object_new (EA_TYPE_CAL_VIEW_EVENT,
						    NULL));
		atk_object_initialize (atk_obj, target_obj);
		if (event_role == ATK_ROLE_INVALID)
			event_role = atk_role_register ("Calendar Event");
		atk_obj->role = event_role;
#ifdef ACC_DEBUG
		++n_ea_cal_view_event_created;
		printf ("ACC_DEBUG: n_ea_cal_view_event_created = %d\n",
			n_ea_cal_view_event_created);
#endif
	}

	/* the registered factory for E_TEXT is cannot create a EaCalViewEvent,
	 * we should save the EaCalViewEvent object in it.
	 */
	g_object_set_data (obj, "accessible-object", atk_obj);

	return atk_obj;
}

static G_CONST_RETURN gchar*
ea_cal_view_event_get_name (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarViewEvent *event;
	gchar *tmp_name;
	gchar *new_name = g_strdup ("");
        const char *summary;


	g_return_val_if_fail (EA_IS_CAL_VIEW_EVENT (accessible), NULL);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj || !E_IS_TEXT (g_obj))
		return NULL;
	event = ea_calendar_helpers_get_cal_view_event_from (GNOME_CANVAS_ITEM(g_obj));

	if (event && event->comp_data) {
		if (e_cal_util_component_has_alarms (event->comp_data->icalcomp)) {
			tmp_name = new_name;
			new_name = g_strconcat (new_name, _("alarm "), NULL);
			g_free (tmp_name);
		}

		if (e_cal_util_component_has_recurrences (event->comp_data->icalcomp)) {
			tmp_name = new_name;
			new_name = g_strconcat (new_name, _("recurrence "), NULL);
			g_free (tmp_name);
		}

		if (event->different_timezone) {
			tmp_name = new_name;
			new_name = g_strconcat (new_name, _("time-zone "), NULL);
			g_free (tmp_name);
		}

		if (e_cal_util_component_has_organizer (event->comp_data->icalcomp)) {
			tmp_name = new_name;
			new_name = g_strconcat (new_name, _("meeting "), NULL);
			g_free (tmp_name);
		}
	}
	tmp_name = new_name;
	new_name = g_strconcat (new_name, _("event. Summary is "), NULL);
	g_free (tmp_name);

	summary = icalcomponent_get_summary (event->comp_data->icalcomp);
	if (summary) {
		tmp_name = new_name;
		new_name = g_strconcat (new_name, summary, NULL);
		g_free (tmp_name);
	}
	else {
		tmp_name = new_name;
		new_name = g_strconcat (new_name, _("empty"), NULL);
		g_free (tmp_name);
	}

	ATK_OBJECT_CLASS (parent_class)->set_name (accessible, new_name);
#ifdef ACC_DEBUG
	printf("EvoAcc:  name for event accobj=%p, is %s\n",
	       (void *)accessible, new_name);
#endif
	g_free (new_name);
	return accessible->name;
}

static G_CONST_RETURN gchar*
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
	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE(accessible));
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
		gint day, event_num, num_before;
		EDayViewEvent *day_view_event;
		EDayView *day_view = E_DAY_VIEW (cal_view);

		/* the long event comes first in the order */
		for (event_num = day_view->long_events->len - 1; event_num >= 0;
		     --event_num) {
			day_view_event = &g_array_index (day_view->long_events,
							 EDayViewEvent, event_num);
			if (cal_view_event == (ECalendarViewEvent*)day_view_event)
				return event_num;

		}
		num_before = day_view->long_events->len;

		for (day = 0; day < day_view->days_shown; ++day) {
			for (event_num = day_view->events[day]->len - 1; event_num >= 0;
			     --event_num) {
				day_view_event = &g_array_index (day_view->events[day],
							EDayViewEvent, event_num);
				if (cal_view_event == (ECalendarViewEvent*)day_view_event)
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
		g_assert_not_reached ();
		return -1;
	}
	return -1;
}

/* Atk Component Interface */

static void 
atk_component_interface_init (AtkComponentIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->get_extents = ea_cal_view_get_extents;
}

static void 
ea_cal_view_get_extents (AtkComponent   *component,
                         gint           *x,
                         gint           *y,
                         gint           *width,
                         gint           *height,
                         AtkCoordType   coord_type)
{
	GObject *g_obj;
	GnomeCanvasItem *canvas_item;
	gint x_window, y_window;
	gint scroll_x, scroll_y;
	ECalendarView *cal_view;
	gint item_x, item_y, item_w, item_h;
	GtkWidget *canvas = NULL;

	g_return_if_fail (EA_IS_CAL_VIEW_EVENT (component));

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE(component));
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

	gdk_window_get_origin (canvas->window,
			       &x_window, &y_window);
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (canvas), &scroll_x, &scroll_y);

	*x = item_x + x_window - scroll_x;
	*y = item_y + y_window - scroll_y;
	*width = item_w;
	*height = item_h;

	if (coord_type == ATK_XY_WINDOW) {
		GdkWindow *window;
		gint x_toplevel, y_toplevel;

		window = gdk_window_get_toplevel (GTK_WIDGET (cal_view)->window);
		gdk_window_get_origin (window, &x_toplevel, &y_toplevel);

		*x -= x_toplevel;
		*y -= y_toplevel;
	}

#ifdef ACC_DEBUG
	printf ("Event Bounds (%d, %d, %d, %d)\n", *x, *y, *width, *height);
#endif
}

#define CAL_VIEW_EVENT_ACTION_NUM 1

static const char * action_name [CAL_VIEW_EVENT_ACTION_NUM] = {
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
ea_cal_view_event_do_action (AtkAction *action, gint i)
{
        AtkGObjectAccessible *atk_gobj;
        AtkComponent *atk_comp;

        atk_gobj = ATK_GOBJECT_ACCESSIBLE (action);

        if (i == 0) {
                atk_comp = (AtkComponent *)atk_gobj;
                return atk_component_grab_focus (atk_comp);
        }

        return FALSE;

}

static gint
ea_cal_view_event_get_n_actions (AtkAction *action)
{
        return CAL_VIEW_EVENT_ACTION_NUM;
}

static G_CONST_RETURN gchar*
ea_cal_view_event_action_get_name (AtkAction *action, gint i)
{
        if (i >= 0 && i < CAL_VIEW_EVENT_ACTION_NUM)
                return action_name [i];
        return NULL;
}

