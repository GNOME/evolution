/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-column-view.c: A canvas item based view of the ETableColumn.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright 1999, 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkdnd.h>
#include <libgnomeui/gnome-canvas.h>
#include <libgnomeui/gnome-canvas-util.h>
#include <libgnomeui/gnome-canvas-polygon.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "e-util/e-xml-utils.h"
#include "e-util/e-canvas.h"

#include "e-table-header.h"
#include "e-table-col-dnd.h"
#include "e-table-defines.h"

#include "e-table-field-chooser-item.h"

#if 0
enum {
	BUTTON_PRESSED,
	LAST_SIGNAL
};

static guint etfci_signals [LAST_SIGNAL] = { 0, };
#endif

#define PARENT_OBJECT_TYPE gnome_canvas_item_get_type ()

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))

static GnomeCanvasItemClass *etfci_parent_class;

static void etfci_drop_table_header (ETableFieldChooserItem *etfci);

enum {
	ARG_0,
	ARG_FULL_HEADER,
	ARG_DND_CODE,
	ARG_WIDTH,
	ARG_HEIGHT,
};

static void
etfci_destroy (GtkObject *object){
	ETableFieldChooserItem *etfci = E_TABLE_FIELD_CHOOSER_ITEM (object);

	etfci_drop_table_header (etfci);
	
	gdk_font_unref(etfci->font);

	if (GTK_OBJECT_CLASS (etfci_parent_class)->destroy)
		(*GTK_OBJECT_CLASS (etfci_parent_class)->destroy) (object);
}

static void
etfci_reflow (GnomeCanvasItem *item, gint flags)
{
	ETableFieldChooserItem *etfci = E_TABLE_FIELD_CHOOSER_ITEM (item);
	double old_height;

	old_height = etfci->height;

	etfci->height = e_table_header_count (etfci->full_header) * etfci->button_height;
	
	if (old_height != etfci->height)
		e_canvas_item_request_parent_reflow(item);
	
	gnome_canvas_item_request_update(item);
}

static void
etfci_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	ETableFieldChooserItem *etfci = E_TABLE_FIELD_CHOOSER_ITEM (item);
	double   i2c [6];
	ArtPoint c1, c2, i1, i2;
	
	if (GNOME_CANVAS_ITEM_CLASS (etfci_parent_class)->update)
		(*GNOME_CANVAS_ITEM_CLASS (etfci_parent_class)->update)(item, affine, clip_path, flags);

	i1.x = i1.y = 0;
	i2.x = etfci->width;
	i2.y = etfci->height;

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

			gnome_canvas_group_child_bounds (GNOME_CANVAS_GROUP (item->parent), item);
		}
	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
}

static void
etfci_font_load (ETableFieldChooserItem *etfci, char *font)
{
	if (etfci->font)
		gdk_font_unref (etfci->font);
	etfci->font = NULL;
	
	if (font)
		etfci->font = gdk_fontset_load (font);

	if (etfci->font == NULL) {
		etfci->font = GTK_WIDGET(GNOME_CANVAS_ITEM(etfci)->canvas)->style->font;
		gdk_font_ref(etfci->font);
	}
	
	etfci->button_height = etfci->font->ascent + etfci->font->descent + HEADER_PADDING;
}

static void
etfci_drop_table_header (ETableFieldChooserItem *etfci)
{
	GtkObject *header;
	
	if (!etfci->full_header)
		return;

	header = GTK_OBJECT (etfci->full_header);
	if (etfci->structure_change_id)
		gtk_signal_disconnect (header, etfci->structure_change_id);
	if (etfci->dimension_change_id)
		gtk_signal_disconnect (header, etfci->dimension_change_id);
	etfci->structure_change_id = 0;
	etfci->dimension_change_id = 0;

	if (header)
		gtk_object_unref (header);
	etfci->full_header = NULL;
	etfci->height = 0;
}

static void 
structure_changed (ETableHeader *header, ETableFieldChooserItem *etfci)
{
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(etfci));
}

static void
dimension_changed (ETableHeader *header, int col, ETableFieldChooserItem *etfci)
{
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(etfci));
}

