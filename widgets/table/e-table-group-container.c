/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-Table-Group.c: Implements the grouping objects for elements on a table
 *
 * Author:
 *   Chris Lahey <clahey@helixcode.com>
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright 1999, 2000 Helix Code, Inc.
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include "e-table-group-container.h"
#include "e-table-group-leaf.h"
#include "e-table-item.h"
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include "e-util/e-util.h"
#include "e-util/e-canvas.h"
#include "e-util/e-canvas-utils.h"
#include "widgets/e-text/e-text.h"
#include "e-table-defines.h"

#define TITLE_HEIGHT         16

#define PARENT_TYPE e_table_group_get_type ()

static GnomeCanvasGroupClass *etgc_parent_class;

/* The arguments we take */
enum {
	ARG_0,
	ARG_HEIGHT,
	ARG_WIDTH,
	ARG_MINIMUM_WIDTH,
	ARG_FROZEN,
	ARG_TABLE_DRAW_GRID,
	ARG_TABLE_DRAW_FOCUS,
	ARG_CURSOR_MODE,
	ARG_LENGTH_THRESHOLD,
};

typedef struct {
	ETableGroup *child;
	void *key;
	GnomeCanvasItem *text;
	GnomeCanvasItem *rect;
	gint count;
} ETableGroupContainerChildNode;

static void
e_table_group_container_child_node_free (ETableGroupContainer          *etgc,
					ETableGroupContainerChildNode *child_node)
{
	ETableGroup *etg = E_TABLE_GROUP (etgc);
	ETableGroup *child = child_node->child;

	gtk_object_destroy (GTK_OBJECT (child));
	e_table_model_free_value (etg->model, etgc->ecol->col_idx,
				  child_node->key);
	gtk_object_destroy (GTK_OBJECT (child_node->text));
	gtk_object_destroy (GTK_OBJECT (child_node->rect));
}

static void
e_table_group_container_list_free (ETableGroupContainer *etgc)
{
	ETableGroupContainerChildNode *child_node;
	GList *list;

	for (list = etgc->children; list; list = g_list_next (list)) {
		child_node = (ETableGroupContainerChildNode *) list->data;
		e_table_group_container_child_node_free (etgc, child_node);
	}

	g_list_free (etgc->children);
}

static void
etgc_destroy (GtkObject *object)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER (object);

	if (etgc->font)
		gdk_font_unref (etgc->font);
		etgc->font = NULL;

	if (etgc->ecol)
		gtk_object_unref (GTK_OBJECT(etgc->ecol));

	if (etgc->sort_info)
		gtk_object_unref (GTK_OBJECT(etgc->sort_info));

	if (etgc->rect)
		gtk_object_destroy (GTK_OBJECT(etgc->rect));

	e_table_group_container_list_free (etgc);

	GTK_OBJECT_CLASS (etgc_parent_class)->destroy (object);
}

void
e_table_group_container_construct (GnomeCanvasGroup *parent, ETableGroupContainer *etgc,
				   ETableHeader *full_header,
				   ETableHeader     *header,
				   ETableModel *model, ETableSortInfo *sort_info, int n)
{
	ETableCol *col;
	ETableSortColumn column = e_table_sort_info_grouping_get_nth(sort_info, n);

	if (column.column > e_table_header_count (full_header))
		col = e_table_header_get_column (full_header, e_table_header_count (full_header) - 1);
	else
		col = e_table_header_get_column (full_header, column.column);

	e_table_group_construct (parent, E_TABLE_GROUP (etgc), full_header, header, model);
	etgc->ecol = col;
	gtk_object_ref (GTK_OBJECT(etgc->ecol));
	etgc->sort_info = sort_info;
	gtk_object_ref (GTK_OBJECT(etgc->sort_info));
	etgc->n = n;
	etgc->ascending = column.ascending;

	
	etgc->font = gdk_font_load ("lucidasans-10");
	if (!etgc->font){
		etgc->font = GTK_WIDGET (GNOME_CANVAS_ITEM (etgc)->canvas)->style->font;
		
		gdk_font_ref (etgc->font);
	}
	etgc->open = TRUE;
}

