/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-calendar-cell.c
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

#include "ea-calendar-cell.h"
#include "ea-calendar-item.h"
#include "ea-factory.h"

/* ECalendarCell */

static void e_calendar_cell_class_init (ECalendarCellClass *class);

EA_FACTORY_GOBJECT (EA_TYPE_CALENDAR_CELL, ea_calendar_cell, ea_calendar_cell_new)

GType
e_calendar_cell_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (ECalendarCellClass),
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) e_calendar_cell_class_init, /* class init */
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			sizeof (ECalendarCell), /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL /* value table */
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "ECalendarCell", &tinfo, 0);
	}

	return type;
}

static void
e_calendar_cell_class_init (ECalendarCellClass *class)
{
    EA_SET_FACTORY (e_calendar_cell_get_type (), ea_calendar_cell);
}

ECalendarCell *
e_calendar_cell_new (ECalendarItem *calitem, gint row, gint column)
{
	GObject *object;
	ECalendarCell *cell;

	g_return_val_if_fail (E_IS_CALENDAR_ITEM (calitem), NULL);

	object = g_object_new (E_TYPE_CALENDAR_CELL, NULL);
	cell = E_CALENDAR_CELL (object);
	cell->calitem = calitem;
	cell->row = row;
	cell->column = column;

#ifdef ACC_DEBUG
	g_print ("EvoAcc: e_calendar_cell created %p\n", (void *)cell);
#endif

	return cell;
}

/* EaCalendarCell */

static void ea_calendar_cell_class_init (EaCalendarCellClass *klass);

static G_CONST_RETURN gchar* ea_calendar_cell_get_name (AtkObject *accessible);
static G_CONST_RETURN gchar* ea_calendar_cell_get_description (AtkObject *accessible);
static AtkObject * ea_calendar_cell_get_parent (AtkObject *accessible);
static gint ea_calendar_cell_get_index_in_parent (AtkObject *accessible);

/* component interface */
static void atk_component_interface_init (AtkComponentIface *iface);
static void component_interface_get_extents (AtkComponent *component,
					     gint *x, gint *y,
					     gint *width, gint *height,
					     AtkCoordType coord_type);

static gpointer parent_class = NULL;

#ifdef ACC_DEBUG
static gint n_ea_calendar_cell_created = 0, n_ea_calendar_cell_destroyed = 0;
static void ea_calendar_cell_finalize (GObject *object);
#endif

GType
ea_calendar_cell_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (EaCalendarCellClass),
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) ea_calendar_cell_class_init, /* class init */
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EaCalendarCell), /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL /* value table */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) atk_component_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = g_type_register_static (ATK_TYPE_GOBJECT_ACCESSIBLE,
					       "EaCalendarCell", &tinfo, 0);
		g_type_add_interface_static (type, ATK_TYPE_COMPONENT,
					     &atk_component_info);
	}

	return type;
}

static void
ea_calendar_cell_class_init (EaCalendarCellClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

#ifdef ACC_DEBUG
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = ea_calendar_cell_finalize;
#endif

	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_calendar_cell_get_name;
	class->get_description = ea_calendar_cell_get_description;

	class->get_parent = ea_calendar_cell_get_parent;
	class->get_index_in_parent = ea_calendar_cell_get_index_in_parent;
}

AtkObject* 
ea_calendar_cell_new (GObject *obj)
{
	gpointer object;
	AtkObject *atk_object;

	g_return_val_if_fail (E_IS_CALENDAR_CELL (obj), NULL);
	object = g_object_new (EA_TYPE_CALENDAR_CELL, NULL);
	atk_object = ATK_OBJECT (object);
	atk_object_initialize (atk_object, obj);
	atk_object->role = ATK_ROLE_UNKNOWN;

#ifdef ACC_DEBUG
	++n_ea_calendar_cell_created;
	g_print ("ACC_DEBUG: n_ea_calendar_cell_created = %d\n",
		n_ea_calendar_cell_created);
#endif
	return atk_object;
}

#ifdef ACC_DEBUG
static void ea_calendar_cell_finalize (GObject *object)
{
	++n_ea_calendar_cell_destroyed;
	g_print ("ACC_DEBUG: n_ea_calendar_cell_destroyed = %d\n",
		n_ea_calendar_cell_destroyed);
}
#endif

