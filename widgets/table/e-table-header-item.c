/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-column-view.c: A canvas item based view of the ETableColumn.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright 1999, Helix Code, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkdnd.h>
#include <libgnomeui/gnome-canvas.h>
#include <libgnomeui/gnome-canvas-util.h>
#include <libgnomeui/gnome-canvas-polygon.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "e-util/e-cursors.h"
#include "e-table-header.h"
#include "e-table-header-item.h"
#include "e-table-col-dnd.h"

#include "add-col.xpm"
#include "remove-col.xpm"

/* Padding above and below of the string in the header display */
#define PADDING 4

#define MIN_ARROW_SIZE 10

/* Defines the tolerance for proximity of the column division to the cursor position */
#define TOLERANCE 2

#define ETHI_RESIZING(x) ((x)->resize_col != -1)

#define PARENT_OBJECT_TYPE gnome_canvas_item_get_type ()

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))

static GnomeCanvasItemClass *ethi_parent_class;

static void ethi_request_redraw (ETableHeaderItem *ethi);


/*
 * DnD icons
 */
static GdkColormap *dnd_colormap;
static GdkPixmap *remove_col_pixmap, *remove_col_mask;
static GdkPixmap *add_col_pixmap, *add_col_mask;

enum {
	ARG_0,
	ARG_TABLE_HEADER,
	ARG_TABLE_X,
	ARG_TABLE_Y,
	ARG_TABLE_FONTSET
};

static GtkTargetEntry  ethi_drag_types [] = {
	{ TARGET_ETABLE_COL_TYPE, 0, TARGET_ETABLE_COL_HEADER },
};

static GtkTargetEntry  ethi_drop_types [] = {
	{ TARGET_ETABLE_COL_TYPE, 0, TARGET_ETABLE_COL_HEADER },
};

static void
ethi_destroy (GtkObject *object){
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (object);
	
	ethi_drop_table_header (ethi);
	
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
	if ( ethi->height < MIN_ARROW_SIZE + 4 + PADDING )
		ethi->height = MIN_ARROW_SIZE + 4 + PADDING;
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

static int
ethi_find_col_by_x (ETableHeaderItem *ethi, int x)
{
	const int cols = e_table_header_count (ethi->eth);
	int x1 = ethi->x1;
	int col;

	if (x < x1)
		return -1;
	
	for (col = 0; col < cols; col++){
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);

		if ((x >= x1) && (x <= x1 + ecol->width))
			return col;

		x1 += ecol->width;
	}
	return -1;
}

static void
ethi_remove_drop_marker (ETableHeaderItem *ethi)
{
	if (ethi->drag_mark == -1)
		return;
	
	ethi->drag_mark = -1;
	gtk_object_destroy (GTK_OBJECT (ethi->drag_mark_item));
	ethi->drag_mark_item = NULL;
}

static void
ethi_add_drop_marker (ETableHeaderItem *ethi, int col)
{
	GnomeCanvasPoints *points;
	int x;
	
	if (ethi->drag_mark == col)
		return;

	if (ethi->drag_mark_item)
		gtk_object_destroy (GTK_OBJECT (ethi->drag_mark_item));		

	ethi->drag_mark = col;
	
	ethi->drag_mark_item = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (GNOME_CANVAS_ITEM (ethi)->canvas->root),
		gnome_canvas_group_get_type (),
		"x", 0,
		"y", 0,
		NULL);
	
	points = gnome_canvas_points_new (3);

	x = e_table_header_col_diff (ethi->eth, 0, col);
	
	points->coords [0] = ethi->x1 + x - 5;
	points->coords [1] = ethi->y1;
	points->coords [2] = points->coords [0] + 10;
	points->coords [3] = points->coords [1];
	points->coords [4] = ethi->x1 + x;
	points->coords [5] = ethi->y1 + 5;
		
	gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (ethi->drag_mark_item),
		gnome_canvas_polygon_get_type (),
		"points",     points,
		"fill_color", "red",
		NULL);

	points->coords [0] --;
	points->coords [1] += ethi->height - 1;
	points->coords [3] = points->coords [1];
	points->coords [5] = points->coords [1] - 6;
	
	gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (ethi->drag_mark_item),
		gnome_canvas_polygon_get_type (),
		"points",     points,
		"fill_color", "red",
		NULL);
	
	gnome_canvas_points_unref (points);
}

#define gray50_width    2
#define gray50_height   2
static char gray50_bits [] = {
  0x02, 0x01, };

