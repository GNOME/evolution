/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-header-item.c
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <string.h>

#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/gnome-canvas-util.h>
#include <libgnomecanvas/gnome-canvas-polygon.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>

#include "e-util/e-i18n.h"
#include "e-util/e-util-marshal.h"
#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"
#include "misc/e-canvas.h"
#include "misc/e-cursors.h"
#include "misc/e-gui-utils.h"
#include "misc/e-popup-menu.h"

#include "e-table.h"
#include "e-table-col-dnd.h"
#include "e-table-config.h"
#include "e-table-defines.h"
#include "e-table-field-chooser-dialog.h"
#include "e-table-header.h"
#include "e-table-header-utils.h"

#include "e-table-header-item.h"

#include "add-col.xpm"
#include "remove-col.xpm"
#include "arrow-up.xpm"
#include "arrow-down.xpm"

enum {
	BUTTON_PRESSED,
	LAST_SIGNAL
};

static guint ethi_signals [LAST_SIGNAL] = { 0, };

#define ARROW_DOWN_HEIGHT 16
#define ARROW_PTR          7

/* Defines the tolerance for proximity of the column division to the cursor position */
#define TOLERANCE 4

#define ETHI_RESIZING(x) ((x)->resize_col != -1)

#define PARENT_OBJECT_TYPE gnome_canvas_item_get_type ()

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))
#define d(x)

static GnomeCanvasItemClass *ethi_parent_class;

static void ethi_drop_table_header (ETableHeaderItem *ethi);

/*
 * They display the arrows for the drop location.
 */
	
static GtkWidget *arrow_up, *arrow_down;

/*
 * DnD icons
 */
static GdkColormap *dnd_colormap;
static GdkPixmap *remove_col_pixmap, *remove_col_mask;
static GdkPixmap *add_col_pixmap, *add_col_mask;

enum {
	PROP_0,
	PROP_TABLE_HEADER,
	PROP_FULL_HEADER,
	PROP_DND_CODE,
	PROP_TABLE_FONTSET,
	PROP_SORT_INFO,
	PROP_TABLE,
	PROP_TREE
};

enum {
	ET_SCROLL_UP = 1 << 0,
	ET_SCROLL_DOWN = 1 << 1,
	ET_SCROLL_LEFT = 1 << 2,
	ET_SCROLL_RIGHT = 1 << 3
};

static void scroll_off (ETableHeaderItem *ethi);
static void scroll_on (ETableHeaderItem *ethi, guint scroll_direction);

static void
ethi_dispose (GObject *object){
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (object);

	ethi_drop_table_header (ethi);

	scroll_off (ethi);

	if (ethi->dnd_code) {
		g_free (ethi->dnd_code);
		ethi->dnd_code = NULL;
	}

	if (ethi->sort_info) {
		if (ethi->sort_info_changed_id)
			g_signal_handler_disconnect (ethi->sort_info, ethi->sort_info_changed_id);
		if (ethi->group_info_changed_id)
			g_signal_handler_disconnect (ethi->sort_info, ethi->group_info_changed_id);
		g_object_unref (ethi->sort_info);
		ethi->sort_info = NULL;
	}

	if (ethi->full_header)
		g_object_unref (ethi->full_header);
	ethi->full_header = NULL;

	
	if (ethi->etfcd)
		g_object_remove_weak_pointer (G_OBJECT (ethi->etfcd), (gpointer *)&ethi->etfcd);
	

	if (ethi->config)
		g_object_unref (ethi->config);
	ethi->config = NULL;
	
	if (G_OBJECT_CLASS (ethi_parent_class)->dispose)
		(*G_OBJECT_CLASS (ethi_parent_class)->dispose) (object);
}

static int
e_table_header_item_get_height (ETableHeaderItem *ethi)
{
	ETableHeader *eth;
	int numcols, col;
	int maxheight;
	GtkStyle *style;

	g_return_val_if_fail (ethi != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_HEADER_ITEM (ethi), 0);
       
	eth = ethi->eth;
	numcols = e_table_header_count (eth);

	maxheight = 0;

	style = GTK_WIDGET (GNOME_CANVAS_ITEM (ethi)->canvas)->style;

	for (col = 0; col < numcols; col++) {
		ETableCol *ecol = e_table_header_get_column (eth, col);
		int height;

		height = e_table_header_compute_height (ecol,
							GTK_WIDGET (GNOME_CANVAS_ITEM (ethi)->canvas));

		if (height > maxheight)
			maxheight = height;
	}

	return maxheight;
}

static void
ethi_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);
	
	double   i2c [6];
	ArtPoint c1, c2, i1, i2;
	
	if (GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->update)
		(*GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->update)(item, affine, clip_path, flags);

	if (ethi->sort_info)
		ethi->group_indent_width = e_table_sort_info_grouping_get_count(ethi->sort_info) * GROUP_INDENT;
	else
		ethi->group_indent_width = 0;

	ethi->width = e_table_header_total_width (ethi->eth) + ethi->group_indent_width;

	i1.x = i1.y = 0;
	i2.x = ethi->width;
	i2.y = ethi->height;

	gnome_canvas_item_i2c_affine (item, i2c);
	art_affine_point (&c1, &i1, i2c);
	art_affine_point (&c2, &i2, i2c);

	if (item->x1 != c1.x ||
	    item->y1 != c1.y ||
	    item->x2 != c2.x ||
	    item->y2 != c2.y)
		{
			gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
			item->x1 = c1.x;
			item->y1 = c1.y;
			item->x2 = c2.x;
			item->y2 = c2.y;
#ifndef NO_WARNINGS
#warning FOO BAA
#endif
#if 0
			gnome_canvas_group_child_bounds (GNOME_CANVAS_GROUP (item->parent), item);
#endif
		}
	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
}

static void
ethi_font_set (ETableHeaderItem *ethi, GdkFont *font)
{
	if (ethi->font)
		gdk_font_unref (ethi->font);

	ethi->font = font;
	gdk_font_ref (font);
	
	ethi->height = e_table_header_item_get_height (ethi);
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(ethi));
}

static void
ethi_font_load (ETableHeaderItem *ethi, const char *fontname)
{
	GdkFont *font = NULL;

	if (fontname != NULL)
		font = gdk_fontset_load (fontname);

	if (font == NULL) {
		font = gtk_style_get_font (GTK_WIDGET (GNOME_CANVAS_ITEM (ethi)->canvas)->style);
		gdk_font_ref (font);
	}
	
	ethi_font_set (ethi, font);
	gdk_font_unref (font);
}

static void
ethi_drop_table_header (ETableHeaderItem *ethi)
{
	GObject *header;
	
	if (!ethi->eth)
		return;

	header = G_OBJECT (ethi->eth);
	g_signal_handler_disconnect (header, ethi->structure_change_id);
	g_signal_handler_disconnect (header, ethi->dimension_change_id);

	g_object_unref (header);
	ethi->eth = NULL;
	ethi->width = 0;
}

