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
#include <time.h>
#include <string.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <glib/gdate.h>
#include <gal/util/e-util.h>
#include <libgnome/gnome-i18n.h>
#include "ea-calendar-item.h"
#include "ea-calendar-cell.h"
#include "ea-cell-table.h"

#define EA_CALENDAR_COLUMN_NUM E_CALENDAR_COLS_PER_MONTH

/* EaCalendarItem */
static void ea_calendar_item_class_init (EaCalendarItemClass *klass);
static void ea_calendar_item_finalize (GObject *object);

static G_CONST_RETURN gchar* ea_calendar_item_get_name (AtkObject *accessible);
static G_CONST_RETURN gchar* ea_calendar_item_get_description (AtkObject *accessible);
static gint ea_calendar_item_get_n_children (AtkObject *accessible);
static AtkObject *ea_calendar_item_ref_child (AtkObject *accessible, gint index);
static AtkStateSet* ea_calendar_item_ref_state_set (AtkObject *accessible);

/* atk table interface */
static void atk_table_interface_init (AtkTableIface *iface);
static gint table_interface_get_index_at (AtkTable *table,
					  gint     row,
					  gint     column);
static gint table_interface_get_column_at_index (AtkTable *table,
						 gint     index);
static gint table_interface_get_row_at_index (AtkTable *table,
					      gint     index);
static AtkObject* table_interface_ref_at (AtkTable *table,
					  gint     row, 
					  gint     column);
static gint table_interface_get_n_rows (AtkTable *table);
static gint table_interface_get_n_columns (AtkTable *table);
static gint table_interface_get_column_extent_at (AtkTable      *table,
						  gint          row,
						  gint          column);
static gint table_interface_get_row_extent_at (AtkTable      *table,
					       gint          row,
					       gint          column);

static gboolean table_interface_is_row_selected (AtkTable *table,
						 gint     row);
static gboolean table_interface_is_column_selected (AtkTable *table,
						    gint     row);
static gboolean table_interface_is_selected (AtkTable *table,
					     gint     row,
					     gint     column);
static gint table_interface_get_selected_rows (AtkTable *table,
					       gint **rows_selected);
static gint table_interface_get_selected_columns (AtkTable *table,
						  gint     **columns_selected);
static gboolean table_interface_add_row_selection (AtkTable *table, gint row);
static gboolean table_interface_remove_row_selection (AtkTable *table,
						      gint row);
static gboolean table_interface_add_column_selection (AtkTable *table,
						      gint column);
static gboolean table_interface_remove_column_selection (AtkTable *table,
							 gint column);
static AtkObject* table_interface_get_row_header (AtkTable *table, gint row);
static AtkObject* table_interface_get_column_header (AtkTable *table,
						     gint in_col);
static AtkObject* table_interface_get_caption (AtkTable *table);

static G_CONST_RETURN gchar*
table_interface_get_column_description (AtkTable *table, gint in_col);

static G_CONST_RETURN gchar*
table_interface_get_row_description (AtkTable *table, gint row);

static AtkObject* table_interface_get_summary (AtkTable *table);

/* atk selection interface */
static void atk_selection_interface_init (AtkSelectionIface *iface);
static gboolean selection_interface_add_selection (AtkSelection *selection,
						   gint i);
static gboolean selection_interface_clear_selection (AtkSelection *selection);
static AtkObject* selection_interface_ref_selection (AtkSelection *selection,
						     gint i);
static gint selection_interface_get_selection_count (AtkSelection *selection);
static gboolean selection_interface_is_child_selected (AtkSelection *selection,
						       gint i);

/* callbacks */
static void selection_preview_change_cb (ECalendarItem *calitem);
static void date_range_changed_cb (ECalendarItem *calitem);
static void selection_changed_cb (ECalendarItem *calitem);

/* helpers */
static EaCellTable *ea_calendar_item_get_cell_data (EaCalendarItem *ea_calitem);
static void ea_calendar_item_destory_cell_data (EaCalendarItem *ea_calitem);
static gboolean ea_calendar_item_get_column_label (EaCalendarItem *ea_calitem,
						   gint column,
						   gchar *buffer,
						   gint buffer_size);
static gboolean ea_calendar_item_get_row_label (EaCalendarItem *ea_calitem,
						gint row,
						gchar *buffer,
						gint buffer_size);