static void
ethi_add_destroy_marker (ETableHeaderItem *ethi)
{
	double x1;
	
	if (ethi->remove_item)
		gtk_object_destroy (GTK_OBJECT (ethi->remove_item));

	if (!ethi->stipple)
		ethi->stipple = gdk_bitmap_create_from_data  (NULL, gray50_bits, gray50_width, gray50_height);
	
	x1 = ethi->x1 + (double) e_table_header_col_diff (ethi->eth, 0, ethi->drag_col);
	ethi->remove_item = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (GNOME_CANVAS_ITEM (ethi)->canvas->root),
		gnome_canvas_rect_get_type (),
		"x1", x1 + 1,
		"y1", (double) ethi->y1 + 1,
		"x2", (double) x1 + e_table_header_col_diff (ethi->eth, ethi->drag_col, ethi->drag_col+1) - 2,
		"y2", (double) ethi->y1 + ethi->height - 2,
		"fill_color", "red",
		"fill_stipple", ethi->stipple,
		NULL);
}

static void
ethi_remove_destroy_marker (ETableHeaderItem *ethi)
{
	if (!ethi->remove_item)
		return;
	
	gtk_object_destroy (GTK_OBJECT (ethi->remove_item));
	ethi->remove_item = NULL;
}

static gboolean
ethi_drag_motion (GtkObject *canvas, GdkDragContext *context,
		  gint x, gint y, guint time,
		  ETableHeaderItem *ethi)
{
	/* Check if it's the correct ethi */
	if (ethi->drag_col == -1)
		return FALSE;

	gdk_drag_status (context, 0, time);
	if (GTK_WIDGET(canvas) == gtk_drag_get_source_widget(context)) {
		if ((x >= ethi->x1) && (x <= (ethi->x1 + ethi->width)) &&
		    (y >= ethi->y1) && (y <= (ethi->y1 + ethi->height))){
			int col;
			
			col = ethi_find_col_by_x (ethi, x);
			
			if (col != -1){
				ethi_remove_destroy_marker (ethi);
				ethi_add_drop_marker (ethi, col);
				gdk_drag_status (context, context->suggested_action, time);
			} else {
				ethi_remove_drop_marker (ethi);
				ethi_add_destroy_marker (ethi);
			}
		} else {
			ethi_remove_drop_marker (ethi);
			ethi_add_destroy_marker (ethi);
		}
	}

	return TRUE;
}

static void
ethi_drag_end (GtkWidget *canvas, GdkDragContext *context, ETableHeaderItem *ethi)
{
	if (ethi->drag_col == -1)
		return;

	if (canvas == gtk_drag_get_source_widget(context)) {
		if (context->action == 0) {
			ethi_request_redraw (ethi);
			e_table_header_remove(ethi->eth, ethi->drag_col);
		}
		ethi_remove_drop_marker (ethi);
		ethi_remove_destroy_marker (ethi);
		ethi->drag_col = -1;
	}
}

static gboolean
ethi_drag_drop (GtkWidget *canvas,
		GdkDragContext *context,
		gint x,
		gint y,
		guint time,
		ETableHeaderItem *ethi)
{
	gboolean successful = FALSE;

	if (ethi->drag_col == -1)
		return FALSE;

	if (GTK_WIDGET(canvas) == gtk_drag_get_source_widget(context)) {
		if ((x >= ethi->x1) && (x <= (ethi->x1 + ethi->width)) &&
		    (y >= ethi->y1) && (y <= (ethi->y1 + ethi->height))){
			int col;
			
			col = ethi_find_col_by_x (ethi, x);
			ethi_add_drop_marker(ethi, col);
			
			if (col != -1) {
				if (col != ethi->drag_col) {
					ethi_request_redraw (ethi);
					e_table_header_move(ethi->eth, ethi->drag_col, col);
				}
				successful = TRUE;
			}
		}
	}
	gtk_drag_finish(context, successful, successful, time);
	return successful;
}

static void
ethi_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, ETableHeaderItem *ethi)
{
	if (ethi->drag_col == -1)
		return;

	if (widget == gtk_drag_get_source_widget(context)) {
		ethi_remove_drop_marker (ethi);
		ethi_add_destroy_marker (ethi);
	}
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

	if (!ethi->font){
		g_warning ("Font had not been set for this ETableHeader");
		ethi_font_load (ethi, "fixed");
	}

	/*
	 * Now, configure DnD
	 */
	gtk_drag_dest_set (GTK_WIDGET (item->canvas), 0,
			   ethi_drop_types, ELEMENTS (ethi_drop_types),
			   GDK_ACTION_MOVE);

	ethi->drag_motion_id = gtk_signal_connect (
		GTK_OBJECT (item->canvas), "drag_motion",
		GTK_SIGNAL_FUNC (ethi_drag_motion), ethi);

	ethi->drag_leave_id = gtk_signal_connect (
		GTK_OBJECT (item->canvas), "drag_leave",
		GTK_SIGNAL_FUNC (ethi_drag_leave), ethi);

	ethi->drag_end_id = gtk_signal_connect (
		GTK_OBJECT (item->canvas), "drag_end",
		GTK_SIGNAL_FUNC (ethi_drag_end), ethi);

	ethi->drag_drop_id = gtk_signal_connect (
		GTK_OBJECT (item->canvas), "drag_drop",
		GTK_SIGNAL_FUNC (ethi_drag_drop), ethi);
}

