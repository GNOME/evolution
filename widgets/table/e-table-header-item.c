/*
 * E-table-column-view.c: A canvas item based view of the ETableColumn.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright 1999, Helix Code, Inc.
 */
#include <config.h>
#include "e-table-header.h"
#include "e-table-header-item.h"
#include "e-cursors.h"

/* Padding above and below of the string in the header display */
#define PADDING 4

/* Defines the tolerance for proximity of the column division to the cursor position */
#define TOLERANCE 2

#define ETHI_RESIZING(x) ((x)->resize_col != -1)

#define PARENT_OBJECT_TYPE gnome_canvas_item_get_type ()

static GnomeCanvasItemClass *ethi_parent_class;

enum {
	ARG_0,
	ARG_TABLE_HEADER,
	ARG_TABLE_X,
	ARG_TABLE_Y,
	ARG_TABLE_FONTSET
};

static void
ethi_destroy (GtkObject *object)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (object);

	gtk_object_unref (GTK_OBJECT (ethi->eth));
	
	if (GTK_OBJECT_CLASS (ethi_parent_class)->destroy)
		(*GTK_OBJECT_CLASS (ethi_parent_class)->destroy) (object);
}

static void
ethi_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);
	
	if (GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->update)
		(*GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->update)(item, affine, clip_path, flags);

	item->x1 = ethi->x1;
	item->y1 = ethi->y1;
	item->x2 = ethi->x1 + ethi->width;
	item->y2 = ethi->y1 + ethi->height;

	gnome_canvas_group_child_bounds (GNOME_CANVAS_GROUP (item->parent), item);
}

static void
ethi_font_load (ETableHeaderItem *ethi, char *font)
{
	if (ethi->font)
		gdk_font_unref (ethi->font);
	
	ethi->font = gdk_fontset_load (font);
	if (ethi->font == NULL)
		ethi->font = gdk_font_load ("fixed");
	
	ethi->height = ethi->font->ascent + ethi->font->descent + PADDING;
}

static void
ethi_drop_table_header (ETableHeaderItem *ethi)
{
	GtkObject *header;
	
	if (!ethi->eth)
		return;

	header = GTK_OBJECT (ethi->eth);
	gtk_signal_disconnect (header, ethi->structure_change_id);
	gtk_signal_disconnect (header, ethi->dimension_change_id);

	gtk_object_unref (header);
	ethi->eth = NULL;
	ethi->width = 0;
}

static void 
structure_changed (ETableHeader *header, ETableHeaderItem *ethi)
{
	ethi->width = e_table_header_total_width (header);

	ethi_update (GNOME_CANVAS_ITEM (ethi), NULL, NULL, 0);
}

static void
dimension_changed (ETableHeader *header, int col, ETableHeaderItem *ethi)
{
	ethi->width = e_table_header_total_width (header);

	ethi_update (GNOME_CANVAS_ITEM (ethi), NULL, NULL, 0);
}

static void
ethi_add_table_header (ETableHeaderItem *ethi, ETableHeader *header)
{
	ethi->eth = header;
	gtk_object_ref (GTK_OBJECT (ethi->eth));
	ethi->width = e_table_header_total_width (header);

	ethi->structure_change_id = gtk_signal_connect (
		GTK_OBJECT (header), "structure_change",
		GTK_SIGNAL_FUNC(structure_changed), ethi);
	ethi->dimension_change_id = gtk_signal_connect (
		GTK_OBJECT (header), "dimension_change",
		GTK_SIGNAL_FUNC(dimension_changed), ethi);
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
		ethi_drop_table_header (ethi);
		ethi_add_table_header (ethi, GTK_VALUE_POINTER (*arg));
		break;

	case ARG_TABLE_X:
		ethi->x1 = GTK_VALUE_INT (*arg);
		break;

	case ARG_TABLE_Y:
		ethi->y1 = GTK_VALUE_INT (*arg);
		break;

	case ARG_TABLE_FONTSET:
		ethi_font_load (ethi, GTK_VALUE_STRING (*arg));
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

	if (!ethi->font)
		ethi_font_load (ethi, "fixed");
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
draw_button (ETableHeaderItem *ethi, ETableCol *col,
	     GdkDrawable *drawable, GdkGC *gc, GtkStyle *style,
	     int x, int y, int width, int height)
{
	GdkRectangle clip;
	int xtra;
	
	gdk_draw_rectangle (
		drawable, gc, TRUE,
		x + 1, y + 1, width - 2, height -2);
	
	gtk_draw_shadow (
		style, drawable, 
		GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		x , y, width, height);

	clip.x = x + 2;
	clip.y = y + 2;
	clip.width = width - 4;
	clip.height = ethi->height;
	
	gdk_gc_set_clip_rectangle (ethi->gc, &clip);

	/* Center the thing */
	xtra = (clip.width - gdk_string_measure (ethi->font, col->id))/2;

	if (xtra < 0)
		xtra = 0;
	
	/* Skip over border */
	x += xtra + 2;
	
	gdk_draw_text (
		drawable, ethi->font,
		ethi->gc, x, y + ethi->height - PADDING,
		col->id, strlen (col->id));
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
	x = -x1;

#if 0
	printf ("My coords are: %g %g %g %g\n",
		item->x1, item->y1, item->x2, item->y2);
#endif
	
	for (col = 0; col < cols; col++){
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);
		int col_width;

		if (col == ethi->resize_col)
			col_width = ethi->resize_width;
		else
			col_width = ecol->width;
				
		if (x1 > total + col_width){
			total += col_width;
			x += col_width;
			continue;
		}

		if (x2 < total)
			return;

		gc = GTK_WIDGET (canvas)->style->bg_gc [GTK_STATE_ACTIVE];

		draw_button (ethi, ecol, drawable, gc,
			     GTK_WIDGET (canvas)->style,
			     x, ethi->y1 - y1, col_width, ethi->height);

		x += col_width;
		total += col_width;
	}
}