static gboolean e_calendar_item_get_offset_for_date (ECalendarItem *calitem,
						     gint year, gint month, gint day,
						     gint *offset);

#ifdef ACC_DEBUG
static gint n_ea_calendar_item_created = 0;
static gint n_ea_calendar_item_destroyed = 0;
#endif

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

		static const GInterfaceInfo atk_table_info = {
			(GInterfaceInitFunc) atk_table_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};
		static const GInterfaceInfo atk_selection_info = {
			(GInterfaceInitFunc) atk_selection_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
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
		g_type_add_interface_static (type, ATK_TYPE_TABLE,
					     &atk_table_info);
		g_type_add_interface_static (type, ATK_TYPE_SELECTION,
					     &atk_selection_info);
	}

	return type;
}

static void
ea_calendar_item_class_init (EaCalendarItemClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	gobject_class->finalize = ea_calendar_item_finalize;
	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_calendar_item_get_name;
	class->get_description = ea_calendar_item_get_description;
        class->ref_state_set = ea_calendar_item_ref_state_set;

	class->get_n_children = ea_calendar_item_get_n_children;
	class->ref_child = ea_calendar_item_ref_child;
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
	atk_object->role = ATK_ROLE_TABLE;
#ifdef ACC_DEBUG
	++n_ea_calendar_item_created;
	g_print ("ACC_DEBUG: n_ea_calendar_item_created = %d\n",
		n_ea_calendar_item_created);
#endif
	/* connect signal handlers */
	g_signal_connect (obj, "selection_preview_changed",
			  G_CALLBACK (selection_preview_change_cb),
			  atk_object);
	g_signal_connect (obj, "date_range_changed",
			  G_CALLBACK (date_range_changed_cb),
			  atk_object);
	g_signal_connect (obj, "selection_preview_changed",
			  G_CALLBACK (selection_changed_cb),
			  atk_object);

	return atk_object;
}

static void
ea_calendar_item_finalize (GObject *object)
{
	EaCalendarItem *ea_calitem;

	g_return_if_fail (EA_IS_CALENDAR_ITEM (object));

	ea_calitem = EA_CALENDAR_ITEM (object);

	/* Free the allocated cell data */
	ea_calendar_item_destory_cell_data (ea_calitem);

	G_OBJECT_CLASS (parent_class)->finalize (object);
#ifdef ACC_DEBUG
	++n_ea_calendar_item_destroyed;
	printf ("ACC_DEBUG: n_ea_calendar_item_destroyed = %d\n",
		n_ea_calendar_item_destroyed);
#endif
}

static G_CONST_RETURN gchar*
ea_calendar_item_get_name (AtkObject *accessible)
{
	GObject *g_obj;
	ECalendarItem *calitem;
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	static gchar new_name[256] = "";
        gchar buffer_start[128] = "";
        gchar buffer_end[128] = "";
        struct tm day_start = { 0 };
        struct tm day_end = { 0 };

	g_return_val_if_fail (EA_IS_CALENDAR_ITEM (accessible), NULL);

	if (accessible->name)
		return accessible->name;

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE(accessible));
	g_return_val_if_fail (E_IS_CALENDAR_ITEM (g_obj), NULL);

	calitem = E_CALENDAR_ITEM (g_obj);
	if (e_calendar_item_get_date_range (calitem,
					    &start_year, &start_month, &start_day,
                                            &end_year, &end_month, &end_day)) {

                day_start.tm_year = start_year - 1900;
                day_start.tm_mon = start_month;
                day_start.tm_mday = start_day;
                day_start.tm_isdst = -1;
                e_utf8_strftime (buffer_start, sizeof (buffer_start), _(" %d %B %Y"), &day_start);

                day_end.tm_year = end_year - 1900;
                day_end.tm_mon = end_month;
                day_end.tm_mday = end_day;
                day_end.tm_isdst = -1;
                e_utf8_strftime (buffer_end, sizeof (buffer_end), _(" %d %B %Y"), &day_end);

                strcat (new_name, _("calendar (from "));
                strcat (new_name, buffer_start);
                strcat (new_name, _(" to "));
                strcat (new_name, buffer_end);
                strcat (new_name, _(")"));
        }

