/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-week-view-main-item.c
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
 * Author: Yang Wu <Yang.Wu@sun.com> Sun Microsystem Inc., 2003
 *
 */

#include "ea-week-view-main-item.h"
#include "ea-week-view.h"
#include "ea-week-view-cell.h"
#include "ea-cell-table.h"
#include <libgnome/gnome-i18n.h>

/* EaWeekViewMainItem */
static void ea_week_view_main_item_class_init (EaWeekViewMainItemClass *klass);

static void ea_week_view_main_item_finalize (GObject *object);
static G_CONST_RETURN gchar* ea_week_view_main_item_get_name (AtkObject *accessible);
static G_CONST_RETURN gchar* ea_week_view_main_item_get_description (AtkObject *accessible);

static gint         ea_week_view_main_item_get_n_children (AtkObject *obj);
static AtkObject*   ea_week_view_main_item_ref_child (AtkObject *obj,
						     gint i);
static AtkObject * ea_week_view_main_item_get_parent (AtkObject *accessible);
static gint ea_week_view_main_item_get_index_in_parent (AtkObject *accessible);

/* callbacks */
static void ea_week_view_main_item_dates_change_cb (GnomeCalendar *gcal, gpointer data);
static void ea_week_view_main_item_time_change_cb (EWeekView *week_view, gpointer data);

/* component interface */
static void atk_component_interface_init (AtkComponentIface *iface);
static void component_interface_get_extents (AtkComponent *component,
					     gint *x, gint *y,
					     gint *width, gint *height,
					     AtkCoordType coord_type);

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

/* helpers */
static EaCellTable *
ea_week_view_main_item_get_cell_data (EaWeekViewMainItem *ea_main_item);

static void
ea_week_view_main_item_destory_cell_data (EaWeekViewMainItem *ea_main_item);

static gint
ea_week_view_main_item_get_child_index_at (EaWeekViewMainItem *ea_main_item,
					  gint row, gint column);
static gint
ea_week_view_main_item_get_row_at_index (EaWeekViewMainItem *ea_main_item,
					gint index);
static gint
ea_week_view_main_item_get_column_at_index (EaWeekViewMainItem *ea_main_item,
					   gint index);
static gint
ea_week_view_main_item_get_row_label (EaWeekViewMainItem *ea_main_item,
				     gint row, gchar *buffer,
				     gint buffer_size);

#ifdef ACC_DEBUG
static gint n_ea_week_view_main_item_created = 0;
static gint n_ea_week_view_main_item_destroyed = 0;
#endif

static gpointer parent_class = NULL;

GType
ea_week_view_main_item_get_type (void)
{
	static GType type = 0;
	AtkObjectFactory *factory;
	GTypeQuery query;
	GType derived_atk_type;

	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (EaWeekViewMainItemClass),
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) ea_week_view_main_item_class_init,
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EaWeekViewMainItem), /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL /* value table */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) atk_component_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
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
		 *
		 */

		factory = atk_registry_get_factory (atk_get_default_registry (),
						    e_week_view_main_item_get_type());
		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);

		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		type = g_type_register_static (derived_atk_type,
					       "EaWeekViewMainItem", &tinfo, 0);
		g_type_add_interface_static (type, ATK_TYPE_COMPONENT,
					     &atk_component_info);
		g_type_add_interface_static (type, ATK_TYPE_TABLE,
					     &atk_table_info);
		g_type_add_interface_static (type, ATK_TYPE_SELECTION,
					     &atk_selection_info);
	}

	return type;
}

static void
ea_week_view_main_item_class_init (EaWeekViewMainItemClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	gobject_class->finalize = ea_week_view_main_item_finalize;
	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_week_view_main_item_get_name;
	class->get_description = ea_week_view_main_item_get_description;

	class->get_n_children = ea_week_view_main_item_get_n_children;
	class->ref_child = ea_week_view_main_item_ref_child;
	class->get_parent = ea_week_view_main_item_get_parent;
	class->get_index_in_parent = ea_week_view_main_item_get_index_in_parent;
}

