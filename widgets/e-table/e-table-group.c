/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-Table-Group.c: Implements the grouping objects for elements on a table
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *   Chris Lahey (clahey@helixcode.com)
 *
 * Copyright 1999, 2000 Helix Code, Inc.
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include "e-table-group.h"
#include "e-table-group-container.h"
#include "e-table-group-leaf.h"
#include "e-table-item.h"
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include "e-util/e-util.h"

#define TITLE_HEIGHT         16
#define GROUP_INDENT         10

#define PARENT_TYPE gnome_canvas_group_get_type ()

#define ETG_CLASS(e) (E_TABLE_GROUP_CLASS(GTK_OBJECT(e)->klass))

static GnomeCanvasGroupClass *etg_parent_class;

enum {
	ROW_SELECTION,
	CURSOR_CHANGE,
	DOUBLE_CLICK,
	RIGHT_CLICK,
	KEY_PRESS,
	LAST_SIGNAL
};

static gint etg_signals [LAST_SIGNAL] = { 0, };

static gboolean etg_get_focus (ETableGroup      *etg);
static void etg_destroy (GtkObject *object);

static void
etg_destroy (GtkObject *object)
{
	ETableGroup *etg = E_TABLE_GROUP(object);
	if (etg->header)
		gtk_object_unref (GTK_OBJECT(etg->header));
	if (etg->full_header)
		gtk_object_unref (GTK_OBJECT(etg->full_header));
	if (etg->model)
		gtk_object_unref (GTK_OBJECT(etg->model));
	if (GTK_OBJECT_CLASS (etg_parent_class)->destroy)
		GTK_OBJECT_CLASS (etg_parent_class)->destroy (object);
}

ETableGroup *
e_table_group_new (GnomeCanvasGroup *parent,
		   ETableHeader     *full_header,
		   ETableHeader     *header,
		   ETableModel      *model,
		   ETableSortInfo   *sort_info,
		   int               n)
{
	g_return_val_if_fail (model != NULL, NULL);
	
	if (n < e_table_sort_info_grouping_get_count(sort_info)) {
		return e_table_group_container_new (parent, full_header, header, model, sort_info, n);
	} else {
		return e_table_group_leaf_new (parent, full_header, header, model, sort_info);
	}
	return NULL;
}

void
e_table_group_construct (GnomeCanvasGroup *parent,
			 ETableGroup      *etg,
			 ETableHeader     *full_header,
			 ETableHeader     *header,
			 ETableModel      *model)
{
	etg->full_header = full_header;
	gtk_object_ref (GTK_OBJECT(etg->full_header));
	etg->header = header;
	gtk_object_ref (GTK_OBJECT(etg->header));
	etg->model = model;
	gtk_object_ref (GTK_OBJECT(etg->model));
	gnome_canvas_item_constructv (GNOME_CANVAS_ITEM (etg), parent, 0, NULL);
}

void
e_table_group_add (ETableGroup *etg,
		   gint row)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));

	if (ETG_CLASS (etg)->add)
		ETG_CLASS (etg)->add (etg, row);
}

void
e_table_group_add_all (ETableGroup *etg)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));

	if (ETG_CLASS (etg)->add_all)
		ETG_CLASS (etg)->add_all (etg);
}

gboolean
e_table_group_remove (ETableGroup *etg,
		      gint row)
{
	g_return_val_if_fail (etg != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), FALSE);

	if (ETG_CLASS (etg)->remove)
		return ETG_CLASS (etg)->remove (etg, row);
	else
		return FALSE;
}

gint
e_table_group_get_count (ETableGroup *etg)
{
	g_return_val_if_fail (etg != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), 0);

	if (ETG_CLASS (etg)->get_count)
		return ETG_CLASS (etg)->get_count (etg);
	else
		return 0;
}

gint
e_table_group_row_count (ETableGroup *etg)
{
	g_return_val_if_fail (etg != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), 0);

	if (ETG_CLASS (etg)->row_count)
		return ETG_CLASS (etg)->row_count (etg);
	else
		return 0;
}

void
e_table_group_increment (ETableGroup *etg,
			 gint position,
			 gint amount)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));

	if (ETG_CLASS (etg)->increment)
		ETG_CLASS (etg)->increment (etg, position, amount);
}

void
e_table_group_set_focus (ETableGroup *etg,
			 EFocus direction,
			 gint row)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));

	if (ETG_CLASS (etg)->set_focus)
		ETG_CLASS (etg)->set_focus (etg, direction, row);
}

void
e_table_group_select_row (ETableGroup *etg,
			  gint row)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));

	if (ETG_CLASS (etg)->select_row)
		ETG_CLASS (etg)->select_row (etg, row);
}

int
e_table_group_get_selected_view_row (ETableGroup *etg)
{
	g_return_val_if_fail (etg != NULL, -1);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), -1);

	if (ETG_CLASS (etg)->get_selected_view_row)
		return ETG_CLASS (etg)->get_selected_view_row (etg);
	else
		return -1;
}

gboolean
e_table_group_get_focus (ETableGroup *etg)
{
	g_return_val_if_fail (etg != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), FALSE);

	if (ETG_CLASS (etg)->get_focus)
		return ETG_CLASS (etg)->get_focus (etg);
	else
		return FALSE;
}