static void 
structure_changed (ETableHeader *header, ETableHeaderItem *ethi)
{
	gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(ethi));
}

static void
dimension_changed (ETableHeader *header, int col, ETableHeaderItem *ethi)
{
	gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(ethi));
}

static void
ethi_add_table_header (ETableHeaderItem *ethi, ETableHeader *header)
{
	ethi->eth = header;
	g_object_ref (ethi->eth);

	ethi->height = e_table_header_item_get_height (ethi);

	ethi->structure_change_id = g_signal_connect (
		header, "structure_change",
		G_CALLBACK (structure_changed), ethi);
	ethi->dimension_change_id = g_signal_connect (
		header, "dimension_change",
		G_CALLBACK (dimension_changed), ethi);
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(ethi));
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(ethi));
}

static void
ethi_sort_info_changed (ETableSortInfo *sort_info, ETableHeaderItem *ethi)
{
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(ethi));
}

static void
ethi_set_property (GObject *object,
		   guint prop_id,
		   const GValue *value,
		   GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	ETableHeaderItem *ethi;

	item = GNOME_CANVAS_ITEM (object);
	ethi = E_TABLE_HEADER_ITEM (object);

	switch (prop_id){
	case PROP_TABLE_HEADER:
		ethi_drop_table_header (ethi);
		ethi_add_table_header (ethi, E_TABLE_HEADER(g_value_get_object (value)));
		break;

	case PROP_FULL_HEADER:
		if (ethi->full_header)
			g_object_unref(ethi->full_header);
		ethi->full_header = E_TABLE_HEADER(g_value_get_object (value));
		if (ethi->full_header)
			g_object_ref(ethi->full_header);
		break;

	case PROP_DND_CODE:
		g_free(ethi->dnd_code);
		ethi->dnd_code = g_strdup (g_value_get_string (value));
		break;

	case PROP_TABLE_FONTSET:
		ethi_font_load (ethi, g_value_get_string (value));
		break;

	case PROP_SORT_INFO:
		if (ethi->sort_info){
			if (ethi->sort_info_changed_id)
				g_signal_handler_disconnect (
					ethi->sort_info,
					ethi->sort_info_changed_id);

			if (ethi->group_info_changed_id)
				g_signal_handler_disconnect (
					ethi->sort_info,
					ethi->group_info_changed_id);
			g_object_unref (ethi->sort_info);
		}
		ethi->sort_info = g_value_get_object (value);
		g_object_ref (ethi->sort_info);
		ethi->sort_info_changed_id =
			g_signal_connect (
				ethi->sort_info, "sort_info_changed",
				G_CALLBACK (ethi_sort_info_changed), ethi);
		ethi->group_info_changed_id =
			g_signal_connect (
				ethi->sort_info, "group_info_changed",
				G_CALLBACK(ethi_sort_info_changed), ethi);
		break;
	case PROP_TABLE:
		if (g_value_get_object (value))
			ethi->table = E_TABLE(g_value_get_object (value));
		else
			ethi->table = NULL;
		break;
	case PROP_TREE:
		if (g_value_get_object (value))
			ethi->tree = E_TREE(g_value_get_object (value));
		else
			ethi->tree = NULL;
		break;
	}
	gnome_canvas_item_request_update(item);
}

static void
ethi_get_property (GObject *object,
		   guint prop_id,
		   GValue *value,
		   GParamSpec *pspec)
{
	ETableHeaderItem *ethi;

	ethi = E_TABLE_HEADER_ITEM (object);

	switch (prop_id){
	case PROP_FULL_HEADER:
		g_value_set_object (value, ethi->full_header);
		break;
	case PROP_DND_CODE:
		g_value_set_string (value, g_strdup (ethi->dnd_code));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static int
ethi_find_col_by_x (ETableHeaderItem *ethi, int x)
{
	const int cols = e_table_header_count (ethi->eth);
	int x1 = 0;
	int col;
	
	d(g_print ("%s:%d: x = %d, x1 = %d\n", __FUNCTION__, __LINE__, x, x1));

	x1 += ethi->group_indent_width;
	
	if (x < x1) {
		d(g_print ("%s:%d: Returning 0\n", __FUNCTION__, __LINE__));
		return 0;
	}

	for (col = 0; col < cols; col++){
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);

		if ((x >= x1) && (x <= x1 + ecol->width)) {
			d(g_print ("%s:%d: Returning %d\n", __FUNCTION__, __LINE__, col));
			return col;
		}

		x1 += ecol->width;
	}
	d(g_print ("%s:%d: Returning %d\n", __FUNCTION__, __LINE__, cols - 1));
	return cols - 1;
}

static int
ethi_find_col_by_x_nearest (ETableHeaderItem *ethi, int x)
{
	const int cols = e_table_header_count (ethi->eth);
	int x1 = 0;
	int col;

	x1 += ethi->group_indent_width;
	
	if (x < x1)
		return 0;

	for (col = 0; col < cols; col++){
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);

		x1 += (ecol->width / 2);

		if (x <= x1)
			return col;

		x1 += (ecol->width + 1) / 2;
	}
	return col;
}

static void
ethi_remove_drop_marker (ETableHeaderItem *ethi)
{
	if (ethi->drag_mark == -1)
		return;

	gtk_widget_hide (arrow_up);
	gtk_widget_hide (arrow_down);
	
	ethi->drag_mark = -1;
}

static GtkWidget *
make_shaped_window_from_xpm (const char **xpm)
{
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	GtkWidget *win, *pix;
	
	pixbuf = gdk_pixbuf_new_from_xpm_data (xpm);
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 128);
	gdk_pixbuf_unref (pixbuf);

	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
	win = gtk_window_new (GTK_WINDOW_POPUP);

	pix = gtk_image_new_from_pixmap (pixmap, bitmap);
	gtk_widget_realize (win);
	gtk_container_add (GTK_CONTAINER (win), pix);
	gtk_widget_shape_combine_mask (win, bitmap, 0, 0);
	gtk_widget_pop_colormap ();
	
	gdk_pixmap_unref (pixmap);
	gdk_bitmap_unref (bitmap);
	
	return win;
}

