/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * demo.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <string.h>
#include <gnome.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include "e-util/e-cursors.h"
#include "e-canvas.h"
#include "e-table-simple.h"
#include "e-table-header.h"
#include "e-table-header-item.h"
#include "e-table-item.h"
#include "e-cell-text.h"
#include "e-cell-checkbox.h"
#include "e-table.h"
#include "e-reflow.h"
#include "e-minicard.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "e-test-model.h"

#include "addressbook-widget.h"

#define COLS 4

/* Here we define the initial layout of the table.  This is an xml
   format that allows you to change the initial ordering of the
   columns or to do sorting or grouping initially.  This specification
   shows all 5 columns, but moves the importance column nearer to the
   front.  It also sorts by the "Full Name" column (ascending.)
   Sorting and grouping take the model column as their arguments
   (sorting is specified by the "column" argument to the leaf elemnt. */
#define INITIAL_SPEC "<ETableSpecification>                    	       \
	<columns-shown>                  			       \
		<column> 0 </column>     			       \
		<column> 1 </column>     			       \
		<column> 2 </column>     			       \
		<column> 3 </column>     			       \
	</columns-shown>                 			       \
	<grouping> <leaf column=\"1\" ascending=\"1\"/> </grouping>    \
</ETableSpecification>"

char *headers[COLS] = {
  "Email",
  "Full Name",
  "Address",
  "Phone"
};

static int window_count = 0;
static GHashTable *models = NULL;

static void
remove_model(ETableModel *model, gchar *filename)
{
	g_hash_table_remove(models, filename);
	g_free(filename);
}

static ETestModel *
get_model(char *filename)
{
	ETestModel *model;
	gboolean free_filename = FALSE;

	if ( filename == NULL ) {
		filename = gnome_util_prepend_user_home("addressbook.xml");
		free_filename = TRUE;
	}

	if ( models == NULL ) {
		models = g_hash_table_new(g_str_hash, g_str_equal);
	}

	model = g_hash_table_lookup(models, filename);
	if ( model ) {
		if (free_filename)
			g_free(filename);
		return model;
	}

	if ( !free_filename )
		filename = g_strdup(filename);
	
	model = E_TEST_MODEL(e_test_model_new(filename));
	g_hash_table_insert(models,
			    filename, model);
	gtk_signal_connect(GTK_OBJECT(model), "destroy",
			   GTK_SIGNAL_FUNC(remove_model), filename);

	return model;
}

static void
add_address_cb(GtkWidget *button, gpointer data)
{
	View *view = (View *) data;
	Address *newadd = g_new(Address, 1);
	newadd->email = g_strdup("");
	newadd->phone = g_strdup("");
	newadd->full_name = g_strdup("");
	newadd->street = g_strdup("");
	e_test_model_add_column (view->model, newadd);
}

static void
rebuild_reflow(ETableModel *model, gpointer data)
{
	int i;
	View *view = (View *) data;
	Reflow *reflow = view->reflow;
	if (!reflow)
		return;
	gtk_object_destroy(GTK_OBJECT(reflow->reflow));
	reflow->reflow = gnome_canvas_item_new( gnome_canvas_root( GNOME_CANVAS( reflow->canvas ) ),
					      e_reflow_get_type(),
					      "height", (double) reflow->last_alloc.height,
					      "minimum_width", (double) reflow->last_alloc.width,
					      NULL );

	for ( i = 0; i < view->model->data_count; i++ )
		{
			GnomeCanvasItem *item;
			item = gnome_canvas_item_new( GNOME_CANVAS_GROUP(reflow->reflow),
						      e_minicard_get_type(),
						      "model", view->model,
						      "row", i,
						      NULL);
			e_reflow_add_item(E_REFLOW(reflow->reflow), item);
		}
	e_canvas_item_request_reflow(reflow->reflow);
}

static void
destroy_reflow(View *view)
{
	Reflow *reflow = view->reflow;
	if ( !reflow )
		return;

	gtk_signal_disconnect(GTK_OBJECT(view->model),
			      reflow->model_changed_id);
	g_free(reflow);
	gtk_object_unref(GTK_OBJECT(view->model));
	view->reflow = NULL;
}

