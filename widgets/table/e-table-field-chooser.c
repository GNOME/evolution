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

#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"

#include "misc/e-canvas.h"

#include "e-table-field-chooser.h"
#include "e-table-field-chooser-item.h"

static void e_table_field_chooser_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_table_field_chooser_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void e_table_field_chooser_dispose (GObject *object);

/* The arguments we take */
enum {
	PROP_0,
	PROP_FULL_HEADER,
	PROP_HEADER,
	PROP_DND_CODE
};

G_DEFINE_TYPE (ETableFieldChooser, e_table_field_chooser, GTK_TYPE_VBOX)

static void
e_table_field_chooser_class_init (ETableFieldChooserClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;

	object_class->set_property = e_table_field_chooser_set_property;
	object_class->get_property = e_table_field_chooser_get_property;
	object_class->dispose      = e_table_field_chooser_dispose;

	g_object_class_install_property (object_class, PROP_DND_CODE,
					 g_param_spec_string ("dnd_code",
							      "DnD code",
							      NULL,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FULL_HEADER,
					 g_param_spec_object ("full_header",
							      "Full Header",
							      NULL,
							      E_TABLE_HEADER_TYPE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_HEADER,
					 g_param_spec_object ("header",
							      "Header",
							      NULL,
							      E_TABLE_HEADER_TYPE,
							      G_PARAM_READWRITE));
}

static void
ensure_nonzero_step_increments (ETableFieldChooser *etfc)
{
	GtkAdjustment *va, *ha;

	va = gtk_layout_get_vadjustment (GTK_LAYOUT (etfc->canvas));
	ha = gtk_layout_get_hadjustment (GTK_LAYOUT (etfc->canvas));

	/*
	  it looks pretty complicated to get height of column header
	  so use 16 pixels which should be OK
	*/
	if (va)
		gtk_adjustment_set_step_increment (va, 16.0);
	if (ha)
		gtk_adjustment_set_step_increment (ha, 16.0);
}

static void allocate_callback (GtkWidget *canvas, GtkAllocation *allocation, ETableFieldChooser *etfc)
{
	gdouble height;
	etfc->last_alloc = *allocation;
	gnome_canvas_item_set ( etfc->item,
			       "width", (gdouble) allocation->width,
			       NULL );
	g_object_get (etfc->item,
		     "height", &height,
		     NULL);
	height = MAX (height, allocation->height);
	gnome_canvas_set_scroll_region (GNOME_CANVAS ( etfc->canvas ), 0, 0, allocation->width - 1, height - 1);
	gnome_canvas_item_set ( etfc->rect,
			       "x2", (gdouble) allocation->width,
			       "y2", (gdouble) height,
			       NULL );
	ensure_nonzero_step_increments (etfc);
}

static void resize (GnomeCanvas *canvas, ETableFieldChooser *etfc)
{
	gdouble height;
	g_object_get (etfc->item,
		     "height", &height,
		     NULL);

	height = MAX (height, etfc->last_alloc.height);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (etfc->canvas), 0, 0, etfc->last_alloc.width - 1, height - 1);
	gnome_canvas_item_set ( etfc->rect,
			       "x2", (gdouble) etfc->last_alloc.width,
			       "y2", (gdouble) height,
			       NULL );
	ensure_nonzero_step_increments (etfc);
}

static GtkWidget *
create_content (GnomeCanvas **canvas)
{
	GtkWidget *vbox_top;
	GtkWidget *label1;
	GtkWidget *scrolledwindow1;
	GtkWidget *canvas_buttons;

	g_return_val_if_fail (canvas != NULL, NULL);

	vbox_top = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox_top);

	label1 = gtk_label_new (_("To add a column to your table, drag it into\nthe location in which you want it to appear."));
	gtk_widget_show (label1);
	gtk_box_pack_start (GTK_BOX (vbox_top), label1, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label1), GTK_JUSTIFY_CENTER);

	scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow1);
	gtk_box_pack_start (GTK_BOX (vbox_top), scrolledwindow1, TRUE, TRUE, 0);
	gtk_widget_set_can_focus (scrolledwindow1, FALSE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	canvas_buttons = e_canvas_new ();
	gtk_widget_show (canvas_buttons);
	gtk_container_add (GTK_CONTAINER (scrolledwindow1), canvas_buttons);
	gtk_widget_set_can_focus (canvas_buttons, FALSE);
	gtk_widget_set_can_default (canvas_buttons, FALSE);

	*canvas = GNOME_CANVAS (canvas_buttons);

	return vbox_top;
}