static void
ethi_add_drop_marker (ETableHeaderItem *ethi, int col, gboolean recreate)
{
	int rx, ry;
	int x;
	
	if (!recreate && ethi->drag_mark == col)
		return;

	ethi->drag_mark = col;

	x = e_table_header_col_diff (ethi->eth, 0, col);
	if (col > 0)
		x += ethi->group_indent_width;
	
	if (!arrow_up){
		arrow_up   = make_shaped_window_from_xpm (arrow_up_xpm);
		arrow_down = make_shaped_window_from_xpm (arrow_down_xpm);
	}

	gdk_window_get_origin (
		GTK_WIDGET (GNOME_CANVAS_ITEM (ethi)->canvas)->window,
		&rx, &ry);

	rx -= gtk_layout_get_hadjustment (GTK_LAYOUT (GNOME_CANVAS_ITEM (ethi)->canvas))->value;
	ry -= gtk_layout_get_vadjustment (GTK_LAYOUT (GNOME_CANVAS_ITEM (ethi)->canvas))->value;

	gtk_widget_set_uposition (arrow_down, rx + x - ARROW_PTR, ry - ARROW_DOWN_HEIGHT);
	gtk_widget_show_all (arrow_down);

	gtk_widget_set_uposition (arrow_up, rx + x - ARROW_PTR, ry + ethi->height);
	gtk_widget_show_all (arrow_up);
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
		ethi->stipple = gdk_bitmap_create_from_data  (
			NULL, gray50_bits, gray50_width, gray50_height);
	
	x1 = (double) e_table_header_col_diff (ethi->eth, 0, ethi->drag_col);
	if (ethi->drag_col > 0)
		x1 += ethi->group_indent_width;
	
	ethi->remove_item = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (GNOME_CANVAS_ITEM (ethi)->canvas->root),
		gnome_canvas_rect_get_type (),
		"x1", x1 + 1,
		"y1", (double) 1,
		"x2", (double) x1 + e_table_header_col_diff (
			ethi->eth, ethi->drag_col, ethi->drag_col+1) - 2,

		"y2", (double) ethi->height - 2,
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

#if 0
static gboolean
moved (ETableHeaderItem *ethi, guint col, guint model_col)
{
	if (col == -1)
		return TRUE;
	ecol = e_table_header_get_column (ethi->eth, col);
	if (ecol->col_idx == model_col)
		return FALSE;
	if (col > 0) {
		ecol = e_table_header_get_column (ethi->eth, col - 1);
		if (ecol->col_idx == model_col)
			return FALSE;
	}
	return TRUE;
}
#endif

static void
do_drag_motion(ETableHeaderItem *ethi,
	       GdkDragContext *context,
	       gint x,
	       gint y,
	       guint time,
	       gboolean recreate)
{
	d(g_print("In do_drag_motion\n"));
	d(g_print("x = %d, y = %d, ethi->width = %d, ethi->height = %d\n", x, y, ethi->width, ethi->height));

	if ((x >= 0) && (x <= (ethi->width)) &&
	    (y >= 0) && (y <= (ethi->height))){
		int col;
		d(g_print("In header\n"));
		
		col = ethi_find_col_by_x_nearest (ethi, x);

		if (ethi->drag_col != -1 && (col == ethi->drag_col || col == ethi->drag_col + 1)) {
			if (ethi->drag_col != -1)
				ethi_remove_destroy_marker (ethi);
			
			ethi_remove_drop_marker (ethi);
			gdk_drag_status (context, context->suggested_action, time);
		} 
		else if (col != -1){
			if (ethi->drag_col != -1)
				ethi_remove_destroy_marker (ethi);

			ethi_add_drop_marker (ethi, col, recreate);
			gdk_drag_status (context, context->suggested_action, time);
		} else {
			ethi_remove_drop_marker (ethi);
			if (ethi->drag_col != -1)
				ethi_add_destroy_marker (ethi);
		}
	} else {
		ethi_remove_drop_marker (ethi);
		if (ethi->drag_col != -1)
			ethi_add_destroy_marker (ethi);
	}
}

static gboolean
scroll_timeout (gpointer data)
{
	ETableHeaderItem *ethi = data;
	int dx = 0;
	GtkAdjustment *h, *v;
	double value;

	if (ethi->scroll_direction & ET_SCROLL_RIGHT)
		dx += 20;
	if (ethi->scroll_direction & ET_SCROLL_LEFT)
		dx -= 20;

	h = GTK_LAYOUT(GNOME_CANVAS_ITEM (ethi)->canvas)->hadjustment;
	v = GTK_LAYOUT(GNOME_CANVAS_ITEM (ethi)->canvas)->vadjustment;

	value = h->value;

	gtk_adjustment_set_value(h, CLAMP(h->value + dx, h->lower, h->upper - h->page_size));

	if (h->value != value)
		do_drag_motion(ethi,
			       ethi->last_drop_context,
			       ethi->last_drop_x + h->value,
			       ethi->last_drop_y + v->value,
			       ethi->last_drop_time,
			       TRUE);

	return TRUE;
}

static void
scroll_on (ETableHeaderItem *ethi, guint scroll_direction)
{
	if (ethi->scroll_idle_id == 0 || scroll_direction != ethi->scroll_direction) {
		if (ethi->scroll_idle_id != 0)
			g_source_remove (ethi->scroll_idle_id);
		ethi->scroll_direction = scroll_direction;
		ethi->scroll_idle_id = g_timeout_add (100, scroll_timeout, ethi);
	}
}

static void
scroll_off (ETableHeaderItem *ethi)
{
	if (ethi->scroll_idle_id) {
		g_source_remove (ethi->scroll_idle_id);
		ethi->scroll_idle_id = 0;
	}
}

static void
context_destroyed (gpointer data)
{
	ETableHeaderItem *ethi = data;

	ethi->last_drop_x       = 0;
	ethi->last_drop_y       = 0;
	ethi->last_drop_time    = 0;
	ethi->last_drop_context = NULL;
	scroll_off (ethi);

	g_object_unref (ethi);
}

static void
context_connect (ETableHeaderItem *ethi, GdkDragContext *context)
{
	if (g_dataset_get_data (context, "e-table-header-item") == NULL) {
		g_object_ref (ethi);
		g_dataset_set_data_full (context, "e-table-header-item", ethi, context_destroyed);
	}
}

static gboolean
ethi_drag_motion (GtkWidget *widget, GdkDragContext *context,
		  gint x, gint y, guint time,
		  ETableHeaderItem *ethi)
{
	char *droptype, *headertype;
	guint direction = 0;

	gdk_drag_status (context, 0, time);

	droptype = gdk_atom_name (GDK_POINTER_TO_ATOM (context->targets->data));
	headertype = g_strdup_printf ("%s-%s", TARGET_ETABLE_COL_TYPE,
				      ethi->dnd_code);

	if (strcmp (droptype, headertype) != 0) {
		g_free (headertype);
		return FALSE;
	}

	g_free (headertype);

	d(g_print ("y = %d, widget->allocation.y = %d, GTK_LAYOUT (widget)->vadjustment->value = %f\n", y, widget->allocation.y, GTK_LAYOUT (widget)->vadjustment->value));

	if (x < 20)
		direction |= ET_SCROLL_LEFT;
	if (x > widget->allocation.width - 20)
		direction |= ET_SCROLL_RIGHT;

	ethi->last_drop_x = x;
	ethi->last_drop_y = y;
	ethi->last_drop_time = time;
	ethi->last_drop_context = context;
	context_connect (ethi, context);

	do_drag_motion (ethi,
			context,
			x + GTK_LAYOUT(widget)->hadjustment->value,
			y + GTK_LAYOUT(widget)->vadjustment->value,
			time,
			FALSE);

	if (direction != 0)
		scroll_on (ethi, direction);
	else
		scroll_off (ethi);

	return TRUE;
}

static void
ethi_drag_end (GtkWidget *canvas, GdkDragContext *context, ETableHeaderItem *ethi)
{
	if (context->action == 0) {
		e_table_header_remove (ethi->eth, ethi->drag_col);
		gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(ethi));
	}
	ethi_remove_drop_marker (ethi);
	ethi_remove_destroy_marker (ethi);
	ethi->drag_col = -1;
	scroll_off (ethi);
}