static void
destroy_callback(GtkWidget *app, gpointer data)
{
	View *view = (View *)data;
	if ( view->reflow ) {
		destroy_reflow(view);
	}
	gtk_object_unref(GTK_OBJECT(view->model));
	g_free(view);
	window_count --;
	if ( window_count <= 0 )
		gtk_main_quit();
}

static void allocate_callback(GtkWidget *canvas, GtkAllocation *allocation, gpointer data)
{
	double width;
	View *view = (View *)data;
	Reflow *reflow = view->reflow;
	if ( !reflow )
		return;
	reflow->last_alloc = *allocation;
	gnome_canvas_item_set( reflow->reflow,
			       "height", (double) allocation->height,
			       NULL );
	gnome_canvas_item_set( reflow->reflow,
			       "minimum_width", (double) allocation->width,
			       NULL );
	gtk_object_get(GTK_OBJECT(reflow->reflow),
		       "width", &width,
		       NULL);
	width = MAX(width, allocation->width);
	gnome_canvas_set_scroll_region(GNOME_CANVAS( reflow->canvas ), 0, 0, width, allocation->height );
	gnome_canvas_item_set( reflow->rect,
			       "x2", (double) width,
			       "y2", (double) allocation->height,
			       NULL );
}

static void resize(ECanvas *canvas, gpointer data)
{
	double width;
	View *view = (View *)data;
	Reflow *reflow = view->reflow;
	if ( !reflow )
		return;
  	gtk_object_get(GTK_OBJECT(reflow->reflow),
		       "width", &width,
		       NULL);
	width = MAX(width, reflow->last_alloc.width);
	gnome_canvas_set_scroll_region(GNOME_CANVAS(reflow->canvas), 0, 0, width, reflow->last_alloc.height );
	gnome_canvas_item_set( reflow->rect,
			       "x2", (double) width,
			       "y2", (double) reflow->last_alloc.height,
			       NULL );	
}

static void
canvas_realized(GtkLayout *layout, View *view)
{
	gdk_window_set_back_pixmap( layout->bin_window, NULL, FALSE);
}

static GtkWidget *
create_reflow(View *view)
{
	GtkWidget *inner_vbox;
	GtkWidget *scrollbar;
	int i;
	Reflow *reflow = g_new(Reflow, 1);
	view->reflow = reflow;

	view->type = VIEW_TYPE_REFLOW;

	/* Next we create our model.  This uses the functions we defined
	   earlier. */

	inner_vbox = gtk_vbox_new(FALSE, 0);
	reflow->canvas = e_canvas_new();
	reflow->rect = gnome_canvas_item_new( gnome_canvas_root( GNOME_CANVAS( reflow->canvas ) ),
					    gnome_canvas_rect_get_type(),
					    "x1", (double) 0,
					    "y1", (double) 0,
					    "x2", (double) 100,
					    "y2", (double) 100,
					    "fill_color", "white",
					    NULL );
	reflow->reflow = gnome_canvas_item_new( gnome_canvas_root( GNOME_CANVAS( reflow->canvas ) ),
					      e_reflow_get_type(),
					      "height", (double) 100,
					      "minimum_width", (double) 100,
					      NULL );
	/* Connect the signals */
	gtk_signal_connect( GTK_OBJECT( reflow->canvas ), "reflow",
			    GTK_SIGNAL_FUNC( resize ),
			    ( gpointer ) view);
	
	for ( i = 0; i < view->model->data_count; i++ )
		{
			GnomeCanvasItem *item;
			item = gnome_canvas_item_new( GNOME_CANVAS_GROUP(reflow->reflow),
						      e_minicard_get_type(),
						      "model", view->model,
						      "row", i,
						      NULL);
			e_reflow_add_item(E_REFLOW(reflow->reflow), item);
		}
	gnome_canvas_set_scroll_region ( GNOME_CANVAS( reflow->canvas ),
					 0, 0,
					 100, 100 );

	scrollbar = gtk_hscrollbar_new(gtk_layout_get_hadjustment(GTK_LAYOUT(reflow->canvas)));

	gtk_signal_connect( GTK_OBJECT( reflow->canvas ), "size_allocate",
			    GTK_SIGNAL_FUNC( allocate_callback ),
			    ( gpointer ) view );

	gtk_signal_connect( GTK_OBJECT(reflow->canvas), "realize",
			    GTK_SIGNAL_FUNC(canvas_realized), view);
	
	reflow->model_changed_id = gtk_signal_connect(GTK_OBJECT( view->model ), "model_changed",
						      GTK_SIGNAL_FUNC(rebuild_reflow), view);

	gtk_object_ref(GTK_OBJECT(view->model));

	/* Build the gtk widget hierarchy. */
	gtk_box_pack_start(GTK_BOX(inner_vbox), reflow->canvas, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(inner_vbox), scrollbar, FALSE, FALSE, 0);

	return inner_vbox;
}

