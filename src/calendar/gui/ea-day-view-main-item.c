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

#include <glib/gi18n.h>

#include "ea-day-view-main-item.h"
#include "e-day-view-top-item.h"
#include "ea-day-view.h"
#include "ea-day-view-cell.h"

/* EaDayViewMainItem */
static void	ea_day_view_main_item_finalize	(GObject *object);
static const gchar *
		ea_day_view_main_item_get_name	(AtkObject *accessible);
static const gchar *
		ea_day_view_main_item_get_description
						(AtkObject *accessible);

static gint         ea_day_view_main_item_get_n_children (AtkObject *obj);
static AtkObject *   ea_day_view_main_item_ref_child (AtkObject *obj,
						     gint i);
static AtkObject * ea_day_view_main_item_get_parent (AtkObject *accessible);
static gint ea_day_view_main_item_get_index_in_parent (AtkObject *accessible);

/* callbacks */
static void ea_day_view_main_item_time_range_changed_cb (ECalModel *model, gint64 i64_start, gint64 i64_end, gpointer data);
static void ea_day_view_main_item_time_change_cb (EDayView *day_view, gpointer data);

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
static AtkObject * table_interface_ref_at (AtkTable *table,
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
static AtkObject * table_interface_get_row_header (AtkTable *table, gint row);
static AtkObject * table_interface_get_column_header (AtkTable *table,
						     gint in_col);
static AtkObject * table_interface_get_caption (AtkTable *table);

static const gchar *
		table_interface_get_column_description
						(AtkTable *table,
						 gint in_col);

static const gchar *
		table_interface_get_row_description
						(AtkTable *table,
						 gint row);

static AtkObject * table_interface_get_summary (AtkTable *table);

/* atk selection interface */
static void atk_selection_interface_init (AtkSelectionIface *iface);
static gboolean selection_interface_add_selection (AtkSelection *selection,
						   gint i);
static gboolean selection_interface_clear_selection (AtkSelection *selection);
static AtkObject * selection_interface_ref_selection (AtkSelection *selection,
						     gint i);
static gint selection_interface_get_selection_count (AtkSelection *selection);
static gboolean selection_interface_is_child_selected (AtkSelection *selection,
						       gint i);

/* helpers */
static EaCellTable *
ea_day_view_main_item_get_cell_data (EaDayViewMainItem *ea_main_item);

static void
ea_day_view_main_item_destory_cell_data (EaDayViewMainItem *ea_main_item);

static gint
ea_day_view_main_item_get_child_index_at (EaDayViewMainItem *ea_main_item,
                                          gint row,
                                          gint column);
static gint
ea_day_view_main_item_get_row_at_index (EaDayViewMainItem *ea_main_item,
                                        gint index);
static gint
ea_day_view_main_item_get_column_at_index (EaDayViewMainItem *ea_main_item,
                                           gint index);
static gint
ea_day_view_main_item_get_row_label (EaDayViewMainItem *ea_main_item,
                                     gint row,
                                     gchar *buffer,
                                     gint buffer_size);

#ifdef ACC_DEBUG
static gint n_ea_day_view_main_item_created = 0;
static gint n_ea_day_view_main_item_destroyed = 0;
#endif

static gpointer parent_class = NULL;

G_DEFINE_TYPE_WITH_CODE (EaDayViewMainItem, ea_day_view_main_item, GAIL_TYPE_CANVAS_ITEM,
	G_IMPLEMENT_INTERFACE (
		ATK_TYPE_COMPONENT, atk_component_interface_init)
	G_IMPLEMENT_INTERFACE (
		ATK_TYPE_SELECTION, atk_selection_interface_init)
	G_IMPLEMENT_INTERFACE (
		ATK_TYPE_TABLE, atk_table_interface_init))

static void
ea_day_view_main_item_init (EaDayViewMainItem *item)
{
}

static void
ea_day_view_main_item_class_init (EaDayViewMainItemClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	gobject_class->finalize = ea_day_view_main_item_finalize;
	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_day_view_main_item_get_name;
	class->get_description = ea_day_view_main_item_get_description;

	class->get_n_children = ea_day_view_main_item_get_n_children;
	class->ref_child = ea_day_view_main_item_ref_child;
	class->get_parent = ea_day_view_main_item_get_parent;
	class->get_index_in_parent = ea_day_view_main_item_get_index_in_parent;
}

AtkObject *
ea_day_view_main_item_new (GObject *obj)
{
	AtkObject *accessible;
	ECalModel *model;
	EDayViewMainItem *main_item;
	EDayView *day_view;

	g_return_val_if_fail (E_IS_DAY_VIEW_MAIN_ITEM (obj), NULL);

	accessible = ATK_OBJECT (
		g_object_new (EA_TYPE_DAY_VIEW_MAIN_ITEM,
		NULL));

	atk_object_initialize (accessible, obj);
	accessible->role = ATK_ROLE_TABLE;

#ifdef ACC_DEBUG
	++n_ea_day_view_main_item_created;
	printf (
		"ACC_DEBUG: n_ea_day_view_main_item_created = %d\n",
		n_ea_day_view_main_item_created);
#endif

	main_item = E_DAY_VIEW_MAIN_ITEM (obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	g_signal_connect (
		day_view, "selected_time_changed",
		G_CALLBACK (ea_day_view_main_item_time_change_cb),
		accessible);

	/* listen for date changes of calendar */
	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));
	if (model)
		g_signal_connect_after (
			model, "time-range-changed",
			G_CALLBACK (ea_day_view_main_item_time_range_changed_cb),
			accessible);

	return accessible;
}