#if 0
	if (e_calendar_item_get_selection (calitem, &select_start, &select_end)) {
		GDate select_start, select_end;
		gint year1, year2, month1, month2, day1, day2;

		year1 = g_date_get_year (&select_start);
		month1  = g_date_get_month (&select_start);
		day1 = g_date_get_day (&select_start);

		year2 = g_date_get_year (&select_end);
		month2  = g_date_get_month (&select_end);
		day2 = g_date_get_day (&select_end);

		sprintf (new_name + strlen (new_name),
			 " : current selection: from %d-%d-%d to %d-%d-%d.",
			 year1, month1, day1,
			 year2, month2, day2);
	}
#endif

	return new_name;
}

static G_CONST_RETURN gchar*
ea_calendar_item_get_description (AtkObject *accessible)
{
	if (accessible->description)
		return accessible->description;

	return _("evolution calendar item");
}

static AtkStateSet*
ea_calendar_item_ref_state_set (AtkObject *accessible)
{
        AtkStateSet *state_set;
        GObject *g_obj;

        state_set = ATK_OBJECT_CLASS (parent_class)->ref_state_set (accessible);
        g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE(accessible));
        if (!g_obj)
                return state_set;

        atk_state_set_add_state (state_set, ATK_STATE_ENABLED);
        atk_state_set_add_state (state_set, ATK_STATE_SENSITIVE);
        
        return state_set;
}

static gint
ea_calendar_item_get_n_children (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarItem *calitem;
	gint n_children = 0;
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	GDate *start_date, *end_date;

	g_return_val_if_fail (EA_IS_CALENDAR_ITEM (accessible), -1);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	calitem = E_CALENDAR_ITEM (g_obj);
	if (!e_calendar_item_get_date_range (calitem, &start_year,
					     &start_month, &start_day,
					     &end_year, &end_month,
					     &end_day))
		return 0;

	start_date = g_date_new_dmy (start_day, start_month + 1, start_year);
	end_date = g_date_new_dmy (end_day, end_month + 1, end_year);

	n_children = g_date_days_between (start_date, end_date) + 1;
	g_free (start_date);
	g_free (end_date);
	return n_children;
}

static AtkObject *
ea_calendar_item_ref_child (AtkObject *accessible, gint index)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarItem *calitem;
	gint n_children;
	ECalendarCell *cell;
	EaCellTable *cell_data;
	EaCalendarItem *ea_calitem;

	g_return_val_if_fail (EA_IS_CALENDAR_ITEM (accessible), NULL);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	calitem = E_CALENDAR_ITEM (g_obj);

	n_children = ea_calendar_item_get_n_children (accessible);
	if (index < 0 || index >= n_children)
		return NULL;

	ea_calitem = EA_CALENDAR_ITEM (accessible);
	cell_data = ea_calendar_item_get_cell_data (ea_calitem);
	if (!cell_data)
		return NULL;

	cell = ea_cell_table_get_cell_at_index (cell_data, index);
	if (!cell) {
		cell = e_calendar_cell_new (calitem,
					    index / EA_CALENDAR_COLUMN_NUM,
					    index % EA_CALENDAR_COLUMN_NUM);
		ea_cell_table_set_cell_at_index (cell_data, index, cell);
		g_object_unref (cell);
	}

#ifdef ACC_DEBUG
	g_print ("AccDebug: ea_calendar_item children[%d]=%p\n", index,
		 (gpointer)cell);
#endif
	return g_object_ref (atk_gobject_accessible_for_object (G_OBJECT(cell)));
}

/* atk table interface */

static void 
atk_table_interface_init (AtkTableIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->ref_at = table_interface_ref_at;

	iface->get_n_rows = table_interface_get_n_rows;
	iface->get_n_columns = table_interface_get_n_columns;
	iface->get_index_at = table_interface_get_index_at;
	iface->get_column_at_index = table_interface_get_column_at_index;
	iface->get_row_at_index = table_interface_get_row_at_index;
	iface->get_column_extent_at = table_interface_get_column_extent_at;
	iface->get_row_extent_at = table_interface_get_row_extent_at;

	iface->is_selected = table_interface_is_selected;
	iface->get_selected_rows = table_interface_get_selected_rows;
	iface->get_selected_columns = table_interface_get_selected_columns;
	iface->is_row_selected = table_interface_is_row_selected;
	iface->is_column_selected = table_interface_is_column_selected;
	iface->add_row_selection = table_interface_add_row_selection;
	iface->remove_row_selection = table_interface_remove_row_selection;
	iface->add_column_selection = table_interface_add_column_selection;
	iface->remove_column_selection = table_interface_remove_column_selection;

	iface->get_row_header = table_interface_get_row_header;
	iface->get_column_header = table_interface_get_column_header;
	iface->get_caption = table_interface_get_caption;
	iface->get_summary = table_interface_get_summary;
	iface->get_row_description = table_interface_get_row_description;
	iface->get_column_description = table_interface_get_column_description;
}

