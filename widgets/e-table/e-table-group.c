/*
 * E-Table-Group.c: Implements the grouping objects for elements on a table
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org()
 *
 * Copyright 1999, Helix Code, Inc.
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include "e-table-group.h"
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include "e-util.h"

#define TITLE_HEIGHT         16
#define GROUP_INDENT         10

#define PARENT_TYPE gnome_canvas_group_get_type ()

static GnomeCanvasGroupClass *etg_parent_class;

static void
etg_destroy (GtkObject *object)
{
	ETableGroup *etg = E_TABLE_GROUP (object);
	
	gtk_object_unref (GTK_OBJECT (etg->ecol));
	
	GTK_OBJECT_CLASS (etg_parent_class)->destroy (object);
}

static int
etg_width (ETableGroup *etg)
{
	return e_table_header_total_width (etg->header) + GROUP_INDENT;
}

static int
etg_height (ETableGroup *etg)
{
	GnomeCanvasItem *child = etg->child;
	
	return TITLE_HEIGHT + (child->y2 - child->y1);
}

static void
etg_header_changed (ETableHeader *header, ETableGroup *etg)
{
	gnome_canvas_item_set (
		etg->rect,
		"x2", (double) etg_width (etg),
		NULL);
}

void
e_table_group_construct (GnomeCanvasGroup *parent, ETableGroup *etg, 
			 ETableHeader *header, int col,
			 GnomeCanvasItem *child, int open)
{
	gnome_canvas_item_constructv (GNOME_CANVAS_ITEM (etg), parent, 0, NULL);
	
	gtk_object_ref (GTK_OBJECT (header));

	etg->header = header;
	etg->col = col;
	etg->ecol = e_table_header_get_column (header, col);
	etg->open = open;

	gtk_signal_connect (
		GTK_OBJECT (header), "dimension_change",
		GTK_SIGNAL_FUNC (etg_header_changed), etg);

	etg->child = child;

	etg->rect = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (etg),
		gnome_canvas_rect_get_type (),
		"fill_color", "gray",
		"outline_color", "gray20",
		"x1", 0.0,
		"y1", 0.0,
		"x2", (double) etg_width (etg),
		"y2", (double) etg_height (etg),
		NULL);

	
	/*
	 * Reparent the child into our space.
	 */
	gnome_canvas_item_reparent (child, GNOME_CANVAS_GROUP (etg));

	gnome_canvas_item_set (
		child,
		"x", (double) GROUP_INDENT,
		"y", (double) TITLE_HEIGHT,
		NULL);
}

GnomeCanvasItem *
e_table_group_new (GnomeCanvasGroup *parent, ETableHeader *header, int col, GnomeCanvasItem *child, int open)
{
	ETableGroup *etg;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (header != NULL, NULL);	
	g_return_val_if_fail (child != NULL, NULL);
	
	etg = gtk_type_new (e_table_group_get_type ());

	e_table_group_construct (parent, etg, header, col, child, open);

	return GNOME_CANVAS_ITEM (etg);
}

static void
etg_realize (GnomeCanvasItem *item)
{
	ETableGroup *etg = E_TABLE_GROUP (item);
	
	GNOME_CANVAS_ITEM_CLASS (etg_parent_class)->realize (item);
}

static void
etg_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;

	object_class->destroy = etg_destroy;

	item_class->realize = etg_realize;

	etg_parent_class = gtk_type_class (PARENT_TYPE);
}

E_MAKE_TYPE (e_table_group, "ETableGroup", ETableGroup, etg_class_init, NULL, PARENT_TYPE);