static void
ea_day_view_main_item_finalize (GObject *object)
{
	EaDayViewMainItem *ea_main_item;

	g_return_if_fail (EA_IS_DAY_VIEW_MAIN_ITEM (object));

	ea_main_item = EA_DAY_VIEW_MAIN_ITEM (object);

	/* Free the allocated cell data */
	ea_day_view_main_item_destory_cell_data (ea_main_item);

	G_OBJECT_CLASS (parent_class)->finalize (object);
#ifdef ACC_DEBUG
	++n_ea_day_view_main_item_destroyed;
	printf (
		"ACC_DEBUG: n_ea_day_view_main_item_destroyed = %d\n",
		n_ea_day_view_main_item_destroyed);
#endif
}

static const gchar *
ea_day_view_main_item_get_name (AtkObject *accessible)
{
	AtkObject *parent;
	g_return_val_if_fail (EA_IS_DAY_VIEW_MAIN_ITEM (accessible), NULL);
	parent = atk_object_get_parent (accessible);

	if (!parent)
		return NULL;

	return atk_object_get_name (parent);
}

static const gchar *
ea_day_view_main_item_get_description (AtkObject *accessible)
{
	return _("a table to view and select the current time range");
}

static gint
ea_day_view_main_item_get_n_children (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;

	g_return_val_if_fail (EA_IS_DAY_VIEW_MAIN_ITEM (accessible), -1);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	return day_view->rows * e_day_view_get_days_shown (day_view);
}

static AtkObject *
ea_day_view_main_item_ref_child (AtkObject *accessible,
                                 gint index)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	gint n_children;
	EDayViewCell *cell;
	EaCellTable *cell_data;
	EaDayViewMainItem *ea_main_item;

	g_return_val_if_fail (EA_IS_DAY_VIEW_MAIN_ITEM (accessible), NULL);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	n_children = ea_day_view_main_item_get_n_children (accessible);
	if (index < 0 || index >= n_children)
		return NULL;

	ea_main_item = EA_DAY_VIEW_MAIN_ITEM (accessible);
	cell_data = ea_day_view_main_item_get_cell_data (ea_main_item);
	if (!cell_data)
		return NULL;

	cell = ea_cell_table_get_cell_at_index (cell_data, index);
	if (!cell) {
		gint row, column;

		row = ea_day_view_main_item_get_row_at_index (ea_main_item, index);
		column = ea_day_view_main_item_get_column_at_index (ea_main_item, index);
		cell = e_day_view_cell_new (day_view, row, column);
		ea_cell_table_set_cell_at_index (cell_data, index, cell);
		g_object_unref (cell);
	}
	return g_object_ref (atk_gobject_accessible_for_object (G_OBJECT (cell)));
}