AtkObject* 
ea_week_view_main_item_new (GObject *obj)
{
	AtkObject *accessible;
	GnomeCalendar *gcal;
	EWeekViewMainItem *main_item;

	g_return_val_if_fail (E_IS_WEEK_VIEW_MAIN_ITEM (obj), NULL);

	accessible = ATK_OBJECT (g_object_new (EA_TYPE_WEEK_VIEW_MAIN_ITEM,
					       NULL));

	atk_object_initialize (accessible, obj);
        accessible->role = ATK_ROLE_TABLE;

#ifdef ACC_DEBUG
	++n_ea_week_view_main_item_created;
	printf ("ACC_DEBUG: n_ea_week_view_main_item_created = %d\n",
		n_ea_week_view_main_item_created);
#endif
	main_item = E_WEEK_VIEW_MAIN_ITEM (obj);
	g_signal_connect (main_item->week_view, "selected_time_changed",
			  G_CALLBACK (ea_week_view_main_item_time_change_cb),
			  accessible);

	/* listen for date changes of calendar */
	gcal = e_calendar_view_get_calendar (E_CALENDAR_VIEW (main_item->week_view));
	if (gcal)
		g_signal_connect (gcal, "dates_shown_changed",
				  G_CALLBACK (ea_week_view_main_item_dates_change_cb),
				  accessible);

	return accessible;
}

static void
ea_week_view_main_item_finalize (GObject *object)
{
	EaWeekViewMainItem *ea_main_item;

	g_return_if_fail (EA_IS_WEEK_VIEW_MAIN_ITEM (object));

	ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (object);

	/* Free the allocated cell data */
	ea_week_view_main_item_destory_cell_data (ea_main_item);

	G_OBJECT_CLASS (parent_class)->finalize (object);
#ifdef ACC_DEBUG
	++n_ea_week_view_main_item_destroyed;
	printf ("ACC_DEBUG: n_ea_week_view_main_item_destroyed = %d\n",
		n_ea_week_view_main_item_destroyed);
#endif
}

static G_CONST_RETURN gchar*
ea_week_view_main_item_get_name (AtkObject *accessible)
{
	AtkObject *parent;
	g_return_val_if_fail (EA_IS_WEEK_VIEW_MAIN_ITEM (accessible), NULL);
	parent = atk_object_get_parent (accessible);
	return atk_object_get_name (parent);

}

static G_CONST_RETURN gchar*
ea_week_view_main_item_get_description (AtkObject *accessible)
{
	return _("a table to view and select the current time range");
}

static gint
ea_week_view_main_item_get_n_children (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;

	g_return_val_if_fail (EA_IS_WEEK_VIEW_MAIN_ITEM (accessible), -1);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;
	
	if (week_view->multi_week_view)
		return 7 * week_view->weeks_shown;
	else 
		return 7;
}

static AtkObject *
ea_week_view_main_item_ref_child (AtkObject *accessible, gint index)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	gint n_children;
	EWeekViewCell *cell;
	EaCellTable *cell_data;
	EaWeekViewMainItem *ea_main_item;

	g_return_val_if_fail (EA_IS_WEEK_VIEW_MAIN_ITEM (accessible), NULL);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	n_children = ea_week_view_main_item_get_n_children (accessible);
	if (index < 0 || index >= n_children)
		return NULL;

	ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (accessible);
	cell_data = ea_week_view_main_item_get_cell_data (ea_main_item);
	if (!cell_data)
		return NULL;
	cell = ea_cell_table_get_cell_at_index (cell_data, index);
	if (!cell) {
		gint row, column;

		row = ea_week_view_main_item_get_row_at_index (ea_main_item, index);
		column = ea_week_view_main_item_get_column_at_index (ea_main_item, index);
		cell = e_week_view_cell_new (week_view, row, column);
		ea_cell_table_set_cell_at_index (cell_data, index, cell);
		g_object_unref (cell);
	}

	return g_object_ref (atk_gobject_accessible_for_object (G_OBJECT(cell)));
}

static AtkObject *
ea_week_view_main_item_get_parent (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;

	g_return_val_if_fail (EA_IS_WEEK_VIEW_MAIN_ITEM (accessible), NULL);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	return gtk_widget_get_accessible (GTK_WIDGET (main_item->week_view));
}

static gint
ea_week_view_main_item_get_index_in_parent (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;

	g_return_val_if_fail (EA_IS_WEEK_VIEW_MAIN_ITEM (accessible), -1);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	/* always the first child of ea-week-view */
	return 0;
}

/* callbacks */

static void
ea_week_view_main_item_dates_change_cb (GnomeCalendar *gcal, gpointer data)
{
	EaWeekViewMainItem *ea_main_item;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (data);
	g_return_if_fail (EA_IS_WEEK_VIEW_MAIN_ITEM (data));

	ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (data);

#ifdef ACC_DEBUG
	printf ("EvoAcc: ea_week_view_main_item update cb\n");
#endif

	ea_week_view_main_item_destory_cell_data (ea_main_item);
}

