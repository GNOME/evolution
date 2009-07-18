/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <string.h>

#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/gnome-canvas-util.h>
#include <libgnomecanvas/gnome-canvas-polygon.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"
#include "misc/e-canvas.h"

#include "e-table-col-dnd.h"
#include "e-table-defines.h"
#include "e-table-field-chooser-item.h"
#include "e-table-header-utils.h"
#include "e-table-header.h"

#define d(x)

#if 0
enum {
	BUTTON_PRESSED,
	LAST_SIGNAL
};

static guint etfci_signals [LAST_SIGNAL] = { 0, };
#endif

/* workaround for avoiding API breakage */
#define etfci_get_type e_table_field_chooser_item_get_type
G_DEFINE_TYPE (ETableFieldChooserItem, etfci, GNOME_TYPE_CANVAS_ITEM)

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))

static void etfci_drop_table_header (ETableFieldChooserItem *etfci);
static void etfci_drop_full_header (ETableFieldChooserItem *etfci);

enum {
	PROP_0,
	PROP_FULL_HEADER,
	PROP_HEADER,
	PROP_DND_CODE,
	PROP_WIDTH,
	PROP_HEIGHT
};

static void
etfci_dispose (GObject *object)
{
	ETableFieldChooserItem *etfci = E_TABLE_FIELD_CHOOSER_ITEM (object);

	etfci_drop_table_header (etfci);
	etfci_drop_full_header (etfci);

	if (etfci->combined_header)
		g_object_unref (etfci->combined_header);
	etfci->combined_header = NULL;

	if (etfci->font_desc)
		pango_font_description_free (etfci->font_desc);
	etfci->font_desc = NULL;

	if (G_OBJECT_CLASS (etfci_parent_class)->dispose)
		(*G_OBJECT_CLASS (etfci_parent_class)->dispose) (object);
}

static gint
etfci_find_button (ETableFieldChooserItem *etfci, double loc)
{
	gint i;
	gint count;
	double height = 0;

	count = e_table_header_count(etfci->combined_header);
	for (i = 0; i < count; i++) {
		ETableCol *ecol;

		ecol = e_table_header_get_column (etfci->combined_header, i);
		if (ecol->disabled)
			continue;
		height += e_table_header_compute_height (ecol, GTK_WIDGET (GNOME_CANVAS_ITEM (etfci)->canvas));
		if (height > loc)
			return i;
	}
	return MAX(0, count - 1);
}

static void
etfci_rebuild_combined (ETableFieldChooserItem *etfci)
{
	gint count;
	GHashTable *hash;
	gint i;

	if (etfci->combined_header != NULL)
		g_object_unref (etfci->combined_header);

	etfci->combined_header = e_table_header_new ();

	hash = g_hash_table_new (NULL, NULL);

	count = e_table_header_count (etfci->header);
	for (i = 0; i < count; i++) {
		ETableCol *ecol = e_table_header_get_column (etfci->header, i);
		if (ecol->disabled)
			continue;
		g_hash_table_insert (hash, GINT_TO_POINTER (ecol->col_idx), GINT_TO_POINTER (1));
	}

	count = e_table_header_count (etfci->full_header);
	for (i = 0; i < count; i++) {
		ETableCol *ecol = e_table_header_get_column (etfci->full_header, i);
		if (ecol->disabled)
			continue;
		if (! (GPOINTER_TO_INT (g_hash_table_lookup (hash, GINT_TO_POINTER (ecol->col_idx)))))
			e_table_header_add_column (etfci->combined_header, ecol, -1);
	}

	g_hash_table_destroy (hash);
}

static void
etfci_reflow (GnomeCanvasItem *item, gint flags)
{
	ETableFieldChooserItem *etfci = E_TABLE_FIELD_CHOOSER_ITEM (item);
	double old_height;
	gint i;
	gint count;
	double height = 0;

	etfci_rebuild_combined (etfci);

	old_height = etfci->height;

	count = e_table_header_count(etfci->combined_header);
	for (i = 0; i < count; i++) {
		ETableCol *ecol;

		ecol = e_table_header_get_column (etfci->combined_header, i);
		if (ecol->disabled)
			continue;
		height += e_table_header_compute_height (ecol, GTK_WIDGET (GNOME_CANVAS_ITEM (etfci)->canvas));
	}

	etfci->height = height;

	if (old_height != etfci->height)
		e_canvas_item_request_parent_reflow(item);

	gnome_canvas_item_request_update(item);
}

