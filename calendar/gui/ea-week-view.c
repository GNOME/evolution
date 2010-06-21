/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "ea-week-view.h"
#include "ea-cal-view-event.h"
#include "ea-calendar-helpers.h"
#include "ea-gnome-calendar.h"
#include <text/e-text.h>
#include <glib/gi18n.h>

#include <e-util/gtk-compat.h>

static void ea_week_view_class_init (EaWeekViewClass *klass);

static G_CONST_RETURN gchar * ea_week_view_get_name (AtkObject *accessible);
static G_CONST_RETURN gchar * ea_week_view_get_description (AtkObject *accessible);
static gint         ea_week_view_get_n_children      (AtkObject *obj);
static AtkObject*   ea_week_view_ref_child           (AtkObject *obj,
						      gint i);

static gpointer parent_class = NULL;

GType
ea_week_view_get_type (void)
{
	static GType type = 0;
	AtkObjectFactory *factory;
	GTypeQuery query;
	GType derived_atk_type;

	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (EaWeekViewClass),
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) ea_week_view_class_init, /* class init */
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EaWeekView), /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL /* value table */
		};

		/*
		 * Figure out the size of the class and instance
		 * we are run-time deriving from (EaCalView, in this case)
		 *
		 * Note: we must still use run-time deriving here, because
		 * our parent class EaCalView is run-time deriving.
		 */

		factory = atk_registry_get_factory (atk_get_default_registry (),
						    e_calendar_view_get_type());
		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);

		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		type = g_type_register_static (derived_atk_type,
					       "EaWeekView", &tinfo, 0);

	}

	return type;
}

static void
ea_week_view_class_init (EaWeekViewClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_week_view_get_name;
	class->get_description = ea_week_view_get_description;

	class->get_n_children = ea_week_view_get_n_children;
	class->ref_child = ea_week_view_ref_child;
}

AtkObject*
ea_week_view_new (GtkWidget *widget)
{
	GObject *object;
	AtkObject *accessible;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	object = g_object_new (EA_TYPE_WEEK_VIEW, NULL);

	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, widget);

#ifdef ACC_DEBUG
	printf ("EvoAcc: ea_week_view created %p\n", (gpointer)accessible);
#endif

	return accessible;
}

static G_CONST_RETURN gchar *
ea_week_view_get_name (AtkObject *accessible)
{
	EWeekView *week_view;
	GnomeCalendar *gcal;
	const gchar *label_text;
	GnomeCalendarViewType view_type;
	GtkWidget *widget;
	gint n_events;
	gchar *event_str, *name_str;

	g_return_val_if_fail (EA_IS_WEEK_VIEW (accessible), NULL);

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (widget == NULL)
		return NULL;

	week_view = E_WEEK_VIEW (widget);
	gcal = e_calendar_view_get_calendar (E_CALENDAR_VIEW (week_view));

	label_text = ea_gnome_calendar_get_label_description (gcal);

	n_events = atk_object_get_n_accessible_children (accessible);
	/* the child main item is always there */
	--n_events;
	if (n_events >= 1)
		event_str = g_strdup_printf (
			ngettext ("It has %d event.", "It has %d events.",
			n_events), n_events);
	else
		event_str = g_strdup (_("It has no events."));

	view_type = gnome_calendar_get_view (gcal);

	if (view_type == GNOME_CAL_MONTH_VIEW)
		name_str = g_strdup_printf (_("Month View: %s. %s"),
						label_text, event_str);

	else
		name_str = g_strdup_printf (_("Week View: %s. %s"),
						label_text, event_str);

	ATK_OBJECT_CLASS (parent_class)->set_name (accessible, name_str);
	g_free (name_str);
	g_free (event_str);

	return accessible->name;
}