static void
ea_week_view_main_item_time_change_cb (EWeekView *week_view, gpointer data)
{
	EaWeekViewMainItem *ea_main_item;
	AtkObject *item_cell = NULL;

	g_return_if_fail (E_IS_WEEK_VIEW (week_view));
	g_return_if_fail (data);
	g_return_if_fail (EA_IS_WEEK_VIEW_MAIN_ITEM (data));

	ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (data);

#ifdef ACC_DEBUG
	printf ("EvoAcc: ea_week_view_main_item time changed cb\n");
#endif
	/* only deal with the first selected child, for now */
	item_cell = atk_selection_ref_selection (ATK_SELECTION (ea_main_item),
						 0);
	if (item_cell) {
		AtkStateSet *state_set;
		state_set = atk_object_ref_state_set (item_cell);
		atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
		g_object_unref (state_set);

	        g_signal_emit_by_name (ea_main_item,
			       "active-descendant-changed",
			       item_cell);
        	g_signal_emit_by_name (data, "selection_changed");
                atk_focus_tracker_notify (item_cell);
                g_object_unref (item_cell);
	}
}

/* helpers */

static gint
ea_week_view_main_item_get_child_index_at (EaWeekViewMainItem *ea_main_item,
					  gint row, gint column)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;

	g_return_val_if_fail (ea_main_item, -1);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	if (row >= 0 && row < week_view->weeks_shown &&
	    column >= 0 && column < 7)
		return row * 7 + column;

	return -1;
}

static gint
ea_week_view_main_item_get_row_at_index (EaWeekViewMainItem *ea_main_item,
					gint index)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	gint n_children;

	g_return_val_if_fail (ea_main_item, -1);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	n_children = ea_week_view_main_item_get_n_children (ATK_OBJECT (ea_main_item));
	if (index >= 0 && index < n_children)
		return index / 7;
	return -1;
}

static gint
ea_week_view_main_item_get_column_at_index (EaWeekViewMainItem *ea_main_item,
					   gint index)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	gint n_children;

	g_return_val_if_fail (ea_main_item, -1);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	n_children = ea_week_view_main_item_get_n_children (ATK_OBJECT (ea_main_item));
	if (index >= 0 && index < n_children)
		return index % 7;
	return -1;
}

static gint
ea_week_view_main_item_get_row_label (EaWeekViewMainItem *ea_main_item,
				     gint row, gchar *buffer, gint buffer_size)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;

	g_return_val_if_fail (ea_main_item, 0);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return 0 ;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	return g_snprintf (buffer, buffer_size, "the %i week",
			   row + 1);

}

static EaCellTable *
ea_week_view_main_item_get_cell_data (EaWeekViewMainItem *ea_main_item)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaCellTable *cell_data;

	g_return_val_if_fail (ea_main_item, NULL);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	cell_data = g_object_get_data (G_OBJECT(ea_main_item),
				       "ea-week-view-cell-table");
	if (!cell_data) {
		cell_data = ea_cell_table_create (week_view->weeks_shown, 7, TRUE);
		g_object_set_data (G_OBJECT(ea_main_item),
				   "ea-week-view-cell-table", cell_data);
	}
	return cell_data;
}

static void
ea_week_view_main_item_destory_cell_data (EaWeekViewMainItem *ea_main_item)
{
	EaCellTable *cell_data;

	g_return_if_fail (ea_main_item);

	cell_data = g_object_get_data (G_OBJECT(ea_main_item),
				       "ea-week-view-cell-table");
	if (cell_data) {
		ea_cell_table_destroy (cell_data);
		g_object_set_data (G_OBJECT(ea_main_item),
				   "ea-week-view-cell-table", NULL);
	}
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
	AtkObject *ea_canvas;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;

	*x = *y = *width = *height = 0;

	g_return_if_fail (EA_IS_WEEK_VIEW_MAIN_ITEM (component));

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE(component));
	if (!g_obj)
		/* defunct object*/
		return;
	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	ea_canvas = gtk_widget_get_accessible (week_view->main_canvas);
	atk_component_get_extents (ATK_COMPONENT (ea_canvas), x, y,
				   width, height, coord_type);
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

	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);
	index = ea_week_view_main_item_get_child_index_at (ea_main_item,
							  row, column);
	return ea_week_view_main_item_ref_child (ATK_OBJECT (ea_main_item), index);
}