ETableGroup *
e_table_group_container_new (GnomeCanvasGroup *parent, ETableHeader *full_header,
			     ETableHeader     *header,
			     ETableModel *model, ETableSortInfo *sort_info, int n)
{
	ETableGroupContainer *etgc;

	g_return_val_if_fail (parent != NULL, NULL);
	
	etgc = gtk_type_new (e_table_group_container_get_type ());

	e_table_group_container_construct (parent, etgc, full_header, header,
					   model, sort_info, n);
	return E_TABLE_GROUP (etgc);
}


static int
etgc_event (GnomeCanvasItem *item, GdkEvent *event)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(item);
	gboolean return_val = TRUE;
	gboolean change_focus = FALSE;
	gboolean use_col = FALSE;
	gint start_col = 0;
	gint old_col;
	EFocus direction = E_FOCUS_START;

	switch (event->type) {
	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_Tab || 
		    event->key.keyval == GDK_KP_Tab || 
		    event->key.keyval == GDK_ISO_Left_Tab) {
			change_focus = TRUE;
			use_col      = TRUE;
			start_col    = (event->key.state & GDK_SHIFT_MASK) ? -1 : 0;
			direction    = (event->key.state & GDK_SHIFT_MASK) ? E_FOCUS_END : E_FOCUS_START;
		} else if (event->key.keyval == GDK_Left ||
			   event->key.keyval == GDK_KP_Left) {
			change_focus = TRUE;
			use_col      = TRUE;
			start_col    = -1;
			direction    = E_FOCUS_END;
		} else if (event->key.keyval == GDK_Right ||
			   event->key.keyval == GDK_KP_Right) {
			change_focus = TRUE;
			use_col   = TRUE;
			start_col = 0;
			direction = E_FOCUS_START;
		} else if (event->key.keyval == GDK_Down ||
			   event->key.keyval == GDK_KP_Down) {
			change_focus = TRUE;
			use_col      = FALSE;
			direction    = E_FOCUS_START;
		} else if (event->key.keyval == GDK_Up ||
			   event->key.keyval == GDK_KP_Up) {
			change_focus = TRUE;
			use_col      = FALSE;
			direction    = E_FOCUS_END;
		} else if (event->key.keyval == GDK_Return ||
			   event->key.keyval == GDK_KP_Enter) {
			change_focus = TRUE;
			use_col      = FALSE;
			direction    = E_FOCUS_START;
		}
		if (change_focus){		
			GList *list;
			for (list = etgc->children; list; list = list->next) {
				ETableGroupContainerChildNode *child_node;
				ETableGroup                   *child;

				child_node = (ETableGroupContainerChildNode *)list->data;
				child      = child_node->child;

				if (e_table_group_get_focus (child)) {
					old_col = e_table_group_get_focus_column (child);
					if (old_col == -1)
						old_col = 0;
					if (start_col == -1)
						start_col = e_table_header_count (e_table_group_get_header (child)) - 1;
					
					e_table_group_unfocus(child);
					if (direction == E_FOCUS_END)
						list = list->prev;
					else
						list = list->next;
					
					if (list) {
						child_node = (ETableGroupContainerChildNode *)list->data;
						child = child_node->child;
						if (use_col)
							e_table_group_set_focus (child, direction, start_col);
						else
							e_table_group_set_focus (child, direction, old_col);
						return 1;
					} else {
						return 0;
					}
				}
			}
			if (direction == E_FOCUS_END)
				list = g_list_last(etgc->children);
			else
				list = etgc->children;
			if (list) {
				ETableGroupContainerChildNode *child_node;
				ETableGroup                   *child;

				child_node = (ETableGroupContainerChildNode *)list->data;
				child = child_node->child;

				if (start_col == -1)
					start_col = e_table_header_count (e_table_group_get_header (child)) - 1;

				e_table_group_set_focus (child, direction, start_col);
				return 1;
			}
		}
		return_val = FALSE;
	default:
		return_val = FALSE;
	}
	if (return_val == FALSE) {
		if (GNOME_CANVAS_ITEM_CLASS(etgc_parent_class)->event)
			return GNOME_CANVAS_ITEM_CLASS (etgc_parent_class)->event (item, event);
	}
	return return_val;
	
}