static AtkObject* 
table_interface_ref_at (AtkTable *table,
			gint     row, 
			gint     column)
{
	gint index;

	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (table);
	index = EA_CALENDAR_COLUMN_NUM * row + column;
	return ea_calendar_item_ref_child (ATK_OBJECT (ea_calitem), index);
}

static gint 
table_interface_get_n_rows (AtkTable *table)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (table);
	gint n_children;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	n_children = ea_calendar_item_get_n_children (ATK_OBJECT (ea_calitem));
	return (n_children - 1) / EA_CALENDAR_COLUMN_NUM + 1;
}

static gint 
table_interface_get_n_columns (AtkTable *table)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	return EA_CALENDAR_COLUMN_NUM;
}

static gint
table_interface_get_index_at (AtkTable *table,
			      gint     row,
			      gint     column)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	return row * EA_CALENDAR_COLUMN_NUM + column;
}

static gint
table_interface_get_column_at_index (AtkTable *table,
				     gint index)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (table);
	gint n_children;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	n_children = ea_calendar_item_get_n_children (ATK_OBJECT (ea_calitem));
	if (index >= 0 && index < n_children)
		return index % EA_CALENDAR_COLUMN_NUM;
	return -1;
}

static gint
table_interface_get_row_at_index (AtkTable *table,
				  gint     index)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (table);
	gint n_children;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	n_children = ea_calendar_item_get_n_children (ATK_OBJECT (ea_calitem));
	if (index >= 0 && index < n_children)
		return index / EA_CALENDAR_COLUMN_NUM;
	return -1;
}

static gint
table_interface_get_column_extent_at (AtkTable      *table,
				      gint          row,
				      gint          column)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarItem *calitem;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	calitem = E_CALENDAR_ITEM (g_obj);
	return calitem->cell_width;
}

static gint 
table_interface_get_row_extent_at (AtkTable *table,
				   gint row, gint column)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarItem *calitem;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	calitem = E_CALENDAR_ITEM (g_obj);
	return calitem->cell_height;
}

/* any day in the row is selected, the row is selected */
static gboolean 
table_interface_is_row_selected (AtkTable *table,
				 gint row)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	gint n_rows;
	ECalendarItem *calitem;
	gint row_index_start, row_index_end;
	gint sel_index_start, sel_index_end;

	GDate start_date, end_date;

	g_return_val_if_fail (EA_IS_CALENDAR_ITEM (table), FALSE);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (table);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	n_rows = table_interface_get_n_rows (table);
	if (row < 0 || row >= n_rows)
		return FALSE;

	row_index_start = row * EA_CALENDAR_COLUMN_NUM;
	row_index_end = row_index_start + EA_CALENDAR_COLUMN_NUM - 1;

	calitem = E_CALENDAR_ITEM (g_obj);
	e_calendar_item_get_selection (calitem, &start_date, &end_date);

	e_calendar_item_get_offset_for_date (calitem,
					     g_date_get_year (&start_date),
					     g_date_get_month (&start_date),
					     g_date_get_day (&start_date),
					     &sel_index_start);
	e_calendar_item_get_offset_for_date (calitem,
					     g_date_get_year (&end_date),
					     g_date_get_month (&end_date),
					     g_date_get_day (&end_date),
					     &sel_index_end);

	if ((sel_index_start < row_index_start &&
	     sel_index_end >= row_index_start) ||
	    (sel_index_start >= row_index_start &&
	     sel_index_start <= row_index_end))
	    return TRUE;
	return FALSE;
}