static void
etfci_add_table_header (ETableFieldChooserItem *etfci, ETableHeader *header)
{
	etfci->full_header = header;
	gtk_object_ref (GTK_OBJECT (etfci->full_header));

	etfci->structure_change_id = gtk_signal_connect (
		GTK_OBJECT (header), "structure_change",
		GTK_SIGNAL_FUNC(structure_changed), etfci);
	etfci->dimension_change_id = gtk_signal_connect (
		GTK_OBJECT (header), "dimension_change",
		GTK_SIGNAL_FUNC(dimension_changed), etfci);
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(etfci));
}

static void
etfci_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	ETableFieldChooserItem *etfci;

	item = GNOME_CANVAS_ITEM (o);
	etfci = E_TABLE_FIELD_CHOOSER_ITEM (o);

	switch (arg_id){
	case ARG_FULL_HEADER:
		etfci_drop_table_header (etfci);
		if (GTK_VALUE_OBJECT (*arg))
			etfci_add_table_header (etfci, E_TABLE_HEADER(GTK_VALUE_OBJECT (*arg)));
		break;

	case ARG_DND_CODE:
		g_free(etfci->dnd_code);
		etfci->dnd_code = g_strdup(GTK_VALUE_STRING (*arg));
		break;

	case ARG_WIDTH:
		etfci->width = GTK_VALUE_DOUBLE (*arg);
		gnome_canvas_item_request_update(item);
		break;
	}
}