static gint 
table_interface_get_n_rows (AtkTable *table)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	return week_view->weeks_shown;
}

static gint 
table_interface_get_n_columns (AtkTable *table)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	return 7;
}

static gint
table_interface_get_index_at (AtkTable *table,
			      gint     row,
			      gint     column)
{
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);
	return ea_week_view_main_item_get_child_index_at (ea_main_item,
							 row, column);
}

static gint
table_interface_get_column_at_index (AtkTable *table,
				     gint     index)
{
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);
	return ea_week_view_main_item_get_column_at_index (ea_main_item, index);
}

static gint
table_interface_get_row_at_index (AtkTable *table,
				  gint     index)
{
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);
	return ea_week_view_main_item_get_row_at_index (ea_main_item, index);
}

static gint
table_interface_get_column_extent_at (AtkTable      *table,
				      gint          row,
				      gint          column)
{
	gint index;
	gint width = 0, height = 0;
	AtkObject *child;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);

	index = ea_week_view_main_item_get_child_index_at (ea_main_item,
							  row, column);
	child = atk_object_ref_accessible_child (ATK_OBJECT (ea_main_item),
						 index);
	if (child)
		atk_component_get_size (ATK_COMPONENT (child),
					&width, &height);

	return width;
}

static gint 
table_interface_get_row_extent_at (AtkTable      *table,
				   gint          row,
				   gint          column)
{
	gint index;
	gint width = 0, height = 0;
	AtkObject *child;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);

	index = ea_week_view_main_item_get_child_index_at (ea_main_item,
							  row, column);
	child = atk_object_ref_accessible_child (ATK_OBJECT (ea_main_item),
						 index);
	if (child)
		atk_component_get_size (ATK_COMPONENT (child),
					&width, &height);

	return height;
}

static gboolean 
table_interface_is_row_selected (AtkTable *table,
				 gint     row)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	if (week_view->selection_start_day == -1)
		/* no selection */
		return FALSE;
	if ((row < 0)&&(row + 1 > week_view->weeks_shown ))
		return FALSE;
	if (((week_view->selection_start_day < row*7)&&(week_view->selection_end_day<row*7))
		||((week_view->selection_start_day > row*7+6)&&(week_view->selection_end_day > row*7+6)))
		return FALSE;
	else
		return TRUE;
}

static gboolean 
table_interface_is_selected (AtkTable *table, 
			     gint     row, 
			     gint     column)
{
	return table_interface_is_row_selected (table, row) && table_interface_is_column_selected(table, column);
}

static gboolean 
table_interface_is_column_selected (AtkTable *table,
				    gint     column)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	if ((column <0)||(column >6))
		return FALSE;
	else {
		gint i;
		for (i=0;i<week_view->weeks_shown;i++)
			if ((column + i*7>= week_view->selection_start_day ) &&
			    (column + i*7<= week_view->selection_end_day))
				return TRUE;
		return FALSE;
	}
}

static gint 
table_interface_get_selected_rows (AtkTable *table,
				   gint     **rows_selected)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);
	gint start_row = -1, n_rows = 0;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	if (week_view->selection_start_day == -1)
		return 0;

	start_row = week_view->selection_start_day;
	n_rows = week_view->selection_end_day - start_row + 1;

	if (n_rows > 0 && start_row != -1 && rows_selected) {
		gint index;

		*rows_selected = (gint *) g_malloc (n_rows * sizeof (gint));
		for (index = 0; index < n_rows; ++index)
			(*rows_selected)[index] = start_row + index;
	}
	return n_rows;
}

static gint 
table_interface_get_selected_columns (AtkTable *table,
				      gint **columns_selected)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);
	gint start_column = -1, n_columns = 0;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	if (week_view->selection_start_day == -1)
		return 0;
	if (week_view->selection_end_day - week_view->selection_start_day >= 6 ) {
		start_column = 0;
		n_columns =7;
	} else {
		start_column = week_view->selection_start_day % 7;
		n_columns = (week_view->selection_end_day % 7) - start_column + 1;
	}
	if (n_columns > 0 && start_column != -1 && columns_selected) {
		gint index;
		
		*columns_selected = (gint *) g_malloc (n_columns * sizeof (gint));
		for (index = 0; index < n_columns; ++index)
			(*columns_selected)[index] = start_column + index;
	}
	return n_columns;
}