/* Realize handler for the text item */
static void
etgc_realize (GnomeCanvasItem *item)
{
	ETableGroupContainer *etgc;

	if (GNOME_CANVAS_ITEM_CLASS (etgc_parent_class)->realize)
		(* GNOME_CANVAS_ITEM_CLASS (etgc_parent_class)->realize) (item);

	etgc = E_TABLE_GROUP_CONTAINER (item);

	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (etgc));
}

/* Unrealize handler for the etgc item */
static void
etgc_unrealize (GnomeCanvasItem *item)
{
	ETableGroupContainer *etgc;

	etgc = E_TABLE_GROUP_CONTAINER (item);
	
	if (GNOME_CANVAS_ITEM_CLASS (etgc_parent_class)->unrealize)
		(* GNOME_CANVAS_ITEM_CLASS (etgc_parent_class)->unrealize) (item);
}

static void
compute_text (ETableGroupContainer *etgc, ETableGroupContainerChildNode *child_node)
{
	/* FIXME : What a hack, eh? */
	gchar *text = g_strdup_printf ("%s : %s (%d item%s)",
				       etgc->ecol->text,
				       (gchar *)child_node->key,
				       (gint) child_node->count,
				       child_node->count == 1 ? "" : "s");
	gnome_canvas_item_set (child_node->text, 
			       "text", text,
			       NULL);
	g_free (text);
}

static void
child_row_selection (ETableGroup *etg, int row, gboolean selected,
		     ETableGroupContainer *etgc)
{
	e_table_group_row_selection (E_TABLE_GROUP (etgc), row, selected);
}

static void
child_cursor_change (ETableGroup *etg, int row,
		    ETableGroupContainer *etgc)
{
	e_table_group_cursor_change (E_TABLE_GROUP (etgc), row);
}

static void
child_double_click (ETableGroup *etg, int row,
		    ETableGroupContainer *etgc)
{
	e_table_group_double_click (E_TABLE_GROUP (etgc), row);
}

static gint
child_right_click (ETableGroup *etg, int row, int col, GdkEvent *event,
		   ETableGroupContainer *etgc)
{
	return e_table_group_right_click (E_TABLE_GROUP (etgc), row, col, event);
}

static gint
child_key_press (ETableGroup *etg, int row, int col, GdkEvent *event,
		 ETableGroupContainer *etgc)
{
	return e_table_group_key_press (E_TABLE_GROUP (etgc), row, col, event);
}