static AtkObject *
ea_day_view_main_item_get_parent (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;

	g_return_val_if_fail (EA_IS_DAY_VIEW_MAIN_ITEM (accessible), NULL);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	return gtk_widget_get_accessible (GTK_WIDGET (day_view));
}

static gint
ea_day_view_main_item_get_index_in_parent (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;

	g_return_val_if_fail (EA_IS_DAY_VIEW_MAIN_ITEM (accessible), -1);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (accessible);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	/* always the first child of ea-day-view */
	return 0;
}

/* callbacks */

static void
ea_day_view_main_item_time_range_changed_cb (ECalModel *model,
					     gint64 i64_start,
					     gint64 i64_end,
					     gpointer data)
{
	EaDayViewMainItem *ea_main_item;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (data);
	g_return_if_fail (EA_IS_DAY_VIEW_MAIN_ITEM (data));

	ea_main_item = EA_DAY_VIEW_MAIN_ITEM (data);

#ifdef ACC_DEBUG
	printf ("EvoAcc: ea_day_view_main_item update cb\n");
#endif

	ea_day_view_main_item_destory_cell_data (ea_main_item);
}

static void
ea_day_view_main_item_time_change_cb (EDayView *day_view,
                                      gpointer data)
{
	EaDayViewMainItem *ea_main_item;
	AtkObject *item_cell = NULL;

	g_return_if_fail (E_IS_DAY_VIEW (day_view));
	g_return_if_fail (data);
	g_return_if_fail (EA_IS_DAY_VIEW_MAIN_ITEM (data));

	ea_main_item = EA_DAY_VIEW_MAIN_ITEM (data);

#ifdef ACC_DEBUG
	printf ("EvoAcc: ea_day_view_main_item time changed cb\n");
#endif
	/* only deal with the first selected child, for now */
	item_cell = atk_selection_ref_selection (
		ATK_SELECTION (ea_main_item), 0);
	if (item_cell) {
		AtkStateSet *state_set;
		state_set = atk_object_ref_state_set (item_cell);
		atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
		g_object_unref (state_set);

		g_signal_emit_by_name (
			ea_main_item,
			"active-descendant-changed",
			item_cell);
		g_signal_emit_by_name (data, "selection_changed");

		g_object_unref (item_cell);
	}

}

/* helpers */

static gint
ea_day_view_main_item_get_child_index_at (EaDayViewMainItem *ea_main_item,
                                          gint row,
                                          gint column)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;

	g_return_val_if_fail (ea_main_item, -1);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	if (row >= 0 && row < day_view->rows &&
	    column >= 0 && column < e_day_view_get_days_shown (day_view))
		return column * day_view->rows + row;
	return -1;
}

static gint
ea_day_view_main_item_get_row_at_index (EaDayViewMainItem *ea_main_item,
                                        gint index)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	gint n_children;

	g_return_val_if_fail (ea_main_item, -1);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	n_children = ea_day_view_main_item_get_n_children (ATK_OBJECT (ea_main_item));
	if (index >= 0 && index < n_children)
		return index % day_view->rows;
	return -1;
}

static gint
ea_day_view_main_item_get_column_at_index (EaDayViewMainItem *ea_main_item,
                                           gint index)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	gint n_children;

	g_return_val_if_fail (ea_main_item, -1);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	n_children = ea_day_view_main_item_get_n_children (ATK_OBJECT (ea_main_item));
	if (index >= 0 && index < n_children)
		return index / day_view->rows;
	return -1;
}

static gint
ea_day_view_main_item_get_row_label (EaDayViewMainItem *ea_main_item,
                                     gint row,
                                     gchar *buffer,
                                     gint buffer_size)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	ECalendarView *cal_view;
	const gchar *suffix;
	gint time_divisions;
	gint hour, minute, suffix_width;

	g_return_val_if_fail (ea_main_item, 0);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return 0;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	hour = day_view->first_hour_shown;
	minute = day_view->first_minute_shown;
	minute += row * time_divisions;
	hour = (hour + minute / 60) % 24;
	minute %= 60;

	e_day_view_convert_time_to_display (
		day_view, hour, &hour,
		&suffix, &suffix_width);
	return g_snprintf (
		buffer, buffer_size, "%i:%02i %s",
		hour, minute, suffix);
}

