/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-week-view.c
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

#include "ea-week-view.h"
#include "ea-cal-view.h"
#include "ea-cal-view-event.h"
#include "ea-calendar-helpers.h"
#include "calendar-commands.h"

static void ea_week_view_class_init (EaWeekViewClass *klass);

static G_CONST_RETURN gchar* ea_week_view_get_name (AtkObject *accessible);
static G_CONST_RETURN gchar* ea_week_view_get_description (AtkObject *accessible);
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
						    e_cal_view_get_type());
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
	printf ("EvoAcc: ea_week_view created %p\n", accessible);
#endif

	return accessible;
}

static G_CONST_RETURN gchar*
ea_week_view_get_name (AtkObject *accessible)
{
	EWeekView *week_view;

	g_return_val_if_fail (EA_IS_WEEK_VIEW (accessible), NULL);

	if (!GTK_ACCESSIBLE (accessible)->widget)
		return NULL;
	week_view = E_WEEK_VIEW (GTK_ACCESSIBLE (accessible)->widget);

	if (!accessible->name) {
		GnomeCalendar *gcal;
		const gchar *label_text;
		GnomeCalendarViewType view_type;

		gcal = e_cal_view_get_calendar (E_CAL_VIEW (week_view));
		label_text = calendar_get_text_for_folder_bar_label (gcal);
		view_type = gnome_calendar_get_view (gcal);

		view_type = gnome_calendar_get_view (gcal);

		if (view_type == GNOME_CAL_MONTH_VIEW)
			accessible->name = g_strconcat ("month view :",
							label_text,
							NULL);


		else
			accessible->name = g_strconcat ("week view :",
							label_text, NULL);
	}
	return accessible->name;
}

static G_CONST_RETURN gchar*
ea_week_view_get_description (AtkObject *accessible)
{
	EWeekView *week_view;

	g_return_val_if_fail (EA_IS_WEEK_VIEW (accessible), NULL);

	if (!GTK_ACCESSIBLE (accessible)->widget)
		return NULL;
	week_view = E_WEEK_VIEW (GTK_ACCESSIBLE (accessible)->widget);

	if (accessible->description)
		return accessible->description;
	else {
		GnomeCalendar *gcal;
		GnomeCalendarViewType view_type;

		gcal = e_cal_view_get_calendar (E_CAL_VIEW (week_view));
		view_type = gnome_calendar_get_view (gcal);

		if (view_type == GNOME_CAL_MONTH_VIEW)
			return "calendar view for a month";
		else
			return "calendar view for one or more weeks";
	}
}

static gint
ea_week_view_get_n_children (AtkObject *accessible)
{
	EWeekView *week_view;

	g_return_val_if_fail (EA_IS_WEEK_VIEW (accessible), -1);

	if (!GTK_ACCESSIBLE (accessible)->widget)
		return -1;
	week_view = E_WEEK_VIEW (GTK_ACCESSIBLE (accessible)->widget);

	return week_view->events->len;
}

static AtkObject *
ea_week_view_ref_child (AtkObject *accessible, gint index)
{
	EWeekView *week_view;
	gint child_num;
	AtkObject *atk_object = NULL;
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	gint span_num = 0;

	g_return_val_if_fail (EA_IS_WEEK_VIEW (accessible), NULL);

	child_num = atk_object_get_n_accessible_children (accessible);
	if (child_num <= 0 || index < 0 || index >= child_num)
		return NULL;

	if (!GTK_ACCESSIBLE (accessible)->widget)
		return NULL;
	week_view = E_WEEK_VIEW (GTK_ACCESSIBLE (accessible)->widget);

	event = &g_array_index (week_view->events,
				EWeekViewEvent, index);
	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + span_num);

	if (event) {
		/* Not use atk_gobject_accessible_for_object here,
		 * we need to do special thing here
		 */
		atk_object = ea_calendar_helpers_get_accessible_for (span->text_item);
		g_object_ref (atk_object);
	}
#ifdef ACC_DEBUG
	printf ("EvoAcc: ea_week_view_ref_child [%d]=%p\n",
		index, atk_object);
#endif
	return atk_object;
}