static void
etgc_add (ETableGroup *etg, gint row)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER (etg);
	void *val = e_table_model_value_at (etg->model, etgc->ecol->col_idx, row);
	GCompareFunc comp = etgc->ecol->compare;
	GList *list = etgc->children;
	ETableGroup *child;
	ETableGroupContainerChildNode *child_node;
	int i = 0;

	for (; list; list = g_list_next (list), i++){
		int comp_val;

		child_node = list->data;
		comp_val = (*comp)(child_node->key, val);
		if (comp_val == 0) {
			child = child_node->child;
			child_node->count ++;
			e_table_group_add (child, row);
			compute_text (etgc, child_node);
			return;
		}
		if ((comp_val > 0 && etgc->ascending) ||
		    (comp_val < 0 && (!etgc->ascending)))
			break;
	}
	child_node = g_new (ETableGroupContainerChildNode, 1);
	child_node->rect = gnome_canvas_item_new (GNOME_CANVAS_GROUP (etgc),
						  gnome_canvas_rect_get_type (),
						  "fill_color", "grey70",
						  "outline_color", "grey50",
						  NULL);
	child_node->text = gnome_canvas_item_new (GNOME_CANVAS_GROUP (etgc),
						  e_text_get_type (),
						  "font_gdk", etgc->font,
						  "anchor", GTK_ANCHOR_SW,
						  "x", (double) 0,
						  "y", (double) 0,
						  "fill_color", "black",
						  NULL);
	child = e_table_group_new (GNOME_CANVAS_GROUP (etgc), etg->full_header,
				   etg->header, etg->model, etgc->sort_info, etgc->n + 1);
	gnome_canvas_item_set(GNOME_CANVAS_ITEM(child),
			      "drawgrid", etgc->draw_grid,
			      "drawfocus", etgc->draw_focus,
			      "cursor_mode", etgc->cursor_mode,
			      "length_threshold", etgc->length_threshold,
			      NULL);
	gtk_signal_connect (GTK_OBJECT (child), "row_selection",
			    GTK_SIGNAL_FUNC (child_row_selection), etgc);
	gtk_signal_connect (GTK_OBJECT (child), "cursor_change",
			    GTK_SIGNAL_FUNC (child_cursor_change), etgc);
	gtk_signal_connect (GTK_OBJECT (child), "double_click",
			    GTK_SIGNAL_FUNC (child_double_click), etgc);
	gtk_signal_connect (GTK_OBJECT (child), "right_click",
			    GTK_SIGNAL_FUNC (child_right_click), etgc);
	gtk_signal_connect (GTK_OBJECT (child), "key_press",
			    GTK_SIGNAL_FUNC (child_key_press), etgc);
	child_node->child = child;
	child_node->key = e_table_model_duplicate_value (etg->model, etgc->ecol->col_idx, val);
	child_node->count = 1;
	e_table_group_add (child, row);

	if (list)
		etgc->children = g_list_insert (etgc->children, child_node, i);
	else
		etgc->children = g_list_append (etgc->children, child_node);

	compute_text (etgc, child_node);
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (etgc));
}

static void
etgc_add_all (ETableGroup *etg)
{
	int rows = e_table_model_row_count(etg->model);
	int i;
	for (i = 0; i < rows; i++)
		etgc_add(etg, i);
}

static gboolean
etgc_remove (ETableGroup *etg, gint row)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(etg);
	GList *list;

	for (list = etgc->children ; list; list = g_list_next (list)) {
		ETableGroupContainerChildNode *child_node = list->data;
		ETableGroup                   *child = child_node->child;

		if (e_table_group_remove (child, row)) {
			child_node->count --;
			if (child_node->count == 0) {
				e_table_group_container_child_node_free (etgc, child_node);
				etgc->children = g_list_remove (etgc->children, child_node);
				g_free (child_node);
			} else
				compute_text (etgc, child_node);

			e_canvas_item_request_reflow (GNOME_CANVAS_ITEM (etgc));

			return TRUE;
		}
	}
	return FALSE;
}

static int
etgc_row_count (ETableGroup *etg)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(etg);
	GList *list;
	gint count = 0;
	for (list = etgc->children; list; list = g_list_next(list)) {
		ETableGroup *group = ((ETableGroupContainerChildNode *)list->data)->child;
		gint this_count = e_table_group_row_count(group);
		count += this_count;
	}
	return count;
}

static void
etgc_increment (ETableGroup *etg, gint position, gint amount)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(etg);
	GList *list = etgc->children;

	for (list = etgc->children ; list; list = g_list_next (list))
		e_table_group_increment (((ETableGroupContainerChildNode *)list->data)->child,
					 position, amount);
}

static void
etgc_select_row (ETableGroup *etg, gint row)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(etg);
	GList *list;
	for (list = etgc->children; list; list = g_list_next(list)) {
		ETableGroup *group = ((ETableGroupContainerChildNode *)list->data)->child;
		gint this_count = e_table_group_row_count(group);
		if (row < this_count) {
			e_table_group_select_row(group, row);
			return;
		}
		row -= this_count;
	}
}