static void
ethi_drag_data_received (GtkWidget *canvas,
			 GdkDragContext *drag_context,
			 gint x,
			 gint y,
			 GtkSelectionData *data,
			 guint info,
			 guint time,
			 ETableHeaderItem *ethi)
{
	int found = FALSE;
	int count;
	int column;
	int drop_col;
	int i;

	if (data->data) {
		count = e_table_header_count(ethi->eth);
		column = atoi(data->data);
		drop_col = ethi->drop_col;
		ethi->drop_col = -1;

		if (column >= 0) {
			for (i = 0; i < count; i++) {
				ETableCol *ecol = e_table_header_get_column (ethi->eth, i);
				if (ecol->col_idx == column) {
					e_table_header_move(ethi->eth, i, drop_col);
					found = TRUE;
					break;
				}
			}
			if (!found) {
				count = e_table_header_count(ethi->full_header);
				for (i = 0; i < count; i++) {
					ETableCol *ecol = e_table_header_get_column (ethi->full_header, i);
					if (ecol->col_idx == column) {
						e_table_header_add_column (ethi->eth, ecol, drop_col);
						break;
					}
				}
			}
		}
	}
	ethi_remove_drop_marker (ethi);
	gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(ethi));
}

static void
ethi_drag_data_get (GtkWidget *canvas,
		    GdkDragContext     *context,
		    GtkSelectionData   *selection_data,
		    guint               info,
		    guint               time,
		    ETableHeaderItem *ethi)
{
	if (ethi->drag_col != -1) {
		ETableCol *ecol = e_table_header_get_column (ethi->eth, ethi->drag_col);
		
		gchar *string = g_strdup_printf("%d", ecol->col_idx);
		gtk_selection_data_set(selection_data,
				       GDK_SELECTION_TYPE_STRING,
				       sizeof(string[0]),
				       string,
				       strlen(string));
		g_free(string);
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

	if ((x >= 0) && (x <= (ethi->width)) &&
	    (y >= 0) && (y <= (ethi->height))){
		int col;
		
		col = ethi_find_col_by_x_nearest (ethi, x);
		
		ethi_add_drop_marker (ethi, col, FALSE);

		ethi->drop_col = col;
		
		if (col != -1) {
			char *target = g_strdup_printf ("%s-%s", TARGET_ETABLE_COL_TYPE, ethi->dnd_code);
			d(g_print ("ethi -  %s\n", target));
			gtk_drag_get_data (canvas, context, gdk_atom_intern(target, FALSE), time);
			g_free (target);
		}
	}
	gtk_drag_finish (context, successful, successful, time);
	scroll_off (ethi);
	return successful;
}

static void
ethi_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, ETableHeaderItem *ethi)
{
	ethi_remove_drop_marker (ethi);
	if (ethi->drag_col != -1)
		ethi_add_destroy_marker (ethi);
}

static void
ethi_realize (GnomeCanvasItem *item)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);
	GdkWindow *window;
	GtkTargetEntry  ethi_drop_types [] = {
		{ TARGET_ETABLE_COL_TYPE, 0, TARGET_ETABLE_COL_HEADER },
	};

	
	if (GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)-> realize)
		(*GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->realize)(item);

	window = GTK_WIDGET (item->canvas)->window;

	if (!ethi->font)
		ethi_font_set (ethi, gtk_style_get_font (GTK_WIDGET (item->canvas)->style));

	/*
	 * Now, configure DnD
	 */
	ethi_drop_types[0].target = g_strdup_printf("%s-%s", ethi_drop_types[0].target, ethi->dnd_code);
	gtk_drag_dest_set (GTK_WIDGET (item->canvas), 0,
			   ethi_drop_types, ELEMENTS (ethi_drop_types),
			   GDK_ACTION_MOVE);
  	g_free(ethi_drop_types[0].target); 

	/* Drop signals */
	ethi->drag_motion_id = g_signal_connect (item->canvas, "drag_motion",
						 G_CALLBACK (ethi_drag_motion), ethi);
	ethi->drag_leave_id = g_signal_connect (item->canvas, "drag_leave",
						G_CALLBACK (ethi_drag_leave), ethi);
	ethi->drag_drop_id = g_signal_connect (item->canvas, "drag_drop",
					       G_CALLBACK (ethi_drag_drop), ethi);
	ethi->drag_data_received_id = g_signal_connect (item->canvas, "drag_data_received",
							G_CALLBACK (ethi_drag_data_received), ethi);

	/* Drag signals */
	ethi->drag_end_id = g_signal_connect (item->canvas, "drag_end",
					      G_CALLBACK (ethi_drag_end), ethi);
	ethi->drag_data_get_id = g_signal_connect (item->canvas, "drag_data_get",
						   G_CALLBACK (ethi_drag_data_get), ethi);

}

static void
ethi_unrealize (GnomeCanvasItem *item)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);

	gdk_font_unref (ethi->font);

	g_signal_handler_disconnect (item->canvas, ethi->drag_motion_id);
	g_signal_handler_disconnect (item->canvas, ethi->drag_leave_id);
	g_signal_handler_disconnect (item->canvas, ethi->drag_drop_id);
	g_signal_handler_disconnect (item->canvas, ethi->drag_data_received_id);

	g_signal_handler_disconnect (item->canvas, ethi->drag_end_id);
	g_signal_handler_disconnect (item->canvas, ethi->drag_data_get_id);

	gtk_drag_dest_unset (GTK_WIDGET (item->canvas));

	if (ethi->stipple){
		gdk_bitmap_unref (ethi->stipple);
		ethi->stipple = NULL;
	}
	
	if (GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->unrealize)
		(*GNOME_CANVAS_ITEM_CLASS (ethi_parent_class)->unrealize)(item);
}