static EaCellTable *
ea_day_view_main_item_get_cell_data (EaDayViewMainItem *ea_main_item)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaCellTable *cell_data;

	g_return_val_if_fail (ea_main_item, NULL);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	cell_data = g_object_get_data (
		G_OBJECT (ea_main_item),
		"ea-day-view-cell-table");
	if (!cell_data) {
		cell_data = ea_cell_table_create (
			day_view->rows,
			e_day_view_get_days_shown (day_view), TRUE);
		g_object_set_data_full (
			G_OBJECT (ea_main_item),
			"ea-day-view-cell-table", cell_data,
			(GDestroyNotify) ea_cell_table_destroy);
	}
	return cell_data;
}

static void
ea_day_view_main_item_destory_cell_data (EaDayViewMainItem *ea_main_item)
{
	g_return_if_fail (ea_main_item);

	g_object_set_data (
		G_OBJECT (ea_main_item),
		"ea-day-view-cell-table", NULL);
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
                                 gint *x,
                                 gint *y,
                                 gint *width,
                                 gint *height,
                                 AtkCoordType coord_type)
{
	GObject *g_obj;
	AtkObject *ea_canvas;
	EDayViewMainItem *main_item;
	EDayView *day_view;

	*x = *y = *width = *height = 0;

	g_return_if_fail (EA_IS_DAY_VIEW_MAIN_ITEM (component));

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (component));
	if (!g_obj)
		/* defunct object*/
		return;
	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	ea_canvas = gtk_widget_get_accessible (day_view->main_canvas);
	atk_component_get_extents (
		ATK_COMPONENT (ea_canvas), x, y,
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

static AtkObject *
table_interface_ref_at (AtkTable *table,
                        gint row,
                        gint column)
{
	gint index;

	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);
	index = ea_day_view_main_item_get_child_index_at (
		ea_main_item,
		row, column);
	return ea_day_view_main_item_ref_child (ATK_OBJECT (ea_main_item), index);
}

static gint
table_interface_get_n_rows (AtkTable *table)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	return day_view->rows;
}

static gint
table_interface_get_n_columns (AtkTable *table)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	return e_day_view_get_days_shown (day_view);
}

static gint
table_interface_get_index_at (AtkTable *table,
                              gint row,
                              gint column)
{
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);
	return ea_day_view_main_item_get_child_index_at (
		ea_main_item,
		row, column);
}

static gint
table_interface_get_column_at_index (AtkTable *table,
                                     gint index)
{
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);
	return ea_day_view_main_item_get_column_at_index (ea_main_item, index);
}

static gint
table_interface_get_row_at_index (AtkTable *table,
                                  gint index)
{
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);
	return ea_day_view_main_item_get_row_at_index (ea_main_item, index);
}

static gint
table_interface_get_column_extent_at (AtkTable *table,
                                      gint row,
                                      gint column)
{
	gint index;
	gint width = 0, height = 0;
	AtkObject *child;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);

	index = ea_day_view_main_item_get_child_index_at (
		ea_main_item,
		row, column);
	child = atk_object_ref_accessible_child (
		ATK_OBJECT (ea_main_item),
		index);
	if (child)
		atk_component_get_extents (
			ATK_COMPONENT (child), NULL, NULL, &width, &height,
			ATK_XY_SCREEN);

	return width;
}

static gint
table_interface_get_row_extent_at (AtkTable *table,
                                   gint row,
                                   gint column)
{
	gint index;
	gint width = 0, height = 0;
	AtkObject *child;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);

	index = ea_day_view_main_item_get_child_index_at (
		ea_main_item,
		row, column);
	child = atk_object_ref_accessible_child (
		ATK_OBJECT (ea_main_item),
		index);
	if (child)
		atk_component_get_extents (
			ATK_COMPONENT (child), NULL, NULL, &width, &height,
			ATK_XY_SCREEN);

	return height;
}