/* We create a window containing our new table. */
static GtkWidget *
create_table(View *view)
{
	ECell *cell_left_just;
	ETableHeader *e_table_header;
	GtkWidget *e_table;
	int i;

	view->type = VIEW_TYPE_TABLE;

	/*
	  Next we create a header.  The ETableHeader is used in two
	  different way.  The first is the full_header.  This is the
	  list of possible columns in the view.  The second use is
	  completely internal.  Many of the ETableHeader functions are
	  for that purpose.  The only functions we really need are
	  e_table_header_new and e_table_header_add_col.

	  First we create the header.  */
	e_table_header = e_table_header_new ();
	
	/* Next we have to build renderers for all of the columns.
	   Since all our columns are text columns, we can simply use
	   the same renderer over and over again.  If we had different
	   types of columns, we could use a different renderer for
	   each column. */
	cell_left_just = e_cell_text_new (E_TABLE_MODEL(view->model), NULL, GTK_JUSTIFY_LEFT);
		
	/* Next we create a column object for each view column and add
	   them to the header.  We don't create a column object for
	   the importance column since it will not be shown. */
	for (i = 0; i < LAST_COL; i++){
		/* Create the column. */
		ETableCol *ecol = e_table_col_new (
						   i, headers [i],
						   80, 20, cell_left_just,
						   g_str_compare, TRUE);
		/* Add it to the header. */
		e_table_header_add_column (e_table_header, ecol, i);
	}

	/* Here we create the table.  We give it the three pieces of
	   the table we've created, the header, the model, and the
	   initial layout.  It does the rest.  */
	e_table = e_table_new_from_spec_file (e_table_header, E_TABLE_MODEL(view->model), "spec");

#if 0
	gtk_signal_connect(GTK_OBJECT(E_TABLE(e_table)->sort_info), "sort_info_changed",
			   GTK_SIGNAL_FUNC(queue_header_save), e_table->sort_info);

	gtk_signal_connect(GTK_OBJECT(E_TABLE(e_table)->header), "structure_change",
			   GTK_SIGNAL_FUNC(queue_header_save), e_table->sort_info);
	gtk_signal_connect(GTK_OBJECT(E_TABLE(e_table)->header), "dimension_change",
			   GTK_SIGNAL_FUNC(queue_header_save), e_table->sort_info);
#endif

	return e_table;
}

void
change_type(View *view, ViewType type)
{
	gtk_object_ref(GTK_OBJECT(view->model));
	if (view->reflow)
		destroy_reflow(view);
	gtk_widget_destroy(view->child);
	switch(type) {
	case VIEW_TYPE_REFLOW:
		view->child = create_reflow(view);
		break;
	case VIEW_TYPE_TABLE:
		view->child = create_table(view);
		break;
	}
	gtk_container_add(GTK_CONTAINER(view->frame), view->child);
	gtk_widget_show_all(view->child);
	gtk_object_unref(GTK_OBJECT(view->model));
}