static void
ethi_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int width, int height)
{
	ETableHeaderItem *ethi = E_TABLE_HEADER_ITEM (item);
	GnomeCanvas *canvas = item->canvas;
	const int cols = e_table_header_count (ethi->eth);
	int x1, x2;
	int col;
	GHashTable *arrows = g_hash_table_new (NULL, NULL);


	if (ethi->sort_info) {
		int length = e_table_sort_info_grouping_get_count(ethi->sort_info);
		int i;
		for (i = 0; i < length; i++) {
			ETableSortColumn column = e_table_sort_info_grouping_get_nth(ethi->sort_info, i);
			g_hash_table_insert (arrows, 
					     GINT_TO_POINTER (column.column),
					     GINT_TO_POINTER (column.ascending ?
							      E_TABLE_COL_ARROW_DOWN : 
							      E_TABLE_COL_ARROW_UP));
		}
		length = e_table_sort_info_sorting_get_count(ethi->sort_info);
		for (i = 0; i < length; i++) {
			ETableSortColumn column = e_table_sort_info_sorting_get_nth(ethi->sort_info, i);
			g_hash_table_insert (arrows, 
					     GINT_TO_POINTER (column.column),
					     GINT_TO_POINTER (column.ascending ?
							      E_TABLE_COL_ARROW_DOWN : 
							      E_TABLE_COL_ARROW_UP));
		}
	}

	ethi->width = e_table_header_total_width (ethi->eth) + ethi->group_indent_width;
	x1 = x2 = 0;
	x2 += ethi->group_indent_width;
	for (col = 0; col < cols; col++, x1 = x2){
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);
		int col_width;

		col_width = ecol->width;
				
		x2 += col_width;
		
		if (x1 > (x + width))
			break;

		if (x2 < x)
			continue;

		if (x2 <= x1)
			continue;

		e_table_header_draw_button (drawable, ecol,
					    GTK_WIDGET (canvas)->style,
					    GTK_WIDGET_STATE (canvas),
					    GTK_WIDGET (canvas),
					    x1 - x, -y,
					    width, height,
					    x2 - x1, ethi->height,
					    (ETableColArrow) g_hash_table_lookup (
						    arrows, GINT_TO_POINTER (ecol->col_idx)));
	}

	g_hash_table_destroy (arrows);
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

		if (col == 0)
			total += ethi->group_indent_width;
		
		total += ecol->width;

		if ((total - TOLERANCE < pos)&& (pos < total + TOLERANCE)){
			if (return_col)
				*return_col = col;
			if (the_total)
				*the_total = total;

			return TRUE;
		}
		if (return_col)
			*return_col = col;

		if (total > pos + TOLERANCE)
			return FALSE;
	}

	return FALSE;
}

#define convert(c,sx,sy,x,y) gnome_canvas_w2c (c,sx,sy,x,y)

static void
set_cursor (ETableHeaderItem *ethi, int pos)
{
	int col;
	GtkWidget *canvas = GTK_WIDGET (GNOME_CANVAS_ITEM (ethi)->canvas);
	gboolean resizable = FALSE;
		
	/* We might be invoked before we are realized */
	if (!canvas->window)
		return;

	if (is_pointer_on_division (ethi, pos, NULL, &col)) {
		int last_col = ethi->eth->col_count - 1;
		ETableCol *ecol = e_table_header_get_column (ethi->eth, col);

		/* Last column is not resizable */
		if (ecol->resizable && col != last_col) {
			int c = col + 1;

			/* Column is not resizable if all columns after it
			   are also not resizable */
			for (; c <= last_col; c++){
				ETableCol *ecol2;

				ecol2 = e_table_header_get_column (ethi->eth, c);
				if (ecol2->resizable) {
					resizable = TRUE;
					break;
				}
			}
		}
	}
	
	if (resizable)
		e_cursor_set (canvas->window, E_CURSOR_SIZE_X);
	else
		gdk_window_set_cursor (canvas->window, NULL);
	/*		e_cursor_set (canvas->window, E_CURSOR_ARROW);*/
}

static void
ethi_end_resize (ETableHeaderItem *ethi)
{
	ethi->resize_col = -1;
	ethi->resize_guide = GINT_TO_POINTER (0);

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(ethi));
}

static gboolean
ethi_maybe_start_drag (ETableHeaderItem *ethi, GdkEventMotion *event)
{
	if (!ethi->maybe_drag)
		return FALSE;

	if (ethi->eth->col_count < 2) {
		ethi->maybe_drag = FALSE;
		return FALSE;
	}
		
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
	int group_indent = 0;
	GHashTable *arrows = g_hash_table_new (NULL, NULL);

	GtkTargetEntry  ethi_drag_types [] = {
		{ TARGET_ETABLE_COL_TYPE, 0, TARGET_ETABLE_COL_HEADER },
	};

	ethi->drag_col = ethi_find_col_by_x (ethi, event->motion.x);

	if (ethi->drag_col == -1)
		return;

	if (ethi->sort_info) {
		int length = e_table_sort_info_grouping_get_count(ethi->sort_info);
		int i;
		for (i = 0; i < length; i++) {
			ETableSortColumn column =
				e_table_sort_info_grouping_get_nth(
					ethi->sort_info, i);
			group_indent ++;
			g_hash_table_insert (
				arrows, 
				GINT_TO_POINTER (column.column),
				GINT_TO_POINTER (column.ascending ?
						 E_TABLE_COL_ARROW_DOWN : 
						 E_TABLE_COL_ARROW_UP));
		}
		length = e_table_sort_info_sorting_get_count(ethi->sort_info);
		for (i = 0; i < length; i++) {
			ETableSortColumn column =
				e_table_sort_info_sorting_get_nth (
					ethi->sort_info, i);

			g_hash_table_insert (
				arrows, 
				GINT_TO_POINTER (column.column),
				GINT_TO_POINTER (column.ascending ?
						 E_TABLE_COL_ARROW_DOWN : 
						 E_TABLE_COL_ARROW_UP));
		}
	}

	ethi_drag_types[0].target = g_strdup_printf(
		"%s-%s", ethi_drag_types[0].target, ethi->dnd_code);
	list = gtk_target_list_new (
		ethi_drag_types, ELEMENTS (ethi_drag_types));
	context = gtk_drag_begin (widget, list, GDK_ACTION_MOVE, 1, event);
	g_free(ethi_drag_types[0].target);

	ecol = e_table_header_get_column (ethi->eth, ethi->drag_col);
	col_width = ecol->width;
	pixmap = gdk_pixmap_new (widget->window, col_width, ethi->height, -1);

	e_table_header_draw_button (
		pixmap, ecol,
		widget->style,
		GTK_WIDGET_STATE (widget),
		widget,
		0, 0,
		col_width, ethi->height,
		col_width, ethi->height,
		(ETableColArrow) g_hash_table_lookup (
			arrows, GINT_TO_POINTER (ecol->col_idx)));
	gtk_drag_set_icon_pixmap (
		context,
		gdk_window_get_colormap (widget->window),
		pixmap,
		NULL,
		col_width / 2,
		ethi->height / 2);
	gdk_pixmap_unref (pixmap);

	ethi->maybe_drag = FALSE;
	g_hash_table_destroy (arrows);
}