static void
ethi_unrealize (GnomeCanvasItem *item)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);

	gdk_gc_unref (ethi->gc);
	ethi->gc = NULL;

	gdk_cursor_destroy (ethi->normal_cursor);
	ethi->normal_cursor = NULL;

	gtk_signal_disconnect (GTK_OBJECT (item->canvas), ethi->drag_motion_id);
	gtk_signal_disconnect (GTK_OBJECT (item->canvas), ethi->drag_end_id);
	gtk_signal_disconnect (GTK_OBJECT (item->canvas), ethi->drag_leave_id);
	gtk_signal_disconnect (GTK_OBJECT (item->canvas), ethi->drag_drop_id);

	if (ethi->stipple){
		gdk_bitmap_unref (ethi->stipple);
		ethi->stipple = NULL;
	}
	
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

	clip.x = x + PADDING / 2;
	clip.y = y + PADDING / 2;
	clip.width = width - PADDING;
	clip.height = ethi->height;
	
	gdk_gc_set_clip_rectangle (ethi->gc, &clip);

	if ( col->is_pixbuf ) {
		xtra = (clip.width - gdk_pixbuf_get_width(col->pixbuf))/2;
		
		xtra += PADDING / 2;

		gdk_pixbuf_render_to_drawable_alpha(col->pixbuf, 
						    drawable,
						    0, 0, 
						    x + xtra, y + (clip.height - gdk_pixbuf_get_height(col->pixbuf)) / 2,
						    gdk_pixbuf_get_width(col->pixbuf), gdk_pixbuf_get_height(col->pixbuf),
						    GDK_PIXBUF_ALPHA_FULL, 128,
						    GDK_RGB_DITHER_NORMAL,
						    0, 0);
	} else {
		/* Center the thing */
		xtra = (clip.width - gdk_string_measure (ethi->font, col->text))/2;
		
		/* Skip over border */
		if (xtra < 0)
			xtra = 0;

		xtra += PADDING / 2;
	
		gdk_draw_text (
			       drawable, ethi->font,
			       ethi->gc, x + xtra, y + ethi->height - ethi->font->descent - PADDING / 2,
			       col->text, strlen (col->text));
	}

	switch ( e_table_col_get_arrow(col) ) {
	case E_TABLE_COL_ARROW_NONE:
		break;
	case E_TABLE_COL_ARROW_UP:
	case E_TABLE_COL_ARROW_DOWN:
		gtk_paint_arrow   (gtk_widget_get_style(GTK_WIDGET(GNOME_CANVAS_ITEM(ethi)->canvas)),
				   drawable,
				   GTK_STATE_NORMAL,
				   GTK_SHADOW_IN,
				   &clip,
				   GTK_WIDGET(GNOME_CANVAS_ITEM(ethi)->canvas),
				   "header",
				   e_table_col_get_arrow(col) == E_TABLE_COL_ARROW_UP ? GTK_ARROW_UP : GTK_ARROW_DOWN,
				   TRUE,
				   x + PADDING / 2 + clip.width - MIN_ARROW_SIZE - 2,
				   y + (ethi->height - MIN_ARROW_SIZE) / 2,
				   MIN_ARROW_SIZE,
				   MIN_ARROW_SIZE);
		break;
	}
}

static void
ethi_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int width, int height)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);
	GnomeCanvas *canvas = item->canvas;
	GdkGC *gc;
	const int cols = e_table_header_count (ethi->eth);
	int x1, x2;
	int col;

#if 0
	printf ("My coords are: %g %g %g %g\n",
		item->x1, item->y1, item->x2, item->y2);
#endif
	x1 = x2 = ethi->x1;
	for (col = 0; col < cols; col++, x1 = x2){
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);
		int col_width;

		if (col == ethi->resize_col)
			col_width = ethi->resize_width;
		else
			col_width = ecol->width;
				
		x2 += col_width;
		
		if (x1 > (x + width))
			break;

		if (x2 < x)
			continue;
		
		gc = GTK_WIDGET (canvas)->style->bg_gc [GTK_STATE_ACTIVE];

		draw_button (ethi, ecol, drawable, gc,
			     GTK_WIDGET (canvas)->style,
			     x1 - x, ethi->y1 - y, col_width, ethi->height);
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