static int
etgc_get_selected_view_row (ETableGroup *etg)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(etg);
	GList *list;
	int count = 0;
	for (list = etgc->children; list; list = g_list_next(list)) {
		ETableGroup *group = ((ETableGroupContainerChildNode *)list->data)->child;
		int row = e_table_group_get_selected_view_row(group);
		if (row != -1)
			return count + row;
		count += e_table_group_row_count(group);
	}
	return -1;
}

static void
etgc_set_focus (ETableGroup *etg, EFocus direction, gint view_col)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(etg);
	if (etgc->children) {
		if (direction == E_FOCUS_END)
			e_table_group_set_focus (((ETableGroupContainerChildNode *)g_list_last (etgc->children)->data)->child,
						 direction, view_col);
		else
			e_table_group_set_focus (((ETableGroupContainerChildNode *)etgc->children->data)->child,
						 direction, view_col);
	}
}

static void
etgc_unfocus (ETableGroup *etg)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(etg);
	GList *list;
	for (list = etgc->children; list; list = g_list_next(list)) {
		e_table_group_unfocus (((ETableGroupContainerChildNode *)list->data)->child);
	}
}

static gint
etgc_get_focus_column (ETableGroup *etg)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(etg);
	if (etgc->children) {
		GList *list;
		for (list = etgc->children; list; list = list->next) {
			ETableGroupContainerChildNode *child_node = (ETableGroupContainerChildNode *)list->data;
			ETableGroup *child = child_node->child;
			if (e_table_group_get_focus (child)) {
				return e_table_group_get_focus_column (child);
			}
		}
	}
	return 0;
}

static void etgc_thaw (ETableGroup *etg)
{
	e_canvas_item_request_reflow (GNOME_CANVAS_ITEM(etg));
}

static void
etgc_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableGroup *etg = E_TABLE_GROUP (object);
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER (object);
	GList *list;

	switch (arg_id) {
	case ARG_FROZEN:
		if (GTK_VALUE_BOOL (*arg))
			etg->frozen = TRUE;
		else {
			etg->frozen = FALSE;
			etgc_thaw (etg);
		}
		break;
	case ARG_MINIMUM_WIDTH:
		etgc->minimum_width = GTK_VALUE_DOUBLE(*arg);

		for (list = etgc->children; list; list = g_list_next (list)) {
			ETableGroupContainerChildNode *child_node = (ETableGroupContainerChildNode *)list->data;
			gtk_object_set (GTK_OBJECT(child_node->child),
					"minimum_width", etgc->minimum_width - GROUP_INDENT,
					NULL);
		}
		break;
	case ARG_LENGTH_THRESHOLD:
		etgc->length_threshold = GTK_VALUE_INT (*arg);
		for (list = etgc->children; list; list = g_list_next (list)) {
			ETableGroupContainerChildNode *child_node = (ETableGroupContainerChildNode *)list->data;
			gtk_object_set (GTK_OBJECT(child_node->child),
					"length_threshold", GTK_VALUE_INT (*arg),
					NULL);
		}
		break;

	case ARG_TABLE_DRAW_GRID:
		etgc->draw_grid = GTK_VALUE_BOOL (*arg);
		for (list = etgc->children; list; list = g_list_next (list)) {
			ETableGroupContainerChildNode *child_node = (ETableGroupContainerChildNode *)list->data;
			gtk_object_set (GTK_OBJECT(child_node->child),
					"drawgrid", GTK_VALUE_BOOL (*arg),
					NULL);
		}
		break;

	case ARG_TABLE_DRAW_FOCUS:
		etgc->draw_focus = GTK_VALUE_BOOL (*arg);
		for (list = etgc->children; list; list = g_list_next (list)) {
			ETableGroupContainerChildNode *child_node = (ETableGroupContainerChildNode *)list->data;
			gtk_object_set (GTK_OBJECT(child_node->child),
					"drawfocus", GTK_VALUE_BOOL (*arg),
					NULL);
		}
		break;

	case ARG_CURSOR_MODE:
		etgc->cursor_mode = GTK_VALUE_INT (*arg);
		for (list = etgc->children; list; list = g_list_next (list)) {
			ETableGroupContainerChildNode *child_node = (ETableGroupContainerChildNode *)list->data;
			gtk_object_set (GTK_OBJECT(child_node->child),
					"cursor_mode", GTK_VALUE_INT (*arg),
					NULL);
		}
		break;
	default:
		break;
	}
}