typedef struct {
	ETableHeaderItem *ethi;
	int col;
} EthiHeaderInfo;

static void
ethi_popup_sort_ascending(GtkWidget *widget, EthiHeaderInfo *info)
{
	ETableCol *col;
	int model_col;
	int length;
	int i;
	int found = FALSE;
	ETableHeaderItem *ethi = info->ethi;

	col = e_table_header_get_column (ethi->eth, info->col);
	model_col = col->col_idx;

	length = e_table_sort_info_grouping_get_count(ethi->sort_info);
	for (i = 0; i < length; i++) {
		ETableSortColumn column = e_table_sort_info_grouping_get_nth (
			ethi->sort_info, i);

		if (model_col == column.column){
			column.ascending = 1;
			e_table_sort_info_grouping_set_nth (
				ethi->sort_info, i, column);
			found = 1;
			break;
		}
	}
	if (!found) {
		length = e_table_sort_info_sorting_get_count (
			ethi->sort_info);
		for (i = 0; i < length; i++) {
			ETableSortColumn column =
				e_table_sort_info_sorting_get_nth(
					ethi->sort_info, i);
			if (model_col == column.column){
				column.ascending = 1;
				e_table_sort_info_sorting_set_nth (
					ethi->sort_info, i, column);
				found = 1;
				break;
			}
		}
	}
	if (!found) {
		ETableSortColumn column;
		column.column = model_col;
		column.ascending =  1;
		length = e_table_sort_info_sorting_get_count(ethi->sort_info);
		if (length == 0)
			length++;
		e_table_sort_info_sorting_set_nth(ethi->sort_info, length - 1, column);
	}
}

static void
ethi_popup_sort_descending(GtkWidget *widget, EthiHeaderInfo *info)
{
	ETableCol *col;
	int model_col;
	int length;
	int i;
	int found = FALSE;
	ETableHeaderItem *ethi = info->ethi;

	col = e_table_header_get_column (ethi->eth, info->col);
	model_col = col->col_idx;

	length = e_table_sort_info_grouping_get_count(ethi->sort_info);
	for (i = 0; i < length; i++) {
		ETableSortColumn column = e_table_sort_info_grouping_get_nth(
			ethi->sort_info, i);
		if (model_col == column.column){
			column.ascending = 0;
			e_table_sort_info_grouping_set_nth(
				ethi->sort_info, i, column);
			found = 1;
			break;
		}
	}
	if (!found) {
		length = e_table_sort_info_sorting_get_count (ethi->sort_info);
		for (i = 0; i < length; i++) {
			ETableSortColumn column =
				e_table_sort_info_sorting_get_nth(
					ethi->sort_info, i);

			if (model_col == column.column){
				column.ascending = 0;
				e_table_sort_info_sorting_set_nth (
					ethi->sort_info, i, column);
				found = 1;
				break;
			}
		}
	}
	if (!found) {
		ETableSortColumn column;
		column.column = model_col;
		column.ascending = 0;
		length = e_table_sort_info_sorting_get_count (ethi->sort_info);
		if (length == 0)
			length++;
		e_table_sort_info_sorting_set_nth (
			ethi->sort_info, length - 1, column);
	}
}

static void
ethi_popup_unsort(GtkWidget *widget, EthiHeaderInfo *info)
{
	ETableHeaderItem *ethi = info->ethi;

	e_table_sort_info_grouping_truncate(ethi->sort_info, 0);
	e_table_sort_info_sorting_truncate(ethi->sort_info, 0);
}

static void
ethi_popup_group_field(GtkWidget *widget, EthiHeaderInfo *info)
{
	ETableCol *col;
	int model_col;
	ETableHeaderItem *ethi = info->ethi;
	ETableSortColumn column;

	col = e_table_header_get_column (ethi->eth, info->col);
	model_col = col->col_idx;

	column.column = model_col;
	column.ascending = 1;
	e_table_sort_info_grouping_set_nth(ethi->sort_info, 0, column);
	e_table_sort_info_grouping_truncate(ethi->sort_info, 1);
}

static void
ethi_popup_group_box(GtkWidget *widget, EthiHeaderInfo *info)
{
}

static void
ethi_popup_remove_column(GtkWidget *widget, EthiHeaderInfo *info)
{
	e_table_header_remove(info->ethi->eth, info->col);
}

static void
ethi_popup_field_chooser(GtkWidget *widget, EthiHeaderInfo *info)
{
	if (info->ethi->etfcd) {
		gtk_window_present (GTK_WINDOW (info->ethi->etfcd));
		
		return;
	}
	
	info->ethi->etfcd = e_table_field_chooser_dialog_new();	
	g_object_add_weak_pointer (G_OBJECT (info->ethi->etfcd), (gpointer *)&info->ethi->etfcd);
	
	g_object_set(info->ethi->etfcd,
		     "full_header", info->ethi->full_header,
		     "header", info->ethi->eth,
		     "dnd_code", info->ethi->dnd_code,
		     NULL);

	gtk_widget_show(info->ethi->etfcd);
}

static void
ethi_popup_alignment(GtkWidget *widget, EthiHeaderInfo *info)
{
}

static void
ethi_popup_best_fit(GtkWidget *widget, EthiHeaderInfo *info)
{
	ETableHeaderItem *ethi = info->ethi;
	int width;

	g_signal_emit_by_name (ethi->eth,
			       "request_width",
			       info->col, &width);
	/* Add 10 to stop it from "..."ing */
	e_table_header_set_size (ethi->eth, info->col, width + 10);
	
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(ethi));

}

static void
ethi_popup_format_columns(GtkWidget *widget, EthiHeaderInfo *info)
{
}

static void
config_destroyed (gpointer data, GObject *where_object_was)
{
	ETableHeaderItem *ethi = data;
	ethi->config = NULL;
}

static void
apply_changes (ETableConfig *config, ETableHeaderItem *ethi)
{
	char *state = e_table_state_save_to_string (config->state);

	if (ethi->table)
		e_table_set_state (ethi->table, state);
	if (ethi->tree)
		e_tree_set_state (ethi->tree, state);
	g_free (state);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (config->dialog_toplevel),
					   GTK_RESPONSE_APPLY, FALSE);
}