static void
e_table_field_chooser_init (ETableFieldChooser *etfc)
{
	GtkWidget *widget;

	widget = create_content (&etfc->canvas);
	if (!widget) {
		return;
	}

	gtk_widget_set_size_request (widget, -1, 250);
	gtk_box_pack_start (GTK_BOX (etfc), widget, TRUE, TRUE, 0);

	gtk_widget_push_colormap (gdk_rgb_get_colormap ());

	etfc->rect = gnome_canvas_item_new (gnome_canvas_root ( GNOME_CANVAS ( etfc->canvas ) ),
					   gnome_canvas_rect_get_type (),
					   "x1", (gdouble) 0,
					   "y1", (gdouble) 0,
					   "x2", (gdouble) 100,
					   "y2", (gdouble) 100,
					   "fill_color", "white",
					   NULL );

	etfc->item = gnome_canvas_item_new (gnome_canvas_root (etfc->canvas),
					   e_table_field_chooser_item_get_type (),
					   "width", (gdouble) 100,
					   "full_header", etfc->full_header,
					   "header", etfc->header,
					   "dnd_code", etfc->dnd_code,
					   NULL );

	g_signal_connect( etfc->canvas, "reflow",
			  G_CALLBACK ( resize ),
			  etfc);

	gnome_canvas_set_scroll_region ( GNOME_CANVAS ( etfc->canvas ),
					 0, 0,
					 100, 100 );

	/* Connect the signals */
	g_signal_connect (etfc->canvas, "size_allocate",
			  G_CALLBACK (allocate_callback),
			  etfc);

	gtk_widget_pop_colormap ();
	gtk_widget_show_all (widget);
}

static void
e_table_field_chooser_dispose (GObject *object)
{
	ETableFieldChooser *etfc = E_TABLE_FIELD_CHOOSER (object);

	g_free (etfc->dnd_code);
	etfc->dnd_code = NULL;

	if (etfc->full_header)
		g_object_unref (etfc->full_header);
	etfc->full_header = NULL;

	if (etfc->header)
		g_object_unref (etfc->header);
	etfc->header = NULL;

	if (G_OBJECT_CLASS (e_table_field_chooser_parent_class)->dispose)
		(* G_OBJECT_CLASS (e_table_field_chooser_parent_class)->dispose) (object);
}

GtkWidget*
e_table_field_chooser_new (void)
{
	GtkWidget *widget = GTK_WIDGET (g_object_new (E_TABLE_FIELD_CHOOSER_TYPE, NULL));
	return widget;
}

static void
e_table_field_chooser_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ETableFieldChooser *etfc = E_TABLE_FIELD_CHOOSER (object);

	switch (prop_id) {
	case PROP_DND_CODE:
		g_free (etfc->dnd_code);
		etfc->dnd_code = g_strdup (g_value_get_string (value));
		if (etfc->item)
			g_object_set (etfc->item,
				     "dnd_code", etfc->dnd_code,
				     NULL);
		break;
	case PROP_FULL_HEADER:
		if (etfc->full_header)
			g_object_unref (etfc->full_header);
		if (g_value_get_object (value))
			etfc->full_header = E_TABLE_HEADER (g_value_get_object (value));
		else
			etfc->full_header = NULL;
		if (etfc->full_header)
			g_object_ref (etfc->full_header);
		if (etfc->item)
			g_object_set (etfc->item,
				     "full_header", etfc->full_header,
				     NULL);
		break;
	case PROP_HEADER:
		if (etfc->header)
			g_object_unref (etfc->header);
		if (g_value_get_object (value))
			etfc->header = E_TABLE_HEADER (g_value_get_object (value));
		else
			etfc->header = NULL;
		if (etfc->header)
			g_object_ref (etfc->header);
		if (etfc->item)
			g_object_set (etfc->item,
				     "header", etfc->header,
				     NULL);
		break;
	default:
		break;
	}
}

static void
e_table_field_chooser_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ETableFieldChooser *etfc = E_TABLE_FIELD_CHOOSER (object);

	switch (prop_id) {
	case PROP_DND_CODE:
		g_value_set_string (value, etfc->dnd_code);
		break;
	case PROP_FULL_HEADER:
		g_value_set_object (value, etfc->full_header);
		break;
	case PROP_HEADER:
		g_value_set_object (value, etfc->header);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}
