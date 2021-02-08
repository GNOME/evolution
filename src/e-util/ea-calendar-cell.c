/*
 *
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

#include <gtk/gtk.h>
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

		type = g_type_register_static (
			G_TYPE_OBJECT,
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
e_calendar_cell_new (ECalendarItem *calitem,
                     gint row,
                     gint column)
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
	g_print ("EvoAcc: e_calendar_cell created %p\n", (gpointer) cell);
#endif

	return cell;
}

/* EaCalendarCell */

static void ea_calendar_cell_class_init (EaCalendarCellClass *klass);
static void ea_calendar_cell_init (EaCalendarCell *a11y);

static const gchar * ea_calendar_cell_get_name (AtkObject *accessible);
static const gchar * ea_calendar_cell_get_description (AtkObject *accessible);
static AtkObject * ea_calendar_cell_get_parent (AtkObject *accessible);
static gint ea_calendar_cell_get_index_in_parent (AtkObject *accessible);
static AtkStateSet *ea_calendar_cell_ref_state_set (AtkObject *accessible);

/* component interface */
static void atk_component_interface_init (AtkComponentIface *iface);
static void component_interface_get_extents (AtkComponent *component,
					     gint *x, gint *y,
					     gint *width, gint *height,
					     AtkCoordType coord_type);
static gboolean component_interface_grab_focus (AtkComponent *component);

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
			(GInstanceInitFunc) ea_calendar_cell_init, /* instance init */
			NULL /* value table */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) atk_component_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = g_type_register_static (
			ATK_TYPE_GOBJECT_ACCESSIBLE,
			"EaCalendarCell", &tinfo, 0);
		g_type_add_interface_static (
			type, ATK_TYPE_COMPONENT,
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
	class->ref_state_set = ea_calendar_cell_ref_state_set;
}

static void
ea_calendar_cell_init (EaCalendarCell *a11y)
{
	a11y->state_set = atk_state_set_new ();
	atk_state_set_add_state (a11y->state_set, ATK_STATE_TRANSIENT);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_ENABLED);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_SENSITIVE);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_SELECTABLE);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_SHOWING);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_VISIBLE);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_FOCUSABLE);
}

AtkObject *
ea_calendar_cell_new (GObject *obj)
{
	gpointer object;
	AtkObject *atk_object;

	g_return_val_if_fail (E_IS_CALENDAR_CELL (obj), NULL);
	object = g_object_new (EA_TYPE_CALENDAR_CELL, NULL);
	atk_object = ATK_OBJECT (object);
	atk_object_initialize (atk_object, obj);
	atk_object->role = ATK_ROLE_TABLE_CELL;

#ifdef ACC_DEBUG
	++n_ea_calendar_cell_created;
	g_print (
		"ACC_DEBUG: n_ea_calendar_cell_created = %d\n",
		n_ea_calendar_cell_created);
#endif
	return atk_object;
}

#ifdef ACC_DEBUG
static void ea_calendar_cell_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);

	++n_ea_calendar_cell_destroyed;
	g_print (
		"ACC_DEBUG: n_ea_calendar_cell_destroyed = %d\n",
		n_ea_calendar_cell_destroyed);
}
#endif

static const gchar *
ea_calendar_cell_get_name (AtkObject *accessible)
{
	GObject *g_obj;

	g_return_val_if_fail (EA_IS_CALENDAR_CELL (accessible), NULL);

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible));
	if (!g_obj)
		/* defunct object*/
		return NULL;

	if (!accessible->name) {
		ECalendarCell *cell;
		gint year, month, day;
		gchar buffer[128];

		cell = E_CALENDAR_CELL (g_obj);
		if (e_calendar_item_get_date_for_cell (cell->calitem, cell->row, cell->column, &year, &month, &day))
			g_snprintf (buffer, 128, "%d-%d-%d", year, month + 1, day);
		else
			buffer[0] = '\0';
		ATK_OBJECT_CLASS (parent_class)->set_name (accessible, buffer);
	}
	return accessible->name;
}

static const gchar *
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

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible));
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

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible));
	if (!g_obj)
		return -1;
	cell = E_CALENDAR_CELL (g_obj);
	parent = atk_object_get_parent (accessible);
	return atk_table_get_index_at (
		ATK_TABLE (parent),
		cell->row, cell->column);
}

static AtkStateSet *
ea_calendar_cell_ref_state_set (AtkObject *accessible)
{
	EaCalendarCell *atk_cell = EA_CALENDAR_CELL (accessible);

	g_return_val_if_fail (atk_cell->state_set, NULL);

	g_object_ref (atk_cell->state_set);

	return atk_cell->state_set;

}

/* Atk Component Interface */

static void
atk_component_interface_init (AtkComponentIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->get_extents = component_interface_get_extents;
	iface->grab_focus = component_interface_grab_focus;
}

static void
component_interface_get_extents (AtkComponent *component,
                                 gint *x,
                                 gint *y,
                                 gint *width,
                                 gint *height,
                                 AtkCoordType coord_type)
{
	GObject *g_obj;
	AtkObject *atk_obj, *atk_canvas;
	ECalendarCell *cell;
	ECalendarItem *calitem;
	EaCalendarItem *ea_calitem;
	gint year, month, day;
	gint canvas_x, canvas_y, canvas_width, canvas_height;

	*x = *y = *width = *height = 0;

	g_return_if_fail (EA_IS_CALENDAR_CELL (component));

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (component));
	if (!g_obj)
		/* defunct object*/
		return;

	cell = E_CALENDAR_CELL (g_obj);
	calitem = cell->calitem;
	atk_obj = atk_gobject_accessible_for_object (G_OBJECT (calitem));
	ea_calitem = EA_CALENDAR_ITEM (atk_obj);
	if (!e_calendar_item_get_date_for_cell (calitem, cell->row, cell->column, &year, &month, &day))
		return;

	if (!e_calendar_item_get_day_extents (calitem,
					      year, month, day,
					      x, y, width, height))
	    return;
	atk_canvas = atk_object_get_parent (ATK_OBJECT (ea_calitem));
	atk_component_get_extents (
		ATK_COMPONENT (atk_canvas),
		&canvas_x, &canvas_y,
		&canvas_width, &canvas_height,
		coord_type);
	*x += canvas_x;
	*y += canvas_y;
}

static gboolean
component_interface_grab_focus (AtkComponent *component)
{
	GObject *g_obj;
	GtkWidget *toplevel;
	AtkObject *ea_calitem;
	ECalendarItem *calitem;
	EaCalendarCell *a11y;
	gint index;

	a11y = EA_CALENDAR_CELL (component);
	ea_calitem = ea_calendar_cell_get_parent (ATK_OBJECT (a11y));

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (ea_calitem));
	calitem = E_CALENDAR_ITEM (g_obj);

	index = atk_object_get_index_in_parent (ATK_OBJECT (a11y));

	atk_selection_clear_selection (ATK_SELECTION (ea_calitem));
	atk_selection_add_selection (ATK_SELECTION (ea_calitem), index);

	gtk_widget_grab_focus (GTK_WIDGET (GNOME_CANVAS_ITEM (calitem)->canvas));
	toplevel = gtk_widget_get_toplevel (
		GTK_WIDGET (GNOME_CANVAS_ITEM (calitem)->canvas));
	if (toplevel && gtk_widget_is_toplevel (toplevel))
		gtk_window_present (GTK_WINDOW (toplevel));

	return TRUE;

}