static gboolean 
table_interface_is_selected (AtkTable *table, 
			     gint     row, 
			     gint     column)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	gint n_rows, n_columns;
	ECalendarItem *calitem;
	gint index;
	gint sel_index_start, sel_index_end;

	GDate start_date, end_date;

	g_return_val_if_fail (EA_IS_CALENDAR_ITEM (table), FALSE);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (table);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	n_rows = table_interface_get_n_rows (table);
	if (row < 0 || row >= n_rows)
		return FALSE;
	n_columns = table_interface_get_n_columns (table);
	if (column < 0 || column >= n_columns)
		return FALSE;

	index = table_interface_get_index_at (table, row, column);

	calitem = E_CALENDAR_ITEM (g_obj);
	e_calendar_item_get_selection (calitem, &start_date, &end_date);

	e_calendar_item_get_offset_for_date (calitem,
					     g_date_get_year (&start_date),
					     g_date_get_month (&start_date),
					     g_date_get_day (&start_date),
					     &sel_index_start);
	e_calendar_item_get_offset_for_date (calitem,
					     g_date_get_year (&end_date),
					     g_date_get_month (&end_date),
					     g_date_get_day (&end_date), &sel_index_end);

	if (sel_index_start <= index && sel_index_end >= index)
	    return TRUE;
	return FALSE;
}

static gboolean 
table_interface_is_column_selected (AtkTable *table,
				    gint column)
{
	return FALSE;
}

static gint 
table_interface_get_selected_rows (AtkTable *table,
				   gint **rows_selected)
{
	*rows_selected = NULL;
	return -1;
}

static gint 
table_interface_get_selected_columns (AtkTable *table,
				      gint **columns_selected)
{
	*columns_selected = NULL;
	return -1;
}

static gboolean 
table_interface_add_row_selection (AtkTable *table, 
				   gint row)
{
	return FALSE;
}

static gboolean 
table_interface_remove_row_selection (AtkTable *table, 
				      gint row)
{
	return FALSE;
}

static gboolean 
table_interface_add_column_selection (AtkTable *table, 
				      gint column)
{
	return FALSE;
}

static gboolean 
table_interface_remove_column_selection (AtkTable *table, 
					 gint     column)
{
	/* FIXME: NOT IMPLEMENTED */
	return FALSE;
}

static AtkObject* 
table_interface_get_row_header (AtkTable *table, 
				gint     row)
{
	/* FIXME: NOT IMPLEMENTED */
	return NULL;
}

static AtkObject* 
table_interface_get_column_header (AtkTable *table, 
				   gint     in_col)
{
	/* FIXME: NOT IMPLEMENTED */
	return NULL;
}

static AtkObject*
table_interface_get_caption (AtkTable	*table)
{
	/* FIXME: NOT IMPLEMENTED */
	return NULL;
}

static G_CONST_RETURN gchar*
table_interface_get_column_description (AtkTable *table, gint in_col)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarItem *calitem;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (table);
	const gchar *description = NULL;
	EaCellTable *cell_data;
	gint n_columns;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	calitem = E_CALENDAR_ITEM (g_obj);
	n_columns = table_interface_get_n_columns (table);
	if (in_col < 0 || in_col >= n_columns)
		return NULL;
	cell_data = ea_calendar_item_get_cell_data (ea_calitem);
	if (!cell_data)
		return NULL;

	description = ea_cell_table_get_column_label (cell_data, in_col);
	if (!description) {
		gchar buffer[128] = "column description";
		ea_calendar_item_get_column_label (ea_calitem, in_col,
						   buffer, sizeof (buffer));
		ea_cell_table_set_column_label (cell_data, in_col, buffer);
		description = ea_cell_table_get_column_label (cell_data,
							      in_col);
	}
	return description;
}

static G_CONST_RETURN gchar*
table_interface_get_row_description (AtkTable *table, gint row)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarItem *calitem;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (table);
	const gchar *description = NULL;
	EaCellTable *cell_data;
	gint n_rows;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	calitem = E_CALENDAR_ITEM (g_obj);
	n_rows = table_interface_get_n_rows (table);
	if (row < 0 || row >= n_rows)
		return NULL;
	cell_data = ea_calendar_item_get_cell_data (ea_calitem);
	if (!cell_data)
		return NULL;

	description = ea_cell_table_get_row_label (cell_data, row);
	if (!description) {
		gchar buffer[128] = "row description";
		ea_calendar_item_get_row_label (ea_calitem, row,
						buffer, sizeof (buffer));
		ea_cell_table_set_row_label (cell_data, row, buffer);
		description = ea_cell_table_get_row_label (cell_data,
							   row);
	}
	return description;
}

