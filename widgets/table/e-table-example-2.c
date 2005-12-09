/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-example-2.c
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

#include <stdio.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gal/e-util/e-cursors.h"

#include "e-table-simple.h"
#include "e-table-header.h"
#include "e-table-header-item.h"
#include "e-table-item.h"
#include "e-cell-text.h"
#include "e-cell-checkbox.h"
#include "e-table.h"

#include "table-test.h"

/*
One way in which we make it simpler to build an ETableModel is through
the ETableSimple class.  Instead of creating your own ETableModel
class, you simply create a new object of the ETableSimple class.  You
give it a bunch of functions that act as callbacks.

You also get to pass a void * to ETableSimple and it gets passed to
your callbacks.  This would be for having multiple models of the same
type.  This is just an example though, so we statically define all the
data and ignore the void *data parameter.

In our example we will be creating a table model with 6 columns and 10
rows.  This corresponds to having 6 different types of information and
10 different sets of data in our database.

The headers will be hard coded, as will be the example data.

*/

/*
  There are two different meanings to the word "column".  The first is
  the model column.  A model column corresponds to a specific type of
  data.  This is very much like the usage in a database table where a
  column is a field in the database.

  The second type of column is a view column.  A view column
  corresponds to a visually displayed column.  Each view column
  corresponds to a specific model column, though a model column may
  have any number of view columns associated with it, from zero to
  greater than one.

  Also, a view column doesn't necessarily depend on only one model
  column.  In some cases, the view column renderer can be given a
  reference to another column to get extra information about its
  display.
*/

#define ROWS 10
#define VIEW_COLS 4
#define PHYSICAL_COLS 5
#define VIRTUAL_COLS 6

#define IMPORTANCE_COLUMN 4
#define COLOR_COLUMN 5

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
		<column> 4 </column>     			       \
		<column> 1 </column>     			       \
		<column> 2 </column>     			       \
		<column> 3 </column>     			       \
	</columns-shown>                 			       \
	<grouping> <leaf column=\"1\" ascending=\"true\"/> </grouping> \
</ETableSpecification>"

char *headers[VIEW_COLS] = {
  "Email",
  "Full Name",
  "Address",
  "Phone"
};

/* Virtual Column list:
   0   Full Name
   1   Email
   2   Address
   3   Phone
   4   Importance field.  This field will be a boolean.  It also has a
       special header, so doesn't appear in the headers list.
   5   Color field.  This column is also not displayed.  It is also
       not stored in the database.  It's calculated based on the
       Importance field.
*/

char *table_data[ROWS][VIEW_COLS];
gboolean importance_data[ROWS];

/*
 * ETableSimple callbacks
 * These are the callbacks that define the behavior of our custom model.
 */

/* Since our model is a constant size, we can just return its size in
   the column and row count fields. */

static GdkColor *color1;
static GdkColor *color2;

static int
my_col_count (ETableModel *etc, void *data)
{
	return VIRTUAL_COLS;
}

static int
my_row_count (ETableModel *etc, void *data)
{
	return ROWS;
}

static void *
my_value_at (ETableModel *etc, int col, int row, void *data)
{
	if (col == COLOR_COLUMN){
		if (importance_data[row]){
			return color1;
		} else {
			return color2;
		}
	} else if (col == IMPORTANCE_COLUMN){
		return (gpointer) importance_data[row];
	} else {
		return (void *) table_data [row][col];
	}
}

static void
my_set_value_at (ETableModel *etc, int col, int row, const void *val, void *data)
{
	if (col == COLOR_COLUMN){ 
	} else if (col == IMPORTANCE_COLUMN){
		importance_data[row] = (gboolean) val;
	} else {
		g_free (table_data [row][col]);
		table_data [row][col] = g_strdup (val);
	}
}

static gboolean
my_is_cell_editable (ETableModel *etc, int col, int row, void *data)
{
	if (col == IMPORTANCE_COLUMN)
		return FALSE;
	else
		return TRUE;
}

static void *
my_duplicate_value (ETableModel *etc, int col, const void *value, void *data)
{
	if (col == COLOR_COLUMN){
		return (void *) value;
	} else if (col == IMPORTANCE_COLUMN){
		return (void *) value;
	} else {
		return g_strdup (value);
	}
}