static void
etfci_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, gint flags)
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
/* FIXME: Group Child bounds !? */
#if 0
			gnome_canvas_group_child_bounds (GNOME_CANVAS_GROUP (item->parent), item);
#endif
		}
	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
}

static void
etfci_font_load (ETableFieldChooserItem *etfci)
{
	GtkStyle *style;

	if (etfci->font_desc)
		pango_font_description_free (etfci->font_desc);

	style = GTK_WIDGET (GNOME_CANVAS_ITEM (etfci)->canvas)->style;
	etfci->font_desc = pango_font_description_copy (style->font_desc);
}

static void
etfci_drop_full_header (ETableFieldChooserItem *etfci)
{
	GObject *header;

	if (!etfci->full_header)
		return;

	header = G_OBJECT (etfci->full_header);
	if (etfci->full_header_structure_change_id)
		g_signal_handler_disconnect (header, etfci->full_header_structure_change_id);
	if (etfci->full_header_dimension_change_id)
		g_signal_handler_disconnect (header, etfci->full_header_dimension_change_id);
	etfci->full_header_structure_change_id = 0;
	etfci->full_header_dimension_change_id = 0;

	if (header)
		g_object_unref (header);
	etfci->full_header = NULL;
	etfci->height = 0;
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(etfci));
}

static void
full_header_structure_changed (ETableHeader *header, ETableFieldChooserItem *etfci)
{
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(etfci));
}

static void
full_header_dimension_changed (ETableHeader *header, gint col, ETableFieldChooserItem *etfci)
{
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(etfci));
}

static void
etfci_add_full_header (ETableFieldChooserItem *etfci, ETableHeader *header)
{
	etfci->full_header = header;
	g_object_ref (etfci->full_header);

	etfci->full_header_structure_change_id = g_signal_connect (
		header, "structure_change",
		G_CALLBACK(full_header_structure_changed), etfci);
	etfci->full_header_dimension_change_id = g_signal_connect (
		header, "dimension_change",
		G_CALLBACK(full_header_dimension_changed), etfci);
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(etfci));
}

static void
etfci_drop_table_header (ETableFieldChooserItem *etfci)
{
	GObject *header;

	if (!etfci->header)
		return;

	header = G_OBJECT (etfci->header);
	if (etfci->table_header_structure_change_id)
		g_signal_handler_disconnect (header, etfci->table_header_structure_change_id);
	if (etfci->table_header_dimension_change_id)
		g_signal_handler_disconnect (header, etfci->table_header_dimension_change_id);
	etfci->table_header_structure_change_id = 0;
	etfci->table_header_dimension_change_id = 0;

	if (header)
		g_object_unref (header);
	etfci->header = NULL;
	etfci->height = 0;
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(etfci));
}

static void
table_header_structure_changed (ETableHeader *header, ETableFieldChooserItem *etfci)
{
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(etfci));
}

static void
table_header_dimension_changed (ETableHeader *header, gint col, ETableFieldChooserItem *etfci)
{
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(etfci));
}

static void
etfci_add_table_header (ETableFieldChooserItem *etfci, ETableHeader *header)
{
	etfci->header = header;
	g_object_ref (etfci->header);

	etfci->table_header_structure_change_id = g_signal_connect (
		header, "structure_change",
		G_CALLBACK(table_header_structure_changed), etfci);
	etfci->table_header_dimension_change_id = g_signal_connect (
		header, "dimension_change",
		G_CALLBACK(table_header_dimension_changed), etfci);
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(etfci));
}

static void
etfci_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	ETableFieldChooserItem *etfci;

	item = GNOME_CANVAS_ITEM (object);
	etfci = E_TABLE_FIELD_CHOOSER_ITEM (object);

	switch (prop_id) {
	case PROP_FULL_HEADER:
		etfci_drop_full_header (etfci);
		if (g_value_get_object (value))
			etfci_add_full_header (etfci, E_TABLE_HEADER(g_value_get_object (value)));
		break;

	case PROP_HEADER:
		etfci_drop_table_header (etfci);
		if (g_value_get_object (value))
			etfci_add_table_header (etfci, E_TABLE_HEADER(g_value_get_object (value)));
		break;

	case PROP_DND_CODE:
		g_free(etfci->dnd_code);
		etfci->dnd_code = g_strdup(g_value_get_string (value));
		break;

	case PROP_WIDTH:
		etfci->width = g_value_get_double (value);
		gnome_canvas_item_request_update(item);
		break;
	}
}