static gboolean
table_interface_is_row_selected (AtkTable *table,
                                 gint row)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	if (day_view->selection_start_day == -1)
		/* no selection */
		return FALSE;
	if (day_view->selection_start_day != day_view->selection_end_day)
		/* all row is selected */
		return TRUE;
	if (row >= day_view->selection_start_row &&
	    row <= day_view->selection_end_row)
		return TRUE;
	return FALSE;
}

static gboolean
table_interface_is_selected (AtkTable *table,
                             gint row,
                             gint column)
{
	return table_interface_is_row_selected (table, row) &&
		table_interface_is_column_selected (table, column);
}

static gboolean
table_interface_is_column_selected (AtkTable *table,
                                    gint column)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	if (column >= day_view->selection_start_day &&
	    column <= day_view->selection_end_day)
		return TRUE;
	return FALSE;
}

static gint
table_interface_get_selected_rows (AtkTable *table,
                                   gint **rows_selected)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);
	gint start_row = -1, n_rows = 0;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	if (day_view->selection_start_day == -1)
		return 0;

	if (day_view->selection_start_day != day_view->selection_end_day) {
		/* all the rows should be selected */
		n_rows = day_view->rows;
		start_row = 0;
	}
	else if (day_view->selection_start_row != -1) {
		start_row = day_view->selection_start_row;
		n_rows = day_view->selection_end_row - start_row + 1;
	}
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
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);
	gint start_column = -1, n_columns = 0;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return -1;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	if (day_view->selection_start_day == -1)
		return 0;

	start_column = day_view->selection_start_day;
	n_columns = day_view->selection_end_day - start_column + 1;
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
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	/* FIXME: we need multi-selection */

	day_view->selection_start_day = 0;
	day_view->selection_end_day = 0;
	day_view->selection_start_row = row;
	day_view->selection_end_row = row;

	e_day_view_ensure_rows_visible (
		day_view,
		day_view->selection_start_row,
		day_view->selection_end_row);
	e_day_view_update_calendar_selection_time (day_view);
	gtk_widget_queue_draw (day_view->main_canvas);
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
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	/* FIXME: we need multi-selection */

	day_view->selection_start_day = column;
	day_view->selection_end_day = column;
	day_view->selection_start_row = 0;
	day_view->selection_end_row = day_view->rows;

	e_day_view_update_calendar_selection_time (day_view);
	gtk_widget_queue_draw (day_view->main_canvas);
	return TRUE;
}

static gboolean
table_interface_remove_column_selection (AtkTable *table,
                                         gint column)
{
	/* FIXME: NOT IMPLEMENTED */
	return FALSE;
}

static AtkObject *
table_interface_get_row_header (AtkTable *table,
                                gint row)
{
	/* FIXME: NOT IMPLEMENTED */
	return NULL;
}

static AtkObject *
table_interface_get_column_header (AtkTable *table,
                                   gint in_col)
{
	/* FIXME: NOT IMPLEMENTED */
	return NULL;
}

static AtkObject *
table_interface_get_caption (AtkTable *table)
{
	/* FIXME: NOT IMPLEMENTED */
	return NULL;
}

static const gchar *
table_interface_get_column_description (AtkTable *table,
                                        gint in_col)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);
	const gchar *description;
	EaCellTable *cell_data;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	if (in_col < 0 || in_col >= e_day_view_get_days_shown (day_view))
		return NULL;
	cell_data = ea_day_view_main_item_get_cell_data (ea_main_item);
	if (!cell_data)
		return NULL;

	description = ea_cell_table_get_column_label (cell_data, in_col);
	if (!description) {
		gchar buffer[128];
		e_day_view_top_item_get_day_label (day_view, in_col, buffer, 128);
		ea_cell_table_set_column_label (cell_data, in_col, buffer);
		description = ea_cell_table_get_column_label (cell_data, in_col);
	}
	return description;
}