static void
my_free_value (ETableModel *etc, int col, void *value, void *data)
{
	if (col == COLOR_COLUMN){
	} else if (col == IMPORTANCE_COLUMN){
	} else {
		g_free (value);
	}
}

static void *
my_initialize_value (ETableModel *etc, int col, void *data)
{
	if (col == COLOR_COLUMN){
		return NULL;
	} else if (col == IMPORTANCE_COLUMN){
		return NULL;
	} else {
		return g_strdup ("");
	}
}

static gboolean
my_value_is_empty (ETableModel *etc, int col, const void *value, void *data)
{
	if (col == COLOR_COLUMN){
		return value == NULL;
	} else if (col == IMPORTANCE_COLUMN){
		return value == NULL;
	} else {
		return !(value && *(char *)value);
	}
}

static char *
my_value_to_string (ETableModel *etc, int col, const void *value, void *data)
{
	if (col == COLOR_COLUMN){
		return g_strdup_printf("%d", (int) value);
	} else if (col == IMPORTANCE_COLUMN){
		return g_strdup_printf("%d", (int) value);
	} else {
		return g_strdup(value);
	}
}

/* We create a window containing our new table. */
static void
create_table ()
{
	GtkWidget *e_table, *window, *frame;
	ECell *cell_left_just;
	ECell *cell_checkbox;
	ETableHeader *e_table_header;
	int i, j;
	ETableModel *e_table_model = NULL;
	ETableCol *ecol;
	GdkPixbuf *pixbuf;

	/* First we fill in the simple data. */
	for (i = 0; i < ROWS; i++){
		for (j = 0; j < VIEW_COLS; j++){
			table_data[i][j] = g_strdup ("");
		}
		importance_data[i] = FALSE;
	}
	/* Next we create our model.  This uses the functions we defined
	   earlier. */
	e_table_model = e_table_simple_new (
					    my_col_count, my_row_count, my_value_at,
					    my_set_value_at, my_is_cell_editable,
					    my_duplicate_value, my_free_value,
					    my_initialize_value, my_value_is_empty,
					    my_value_to_string,
					    NULL);
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
	cell_left_just = e_cell_text_new (e_table_model, NULL, GTK_JUSTIFY_LEFT);
		
	/* Next we create a column object for each view column and add
	   them to the header.  We don't create a column object for
	   the importance column since it will not be shown. */
	for (i = 0; i < VIEW_COLS; i++){
		/* Create the column. */
		ETableCol *ecol = e_table_col_new (
						   i, headers [i],
						   1.0, 20, cell_left_just,
						   e_str_compare, TRUE);
		/* Add it to the header. */
		e_table_header_add_column (e_table_header, ecol, i);
	}
       
	/* Next we add a special column for the check box. */

	cell_checkbox = e_cell_checkbox_new ();
	pixbuf = gdk_pixbuf_new_from_file ("clip.png");
	ecol = e_table_col_new_with_pixbuf (i, pixbuf, 0.0, 18, cell_checkbox, g_int_compare, TRUE);
	e_table_header_add_column (e_table_header, ecol, i);

	/*
	 * Setup GUI
	 */
	/* Here we create a window for our new table.  This window
	   will get shown and the person will be able to test their
	   item. */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	/* This frame is simply to get a bevel around our table. */
	frame = gtk_frame_new (NULL);
	/* Here we create the table.  We give it the three pieces of
	   the table we've created, the header, the model, and the
	   initial layout.  It does the rest.  */
	e_table = e_table_new (e_table_header, e_table_model, INITIAL_SPEC);

	/* Build the gtk widget hierarchy. */
	gtk_container_add (GTK_CONTAINER (frame), e_table);
	gtk_container_add (GTK_CONTAINER (window), frame);

	/* Size the initial window. */
	gtk_widget_set_usize (window, 200, 200);
	/* Show it all. */
	gtk_widget_show_all (window);
}



int
main (int argc, char *argv [])
{
	gnome_init ("TableExample", "TableExample", argc, argv);
	e_cursors_init ();

	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	create_table ();
	
	gtk_main ();

	e_cursors_shutdown ();
	return 0;
}