static void
ethi_popup_customize_view(GtkWidget *widget, EthiHeaderInfo *info)
{
	ETableHeaderItem *ethi = info->ethi;
	ETableState *state;
	ETableSpecification *spec;

	if (ethi->config)
		e_table_config_raise (E_TABLE_CONFIG (ethi->config));
	else {
		if (ethi->table) {
			state = e_table_get_state_object(ethi->table);
			spec = ethi->table->spec;
		} else if (ethi->tree) {
			state = e_tree_get_state_object(ethi->tree);
			spec = e_tree_get_spec (ethi->tree);
		} else
			return;

		ethi->config = e_table_config_new (
				_("Customize Current View"),
				spec, state, GTK_WINDOW (gtk_widget_get_toplevel (widget)));
		g_object_weak_ref (G_OBJECT (ethi->config),
				   config_destroyed, ethi);
		g_signal_connect (
			ethi->config, "changed",
			G_CALLBACK (apply_changes), ethi);
	}
}

static void
free_popup_info (GtkWidget *w, EthiHeaderInfo *info)
{
	g_free (info);
}

/* Bit 1 is always disabled. */
/* Bit 2 is disabled if not "sortable". */
/* Bit 4 is disabled if we don't have a pointer to our table object. */
static EPopupMenu ethi_context_menu [] = {
	E_POPUP_ITEM (N_("Sort Ascending"),            G_CALLBACK(ethi_popup_sort_ascending),  2),
	E_POPUP_ITEM (N_("Sort Descending"),           G_CALLBACK(ethi_popup_sort_descending), 2),
	E_POPUP_ITEM (N_("Unsort"),                    G_CALLBACK(ethi_popup_unsort),          0),
	E_POPUP_SEPARATOR,
	E_POPUP_ITEM (N_("Group By This Field"),       G_CALLBACK(ethi_popup_group_field),     16),
	E_POPUP_ITEM (N_("Group By Box"),              G_CALLBACK(ethi_popup_group_box),       128),
	E_POPUP_SEPARATOR,
	E_POPUP_ITEM (N_("Remove This Column"),        G_CALLBACK(ethi_popup_remove_column),   8),
	E_POPUP_ITEM (N_("Add a Column..."),           G_CALLBACK(ethi_popup_field_chooser),   0),
	E_POPUP_SEPARATOR,
	E_POPUP_ITEM (N_("Alignment"),                 G_CALLBACK(ethi_popup_alignment),       128),
	E_POPUP_ITEM (N_("Best Fit"),                  G_CALLBACK(ethi_popup_best_fit),        2),
	E_POPUP_ITEM (N_("Format Columns..."),         G_CALLBACK(ethi_popup_format_columns),  128),
	E_POPUP_SEPARATOR,
	E_POPUP_ITEM (N_("Customize Current View..."), G_CALLBACK(ethi_popup_customize_view),  4),
	E_POPUP_TERMINATOR
};

static void
ethi_header_context_menu (ETableHeaderItem *ethi, GdkEventButton *event)
{
	EthiHeaderInfo *info = g_new(EthiHeaderInfo, 1);
	ETableCol *col;
	GtkMenu *popup;
	info->ethi = ethi;
	info->col = ethi_find_col_by_x (ethi, event->x);
	col = e_table_header_get_column (ethi->eth, info->col);

	popup = e_popup_menu_create_with_domain (ethi_context_menu,
						 1 +
						 (col->sortable ? 0 : 2) +
						 ((ethi->table || ethi->tree) ? 0 : 4) + 
						 ((e_table_header_count (ethi->eth) > 1) ? 0 : 8),
						 ((e_table_sort_info_get_can_group (ethi->sort_info)) ? 0 : 16) +
						 128, info, E_I18N_DOMAIN);
	g_object_ref (popup);
	gtk_object_sink (GTK_OBJECT (popup));
	g_signal_connect (popup, "selection-done",
			  G_CALLBACK (free_popup_info), info);
	e_popup_menu (popup, (GdkEvent *) event);
}

static void
ethi_button_pressed (ETableHeaderItem *ethi, GdkEventButton *event)
{
	g_signal_emit (ethi,
		       ethi_signals [BUTTON_PRESSED], 0, event);
}