static void
etfci_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ETableFieldChooserItem *etfci;

	etfci = E_TABLE_FIELD_CHOOSER_ITEM (object);

	switch (prop_id) {

	case PROP_DND_CODE:
		g_value_set_string (value, etfci->dnd_code);
		break;
	case PROP_WIDTH:
		g_value_set_double (value, etfci->width);
		break;
	case PROP_HEIGHT:
		g_value_set_double (value, etfci->height);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
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
				       (guchar *)string,
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

	if (GNOME_CANVAS_ITEM_CLASS (etfci_parent_class)-> realize)
		(*GNOME_CANVAS_ITEM_CLASS (etfci_parent_class)->realize)(item);

	if (!etfci->font_desc)
		etfci_font_load (etfci);

	etfci->drag_end_id = g_signal_connect (
		item->canvas, "drag_end",
		G_CALLBACK (etfci_drag_end), etfci);
	etfci->drag_data_get_id = g_signal_connect (
		item->canvas, "drag_data_get",
		G_CALLBACK (etfci_drag_data_get), etfci);
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(etfci));
}

static void
etfci_unrealize (GnomeCanvasItem *item)
{
	ETableFieldChooserItem *etfci = E_TABLE_FIELD_CHOOSER_ITEM (item);

	if (etfci->font_desc)
		pango_font_description_free (etfci->font_desc);
	etfci->font_desc = NULL;

	g_signal_handler_disconnect (item->canvas, etfci->drag_end_id);
	etfci->drag_end_id = 0;
	g_signal_handler_disconnect (item->canvas, etfci->drag_data_get_id);
	etfci->drag_data_get_id = 0;

	if (GNOME_CANVAS_ITEM_CLASS (etfci_parent_class)->unrealize)
		(*GNOME_CANVAS_ITEM_CLASS (etfci_parent_class)->unrealize)(item);
}

static void
etfci_draw (GnomeCanvasItem *item, GdkDrawable *drawable, gint x, gint y, gint width, gint height)
{
	ETableFieldChooserItem *etfci = E_TABLE_FIELD_CHOOSER_ITEM (item);
	GnomeCanvas *canvas = item->canvas;
	gint rows;
	gint y1, y2;
	gint row;
	GtkStyle *style;
	GtkStateType state;

	if (etfci->combined_header == NULL)
		return;

	rows = e_table_header_count (etfci->combined_header);

	style = GTK_WIDGET (canvas)->style;
	state = GTK_WIDGET_STATE (canvas);

	y1 = y2 = 0;
	for (row = 0; row < rows; row++, y1 = y2) {
		ETableCol *ecol;

		ecol = e_table_header_get_column (etfci->combined_header, row);

		if (ecol->disabled)
			continue;

		y2 += e_table_header_compute_height (ecol, GTK_WIDGET (canvas));

		if (y1 > (y + height))
			break;

		if (y2 < y)
			continue;

		e_table_header_draw_button (drawable, ecol,
					    style, state,
					    GTK_WIDGET (canvas),
					    -x, y1 - y,
					    width, height,
					    etfci->width, y2 - y1,
					    E_TABLE_COL_ARROW_NONE);
	}
}

static double
etfci_point (GnomeCanvasItem *item, double x, double y, gint cx, gint cy,
	    GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}

static gboolean
etfci_maybe_start_drag (ETableFieldChooserItem *etfci, gint x, gint y)
{
	if (!etfci->maybe_drag)
		return FALSE;

	if (MAX (abs (etfci->click_x - x),
		 abs (etfci->click_y - y)) <= 3)
		return FALSE;

	return TRUE;
}