static AtkObject*
table_interface_get_summary (AtkTable	*table)
{
	/* FIXME: NOT IMPLEMENTED */
	return NULL;
}

/* atkselection interface */

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->add_selection = selection_interface_add_selection;
	iface->clear_selection = selection_interface_clear_selection;
	iface->ref_selection = selection_interface_ref_selection;
	iface->get_selection_count = selection_interface_get_selection_count;
	iface->is_child_selected = selection_interface_is_child_selected;
}

static gboolean
selection_interface_add_selection (AtkSelection *selection, gint index)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarItem *calitem;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (selection);
	gint year, month, day;
	GDate start_date, end_date;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	calitem = E_CALENDAR_ITEM (g_obj);
	if (!e_calendar_item_get_date_for_offset (calitem, index,
						  &year, &month, &day))
		return FALSE;

	/* FIXME: not support mulit-selection */
	g_date_set_dmy (&start_date, day, month + 1, year);
	end_date = start_date;
	e_calendar_item_set_selection (calitem, &start_date, &end_date);
	return TRUE;
}

static gboolean
selection_interface_clear_selection (AtkSelection *selection)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarItem *calitem;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (selection);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	calitem = E_CALENDAR_ITEM (g_obj);
	e_calendar_item_set_selection (calitem, NULL, NULL);

	return TRUE;
}

static AtkObject*  
selection_interface_ref_selection (AtkSelection *selection, gint i)
{
	GObject *g_obj;
	ECalendarItem *calitem;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (selection);
	gint count, sel_offset;
	GDate start_date, end_date;

	count = selection_interface_get_selection_count (selection);
	if (i < 0 || i >= count)
		return NULL;

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (ea_calitem));

	calitem = E_CALENDAR_ITEM (g_obj);
	e_calendar_item_get_selection (calitem, &start_date, &end_date);
	if (!e_calendar_item_get_offset_for_date (calitem,
						  g_date_get_year (&start_date),
						  g_date_get_month (&start_date) - 1,
						  g_date_get_day (&start_date),
						  &sel_offset))
		return NULL;

	return ea_calendar_item_ref_child (ATK_OBJECT (selection), sel_offset + i);
}

static gint
selection_interface_get_selection_count (AtkSelection *selection)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarItem *calitem;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (selection);
	GDate start_date, end_date;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return 0;

	calitem = E_CALENDAR_ITEM (g_obj);
	e_calendar_item_get_selection (calitem, &start_date, &end_date);

	return g_date_days_between (&start_date, &end_date) + 1;
}

static gboolean
selection_interface_is_child_selected (AtkSelection *selection, gint index)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarItem *calitem;
	EaCalendarItem* ea_calitem = EA_CALENDAR_ITEM (selection);
	gint row, column, n_children;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	calitem = E_CALENDAR_ITEM (g_obj);
	n_children = atk_object_get_n_accessible_children (ATK_OBJECT (selection));
	if (index < 0 || index >= n_children)
		return FALSE;

	row = index / EA_CALENDAR_COLUMN_NUM;
	column = index % EA_CALENDAR_COLUMN_NUM;

	return table_interface_is_selected (ATK_TABLE (selection), row, column);
}

/* callbacks */

static void
selection_preview_change_cb (ECalendarItem *calitem)
{
	AtkObject *atk_obj;
	AtkObject *item_cell = NULL;

	g_return_if_fail (E_IS_CALENDAR_ITEM (calitem));
	atk_obj = atk_gobject_accessible_for_object (G_OBJECT (calitem));

	/* only deal with the first selected child, for now */
	item_cell = atk_selection_ref_selection (ATK_SELECTION (atk_obj),
						 0);
	if (item_cell) {
		AtkStateSet *state_set;
		state_set = atk_object_ref_state_set (item_cell);
		atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
		g_object_unref (state_set);
	}
	g_signal_emit_by_name (atk_obj,
			       "active-descendant-changed",
			       item_cell);
	g_signal_emit_by_name (atk_obj, "selection_changed");
}

static void
date_range_changed_cb (ECalendarItem *calitem)
{
	AtkObject *atk_obj;

	g_return_if_fail (E_IS_CALENDAR_ITEM (calitem));
	atk_obj = atk_gobject_accessible_for_object (G_OBJECT (calitem));
	ea_calendar_item_destory_cell_data (EA_CALENDAR_ITEM (atk_obj));

	g_signal_emit_by_name (atk_obj, "model_changed");
}