static G_CONST_RETURN gchar*
ea_calendar_cell_get_name (AtkObject *accessible)
{
	GObject *g_obj;

	g_return_val_if_fail (EA_IS_CALENDAR_CELL (accessible), NULL);

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE(accessible));
	if (!g_obj)
		/* defunct object*/
		return NULL;

	if (!accessible->name) {
		AtkObject *atk_obj;
		EaCalendarItem *ea_calitem;
		ECalendarCell *cell;
		gint day_index;
		gint year, month, day;
		gchar buffer[128];

		cell = E_CALENDAR_CELL (g_obj);
		atk_obj = ea_calendar_cell_get_parent (accessible);
		ea_calitem = EA_CALENDAR_ITEM (atk_obj);
		day_index = atk_table_get_index_at (ATK_TABLE (ea_calitem),
						    cell->row, cell->column);
		e_calendar_item_get_date_for_offset (cell->calitem, day_index,
						     &year, &month, &day);

		g_snprintf (buffer, 128, "%d-%d-%d", year, month + 1, day);
		ATK_OBJECT_CLASS (parent_class)->set_name (accessible, buffer);
	}
	return accessible->name;
}

static G_CONST_RETURN gchar*
ea_calendar_cell_get_description (AtkObject *accessible)
{
	return ea_calendar_cell_get_name (accessible);
}

static AtkObject *
ea_calendar_cell_get_parent (AtkObject *accessible)
{
	GObject *g_obj;
	ECalendarCell *cell;
	ECalendarItem *calitem;

	g_return_val_if_fail (EA_IS_CALENDAR_CELL (accessible), NULL);

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE(accessible));
	if (!g_obj)
		/* defunct object*/
		return NULL;

	cell = E_CALENDAR_CELL (g_obj);
	calitem = cell->calitem;
	return atk_gobject_accessible_for_object (G_OBJECT (calitem));
}

static gint
ea_calendar_cell_get_index_in_parent (AtkObject *accessible)
{
	GObject *g_obj;
	ECalendarCell *cell;
	AtkObject *parent;

	g_return_val_if_fail (EA_IS_CALENDAR_CELL (accessible), -1);

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE(accessible));
	if (!g_obj)
		return -1;
	cell = E_CALENDAR_CELL (g_obj);
	parent = atk_object_get_parent (accessible);
	return atk_table_get_index_at (ATK_TABLE (parent),
				       cell->row, cell->column);
}

/* Atk Component Interface */

static void 
atk_component_interface_init (AtkComponentIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->get_extents = component_interface_get_extents;
}

static void 
component_interface_get_extents (AtkComponent *component,
				 gint *x, gint *y, gint *width, gint *height,
				 AtkCoordType coord_type)
{
	GObject *g_obj;
	AtkObject *atk_obj, *atk_canvas;
	ECalendarCell *cell;
	ECalendarItem *calitem;
	EaCalendarItem *ea_calitem;
	gint day_index;
	gint year, month, day;
	gint canvas_x, canvas_y, canvas_width, canvas_height;

	*x = *y = *width = *height = 0;

	g_return_if_fail (EA_IS_CALENDAR_CELL (component));


	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE(component));
	if (!g_obj)
		/* defunct object*/
		return;

	cell = E_CALENDAR_CELL (g_obj);
	calitem = cell->calitem;
	atk_obj = atk_gobject_accessible_for_object (G_OBJECT (calitem));
	ea_calitem = EA_CALENDAR_ITEM (atk_obj);
	day_index = atk_table_get_index_at (ATK_TABLE (ea_calitem),
					    cell->row, cell->column);
	e_calendar_item_get_date_for_offset (calitem, day_index,
					     &year, &month, &day);

	if (!e_calendar_item_get_day_extents (calitem,
					      year, month, day,
					      x, y, width, height))
	    return;
	atk_canvas = atk_object_get_parent (ATK_OBJECT (ea_calitem));
	atk_component_get_extents (ATK_COMPONENT (atk_canvas),
					     &canvas_x, &canvas_y,
					     &canvas_width, &canvas_height,
					     coord_type);
	*x += canvas_x;
	*y += canvas_y;
}
