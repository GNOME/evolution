/*
 * E-table-column-view.c: A canvas view of the TableColumn.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright 1999, International GNOME Support.
 */
#include <config.h>
#include "e-table-header.h"
#include "e-table-header-item.h"

#define ETHI_HEIGHT 14

#define PARENT_OBJECT_TYPE gnome_canvas_item_get_type ()

static GnomeCanvasItemClass *ethi_parent_class;

enum {
	ARG_0,
	ARG_TABLE_HEADER
};

static void
ethi_destroy (GtkObject *object)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (object);

	gtk_object_unref (GTK_OBJECT (ethi));
	
	if (GTK_OBJECT_CLASS (ethi_parent_class)->destroy)
		(*GTK_OBJECT_CLASS (ethi_parent_class)->destroy) (object);
}

static void
ethi_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	if (GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->update)
		(*GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->update)(item, affine, clip_path, flags);

	item->x1 = 0;
	item->y1 = 0;
	item->x2 = INT_MAX;
	item->y2 = INT_MAX;
	gnome_canvas_group_child_bounds (GNOME_CANVAS_GROUP (item->parent), item);
}

static void
ethi_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	ETableHeaderItem *ethi;
	int v;

	item = GNOME_CANVAS_ITEM (o);
	ethi = E_TABLE_HEADER_ITEM (o);

	switch (arg_id){
	case ARG_TABLE_HEADER:
		ethi->eth = GTK_VALUE_POINTER (*arg);
		break;
	}
	ethi_update (item, NULL, NULL, 0);
}

static void
ethi_realize (GnomeCanvasItem *item)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);
	GdkWindow *window;
	GdkColor c;
	
	if (GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)-> realize)
		(*GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->realize)(item);

	window = GTK_WIDGET (item->canvas)->window;

	ethi->gc = gdk_gc_new (window);
	gnome_canvas_get_color (item->canvas, "black", &c);
	gdk_gc_set_foreground (ethi->gc, &c);

	ethi->normal_cursor = gdk_cursor_new (GDK_ARROW);
}

static void
ethi_unrealize (GnomeCanvasItem *item)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);

	gdk_gc_unref (ethi->gc);
	ethi->gc = NULL;

	gdk_cursor_destroy (ethi->change_cursor);
	ethi->change_cursor = NULL;
	
	gdk_cursor_destroy (ethi->normal_cursor);
	ethi->normal_cursor = NULL;

	if (GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->unrealize)
		(*GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->unrealize)(item);
}

static void
ethi_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x1, int y1, int width, int height)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);
	GnomeCanvas *canvas = item->canvas;
	GdkGC *gc;
	const int cols = e_table_header_count (ethi->eth);
	int x2 = x1 + width;
	int col, total;
	int x;

	total = 0;
	x = 0;
	for (col = 0; col < cols; col++){
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);
		const int col_width = ecol->width;

		if (x1 > total + col_width){
			x += col_width;
			continue;
		}

		if (x2 < total)
			return;

		gc = GTK_WIDGET (canvas)->style->bg_gc [GTK_STATE_ACTIVE];

		gdk_draw_rectangle (
			drawable, gc, TRUE,
			x + 1, -y1 + 1, col_width - 2, ETHI_HEIGHT - 2);
			
		gtk_draw_shadow (
			GTK_WIDGET (canvas)->style, drawable,
			GTK_STATE_NORMAL, GTK_SHADOW_OUT,
			x , -y1, col_width, ETHI_HEIGHT);

		{
			GdkRectangle clip;
			GdkFont *font = GTK_WIDGET (canvas)->style->font;
			
			clip.x = x + 2;
			clip.y = -y1 + 2;
			clip.width = col_width - 4;
			clip.height = ETHI_HEIGHT - 4;
			gdk_gc_set_clip_rectangle (gc, &clip);

			/*
			 * FIXME: should be obvious
			 */
			gdk_draw_text (drawable, font, gc, x, -y1 + 10, "TEST", 4);
			
			gdk_gc_set_clip_rectangle (gc, NULL);
		}
	}
}

static double
ethi_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
	    GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}

static int
ethi_event (GnomeCanvasItem *item, GdkEvent *e)
{
	switch (e->type){
	default:
		return FALSE;
	}
	return TRUE;
}

static void
ethi_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;

	ethi_parent_class = gtk_type_class (gnome_canvas_item_get_type ());
	
	object_class->destroy = ethi_destroy;
	object_class->set_arg = ethi_set_arg;

	item_class->update      = ethi_update;
	item_class->realize     = ethi_realize;
	item_class->unrealize   = ethi_unrealize;
	item_class->draw        = ethi_draw;
	item_class->point       = ethi_point;
	item_class->event       = ethi_event;
	
	gtk_object_add_arg_type ("ETableHeaderItem::ETableHeader", GTK_TYPE_POINTER,
				 GTK_ARG_WRITABLE, ARG_TABLE_HEADER);
}

static void
ethi_init (GnomeCanvasItem *item)
{
	item->x1 = 0;
	item->y1 = 0;
	item->x2 = 0;
	item->y2 = 0;
}

GtkType
e_table_header_item_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETableHeaderItem",
			sizeof (ETableHeaderItem),
			sizeof (ETableHeaderItemClass),
			(GtkClassInitFunc) ethi_class_init,
			(GtkObjectInitFunc) ethi_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_OBJECT_TYPE, &info);
	}

	return type;
}