static void
selection_changed_cb (ECalendarItem *calitem)
{
	selection_preview_change_cb (calitem);
}

/* helpers */

static EaCellTable *
ea_calendar_item_get_cell_data (EaCalendarItem *ea_calitem)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarItem *calitem;
	EaCellTable *cell_data;

	g_return_val_if_fail (ea_calitem, NULL);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	calitem = E_CALENDAR_ITEM (g_obj);

	cell_data = g_object_get_data (G_OBJECT(ea_calitem),
				       "ea-calendar-cell-table");

	if (!cell_data) {
		gint n_cells = ea_calendar_item_get_n_children (ATK_OBJECT(ea_calitem));
		cell_data = ea_cell_table_create (n_cells/EA_CALENDAR_COLUMN_NUM,
						  EA_CALENDAR_COLUMN_NUM,
						  FALSE);
		g_object_set_data (G_OBJECT(ea_calitem),
				   "ea-calendar-cell-table", cell_data);
	}
	return cell_data;
}

static void
ea_calendar_item_destory_cell_data (EaCalendarItem *ea_calitem)
{
	EaCellTable *cell_data;

	g_return_if_fail (ea_calitem);

	cell_data = g_object_get_data (G_OBJECT(ea_calitem),
				       "ea-calendar-cell-table");
	if (cell_data) {
		g_object_set_data (G_OBJECT(ea_calitem),
				   "ea-calendar-cell-table", NULL);
		ea_cell_table_destroy (cell_data);
	}
}

static gboolean
ea_calendar_item_get_row_label (EaCalendarItem *ea_calitem, gint row,
				gchar *buffer, gint buffer_size)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarItem *calitem;
	gint index, week_num;
	gint year, month, day;

	g_return_val_if_fail (ea_calitem, FALSE);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	calitem = E_CALENDAR_ITEM (g_obj);

	index = atk_table_get_index_at (ATK_TABLE (ea_calitem), row, 0);
	if (!e_calendar_item_get_date_for_offset (calitem, index,
						  &year, &month, &day))
		return FALSE;

	week_num = e_calendar_item_get_week_number (calitem,
						    day, month, year);

	g_snprintf (buffer, buffer_size, "week number : %d", week_num);
	return TRUE;
}

static gboolean
ea_calendar_item_get_column_label (EaCalendarItem *ea_calitem, gint column,
				   gchar *buffer, gint buffer_size)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	ECalendarItem *calitem;
	gchar *week_char;
	gint char_size;

	g_return_val_if_fail (ea_calitem, FALSE);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_calitem);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	calitem = E_CALENDAR_ITEM (g_obj);
	week_char = g_utf8_offset_to_pointer (calitem->days, column);
	char_size = strlen (calitem->days) -
		strlen (g_utf8_find_next_char (calitem->days, NULL));

	if (week_char && char_size < buffer_size) {
		memcpy (buffer, week_char, char_size);
		buffer[char_size] = '\0';
		return TRUE;
	}
	return FALSE;
}