View *
create_view(void)
{
	View *view = g_new(View, 1);
	ViewType type = VIEW_TYPE_REFLOW;
	GtkWidget *button;

	view->reflow = NULL;

	view->model = get_model(NULL);

	/* This frame is simply to get a bevel around our table. */
	view->frame = gtk_frame_new (NULL);

	switch(type) {
	case VIEW_TYPE_REFLOW:
		view->child = create_reflow(view);
		break;
	case VIEW_TYPE_TABLE:
		view->child = create_table(view);
		break;
	}


	gtk_signal_connect( GTK_OBJECT( view->child ), "destroy",
			    GTK_SIGNAL_FUNC( destroy_callback ),
			    view );

	/*	
	vbox = gtk_vbox_new(FALSE, 0);
	button = gtk_button_new_with_label("Add address");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(add_address_cb), view);

	change_button = gtk_button_new_with_label("Change View");
	gtk_signal_connect(GTK_OBJECT(change_button), "clicked",
			   GTK_SIGNAL_FUNC(change_callback), view);
	*/
	/* Build the gtk widget hierarchy. */

	view->widget = gtk_vbox_new(FALSE, 0);
	
	button = gtk_button_new_with_label("Add address");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(add_address_cb), view);

	gtk_container_add (GTK_CONTAINER (view->frame), view->child);
	gtk_box_pack_start (GTK_BOX (view->widget), view->frame, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (view->widget), button, FALSE, FALSE, 0);
	/*
	gtk_box_pack_start (GTK_BOX (vbox), change_button, FALSE, FALSE, 0);
	*/
	/* Show it all. */
	gtk_widget_show_all (view->widget);
	gtk_object_ref(GTK_OBJECT(view->model));
	gtk_object_sink(GTK_OBJECT(view->model));
	window_count ++;
	return view;

}

#if 0
static void
change_callback(GtkWidget *button, View *view)
{
	if (view->type == VIEW_TYPE_REFLOW)
		change_type(view, VIEW_TYPE_TABLE);
	else
		change_type(view, VIEW_TYPE_REFLOW);
}

static GtkWidget *
create_window(char *filename, ViewType type)
{
	GtkWidget *button;
	GtkWidget *change_button;
	GtkWidget *vbox;
	View *view = g_new(View, 1);

	view->reflow = NULL;

	view->model = get_model(filename);

	/* Here we create a window for our new table.  This window
	   will get shown and the person will be able to test their
	   item. */
	view->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_signal_connect( GTK_OBJECT( view->window ), "destroy",
			    GTK_SIGNAL_FUNC( destroy_callback ),
			    view );

	/* This frame is simply to get a bevel around our table. */
	view->frame = gtk_frame_new (NULL);

	switch(type) {
	case VIEW_TYPE_REFLOW:
		view->child = create_reflow(view);
		break;
	case VIEW_TYPE_TABLE:
		view->child = create_table(view);
		break;
	}


	vbox = gtk_vbox_new(FALSE, 0);
	
	button = gtk_button_new_with_label("Add address");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(add_address_cb), view);

	change_button = gtk_button_new_with_label("Change View");
	gtk_signal_connect(GTK_OBJECT(change_button), "clicked",
			   GTK_SIGNAL_FUNC(change_callback), view);

	/* Build the gtk widget hierarchy. */

	gtk_container_add (GTK_CONTAINER (view->frame), view->child);
	gtk_box_pack_start (GTK_BOX (vbox), view->frame, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), change_button, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view->window), vbox);

	/* Size the initial window. */
	gtk_widget_set_usize (view->window, 200, 200);
	/* Show it all. */
	gtk_widget_show_all (view->window);
	gtk_object_sink(GTK_OBJECT(view->model));
	window_count ++;
	return view->window;
}

/* This is the main function which just initializes gnome and call our create_table function */

int
main (int argc, char *argv [])
{
	gnome_init ("TableExample", "TableExample", argc, argv);
	e_cursors_init ();

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	create_window("addressbook.xml", VIEW_TYPE_TABLE);
	create_window("addressbook.xml", VIEW_TYPE_TABLE);
	create_window("addressbook.xml", VIEW_TYPE_TABLE);
	create_window("addressbook.xml", VIEW_TYPE_REFLOW);
	create_window("addressbook2.xml", VIEW_TYPE_TABLE);
	create_window("addressbook2.xml", VIEW_TYPE_REFLOW);

	gtk_main ();

	e_cursors_shutdown ();
	return 0;
}
#endif