static void
etgc_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableGroup *etg = E_TABLE_GROUP (object);
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER (object);

	switch (arg_id) {
	case ARG_FROZEN:
		GTK_VALUE_BOOL (*arg) = etg->frozen;
		break;
	case ARG_HEIGHT:
		GTK_VALUE_DOUBLE (*arg) = etgc->height;
		break;
	case ARG_WIDTH:	
		GTK_VALUE_DOUBLE (*arg) = etgc->width;
		break;
	case ARG_MINIMUM_WIDTH:
		etgc->minimum_width = GTK_VALUE_DOUBLE(*arg);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
etgc_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;
	ETableGroupClass *e_group_class = E_TABLE_GROUP_CLASS(object_class);

	object_class->destroy = etgc_destroy;
	object_class->set_arg = etgc_set_arg;
	object_class->get_arg = etgc_get_arg;

	item_class->event = etgc_event;
	item_class->realize = etgc_realize;
	item_class->unrealize = etgc_unrealize;

	etgc_parent_class = gtk_type_class (PARENT_TYPE);

	e_group_class->add = etgc_add;
	e_group_class->add_all = etgc_add_all;
	e_group_class->remove = etgc_remove;
	e_group_class->increment  = etgc_increment;
	e_group_class->row_count  = etgc_row_count;
	e_group_class->set_focus  = etgc_set_focus;
	e_group_class->select_row = etgc_select_row;
	e_group_class->get_selected_view_row = etgc_get_selected_view_row;
	e_group_class->unfocus    = etgc_unfocus;
	e_group_class->get_focus_column = etgc_get_focus_column;

	gtk_object_add_arg_type ("ETableGroupContainer::drawgrid", GTK_TYPE_BOOL,
				 GTK_ARG_WRITABLE, ARG_TABLE_DRAW_GRID);
	gtk_object_add_arg_type ("ETableGroupContainer::drawfocus", GTK_TYPE_BOOL,
				 GTK_ARG_WRITABLE, ARG_TABLE_DRAW_FOCUS);
	gtk_object_add_arg_type ("ETableGroupContainer::cursor_mode", GTK_TYPE_INT,
				 GTK_ARG_WRITABLE, ARG_CURSOR_MODE);
	gtk_object_add_arg_type ("ETableGroupContainer::length_threshold", GTK_TYPE_INT,
				 GTK_ARG_WRITABLE, ARG_LENGTH_THRESHOLD);

	gtk_object_add_arg_type ("ETableGroupContainer::frozen", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_FROZEN);
	gtk_object_add_arg_type ("ETableGroupContainer::height", GTK_TYPE_DOUBLE, 
				 GTK_ARG_READABLE, ARG_HEIGHT);
	gtk_object_add_arg_type ("ETableGroupContainer::width", GTK_TYPE_DOUBLE, 
				 GTK_ARG_READABLE, ARG_WIDTH);
	gtk_object_add_arg_type ("ETableGroupContainer::minimum_width", GTK_TYPE_DOUBLE,
				 GTK_ARG_READWRITE, ARG_MINIMUM_WIDTH);
}

