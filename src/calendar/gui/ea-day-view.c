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

#include "ea-day-view.h"
#include "ea-cal-view-event.h"

#include "ea-calendar-helpers.h"
#include <glib/gi18n.h>

static const gchar * ea_day_view_get_name (AtkObject *accessible);
static const gchar * ea_day_view_get_description (AtkObject *accessible);
static gint         ea_day_view_get_n_children      (AtkObject *obj);
static AtkObject *   ea_day_view_ref_child           (AtkObject *obj,
                                                     gint i);
static gpointer parent_class = NULL;

G_DEFINE_TYPE (EaDayView, ea_day_view, EA_TYPE_CAL_VIEW)

static void
ea_day_view_init (EaDayView *view)
{
}

static void
ea_day_view_class_init (EaDayViewClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_day_view_get_name;
	class->get_description = ea_day_view_get_description;

	class->get_n_children = ea_day_view_get_n_children;
	class->ref_child = ea_day_view_ref_child;
}

AtkObject *
ea_day_view_new (GtkWidget *widget)
{
	GObject *object;
	AtkObject *accessible;

	g_return_val_if_fail (E_IS_DAY_VIEW (widget), NULL);

	object = g_object_new (EA_TYPE_DAY_VIEW, NULL);

	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, widget);

#ifdef ACC_DEBUG
	printf ("EvoAcc: ea_day_view created %p\n", (gpointer) accessible);
#endif

	return accessible;
}

static const gchar *
ea_day_view_get_name (AtkObject *accessible)
{
	EDayView *day_view;
	gchar *label_text;
	GtkWidget *widget;
	gint n_events;
	gchar *event_str, *name_str;

	g_return_val_if_fail (EA_IS_DAY_VIEW (accessible), NULL);

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (widget == NULL)
		return NULL;

	day_view = E_DAY_VIEW (widget);

	label_text = e_calendar_view_get_description_text (E_CALENDAR_VIEW (day_view));

	n_events = atk_object_get_n_accessible_children (accessible);
	/* the child main item is always there */
	--n_events;
	if (n_events >= 1)
		event_str = g_strdup_printf (
			ngettext (
				/* To translators: Here, "It" is either like "Work Week View: July
				10th - July 14th, 2006." or "Day View: Thursday July 13th, 2006." */
				"It has %d event.",
				"It has %d events.",
				n_events),
			n_events);
	else
		/* To translators: Here, "It" is either like "Work Week View: July
		10th - July 14th, 2006." or "Day View: Thursday July 13th, 2006." */
		event_str = g_strdup (_("It has no events."));

	if (e_day_view_get_work_week_view (day_view))
		name_str = g_strdup_printf (
			/* To translators: First %s is the week, for example "July 10th -
			July 14th, 2006". Second %s is the number of events in this work
			week, for example "It has %d event/events." or  "It has no events." */
			_("Work Week View: %s. %s"),
			label_text, event_str);
	else
		name_str = g_strdup_printf (
			/* To translators: First %s is the day, for example "Thursday July
			13th, 2006". Second %s is the number of events on this day, for
			example "It has %d event/events." or  "It has no events." */
			_("Day View: %s. %s"),
			label_text, event_str);

	ATK_OBJECT_CLASS (parent_class)->set_name (accessible, name_str);
	g_free (name_str);
	g_free (event_str);
	g_free (label_text);

	return accessible->name;
}

static const gchar *
ea_day_view_get_description (AtkObject *accessible)
{
	EDayView *day_view;
	GtkWidget *widget;

	g_return_val_if_fail (EA_IS_DAY_VIEW (accessible), NULL);

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (widget == NULL)
		return NULL;

	day_view = E_DAY_VIEW (widget);

	if (accessible->description)
		return accessible->description;
	else {
		if (e_day_view_get_work_week_view (day_view))
			return _("calendar view for a work week");
		else
			return _("calendar view for one or more days");
	}
}

static gint
ea_day_view_get_n_children (AtkObject *accessible)
{
	EDayView *day_view;
	GtkWidget *widget;
	gint day;
	gint days_shown;
	gint child_num = 0;

	g_return_val_if_fail (EA_IS_DAY_VIEW (accessible), -1);

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (widget == NULL)
		return -1;

	day_view = E_DAY_VIEW (widget);
	days_shown = e_day_view_get_days_shown (day_view);

	child_num += day_view->long_events->len;

	for (day = 0; day < days_shown; day++) {
		child_num += day_view->events[day]->len;
	}

	/* "+1" for the main item */
	return child_num + 1;
}

static AtkObject *
ea_day_view_ref_child (AtkObject *accessible,
                       gint index)
{
	EDayView *day_view;
	gint child_num;
	gint day;
	AtkObject *atk_object = NULL;
	EDayViewEvent *event = NULL;
	GtkWidget *widget;

	g_return_val_if_fail (EA_IS_DAY_VIEW (accessible), NULL);

	child_num = atk_object_get_n_accessible_children (accessible);
	if (child_num <= 0 || index < 0 || index >= child_num)
		return NULL;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (widget == NULL)
		return NULL;

	day_view = E_DAY_VIEW (widget);

	if (index == 0) {
		/* index == 0 is the main item */
		atk_object = atk_gobject_accessible_for_object (
			G_OBJECT (day_view->main_canvas_item));
		g_object_ref (atk_object);
	}
	else {
		--index;
		/* a long event */
		if (index < day_view->long_events->len) {
			event = &g_array_index (day_view->long_events,
						EDayViewEvent, index);
		}
		else {
			index -= day_view->long_events->len;
			day = 0;
			while (index >= day_view->events[day]->len) {
				index -= day_view->events[day]->len;
				++day;
			}

			event = &g_array_index (day_view->events[day],
						EDayViewEvent, index);
		}
		if (event && event->canvas_item) {
			/* Not use atk_gobject_accessible_for_object here,
			 * we need to do special thing here
			 */
			atk_object = ea_calendar_helpers_get_accessible_for (
				event->canvas_item);
			g_object_ref (atk_object);
		}
	}
	return atk_object;
}