static const gchar *
table_interface_get_row_description (AtkTable *table,
                                     gint row)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (table);
	const gchar *description;
	EaCellTable *cell_data;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return NULL;

	if (row < 0 || row >= 12 * 24)
		return NULL;
	cell_data = ea_day_view_main_item_get_cell_data (ea_main_item);
	if (!cell_data)
		return NULL;

	description = ea_cell_table_get_row_label (cell_data, row);
	if (!description) {
		gchar buffer[128];
		ea_day_view_main_item_get_row_label (
			ea_main_item, row, buffer, sizeof (buffer));
		ea_cell_table_set_row_label (cell_data, row, buffer);
		description = ea_cell_table_get_row_label (cell_data, row);
	}
	return description;
}

static AtkObject *
table_interface_get_summary (AtkTable *table)
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
selection_interface_add_selection (AtkSelection *selection,
                                   gint i)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (selection);
	gint column, row;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	row = ea_day_view_main_item_get_row_at_index (ea_main_item, i);
	column = ea_day_view_main_item_get_column_at_index (ea_main_item, i);

	if (row == -1 || column == -1)
		return FALSE;

	/*FIXME: multi-selection is needed */
	day_view->selection_start_day = column;
	day_view->selection_end_day = column;
	day_view->selection_start_row = row;
	day_view->selection_end_row = row;

	e_day_view_ensure_rows_visible (
		day_view,
		day_view->selection_start_row,
		day_view->selection_end_row);
	e_day_view_update_calendar_selection_time (day_view);
	gtk_widget_queue_draw (day_view->main_canvas);
	return TRUE;
}

static gboolean
selection_interface_clear_selection (AtkSelection *selection)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (selection);

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	day_view->selection_start_row = -1;
	day_view->selection_start_day = -1;
	day_view->selection_end_row = -1;
	day_view->selection_end_day = -1;

	e_day_view_update_calendar_selection_time (day_view);
	gtk_widget_queue_draw (day_view->main_canvas);

	return TRUE;
}

static AtkObject *
selection_interface_ref_selection (AtkSelection *selection,
                                   gint i)
{
	gint count;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (selection);
	gint start_index;

	count = selection_interface_get_selection_count (selection);
	if (i < 0 || i >=count)
		return NULL;

	g_obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (ea_main_item));

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	start_index = ea_day_view_main_item_get_child_index_at (
		ea_main_item,
		day_view->selection_start_row,
		day_view->selection_start_day);

	return ea_day_view_main_item_ref_child (ATK_OBJECT (selection), start_index + i);
}

static gint
selection_interface_get_selection_count (AtkSelection *selection)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (selection);
	gint start_index, end_index;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return 0;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	if (day_view->selection_start_day == -1 ||
	    day_view->selection_start_row == -1)
		return 0;
	start_index = ea_day_view_main_item_get_child_index_at (
		ea_main_item,
		day_view->selection_start_row,
		day_view->selection_start_day);
	end_index = ea_day_view_main_item_get_child_index_at (
		ea_main_item,
		day_view->selection_end_row,
		day_view->selection_end_day);

	return end_index - start_index + 1;
}

static gboolean
selection_interface_is_child_selected (AtkSelection *selection,
                                       gint i)
{
	AtkGObjectAccessible *atk_gobj;
	GObject *g_obj;
	EDayViewMainItem *main_item;
	EDayView *day_view;
	EaDayViewMainItem * ea_main_item = EA_DAY_VIEW_MAIN_ITEM (selection);
	gint column, row;

	atk_gobj = ATK_GOBJECT_ACCESSIBLE (ea_main_item);
	g_obj = atk_gobject_accessible_get_object (atk_gobj);
	if (!g_obj)
		return FALSE;

	main_item = E_DAY_VIEW_MAIN_ITEM (g_obj);
	day_view = e_day_view_main_item_get_day_view (main_item);

	row = ea_day_view_main_item_get_row_at_index (ea_main_item, i);
	column = ea_day_view_main_item_get_column_at_index (ea_main_item, i);

	if (column < day_view->selection_start_day ||
	    column > day_view->selection_end_day)
		return FALSE;

	if ((column == day_view->selection_start_day ||
	     column == day_view->selection_end_day) &&
	    (row < day_view->selection_start_row ||
	     row > day_view->selection_end_row))
		return FALSE;

	/* if comes here, the cell is selected */
	return TRUE;
}