gboolean
e_table_group_get_focus_column (ETableGroup *etg)
{
	g_return_val_if_fail (etg != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), FALSE);

	if (ETG_CLASS (etg)->get_focus_column)
		return ETG_CLASS (etg)->get_focus_column (etg);
	else
		return FALSE;
}

ETableCol *
e_table_group_get_ecol (ETableGroup *etg)
{
	g_return_val_if_fail (etg != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), NULL);

	if (ETG_CLASS (etg)->get_ecol)
		return ETG_CLASS (etg)->get_ecol (etg);
	else
		return NULL;
}

EPrintable *
e_table_group_get_printable (ETableGroup *etg)
{
	g_return_val_if_fail (etg != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), NULL);

	if (ETG_CLASS (etg)->get_printable)
		return ETG_CLASS (etg)->get_printable (etg);
	else
		return NULL;
}

void
e_table_group_row_selection (ETableGroup *e_table_group, gint row, gboolean selected)
{
	g_return_if_fail (e_table_group != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (e_table_group));

	gtk_signal_emit (GTK_OBJECT (e_table_group),
			 etg_signals [ROW_SELECTION],
			 row, selected);
}

void
e_table_group_cursor_change (ETableGroup *e_table_group, gint row)
{
	g_return_if_fail (e_table_group != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (e_table_group));

	gtk_signal_emit (GTK_OBJECT (e_table_group),
			 etg_signals [CURSOR_CHANGE],
			 row);
}

void
e_table_group_double_click (ETableGroup *e_table_group, gint row)
{
	g_return_if_fail (e_table_group != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (e_table_group));

	gtk_signal_emit (GTK_OBJECT (e_table_group),
			 etg_signals [DOUBLE_CLICK],
			 row);
}

gint
e_table_group_right_click (ETableGroup *e_table_group, gint row, gint col, GdkEvent *event)
{
	gint return_val = 0;

	g_return_val_if_fail (e_table_group != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_GROUP (e_table_group), 0);

	gtk_signal_emit (GTK_OBJECT (e_table_group),
			 etg_signals [RIGHT_CLICK],
			 row, col, event, &return_val);

	return return_val;
}

gint
e_table_group_key_press (ETableGroup *e_table_group, gint row, gint col, GdkEvent *event)
{
	gint return_val = 0;

	g_return_val_if_fail (e_table_group != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_GROUP (e_table_group), 0);

	gtk_signal_emit (GTK_OBJECT (e_table_group),
			 etg_signals [KEY_PRESS],
			 row, col, event, &return_val);

	return return_val;
}

ETableHeader *
e_table_group_get_header (ETableGroup *etg)
{
	g_return_val_if_fail (etg != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), NULL);

	return etg->header;
}

static int
etg_event (GnomeCanvasItem *item, GdkEvent *event)
{
	ETableGroup *etg = E_TABLE_GROUP (item);
	gboolean return_val = TRUE;

	switch (event->type) {

	case GDK_FOCUS_CHANGE:
		etg->has_focus = event->focus_change.in;
		return_val = FALSE;

	default:
		return_val = FALSE;
	}
	if (return_val == FALSE){
		if (GNOME_CANVAS_ITEM_CLASS(etg_parent_class)->event)
			return GNOME_CANVAS_ITEM_CLASS(etg_parent_class)->event (item, event);
	}
	return return_val;

}

static gboolean
etg_get_focus (ETableGroup      *etg)
{
	return etg->has_focus;
}

static void
etg_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;
	ETableGroupClass *klass = (ETableGroupClass *) object_class;

	object_class->destroy = etg_destroy;

	item_class->event = etg_event;

	klass->row_selection = NULL;
	klass->cursor_change = NULL;
	klass->double_click = NULL;
	klass->right_click = NULL;
	klass->key_press = NULL;
	
	klass->add = NULL;
	klass->add_all = NULL;
	klass->remove = NULL;
	klass->get_count  = NULL;
	klass->row_count  = NULL;
	klass->increment  = NULL;
	klass->set_focus  = NULL;
	klass->select_row = NULL;
	klass->get_selected_view_row = NULL;
	klass->get_focus = etg_get_focus;
	klass->get_ecol = NULL;
	klass->get_printable = NULL;

	etg_parent_class = gtk_type_class (PARENT_TYPE);

	etg_signals [ROW_SELECTION] =
		gtk_signal_new ("row_selection",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableGroupClass, row_selection),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);

	etg_signals [CURSOR_CHANGE] =
		gtk_signal_new ("cursor_change",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableGroupClass, cursor_change),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	etg_signals [DOUBLE_CLICK] =
		gtk_signal_new ("double_click",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableGroupClass, double_click),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	etg_signals [RIGHT_CLICK] =
		gtk_signal_new ("right_click",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableGroupClass, right_click),
				e_marshal_INT__INT_INT_POINTER,
				GTK_TYPE_INT, 3, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_POINTER);

	etg_signals [KEY_PRESS] =
		gtk_signal_new ("key_press",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableGroupClass, key_press),
				e_marshal_INT__INT_INT_POINTER,
				GTK_TYPE_INT, 3, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, etg_signals, LAST_SIGNAL);
}

E_MAKE_TYPE (e_table_group, "ETableGroup", ETableGroup, etg_class_init, NULL, PARENT_TYPE);