static gboolean
ethi_maybe_start_drag (ETableHeaderItem *ethi, GdkEventMotion *event)
{
	if (!ethi->maybe_drag)
		return FALSE;

	if (MAX (abs (ethi->click_x - event->x),
		 abs (ethi->click_y - event->y)) <= 3)
		return FALSE;

	return TRUE;
}

static void
ethi_start_drag (ETableHeaderItem *ethi, GdkEvent *event)
{
	GtkWidget *widget = GTK_WIDGET (GNOME_CANVAS_ITEM (ethi)->canvas);
	GtkTargetList *list;
	GdkDragContext *context;
	ETableCol *ecol;
	int col_width;
	GdkPixmap *pixmap;
	GdkGC *gc;

	ethi->drag_col = ethi_find_col_by_x (ethi, event->motion.x);
	if (ethi->drag_col == -1)
		return;

	list = gtk_target_list_new (ethi_drag_types, ELEMENTS (ethi_drag_types));
	context = gtk_drag_begin (widget, list, GDK_ACTION_MOVE, 1, event);

	ecol = e_table_header_get_column (ethi->eth, ethi->drag_col);
	if (ethi->drag_col == ethi->resize_col)
	  col_width = ethi->resize_width;
	else
	  col_width = ecol->width;
	pixmap = gdk_pixmap_new(widget->window, col_width, ethi->height, -1);
	gc = widget->style->bg_gc [GTK_STATE_ACTIVE];
	draw_button (ethi, ecol, pixmap, gc,
		     widget->style,
		     0, 0, col_width, ethi->height);
	gtk_drag_set_icon_pixmap        (context,
					 gdk_window_get_colormap(widget->window),
					 pixmap,
					 NULL,
					 col_width / 2,
					 ethi->height / 2);
	gdk_pixmap_unref(pixmap);

	ethi->maybe_drag = FALSE;
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

	case GDK_LEAVE_NOTIFY:
		e_cursor_set (GTK_WIDGET (canvas)->window, E_CURSOR_ARROW);
		break;
			    
	case GDK_MOTION_NOTIFY:
		convert (canvas, e->motion.x, e->motion.y, &x, &y);
		if (resizing){
			int new_width;
			
			if (ethi->resize_guide == NULL){
				/* Quick hack until I actually bind the views */
				ethi->resize_guide = GINT_TO_POINTER (1);
				gnome_canvas_item_grab (item,
							GDK_POINTER_MOTION_MASK |
							GDK_BUTTON_RELEASE_MASK,
							e_cursor_get (E_CURSOR_SIZE_X),
							e->button.time);
			}

			new_width = x - ethi->resize_start_pos;

			if (new_width <= 0)
				new_width = 1;

			if (new_width < ethi->resize_min_width)
				new_width = ethi->resize_min_width;
			ethi_request_redraw (ethi);

			ethi->resize_width = new_width;
			e_table_header_set_size (ethi->eth, ethi->resize_col, ethi->resize_width);

			ethi_request_redraw (ethi);
		} else if (ethi_maybe_start_drag (ethi, &e->motion)){
			ethi_start_drag (ethi, e);
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

			if (!ecol->resizeable)
				break;
			ethi->resize_col = col;
			ethi->resize_width = ecol->width;
			ethi->resize_start_pos = start - ecol->width;
			ethi->resize_min_width = ecol->min_width;
		} else {
			if (e->button.button == 1){
				ethi->click_x = e->button.x;
				ethi->click_y = e->button.y;
				ethi->maybe_drag = TRUE;
			}
		}
		break;

	case GDK_2BUTTON_PRESS:
		if (!resizing)
			break;

		if (e->button.button != 1)
			break;
		break;
		
	case GDK_BUTTON_RELEASE: {
		gboolean needs_ungrab = FALSE;

		if (ethi->resize_col != -1){
			needs_ungrab = (ethi->resize_guide != NULL);
			ethi_end_resize (ethi, ethi->resize_width);
		}
		if (needs_ungrab)
			gnome_canvas_item_ungrab (item, e->button.time);

		ethi->maybe_drag = FALSE;
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

	/*
	 * Create our pixmaps for DnD
	 */
	dnd_colormap = gtk_widget_get_default_colormap ();
	remove_col_pixmap = gdk_pixmap_colormap_create_from_xpm_d (
		NULL, dnd_colormap,
		&remove_col_mask, NULL, remove_col_xpm);

	add_col_pixmap = gdk_pixmap_colormap_create_from_xpm_d (
		NULL, dnd_colormap,
		&add_col_mask, NULL, add_col_xpm);
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

	ethi->drag_col = -1;
	ethi->drag_mark = -1;
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

