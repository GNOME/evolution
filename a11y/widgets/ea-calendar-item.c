/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-calendar-item.c
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

#include <stdio.h>
#include <string.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <glib/gdate.h>
#include "ea-calendar-item.h"

static void ea_calendar_item_class_init (EaCalendarItemClass *klass);

static G_CONST_RETURN gchar* ea_calendar_item_get_name (AtkObject *accessible);
static G_CONST_RETURN gchar* ea_calendar_item_get_description (AtkObject *accessible);

static gpointer parent_class = NULL;

GType
ea_calendar_item_get_type (void)
{
	static GType type = 0;
	AtkObjectFactory *factory;
	GTypeQuery query;
	GType derived_atk_type;


	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (EaCalendarItemClass),
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) ea_calendar_item_class_init, /* class init */
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EaCalendarItem), /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL /* value table */
		};

		/*
		 * Figure out the size of the class and instance
		 * we are run-time deriving from (GailCanvasItem, in this case)
		 */

		factory = atk_registry_get_factory (atk_get_default_registry (),
						    GNOME_TYPE_CANVAS_ITEM);
		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);

		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		type = g_type_register_static (derived_atk_type,
					       "EaCalendarItem", &tinfo, 0);
	}

	return type;
}

static void
ea_calendar_item_class_init (EaCalendarItemClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_calendar_item_get_name;
	class->get_description = ea_calendar_item_get_description;
}

AtkObject* 
ea_calendar_item_new (GObject *obj)
{
	gpointer object;
	AtkObject *atk_object;

	g_return_val_if_fail (E_IS_CALENDAR_ITEM (obj), NULL);
	object = g_object_new (EA_TYPE_CALENDAR_ITEM, NULL);
	atk_object = ATK_OBJECT (object);
	atk_object_initialize (atk_object, obj);
	atk_object->role = ATK_ROLE_CALENDAR;
#ifdef ACC_DEBUG
	g_print ("ea_calendar_item created %p\n", atk_object);
#endif
	return atk_object;
}

static G_CONST_RETURN gchar*
ea_calendar_item_get_name (AtkObject *accessible)
{
	GObject *g_obj;
	ECalendarItem *calitem;
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	GDate select_start, select_end;
	static gchar new_name[256] = "";

	g_return_val_if_fail (EA_IS_CALENDAR_ITEM (accessible), NULL);

	if (accessible->name)
		return accessible->name;

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE(accessible));
	g_return_val_if_fail (E_IS_CALENDAR_ITEM (g_obj), NULL);

	calitem = E_CALENDAR_ITEM (g_obj);
	if (e_calendar_item_get_date_range (calitem,
					    &start_year, &start_month, &start_day,
					    &end_year, &end_month, &end_day)) {
		++start_month;
		++end_month;
		sprintf (new_name, "calendar date range: from %d-%d-%d to %d-%d-%d.",
			 start_year, start_month, start_day,
			 end_year, end_month, end_day);
	}
	if (e_calendar_item_get_selection (calitem, &select_start, &select_end)) {
		gint year1, year2, month1, month2, day1, day2;

		year1 = g_date_get_year (&select_start);
		month1  = g_date_get_month (&select_start);
		day1 = g_date_get_day (&select_start);

		year2 = g_date_get_year (&select_end);
		month2  = g_date_get_month (&select_end);
		day2 = g_date_get_day (&select_end);

		sprintf (new_name + strlen (new_name),
			 "current selection: from %d-%d-%d to %d-%d-%d.",
			 year1, month1, day1,
			 year2, month2, day2);
	}

	return new_name;
}

static G_CONST_RETURN gchar*
ea_calendar_item_get_description (AtkObject *accessible)
{
	if (accessible->description)
		return accessible->description;

	return "evolution calendar widget";
}