static void
etgc_reflow (GnomeCanvasItem *item, gint flags)
{
	ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER(item);
	gboolean frozen;

        gtk_object_get (GTK_OBJECT(etgc), 
			"frozen", &frozen,
			NULL);

	if (frozen)
		return;
	

	if (GTK_OBJECT_FLAGS(etgc)& GNOME_CANVAS_ITEM_REALIZED){
		gdouble running_height = 0;
		gdouble running_width = 0;
		gdouble old_height;
		gdouble old_width;
		
		old_height = etgc->height;
		old_width = etgc->width;
		if (etgc->children == NULL){
		} else {
			GList *list;
			gdouble extra_height = 0;
			gdouble item_height = 0;
			gdouble item_width = 0;
			
			if (etgc->font)
				extra_height += etgc->font->ascent + etgc->font->descent + BUTTON_PADDING * 2;
			
			extra_height = MAX(extra_height, BUTTON_HEIGHT + BUTTON_PADDING * 2);
				
			running_height = extra_height;
			
			for ( list = etgc->children; list; list = g_list_next (list)){
				ETableGroupContainerChildNode *child_node = (ETableGroupContainerChildNode *) list->data;
				ETableGroup *child = child_node->child;
				
				gtk_object_get (GTK_OBJECT(child),
						"width", &item_width,
						NULL);

				if (item_width > running_width)
					running_width = item_width;
			}
			for ( list = etgc->children; list; list = g_list_next (list)){
				ETableGroupContainerChildNode *child_node = (ETableGroupContainerChildNode *) list->data;
				ETableGroup *child = child_node->child;
				gtk_object_get (GTK_OBJECT(child),
						"height", &item_height,
						NULL);
				
				e_canvas_item_move_absolute (GNOME_CANVAS_ITEM(child_node->text),
							    GROUP_INDENT,
							    running_height - BUTTON_PADDING);
				
				e_canvas_item_move_absolute (GNOME_CANVAS_ITEM(child),
							    GROUP_INDENT,
							    running_height);
				
				gnome_canvas_item_set (GNOME_CANVAS_ITEM(child_node->rect),
						      "x1", (double) 0,
						      "x2", (double) running_width + GROUP_INDENT,
						      "y1", (double) running_height - extra_height,
						      "y2", (double) running_height + item_height,
						      NULL);
				
				running_height += item_height + extra_height;
			}
			running_height -= extra_height;
		}
		if (running_height != old_height || running_width != old_width) {
			etgc->height = running_height;
			etgc->width = running_width;
			e_canvas_item_request_parent_reflow (item);
		}
	}
}

static void
etgc_init (GtkObject *object)
{
	ETableGroupContainer *container = E_TABLE_GROUP_CONTAINER(object);
	container->children = FALSE;
	
	e_canvas_item_set_reflow_callback (GNOME_CANVAS_ITEM(object), etgc_reflow);

	container->draw_grid = 1;
	container->draw_focus = 1;
	container->cursor_mode = E_TABLE_CURSOR_SIMPLE;
	container->length_threshold = -1;
}

E_MAKE_TYPE (e_table_group_container, "ETableGroupContainer", ETableGroupContainer, etgc_class_init, etgc_init, PARENT_TYPE);

void
e_table_group_apply_to_leafs (ETableGroup *etg, ETableGroupLeafFn fn, void *closure)
{
	if (E_IS_TABLE_GROUP_CONTAINER (etg)){
		ETableGroupContainer *etgc = E_TABLE_GROUP_CONTAINER (etg);
		GList *list = etgc->children;

		for (list = etgc->children; list; list = list->next){
			ETableGroupContainerChildNode *child_node = list->data;

			e_table_group_apply_to_leafs (child_node->child, fn, closure);
		}
	} else if (E_IS_TABLE_GROUP_LEAF (etg)){
		(*fn) (E_TABLE_GROUP_LEAF (etg)->item, closure);
	} else {
		g_error ("Unknown ETableGroup found: %s",
			 gtk_type_name (GTK_OBJECT (etg)->klass->type));
	}
}