static double
ethi_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
	    GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}

/*
 * is_pointer_on_division:
 *
 * Returns whether @pos is a column header division;  If @the_total is not NULL,
 * then the actual position is returned here.  If @return_ecol is not NULL,
 * then the ETableCol that actually contains this point is returned here
 */
static gboolean
is_pointer_on_division (ETableHeaderItem *ethi, int pos, int *the_total, int *return_col)
{
	const int cols = e_table_header_count (ethi->eth);
	int col, total;

	total = 0;
	for (col = 0; col < cols; col++){
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);

		total += ecol->width;

		if ((total - TOLERANCE < pos ) && (pos < total + TOLERANCE)){
			if (return_col)
				*return_col = col;
			if (the_total)
				*the_total = total;

			return TRUE;
		}

		if (total > pos + TOLERANCE)
			return FALSE;
	}

	return FALSE;
}

#define convert(c,sx,sy,x,y) gnome_canvas_w2c (c,sx,sy,x,y)

static void
set_cursor (ETableHeaderItem *ethi, int pos)
{
	GtkWidget *canvas = GTK_WIDGET (GNOME_CANVAS_ITEM (ethi)->canvas);
		
	/* We might be invoked before we are realized */
	if (!canvas->window)
		return;

	if (is_pointer_on_division (ethi, pos, NULL, NULL))
		e_cursor_set (canvas->window, E_CURSOR_SIZE_X);
	else
		e_cursor_set (canvas->window, E_CURSOR_ARROW);
}

static void
ethi_request_redraw (ETableHeaderItem *ethi)
{
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (ethi)->canvas;
	
	/*
	 * request a redraw
	 */
	gnome_canvas_request_redraw (
		canvas, ethi->x1, ethi->y1, ethi->x1 + ethi->width, ethi->x1 + ethi->height);
}

static void
ethi_end_resize (ETableHeaderItem *ethi, int new_size)
{
	e_table_header_set_size (ethi->eth, ethi->resize_col, new_size);

	ethi->resize_col = -1;
	ethi_request_redraw (ethi);
}

/*
 * Handles the events on the ETableHeaderItem, particularly it handles resizing
 */
static int
ethi_event (GnomeCanvasItem *item, GdkEvent *e)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);
	GnomeCanvas *canvas = item->canvas;
	const gboolean resizing = ETHI_RESIZING (ethi);
	int x, y, start, col;
	
	switch (e->type){
	case GDK_ENTER_NOTIFY:
		convert (canvas, e->crossing.x, e->crossing.y, &x, &y);
		set_cursor (ethi, x);
		break;

	case GDK_MOTION_NOTIFY:
		convert (canvas, e->motion.x, e->motion.y, &x, &y);
		if (resizing){
			if (ethi->resize_guide == NULL){
				/* Quick hack until I actually bind the views */
				ethi->resize_guide = GINT_TO_POINTER (1);
				gnome_canvas_item_grab (item,
							GDK_POINTER_MOTION_MASK |
							GDK_BUTTON_RELEASE_MASK,
							e_cursor_get (E_CURSOR_SIZE_X),
							e->button.time);
			}

			if (x - ethi->resize_start_pos <= 0)
				break;

			ethi_request_redraw (ethi);

			ethi->resize_width = x - ethi->resize_start_pos;
			e_table_header_set_size (ethi->eth, ethi->resize_col, ethi->resize_width);

			ethi_request_redraw (ethi);
		} else
			set_cursor (ethi, x);
		break;
		
	case GDK_BUTTON_PRESS:
		convert (canvas, e->button.x, e->button.y, &x, &y);

		if (is_pointer_on_division (ethi, x, &start, &col)){
			ETableCol *ecol;
			
			/*
			 * Record the important bits.
			 *
			 * By setting resize_pos to a non -1 value,
			 * we know that we are being resized (used in the
			 * other event handlers).
			 */
			ecol = e_table_header_get_column (ethi->eth, col);
			ethi->resize_col = col;
			ethi->resize_width = ecol->width;
			ethi->resize_start_pos = start - ecol->width;
		}
		break;

	case GDK_2BUTTON_PRESS:
		if (!resizing)
			break;

		if (e->button.button != 1)
			break;

		printf ("Resize this guy\n");
		break;
		
	case GDK_BUTTON_RELEASE: {
		gboolean needs_ungrab = FALSE;

		if (ethi->resize_col != -1){
			needs_ungrab = (ethi->resize_guide != NULL);
			ethi_end_resize (ethi, ethi->resize_width);
		}
		if (needs_ungrab)
			gnome_canvas_item_ungrab (item, e->button.time);

		break;
	}
	
	default:
		return FALSE;
	}
	return TRUE;
}

static void
ethi_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;

	ethi_parent_class = gtk_type_class (PARENT_OBJECT_TYPE);
	
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
	gtk_object_add_arg_type ("ETableHeaderItem::x", GTK_TYPE_INT,
				 GTK_ARG_WRITABLE, ARG_TABLE_X);
	gtk_object_add_arg_type ("ETableHeaderItem::y", GTK_TYPE_INT,
				 GTK_ARG_WRITABLE, ARG_TABLE_Y);
	gtk_object_add_arg_type ("ETableHeaderItem::fontset", GTK_TYPE_STRING,
				 GTK_ARG_WRITABLE, ARG_TABLE_FONTSET);
}

static void
ethi_init (GnomeCanvasItem *item)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);

	ethi->resize_col = -1;

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