/* the coordinate the e-calendar canvas coord */
gboolean
e_calendar_item_get_day_extents (ECalendarItem *calitem,
				 gint year, gint month, gint date,
				 gint *x, gint *y,
				 gint *width, gint *height)
{
	GnomeCanvasItem *item;
	GtkWidget *widget;
	GtkStyle *style;
	PangoFontDescription *font_desc;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	gint char_height, xthickness, ythickness, text_y;
	gint new_year, new_month, num_months, months_offset;
	gint month_x, month_y, month_cell_x, month_cell_y;
	gint month_row, month_col;
	gint day_row, day_col;
	gint days_from_week_start;

	g_return_val_if_fail (E_IS_CALENDAR_ITEM (calitem), FALSE);

	item = GNOME_CANVAS_ITEM (calitem);
	widget = GTK_WIDGET (item->canvas);
	style = widget->style;

	/* Set up Pango prerequisites */
	font_desc = calitem->font_desc;
	if (!font_desc)
		font_desc = style->font_desc;
	pango_context = gtk_widget_get_pango_context (widget);
	font_metrics = pango_context_get_metrics (pango_context, font_desc,
						  pango_context_get_language (pango_context));

	char_height =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics));

	xthickness = style->xthickness;
	ythickness = style->ythickness;

	new_year = year;
	new_month = month;
	e_calendar_item_normalize_date	(calitem, &new_year, &new_month);
	num_months = calitem->rows * calitem->cols;
	months_offset = (new_year - calitem->year) * 12
		+ new_month - calitem->month;

	if (months_offset > num_months || months_offset < 0)
		return FALSE;

	month_row = months_offset / calitem->cols;
	month_col = months_offset % calitem->cols;

	month_x = item->x1 + xthickness + calitem->x_offset
		+ month_col * calitem->month_width;
	month_y = item->y1 + ythickness + month_row * calitem->month_height;

	month_cell_x = month_x + E_CALENDAR_ITEM_XPAD_BEFORE_WEEK_NUMBERS
 		+ calitem->month_lpad + E_CALENDAR_ITEM_XPAD_BEFORE_CELLS;
	text_y = month_y + ythickness * 2
		+ E_CALENDAR_ITEM_YPAD_ABOVE_MONTH_NAME
		+ char_height + E_CALENDAR_ITEM_YPAD_BELOW_MONTH_NAME
		+ E_CALENDAR_ITEM_YPAD_ABOVE_DAY_LETTERS + calitem->month_tpad;

	month_cell_y = text_y + char_height
		+ E_CALENDAR_ITEM_YPAD_BELOW_DAY_LETTERS + 1
		+ E_CALENDAR_ITEM_YPAD_ABOVE_CELLS;

	days_from_week_start =
		e_calendar_item_get_n_days_from_week_start (calitem, new_year,
							    new_month);
	day_row = (date + days_from_week_start - 1) / EA_CALENDAR_COLUMN_NUM;
	day_col = (date + days_from_week_start - 1) % EA_CALENDAR_COLUMN_NUM;

	*x = month_cell_x + day_col * calitem->cell_width;
	*y = month_cell_y + day_row * calitem->cell_height;
	*width = calitem->cell_width;
	*height = calitem->cell_height;

	return TRUE;
}

/* month is from 0 to 11 */
gboolean
e_calendar_item_get_date_for_offset (ECalendarItem *calitem, gint day_offset,
				     gint *year, gint *month, gint *day)
{
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	GDate *start_date;

	g_return_val_if_fail (E_IS_CALENDAR_ITEM (calitem), FALSE);

	if (!e_calendar_item_get_date_range (calitem, &start_year,
					     &start_month, &start_day,
					     &end_year, &end_month,
					     &end_day))
		return FALSE;

	start_date = g_date_new_dmy (start_day, start_month + 1, start_year);

	g_date_add_days (start_date, day_offset);

	*year = g_date_get_year (start_date);
	*month = g_date_get_month (start_date) - 1;
	*day = g_date_get_day (start_date);

	return TRUE;
}

/* the arg month is from 0 to 11 */
static gboolean
e_calendar_item_get_offset_for_date (ECalendarItem *calitem,
				     gint year, gint month, gint day,
				     gint *offset)
{
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	GDate *start_date, *end_date;
	gint n_days;

	*offset = 0;
	g_return_val_if_fail (E_IS_CALENDAR_ITEM (calitem), FALSE);

	if (!e_calendar_item_get_date_range (calitem, &start_year,
					     &start_month, &start_day,
					     &end_year, &end_month,
					     &end_day))
		return FALSE;

	start_date = g_date_new_dmy (start_day, start_month + 1, start_year);
	end_date = g_date_new_dmy (day, month + 1, year);

	*offset = g_date_days_between (start_date, end_date);
	g_free (start_date);
	g_free (end_date);

	return TRUE;
}

gint
e_calendar_item_get_n_days_from_week_start (ECalendarItem *calitem,
					    gint year, gint month)
{
	struct tm tmp_tm;
	gint start_weekday, days_from_week_start;

	memset (&tmp_tm, 0, sizeof (tmp_tm));
	tmp_tm.tm_year = year - 1900;
	tmp_tm.tm_mon = month;
	tmp_tm.tm_mday = 1;
	tmp_tm.tm_isdst = -1;
	mktime (&tmp_tm);
	start_weekday = (tmp_tm.tm_wday + 6) % 7;   /* 0 to 6 */
	days_from_week_start = (start_weekday + 7 - calitem->week_start_day)
		% 7;
	return days_from_week_start;
}