static void
etfci_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	ETableFieldChooserItem *etfci;

	item = GNOME_CANVAS_ITEM (o);
	etfci = E_TABLE_FIELD_CHOOSER_ITEM (o);

	switch (arg_id){

	case ARG_DND_CODE:
		GTK_VALUE_STRING (*arg) = g_strdup (etfci->dnd_code);
		break;
	case ARG_WIDTH:
		GTK_VALUE_DOUBLE (*arg) = etfci->width;
		break;
	case ARG_HEIGHT:
		GTK_VALUE_DOUBLE (*arg) = etfci->height;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
etfci_drag_data_get (GtkWidget          *widget,
		     GdkDragContext     *context,
		     GtkSelectionData   *selection_data,
		     guint               info,
		     guint               time,
		     ETableFieldChooserItem *etfci)
{
	if (etfci->drag_col != -1) {
		gchar *string = g_strdup_printf("%d", etfci->drag_col);
		gtk_selection_data_set(selection_data,
				       GDK_SELECTION_TYPE_STRING,
				       sizeof(string[0]),
				       string,
				       strlen(string));
		g_free(string);
	}
}

static void
etfci_drag_end (GtkWidget      *canvas, 
		GdkDragContext *context,
		ETableFieldChooserItem *etfci)
{
	etfci->drag_col = -1;
}

static void
etfci_realize (GnomeCanvasItem *item)
{
	ETableFieldChooserItem *etfci = E_TABLE_FIELD_CHOOSER_ITEM (item);
	GdkWindow *window;

	if (GNOME_CANVAS_ITEM_CLASS (etfci_parent_class)-> realize)
		(*GNOME_CANVAS_ITEM_CLASS (etfci_parent_class)->realize)(item);

	window = GTK_WIDGET (item->canvas)->window;

	if (!etfci->font)
		etfci_font_load (etfci, NULL);

	etfci->drag_end_id = gtk_signal_connect (
		GTK_OBJECT (item->canvas), "drag_end",
		GTK_SIGNAL_FUNC (etfci_drag_end), etfci);
	etfci->drag_data_get_id = gtk_signal_connect (
		GTK_OBJECT (item->canvas), "drag_data_get",
		GTK_SIGNAL_FUNC (etfci_drag_data_get), etfci);
}

static void
etfci_unrealize (GnomeCanvasItem *item)
{
	ETableFieldChooserItem *etfci = E_TABLE_FIELD_CHOOSER_ITEM (item);

	if (etfci->font)
		gdk_font_unref (etfci->font);
	etfci->font = NULL;

	gtk_signal_disconnect (GTK_OBJECT (item->canvas), etfci->drag_end_id);
	etfci->drag_end_id = 0;
	gtk_signal_disconnect (GTK_OBJECT (item->canvas), etfci->drag_data_get_id);
	etfci->drag_data_get_id = 0;
	
	if (GNOME_CANVAS_ITEM_CLASS (etfci_parent_class)->unrealize)
		(*GNOME_CANVAS_ITEM_CLASS (etfci_parent_class)->unrealize)(item);
}

static void
draw_button (ETableFieldChooserItem *etfci, ETableCol *col,
	     GdkDrawable *drawable, GtkStyle *style,
	     int x, int y, int width, int height)
{
	GdkRectangle clip;
	int xtra;
	GdkRectangle area;

	GtkWidget *widget = GTK_WIDGET(GNOME_CANVAS_ITEM(etfci)->canvas);
	
	gdk_window_clear_area (drawable, x, y, width, height);

	area.x = x;
	area.y = y;
	area.width = width;
	area.height = height;

	gtk_paint_box (style, drawable,
		       GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		       &area, widget, "button",
		       x, y, width, height);

	clip.x = x + HEADER_PADDING / 2;
	clip.y = y + HEADER_PADDING / 2;
	clip.width = width - HEADER_PADDING;
	clip.height = etfci->button_height;
	
	if (col->is_pixbuf){
		xtra = (clip.width - gdk_pixbuf_get_width (col->pixbuf))/2;
		
		xtra += HEADER_PADDING / 2;

		gdk_pixbuf_render_to_drawable_alpha (col->pixbuf, 
						     drawable,
						     0, 0, 
						     x + xtra, y + (clip.height - gdk_pixbuf_get_height (col->pixbuf)) / 2,
						     gdk_pixbuf_get_width (col->pixbuf), gdk_pixbuf_get_height(col->pixbuf),
						     GDK_PIXBUF_ALPHA_FULL, 128,
						     GDK_RGB_DITHER_NORMAL,
						     0, 0);
	} else {
		/* Center the thing */
		xtra = (clip.width - gdk_string_measure (etfci->font, col->text))/2;
		
		/* Skip over border */
		if (xtra < 0)
			xtra = 0;

		xtra += HEADER_PADDING / 2;
	
		gdk_draw_text (
			       drawable, etfci->font,
			       style->text_gc[GTK_STATE_NORMAL], x + xtra, y + etfci->button_height - etfci->font->descent - HEADER_PADDING / 2,
			       col->text, strlen (col->text));
	}
}

static void
etfci_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int width, int height)
{
	ETableFieldChooserItem *etfci = E_TABLE_FIELD_CHOOSER_ITEM (item);
	GnomeCanvas *canvas = item->canvas;
	const int rows = e_table_header_count (etfci->full_header);
	int y1, y2;
	int row;

	y1 = y2 = 0;
	for (row = 0; row < rows; row++, y1 = y2){
		ETableCol *ecol = e_table_header_get_column (etfci->full_header, row);

		y2 += etfci->button_height;
		
		if (y1 > (y + height))
			break;

		if (y2 < y)
			continue;
		
		draw_button (etfci, ecol, drawable,
			     GTK_WIDGET (canvas)->style,
			     - x, y1 - y, etfci->width, y2 - y1);
	}
}

static double
etfci_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
	    GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}

static gboolean
etfci_maybe_start_drag (ETableFieldChooserItem *etfci, double x, double y)
{
	if (!etfci->maybe_drag)
		return FALSE;

	if (MAX (abs (etfci->click_x - x),
		 abs (etfci->click_y - y)) <= 3)
		return FALSE;

	return TRUE;
}

