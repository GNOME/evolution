/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-field-chooser.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
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

#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>

#include "gal/util/e-i18n.h"
#include "gal/util/e-util.h"
#include "gal/util/e-util-private.h"

#include "e-table-field-chooser.h"
#include "e-table-field-chooser-item.h"

static void e_table_field_chooser_init		(ETableFieldChooser		 *card);
static void e_table_field_chooser_class_init	(ETableFieldChooserClass	 *klass);
static void e_table_field_chooser_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_table_field_chooser_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void e_table_field_chooser_dispose (GObject *object);

#define PARENT_TYPE GTK_TYPE_VBOX
static GtkVBoxClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_FULL_HEADER,
	PROP_HEADER,
	PROP_DND_CODE
};

E_MAKE_TYPE (e_table_field_chooser,
	     "ETableFieldChooser",
	     ETableFieldChooser,
	     e_table_field_chooser_class_init,
	     e_table_field_chooser_init,
	     PARENT_TYPE);

static void
e_table_field_chooser_class_init (ETableFieldChooserClass *klass)
{
	GObjectClass *object_class;
	GtkVBoxClass *vbox_class;

	object_class = (GObjectClass*) klass;
	vbox_class = (GtkVBoxClass *) klass;

	glade_init();

	parent_class = g_type_class_ref (GTK_TYPE_VBOX);

	object_class->set_property = e_table_field_chooser_set_property;
	object_class->get_property = e_table_field_chooser_get_property;
	object_class->dispose      = e_table_field_chooser_dispose;

	g_object_class_install_property (object_class, PROP_DND_CODE,
					 g_param_spec_string ("dnd_code",
							      _("DnD code"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_FULL_HEADER,
					 g_param_spec_object ("full_header",
							      _("Full Header"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TABLE_HEADER_TYPE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_HEADER,
					 g_param_spec_object ("header",
							      _("Header"),
							      /*_( */"XXX blurb" /*)*/,
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
		va->step_increment = 16.0;
	if (ha)
		ha->step_increment = 16.0;
}

static void allocate_callback(GtkWidget *canvas, GtkAllocation *allocation, ETableFieldChooser *etfc)
{
	double height;
	etfc->last_alloc = *allocation;
	gnome_canvas_item_set( etfc->item,
			       "width", (double) allocation->width,
			       NULL );
	g_object_get(etfc->item,
		     "height", &height,
		     NULL);
	height = MAX(height, allocation->height);
	gnome_canvas_set_scroll_region(GNOME_CANVAS( etfc->canvas ), 0, 0, allocation->width - 1, height - 1);
	gnome_canvas_item_set( etfc->rect,
			       "x2", (double) allocation->width,
			       "y2", (double) height,
			       NULL );
	ensure_nonzero_step_increments (etfc);
}

static void resize(GnomeCanvas *canvas, ETableFieldChooser *etfc)
{
	double height;
	g_object_get(etfc->item,
		     "height", &height,
		     NULL);

	height = MAX(height, etfc->last_alloc.height);

	gnome_canvas_set_scroll_region (GNOME_CANVAS(etfc->canvas), 0, 0, etfc->last_alloc.width - 1, height - 1);
	gnome_canvas_item_set( etfc->rect,
			       "x2", (double) etfc->last_alloc.width,
			       "y2", (double) height,
			       NULL );	
	ensure_nonzero_step_increments (etfc);
}

static void
e_table_field_chooser_init (ETableFieldChooser *etfc)
{
	GladeXML *gui;
	GtkWidget *widget;
	gchar *filename = g_build_filename (GAL_GLADEDIR,
					    "e-table-field-chooser.glade",
					    NULL);
	gui = glade_xml_new (filename, NULL, E_I18N_DOMAIN);
	g_free (filename);
	etfc->gui = gui;

	widget = glade_xml_get_widget(gui, "vbox-top");
	if (!widget) {
		return;
	}
	gtk_widget_reparent(widget,
			    GTK_WIDGET(etfc));

	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	etfc->canvas = GNOME_CANVAS(glade_xml_get_widget(gui, "canvas-buttons"));

	etfc->rect = gnome_canvas_item_new(gnome_canvas_root( GNOME_CANVAS( etfc->canvas ) ),
					   gnome_canvas_rect_get_type(),
					   "x1", (double) 0,
					   "y1", (double) 0,
					   "x2", (double) 100,
					   "y2", (double) 100,
					   "fill_color", "white",
					   NULL );

	etfc->item = gnome_canvas_item_new(gnome_canvas_root(etfc->canvas),
					   e_table_field_chooser_item_get_type(),
					   "width", (double) 100,
					   "full_header", etfc->full_header,
					   "header", etfc->header,
					   "dnd_code", etfc->dnd_code,
					   NULL );

	g_signal_connect( etfc->canvas, "reflow",
			  G_CALLBACK ( resize ),
			  etfc);

	gnome_canvas_set_scroll_region ( GNOME_CANVAS( etfc->canvas ),
					 0, 0,
					 100, 100 );

	/* Connect the signals */
	g_signal_connect (etfc->canvas, "size_allocate",
			  G_CALLBACK (allocate_callback),
			  etfc);

	gtk_widget_pop_colormap ();
	gtk_widget_show_all(widget);
}

static void
e_table_field_chooser_dispose (GObject *object)
{
	ETableFieldChooser *etfc = E_TABLE_FIELD_CHOOSER(object);

	g_free (etfc->dnd_code);
	etfc->dnd_code = NULL;

	if (etfc->full_header)
		g_object_unref (etfc->full_header);
	etfc->full_header = NULL;
	
	if (etfc->header)
		g_object_unref (etfc->header);
	etfc->header = NULL;

	if (etfc->gui)
		g_object_unref (etfc->gui);
	etfc->gui = NULL;

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
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
	ETableFieldChooser *etfc = E_TABLE_FIELD_CHOOSER(object);

	switch (prop_id){
	case PROP_DND_CODE:
		g_free(etfc->dnd_code);
		etfc->dnd_code = g_strdup(g_value_get_string(value));
		if (etfc->item)
			g_object_set(etfc->item,
				     "dnd_code", etfc->dnd_code,
				     NULL);
		break;
	case PROP_FULL_HEADER:
		if (etfc->full_header)
			g_object_unref (etfc->full_header);
		if (g_value_get_object (value))
			etfc->full_header = E_TABLE_HEADER(g_value_get_object (value));
		else
			etfc->full_header = NULL;
		if (etfc->full_header)
			g_object_ref (etfc->full_header);
		if (etfc->item)
			g_object_set(etfc->item,
				     "full_header", etfc->full_header,
				     NULL);
		break;
	case PROP_HEADER:
		if (etfc->header)
			g_object_unref (etfc->header);
		if (g_value_get_object (value))
			etfc->header = E_TABLE_HEADER(g_value_get_object (value));
		else
			etfc->header = NULL;
		if (etfc->header)
			g_object_ref (etfc->header);
		if (etfc->item)
			g_object_set(etfc->item,
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
	ETableFieldChooser *etfc = E_TABLE_FIELD_CHOOSER(object);

	switch (prop_id) {
	case PROP_DND_CODE:
		g_value_set_string (value, g_strdup (etfc->dnd_code));
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
