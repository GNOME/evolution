/*
 * Test code for the ETable package
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 */
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <gnome.h>
#include "e-util/e-canvas-utils.h"
#include "e-util/e-cursors.h"
#include "e-util/e-util.h"
#include "e-table-simple.h"
#include "e-table-header.h"
#include "e-table-header-item.h"
#include "e-table-item.h"
#include "e-cell-text.h"
#include "e-cell-toggle.h"

#include "table-test.h"

#define LINES 4

static struct {
	int   value;
	char *string;
} my_table [LINES] = {
	{ 0, "You are not" },
	{ 1, "A beautiful and unique " },
	{ 0, "Snowflake" },
	{ 2, "You are not your wallet" },
};
/*
 * ETableSimple callbacks
 */
static int
col_count (ETableModel *etc, void *data)
{
	return 2;
}

static int
row_count (ETableModel *etc, void *data)
{
	return LINES;
}

static void *
value_at (ETableModel *etc, int col, int row, void *data)
{
	g_assert (col < 2);
	g_assert (row < LINES);

	if (col == 0)
		return GINT_TO_POINTER (my_table [row].value);
	else
		return my_table [row].string;
	
}

static void
set_value_at (ETableModel *etc, int col, int row, const void *val, void *data)
{
	g_assert (col < 2);
	g_assert (row < LINES);

	if (col == 0){
		my_table [row].value = GPOINTER_TO_INT (val);
		printf ("Value at %d,%d set to %d\n", col, row, GPOINTER_TO_INT (val));
	} else {
		my_table [row].string = g_strdup (val);
		printf ("Value at %d,%d set to %s\n", col, row, (char *) val);
	}
}

static gboolean
is_cell_editable (ETableModel *etc, int col, int row, void *data)
{
	return TRUE;
}

static void *
duplicate_value (ETableModel *etc, int col, const void *value, void *data)
{
  if (col == 0){
    return (void *)value;
  } else {
    return g_strdup(value);
  }
}

static void
free_value (ETableModel *etc, int col, void *value, void *data)
{
  if (col != 0){
    g_free(value);
  }
}

static void
set_canvas_size (GnomeCanvas *canvas, GtkAllocation *alloc)
{
	gnome_canvas_set_scroll_region (canvas, 0, 0, alloc->width, alloc->height);
}

static void
thaw (ETableModel *etc, void *data)
{
	e_table_model_changed(etc);
}

void
multi_cols_test (void)
{
	GtkWidget *canvas, *window;
	ETableModel *e_table_model;
	ETableHeader *e_table_header, *e_table_header_multiple;
	ETableCol *col_0, *col_1;
	ECell *cell_left_just, *cell_image_toggle;
	GnomeCanvasItem *item;

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
	
	e_table_model = e_table_simple_new (
		col_count, row_count, value_at,
		set_value_at, is_cell_editable,
		duplicate_value, free_value,
		thaw, NULL);

	/*
	 * Header
	 */
	e_table_header = e_table_header_new ();

	cell_left_just = e_cell_text_new (e_table_model, NULL, GTK_JUSTIFY_LEFT, TRUE);

	{
		GdkPixbuf **images = g_new (GdkPixbuf *, 3);
		int i;
		
		images [0] = gdk_pixbuf_new_from_file ("image1.png");
		images [1] = gdk_pixbuf_new_from_file ("image2.png");
		images [2] = gdk_pixbuf_new_from_file ("image3.png");

		cell_image_toggle = e_cell_toggle_new (0, 3, images);

		for (i = 0; i < 3; i++)
			gdk_pixbuf_unref (images [i]);
		
		g_free (images);
	} 
					       
	col_1 = e_table_col_new (1, "Item Name", 180, 20, cell_left_just, g_str_compare, TRUE);
	e_table_header_add_column (e_table_header, col_1, 0);

	col_0 = e_table_col_new (0, "A", 48, 48, cell_image_toggle, g_int_compare, TRUE);
	e_table_header_add_column (e_table_header, col_0, 1);

	/*
	 * Second test
	 */
	e_table_header_multiple = e_table_header_new ();
	e_table_header_add_column (e_table_header_multiple, col_0, 0);
	e_table_header_add_column (e_table_header_multiple, col_1, 1);
	e_table_header_add_column (e_table_header_multiple, col_1, 2);
	
	/*
	 * GUI
	 */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	canvas = gnome_canvas_new ();

	gtk_signal_connect (GTK_OBJECT (canvas), "size_allocate",
			    GTK_SIGNAL_FUNC (set_canvas_size), NULL);
	
	gtk_container_add (GTK_CONTAINER (window), canvas);
	gtk_widget_show_all (window);

	gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (canvas)),
		e_table_header_item_get_type (),
		"ETableHeader", e_table_header,
		"x",  0,
		"y",  0,
		NULL);

	item = gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (canvas)),
		e_table_item_get_type (),
		"ETableHeader", e_table_header,
		"ETableModel", e_table_model,
		"drawgrid", TRUE,
		"drawfocus", TRUE,
		"spreadsheet", TRUE,
		NULL);

	e_canvas_item_move_absolute(item, 0, 30);
	
	gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (canvas)),
		e_table_header_item_get_type (),
		"ETableHeader", e_table_header_multiple,
		"x",  300,
		"y",  0,
		NULL);
	item = gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (canvas)),
		e_table_item_get_type (),
		"ETableHeader", e_table_header_multiple,
		"ETableModel", e_table_model,
		"drawgrid", TRUE,
		"drawfocus", TRUE,
		"spreadsheet", TRUE,
		NULL);
	e_canvas_item_move_absolute(item, 300, 30);
}