static void
etfci_start_drag (ETableFieldChooserItem *etfci, GdkEvent *event)
{
	GtkWidget *widget = GTK_WIDGET (GNOME_CANVAS_ITEM (etfci)->canvas);
	GtkTargetList *list;
	GdkDragContext *context;
	ETableCol *ecol;
	GdkPixmap *pixmap;
	int drag_col;

	GtkTargetEntry  etfci_drag_types [] = {
		{ TARGET_ETABLE_COL_TYPE, 0, TARGET_ETABLE_COL_HEADER },
	};

	drag_col = event->motion.x / etfci->button_height;

	if (drag_col < 0 || drag_col > e_table_header_count(etfci->full_header))
		return;

	ecol = e_table_header_get_column (etfci->full_header, drag_col);

	etfci->drag_col = ecol->col_idx;

	etfci_drag_types[0].target = g_strdup_printf("%s-%s", etfci_drag_types[0].target, etfci->dnd_code);
	list = gtk_target_list_new (etfci_drag_types, ELEMENTS (etfci_drag_types));
	context = gtk_drag_begin (widget, list, GDK_ACTION_MOVE, 1, event);
	g_free(etfci_drag_types[0].target);


	pixmap = gdk_pixmap_new (widget->window, etfci->width, etfci->button_height, -1);
	draw_button (etfci, ecol, pixmap,
		     widget->style,
		     0, 0, etfci->width, etfci->button_height);
	gtk_drag_set_icon_pixmap        (context,
					 gdk_window_get_colormap (widget->window),
					 pixmap,
					 NULL,
					 etfci->width / 2,
					 etfci->button_height / 2);
	gdk_pixmap_unref (pixmap);
	etfci->maybe_drag = FALSE;
}

/*
 * Handles the events on the ETableFieldChooserItem
 */
static int
etfci_event (GnomeCanvasItem *item, GdkEvent *e)
{
	ETableFieldChooserItem *etfci = E_TABLE_FIELD_CHOOSER_ITEM (item);
	GnomeCanvas *canvas = item->canvas;
	int x, y;
	
	switch (e->type){
	case GDK_MOTION_NOTIFY:
		gnome_canvas_w2c (canvas, e->motion.x, e->motion.y, &x, &y);

		if (etfci_maybe_start_drag (etfci, x, y))
			etfci_start_drag (etfci, e);
		break;
		
	case GDK_BUTTON_PRESS:
		gnome_canvas_w2c (canvas, e->button.x, e->button.y, &x, &y);
		
		if (e->button.button == 1){
			etfci->click_x = x;
			etfci->click_y = y;
			etfci->maybe_drag = TRUE;
		}
		break;
		
	case GDK_BUTTON_RELEASE: {
		etfci->maybe_drag = FALSE;
		break;
	}
	
	default:
		return FALSE;
	}
	return TRUE;
}

static void
etfci_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;

	etfci_parent_class = gtk_type_class (PARENT_OBJECT_TYPE);
	
	object_class->destroy = etfci_destroy;
	object_class->set_arg = etfci_set_arg;
	object_class->get_arg = etfci_get_arg;

	item_class->update      = etfci_update;
	item_class->realize     = etfci_realize;
	item_class->unrealize   = etfci_unrealize;
	item_class->draw        = etfci_draw;
	item_class->point       = etfci_point;
	item_class->event       = etfci_event;
	
	gtk_object_add_arg_type ("ETableFieldChooserItem::dnd_code", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_DND_CODE);
	gtk_object_add_arg_type ("ETableFieldChooserItem::full_header", GTK_TYPE_OBJECT,
				 GTK_ARG_WRITABLE, ARG_FULL_HEADER);
	gtk_object_add_arg_type ("ETableFieldChooserItem::width", GTK_TYPE_DOUBLE,
				 GTK_ARG_READWRITE, ARG_WIDTH);
	gtk_object_add_arg_type ("ETableFieldChooserItem::height", GTK_TYPE_DOUBLE,
				 GTK_ARG_READABLE, ARG_HEIGHT);
}

static void
etfci_init (GnomeCanvasItem *item)
{
	ETableFieldChooserItem *etfci = E_TABLE_FIELD_CHOOSER_ITEM (item);

	etfci->full_header = NULL;
	
	etfci->height = etfci->width = 0;
	etfci->button_height = 0;

	etfci->font = NULL;

	etfci->structure_change_id = 0;
	etfci->dimension_change_id = 0;

	etfci->dnd_code = NULL;

	etfci->maybe_drag = 0;
	etfci->drag_end_id = 0;

	e_canvas_item_set_reflow_callback(item, etfci_reflow);
}

GtkType
e_table_field_chooser_item_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETableFieldChooserItem",
			sizeof (ETableFieldChooserItem),
			sizeof (ETableFieldChooserItemClass),
			(GtkClassInitFunc) etfci_class_init,
			(GtkObjectInitFunc) etfci_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_OBJECT_TYPE, &info);
	}

	return type;
}