void
ethi_change_sort_state (ETableHeaderItem *ethi, ETableCol *col)
{
	int model_col;
	int length;
	int i;
	int found = FALSE;
	
	if (col == NULL)
		return;

	model_col = col->col_idx;
	
	length = e_table_sort_info_grouping_get_count(ethi->sort_info);
	for (i = 0; i < length; i++) {
		ETableSortColumn column = e_table_sort_info_grouping_get_nth(ethi->sort_info, i);
		if (model_col == column.column){
			int ascending = column.ascending;
			ascending = ! ascending;
			column.ascending = ascending;
			e_table_sort_info_grouping_set_nth(ethi->sort_info, i, column);
			found = 1;
			break;
		}
	}
	
	if (!col->sortable)
		return;
	
	if (!found) {
		length = e_table_sort_info_sorting_get_count(ethi->sort_info);
		for (i = 0; i < length; i++) {
			ETableSortColumn column = e_table_sort_info_sorting_get_nth(ethi->sort_info, i);

			if (model_col == column.column){
				int ascending = column.ascending;
				
				if (ascending == 0){
					/*
					 * This means the user has clicked twice
					 * already, lets kill sorting now.
					 */
					e_table_sort_info_sorting_truncate (ethi->sort_info, i);
				} else {
					ascending = !ascending;
					column.ascending = ascending;
					e_table_sort_info_sorting_set_nth(ethi->sort_info, i, column);
				}
				found = 1;
				break;
			}
		}
	}

	if (!found) {
		ETableSortColumn column;
		column.column = model_col;
		column.ascending = 1;
		length = e_table_sort_info_sorting_get_count(ethi->sort_info);
		if (length == 0)
			length++;
		e_table_sort_info_sorting_set_nth(ethi->sort_info, length - 1, column);
	}
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
	int was_maybe_drag = 0;
	
	switch (e->type){
	case GDK_ENTER_NOTIFY:
		convert (canvas, e->crossing.x, e->crossing.y, &x, &y);
		set_cursor (ethi, x);
		break;

	case GDK_LEAVE_NOTIFY:
		gdk_window_set_cursor (GTK_WIDGET (canvas)->window, NULL);
		/*		e_cursor_set (GTK_WIDGET (canvas)->window, E_CURSOR_ARROW);*/
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

			e_table_header_set_size (ethi->eth, ethi->resize_col, new_width);

			gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(ethi));
		} else if (ethi_maybe_start_drag (ethi, &e->motion)){
			ethi_start_drag (ethi, e);
		} else
			set_cursor (ethi, x);
		break;
		
	case GDK_BUTTON_PRESS:
		if (e->button.button > 3)
			return FALSE;

		convert (canvas, e->button.x, e->button.y, &x, &y);
		    
		if (is_pointer_on_division (ethi, x, &start, &col) && e->button.button == 1){
			ETableCol *ecol;
				
				/*
				 * Record the important bits.
				 *
				 * By setting resize_pos to a non -1 value,
				 * we know that we are being resized (used in the
				 * other event handlers).
				 */
			ecol = e_table_header_get_column (ethi->eth, col);
			
			if (!ecol->resizable)
				break;
			ethi->resize_col = col;
			ethi->resize_start_pos = start - ecol->width;
			ethi->resize_min_width = ecol->min_width;
		} else {
			if (e->button.button == 1){
				ethi->click_x = e->button.x;
				ethi->click_y = e->button.y;
				ethi->maybe_drag = TRUE;
				is_pointer_on_division (ethi, x, &start, &col);
				ethi->selected_col = col;
				e_canvas_item_grab_focus (item, TRUE);
			} else if (e->button.button == 3){
				ethi_header_context_menu (ethi, &e->button);
			} else
				ethi_button_pressed (ethi, &e->button);
		}
		break;
		
	case GDK_2BUTTON_PRESS:
		if (!resizing)
			break;
		
		if (e->button.button != 1)
			break;
		else {
			int width = 0;
			g_signal_emit_by_name (ethi->eth,
					       "request_width",
					       (int)ethi->resize_col, &width);
			/* Add 10 to stop it from "..."ing */
			e_table_header_set_size (ethi->eth, ethi->resize_col, width + 10);

			gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(ethi));
			ethi->maybe_drag = FALSE;
		}
		break;
		
	case GDK_BUTTON_RELEASE: {
		gboolean needs_ungrab = FALSE;
		
		was_maybe_drag = ethi->maybe_drag;
		
		ethi->maybe_drag = FALSE;

		if (ethi->resize_col != -1){
			needs_ungrab = (ethi->resize_guide != NULL);
			ethi_end_resize (ethi);
		} else if (was_maybe_drag && ethi->sort_info) {
			ETableCol *col;
		
			col = e_table_header_get_column (ethi->eth, ethi_find_col_by_x (ethi, e->button.x));
			ethi_change_sort_state (ethi, col);
		}
		
		if (needs_ungrab)
			gnome_canvas_item_ungrab (item, e->button.time);

		break;
	}
	case GDK_KEY_PRESS:
		if ((e->key.keyval == GDK_F10) && (e->key.state & GDK_SHIFT_MASK)) {
			EthiHeaderInfo *info = g_new(EthiHeaderInfo, 1);
			ETableCol *col;
			GtkMenu *popup;
 
			info->ethi = ethi;
			info->col = ethi->selected_col;
			col = e_table_header_get_column (ethi->eth, info->col);
			
			popup = e_popup_menu_create_with_domain (ethi_context_menu,
								 1 +
								 (col->sortable ? 0 : 2) +
								 ((ethi->table || ethi->tree) ? 0 : 4) + 
								 ((e_table_header_count (ethi->eth) > 1) ? 0 : 8),
								 ((e_table_sort_info_get_can_group (ethi->sort_info)) ? 0 : 16) +
								 128, info, E_I18N_DOMAIN);
			g_object_ref (popup);
			gtk_object_sink (GTK_OBJECT (popup));
			g_signal_connect (popup, "selection-done",
					  G_CALLBACK (free_popup_info), info);
			e_popup_menu (popup, NULL);
		} else if (e->key.keyval == GDK_space) {
			ETableCol *col;
			
			col = e_table_header_get_column (ethi->eth, ethi->selected_col);
			ethi_change_sort_state (ethi, col);
		} else if ((e->key.keyval == GDK_Right) || (e->key.keyval == GDK_KP_Right)) {
			ETableCol *col;

			if ((ethi->selected_col < 0) || (ethi->selected_col >= ethi->eth->col_count - 1))
				ethi->selected_col = 0;
			else 
				ethi->selected_col++;
			col = e_table_header_get_column (ethi->eth, ethi->selected_col);
			ethi_change_sort_state (ethi, col);
		} else if ((e->key.keyval == GDK_Left) || (e->key.keyval == GDK_KP_Left)) {
			ETableCol *col;

			if ((ethi->selected_col <= 0) || (ethi->selected_col >= ethi->eth->col_count))
				ethi->selected_col = ethi->eth->col_count - 1;
			else 
				ethi->selected_col--;
			col = e_table_header_get_column (ethi->eth, ethi->selected_col);
			ethi_change_sort_state (ethi, col);
		}
		break;
	
	default:
		return FALSE;
	}
	return TRUE;
}

static void
ethi_class_init (GObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;

	ethi_parent_class = g_type_class_ref (PARENT_OBJECT_TYPE);
	
	object_class->dispose = ethi_dispose;
	object_class->set_property = ethi_set_property;
	object_class->get_property = ethi_get_property;

	item_class->update      = ethi_update;
	item_class->realize     = ethi_realize;
	item_class->unrealize   = ethi_unrealize;
	item_class->draw        = ethi_draw;
	item_class->point       = ethi_point;
	item_class->event       = ethi_event;

	g_object_class_install_property (object_class, PROP_DND_CODE,
					 g_param_spec_string ("dnd_code",
							      _("DnD code"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_TABLE_FONTSET,
					 g_param_spec_string ("fontset",
							      _("Fontset"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_FULL_HEADER,
					 g_param_spec_object ("full_header",
							      _("Full Header"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TABLE_HEADER_TYPE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_TABLE_HEADER,
					 g_param_spec_object ("ETableHeader",
							      _("Header"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TABLE_HEADER_TYPE,
							      G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_SORT_INFO,
					 g_param_spec_object ("sort_info",
							      _("Sort Info"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TABLE_SORT_INFO_TYPE,
							      G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_TABLE,
					 g_param_spec_object ("table",
							      _("Table"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TABLE_TYPE,
							      G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_TREE,
					 g_param_spec_object ("tree",
							      _("Tree"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TREE_TYPE,
							      G_PARAM_WRITABLE));

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

	ethi_signals [BUTTON_PRESSED] =
		g_signal_new ("button_pressed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableHeaderItemClass, button_pressed),
			      NULL, NULL,
			      e_util_marshal_NONE__BOXED,
			      G_TYPE_NONE, 1, GDK_TYPE_EVENT);
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
	
	ethi->sort_info = NULL;

	ethi->sort_info_changed_id = 0;
	ethi->group_info_changed_id = 0;

	ethi->group_indent_width = 0;
	ethi->table = NULL;
	ethi->tree = NULL;

	ethi->selected_col = 0;
}

E_MAKE_TYPE (e_table_header_item,
	     "ETableHeaderItem",
	     ETableHeaderItem,
	     ethi_class_init,
	     ethi_init,
	     PARENT_OBJECT_TYPE)