static void
etfci_start_drag (ETableFieldChooserItem *etfci, GdkEvent *event, double x, double y)
{
	GtkWidget *widget = GTK_WIDGET (GNOME_CANVAS_ITEM (etfci)->canvas);
	GtkTargetList *list;
	GdkDragContext *context;
	ETableCol *ecol;
	GdkPixmap *pixmap;
	gint drag_col;
	gint button_height;

	GtkTargetEntry  etfci_drag_types [] = {
		{ (gchar *) TARGET_ETABLE_COL_TYPE, 0, TARGET_ETABLE_COL_HEADER },
	};

	if (etfci->combined_header == NULL)
		return;

	drag_col = etfci_find_button(etfci, y);

	if (drag_col < 0 || drag_col > e_table_header_count(etfci->combined_header))
		return;

	ecol = e_table_header_get_column (etfci->combined_header, drag_col);

	if (ecol->disabled)
		return;

	etfci->drag_col = ecol->col_idx;

	etfci_drag_types[0].target = g_strdup_printf("%s-%s", etfci_drag_types[0].target, etfci->dnd_code);
	d(g_print ("etfci - %s\n", etfci_drag_types[0].target));
	list = gtk_target_list_new (etfci_drag_types, ELEMENTS (etfci_drag_types));
	context = gtk_drag_begin (widget, list, GDK_ACTION_MOVE, 1, event);
	g_free(etfci_drag_types[0].target);

	button_height = e_table_header_compute_height (ecol, widget);
	pixmap = gdk_pixmap_new (widget->window, etfci->width, button_height, -1);

	e_table_header_draw_button (pixmap, ecol,
				    widget->style, GTK_WIDGET_STATE (widget),
				    widget,
				    0, 0,
				    etfci->width, button_height,
				    etfci->width, button_height,
				    E_TABLE_COL_ARROW_NONE);

	gtk_drag_set_icon_pixmap        (context,
					 gdk_drawable_get_colormap (GDK_DRAWABLE (widget->window)),
					 pixmap,
					 NULL,
					 etfci->width / 2,
					 button_height / 2);
	g_object_unref (pixmap);
	etfci->maybe_drag = FALSE;
}

/*
 * Handles the events on the ETableFieldChooserItem
 */
static gint
etfci_event (GnomeCanvasItem *item, GdkEvent *e)
{
	ETableFieldChooserItem *etfci = E_TABLE_FIELD_CHOOSER_ITEM (item);
	GnomeCanvas *canvas = item->canvas;
	gint x, y;

	switch (e->type) {
	case GDK_MOTION_NOTIFY:
		gnome_canvas_w2c (canvas, e->motion.x, e->motion.y, &x, &y);

		if (etfci_maybe_start_drag (etfci, x, y))
			etfci_start_drag (etfci, e, x, y);
		break;

	case GDK_BUTTON_PRESS:
		gnome_canvas_w2c (canvas, e->button.x, e->button.y, &x, &y);

		if (e->button.button == 1) {
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
etfci_class_init (ETableFieldChooserItemClass *klass)
{
	GnomeCanvasItemClass *item_class = GNOME_CANVAS_ITEM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = etfci_dispose;
	object_class->set_property = etfci_set_property;
	object_class->get_property = etfci_get_property;

	item_class->update      = etfci_update;
	item_class->realize     = etfci_realize;
	item_class->unrealize   = etfci_unrealize;
	item_class->draw        = etfci_draw;
	item_class->point       = etfci_point;
	item_class->event       = etfci_event;

	g_object_class_install_property (object_class, PROP_DND_CODE,
					 g_param_spec_string ("dnd_code",
							      "DnD code",
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FULL_HEADER,
					 g_param_spec_object ("full_header",
							      "Full Header",
							      /*_( */"XXX blurb" /*)*/,
							      E_TABLE_HEADER_TYPE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_HEADER,
					 g_param_spec_object ("header",
							      "Header",
							      /*_( */"XXX blurb" /*)*/,
							      E_TABLE_HEADER_TYPE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_WIDTH,
					 g_param_spec_double ("width",
							      "Width",
							      /*_( */"XXX blurb" /*)*/,
							      0, G_MAXDOUBLE, 0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_HEIGHT,
					 g_param_spec_double ("height",
							      "Height",
							      /*_( */"XXX blurb" /*)*/,
							      0, G_MAXDOUBLE, 0,
							      G_PARAM_READABLE));
}

static void
etfci_init (ETableFieldChooserItem *etfci)
{
	etfci->full_header = NULL;
	etfci->header = NULL;
	etfci->combined_header = NULL;

	etfci->height = etfci->width = 0;

	etfci->font_desc = NULL;

	etfci->full_header_structure_change_id = 0;
	etfci->full_header_dimension_change_id = 0;
	etfci->table_header_structure_change_id = 0;
	etfci->table_header_dimension_change_id = 0;

	etfci->dnd_code = NULL;

	etfci->maybe_drag = 0;
	etfci->drag_end_id = 0;

	e_canvas_item_set_reflow_callback(GNOME_CANVAS_ITEM (etfci), etfci_reflow);
}