static gboolean 
table_interface_add_row_selection (AtkTable *table, 
				   gint row)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	/* FIXME: we need multi-selection */

	week_view->selection_start_day = row * 7;
	week_view->selection_end_day = row *7 + 6;

	gtk_widget_queue_draw (week_view->main_canvas);
	return TRUE;
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
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	/* FIXME: we need multi-selection */

	week_view->selection_start_day = column;
	week_view->selection_end_day = (week_view->weeks_shown - 1)*7+column;

	gtk_widget_queue_draw (week_view->main_canvas);
	return TRUE;
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
table_interface_get_column_description (AtkTable	  *table,
					gint       in_col)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);
	const gchar *description;
	EaCellTable *cell_data;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	if (in_col < 0 || in_col > 6)
		return NULL;
	cell_data = ea_week_view_main_item_get_cell_data (ea_main_item);
	if (!cell_data)
		return NULL;

	description = ea_cell_table_get_column_label (cell_data, in_col);
	if (!description) {
		gchar buffer[128];

		switch (in_col) {
		case 0:
			g_snprintf(buffer,128,"Monday");
			break;
		case 1:
			g_snprintf(buffer,128,"Tuesday");
			break;
		case 2:
			g_snprintf(buffer,128,"Wednesday");
			break;
		case 3:
			g_snprintf(buffer,128,"Thursday");
			break;
		case 4:
			g_snprintf(buffer,128,"Friday");
			break;
		case 5:
			g_snprintf(buffer,128,"Saturday");
			break;
		case 6:
			g_snprintf(buffer,128,"Sunday");
			break;
		default:
			break;
		}
			
		ea_cell_table_set_column_label (cell_data, in_col, buffer);
		description = ea_cell_table_get_column_label (cell_data, in_col);
	}
	return description;
}

static G_CONST_RETURN gchar*
table_interface_get_row_description (AtkTable    *table,
				     gint        row)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (table);
	const gchar *description;
	EaCellTable *cell_data;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	if (row < 0 || row >= week_view->weeks_shown)
		return NULL;
	cell_data = ea_week_view_main_item_get_cell_data (ea_main_item);
	if (!cell_data)
		return NULL;

	description = ea_cell_table_get_row_label (cell_data, row);
	if (!description) {
		gchar buffer[128];
		ea_week_view_main_item_get_row_label (ea_main_item, row, buffer, sizeof (buffer));
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
selection_interface_add_selection (AtkSelection *selection, gint i)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (selection);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	if (i < 0 || i > week_view->weeks_shown * 7 -1)
		return FALSE;

	/*FIXME: multi-selection is needed */
	week_view->selection_start_day = i;
	week_view->selection_end_day = i;

	gtk_widget_queue_draw (week_view->main_canvas);
	return TRUE;
}

static gboolean
selection_interface_clear_selection (AtkSelection *selection)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (selection);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	week_view->selection_start_day = -1;
	week_view->selection_end_day = -1;

	gtk_widget_queue_draw (week_view->main_canvas);

	return TRUE;
}

static AtkObject*  
selection_interface_ref_selection (AtkSelection *selection, gint i)
{
	gint count;
	GObject *g_obj;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (selection);
	gint start_index;

	count = selection_interface_get_selection_count (selection);
	if (i < 0 || i >=count)
		return NULL;

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (ea_main_item));
	week_view = E_WEEK_VIEW_MAIN_ITEM (g_obj)->week_view;
	start_index = ea_week_view_main_item_get_child_index_at (ea_main_item,
								week_view->selection_start_day / 7,
								week_view->selection_start_day % 7);

	return ea_week_view_main_item_ref_child (ATK_OBJECT (selection), start_index + i);
}

static gint
selection_interface_get_selection_count (AtkSelection *selection)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (selection);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return 0;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	if (week_view->selection_start_day == -1 ||
	    week_view->selection_end_day == -1)
		return 0;

	return week_view->selection_end_day - week_view->selection_start_day + 1;
}

static gboolean
selection_interface_is_child_selected (AtkSelection *selection, gint i)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EWeekViewMainItem *main_item;
	EWeekView *week_view;
	EaWeekViewMainItem* ea_main_item = EA_WEEK_VIEW_MAIN_ITEM (selection);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	main_item = E_WEEK_VIEW_MAIN_ITEM (g_obj);
	week_view = main_item->week_view;

	if ((week_view->selection_start_day <= i)&&(week_view->selection_end_day >= i))
		return TRUE;
	else 
		return FALSE;
}

