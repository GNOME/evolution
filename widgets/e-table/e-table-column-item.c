/*
 * E-table-column-view.c: A canvas view of the TableColumn.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright 1999, Helix Code, Inc.
 */
#include <config.h>
#include "e-table-column.h"
#include "e-table-column-view.h"

#define PARENT_OBJECT_TYPE gnome_canvas_item_get_type ()

static GnomeCanvasItemClass *etci_parent_class;

enum {
	ARG_0,
	ARG_TABLE_COLUMN
};

static void
etci_destroy (GtkObject *object)
{
	ETableColumnItem *etcv = E_TABLE_COLUMN_VIEW (object);

	gtk_object_unref (GTK_OBJECT (etcv));
	
	if (GTK_OBJECT_CLASS (etcv_parent_class)->destroy)
		(*GTK_OBJECT_CLASS (etcv_parent_class)->destroy) (object);
}

static void
etci_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	if (GNOME_CANVAS_ITEM_CLASS(item_bar_parent_class)->update)
		(*GNOME_CANVAS_ITEM_CLASS(item_bar_parent_class)->update)(item, affine, clip_path, flags);

	item->x1 = 0;
	item->y1 = 0;
	item->x2 = INT_MAX;
	item->y2 = INT_MAX;
	gnome_canvas_group_child_bounds (GNOME_CANVAS_GROUP (item->parent), item);
}

static void
etci_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	ETableColumnItem *etci;
	int v;

	item = GNOME_CANVAS_ITEM (o);
	etci = E_TABLE_COLUMN_ITEM (o);

	switch (arg_id){
	case ARG_TABLE_COLUMN:
		etci->etci = GTK_VALUE_POINTER (*arg);
		break;
	}
	etci_update (item, NULL, NULL, 0);
}

static void
etci_realize (GnomeCanvasItem *item)
{
	ETableColumnItem *etci = E_TABLE_COLUMN_ITEM (item);
	GdkWindow *window;
	GdkColor c;
	
	if (GNOME_CANVAS_ITEM_CLASS (etci_parent_class)-> realize)
		(*GNOME_CANVAS_ITEM_CLASS (etci_parent_class)->realize)(item);

	window = GTK_WIDGET (item->canvas)->window;

	etci->gc = gdk_gc_new (window);
	gnome_canvas_get_color (item->canvas, "black", &c);
	gdk_gc_set_foreground (etci->gc, &c);

	etci->normal_cursor = gdk_cursor_new (GDK_ARROW);
}

static void
etci_unrealize (GnomeCanvasItem *item)
{
	ETableColumnItem *etci = E_TABLE_COLUMN_ITEM (item);

	gdk_gc_unref (etci->gc);
	etci->gc = NULL;

	gdk_cursor_destroy (etci->change_cursor);
	etci->change_cursor = NULL;
	
	gdk_cursor_destroy (etci->normal_cursor);
	etci->normal_cursor = NULL;

	if (GNOME_CANVAS_ITEM_CLASS (etci_parent_class)->unrealize)
		(*GNOME_CANVAS_ITEM_CLASS (etci_parent_class)->unrealize)(item);
}

static void
etci_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x1, int y1, int width, int height)
{
	ETableColumnItem *etci = E_TABLE_COLUMN_ITEM (item);
	GnomeCanvas *canvas = item->canvas;
	GdkGC *gc;
	const int cols = e_table_column_count (etci->etc);
	int x2 = x1 + width;
	int col, total;

	total = 0;
	for (col = 0; col < cols; col++){
		ETableCol *col = e_table_column_get_column (etci->etc, col);
		const int col_width = col->width;

		if (x1 > total + col_width)
			continue;

		if (x2 < total)
			return;

		gc = canvas->style->bg_gc [GTK_STATE_ACTIVE];
		gdk_draw_rectangle (drawble, gc, TRUE, 
		gtk_draw_shadow (canvas->style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
				 x, y, width, height
	}
}

static double
etci_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
	    GnomeCanvasItem **actual_item)
{
	*actual_item = *item;
	return 0.0;
}

static void
etci_event (GnomeCanvasItem *item, GdkEvent *e)
{
	switch (e->type){
	default:
		return FALSE;
	}
	return TRUE;
}

static void
etci_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;

	object_class->destroy = etci_destroy;
	object_class->set_arg = etci_set_arg;

	item_class->update      = etci_update;
	item_class->realize     = etci_realize;
	item_class->unrealize   = etci_unrealize;
	item_class->draw        = etci_draw;
	item_class->point       = etci_point;
	item_class->event       = etci_event;
	
	gtk_object_add_arg_type ("ETableColumnItem::ETableColumn", GTK_TYPE_POINTER,
				 GTK_ARG_WRITABLE, ARG_TABLE_COLUMN);
}

static void
etci_init (GnomeCanvasItem *item)
{
	item->x1 = 0;
	item->y1 = 0;
	item->x2 = 0;
	item->y2 = 0;
}

GtkType
e_table_column_view_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETableColumnItem",
			sizeof (ETableColumnItem),
			sizeof (ETableColumnItemClass),
			(GtkClassInitFunc) etci_class_init,
			(GtkObjectInitFunc) etci_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_OBJECT_TYPE, &info);
	}

	return type;
}