static G_CONST_RETURN gchar *
ea_week_view_get_description (AtkObject *accessible)
{
	EWeekView *week_view;
	GtkWidget *widget;

	g_return_val_if_fail (EA_IS_WEEK_VIEW (accessible), NULL);

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (widget == NULL)
		return NULL;

	week_view = E_WEEK_VIEW (widget);

	if (accessible->description)
		return accessible->description;
	else {
		GnomeCalendar *gcal;
		GnomeCalendarViewType view_type;

		gcal = e_calendar_view_get_calendar (E_CALENDAR_VIEW (week_view));
		view_type = gnome_calendar_get_view (gcal);

		if (view_type == GNOME_CAL_MONTH_VIEW)
			return _("calendar view for a month");
		else
			return _("calendar view for one or more weeks");
	}
}

static gint
ea_week_view_get_n_children (AtkObject *accessible)
{
	EWeekView *week_view;
	GtkWidget *widget;
	gint i, count = 0;
	gint event_index;

	g_return_val_if_fail (EA_IS_WEEK_VIEW (accessible), -1);

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (widget == NULL)
		return -1;

	week_view = E_WEEK_VIEW (widget);

	for (event_index = 0; event_index < week_view->events->len;
	     ++event_index) {
		EWeekViewEvent *event;
		EWeekViewEventSpan *span;

                /* If week_view->spans == NULL, there is no visible events. */
                if (!week_view->spans)
                        break;

		event = &g_array_index (week_view->events,
					EWeekViewEvent, event_index);
		if (!event)
			continue;
		span = &g_array_index (week_view->spans, EWeekViewEventSpan,
				       event->spans_index + 0);

		if (!span)
			continue;

		/* at least one of the event spans is visible, count it */
		if (span->text_item)
			++count;
	}

	/* add the number of visible jump buttons */
	for (i = 0; i < E_WEEK_VIEW_MAX_WEEKS * 7; i++) {
		if (week_view->jump_buttons[i]->flags & GNOME_CANVAS_ITEM_VISIBLE)
			++count;
	}

	/* "+1" for the main item */
	count++;

#ifdef ACC_DEBUG
	printf("AccDebug: week view %p has %d children\n", (gpointer)week_view, count);
#endif
	return count;
}

static AtkObject *
ea_week_view_ref_child (AtkObject *accessible, gint index)
{
	EWeekView *week_view;
	gint child_num, max_count;
	AtkObject *atk_object = NULL;
	GtkWidget *widget;
	gint event_index;
	gint jump_button = -1;
	gint span_num = 0;
	gint count = 0;

	g_return_val_if_fail (EA_IS_WEEK_VIEW (accessible), NULL);

	child_num = atk_object_get_n_accessible_children (accessible);
	if (child_num <= 0 || index < 0 || index >= child_num)
		return NULL;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (widget == NULL)
		return NULL;

	week_view = E_WEEK_VIEW (widget);
	max_count = week_view->events->len;

	if (index == 0) {
		/* index == 0 is the main item */
		atk_object = atk_gobject_accessible_for_object (
			G_OBJECT (week_view->main_canvas_item));
		g_object_ref (atk_object);
	} else
	for (event_index = 0; event_index < max_count; ++event_index) {
		EWeekViewEvent *event;
		EWeekViewEventSpan *span;
		gint current_day;

		event = &g_array_index (week_view->events,
					EWeekViewEvent, event_index);
		if (!event)
			continue;

		span = &g_array_index (week_view->spans, EWeekViewEventSpan,
				       event->spans_index + span_num);

		if (!span)
			continue;

		current_day = span->start_day;
		if (span->text_item)
			++count;
		else if  (current_day != jump_button) {
			/* we should go to the jump button */
			jump_button = current_day;
			++count;
		}
		else
			continue;

			if (count == index) {
			if (span->text_item) {
				/* Not use atk_gobject_accessible_for_object for event
				 * text_item we need to do special thing here
				 */
				atk_object = ea_calendar_helpers_get_accessible_for (span->text_item);
			}
			else {
				gint index;

				index = (current_day != -1) ? current_day : 0;
				atk_object = ea_calendar_helpers_get_accessible_for (
					week_view->jump_buttons[index]);
			}
			g_object_ref (atk_object);
			break;
		}
	}

#ifdef ACC_DEBUG
	printf ("EvoAcc: ea_week_view_ref_child [%d]=%p\n",
		index, (gpointer)atk_object);
#endif
	return atk_object;
}
