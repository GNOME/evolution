/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This code is GPL. */
#include <stdio.h>
#include <string.h>
#include <gnome.h>
#include "e-util/e-cursors.h"
#include "e-tree-gnode.h"
#include "e-table-header.h"
#include "e-table-header-item.h"
#include "e-table-item.h"
#include "e-cell-text.h"
#include "e-cell-tree.h"
#include "e-cell-checkbox.h"
#include "e-table.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#define ROWS 10
#define COLS 4

#define IMPORTANCE_COLUMN 4
#define COLOR_COLUMN 5

/*
 * Here we define the initial layout of the table.  This is an xml
 * format that allows you to change the initial ordering of the
 * columns or to do sorting or grouping initially.  This specification
 * shows all 5 columns, but moves the importance column nearer to the
 * front.  It also sorts by the "Full Name" column (ascending.)
 * Sorting and grouping take the model column as their arguments
 * (sorting is specified by the "column" argument to the leaf elemnt.
 */

#define INITIAL_SPEC "<ETableSpecification>                    	       \
	<columns-shown>                  			       \
		<column> 0 </column>     			       \
		<column> 4 </column>     			       \
		<column> 1 </column>     			       \
		<column> 2 </column>     			       \
		<column> 3 </column>     			       \
	</columns-shown>                 			       \
	<grouping></grouping>                                         \
</ETableSpecification>"

/*
 * Virtual Column list:
 * 0   Subject
 * 1   Full Name
 * 2   Email
 * 3   Date
 */
char *headers [COLS] = {
  "Subject",
  "Full Name",
  "Email",
  "Date"
};

/*
 * ETableSimple callbacks
 * These are the callbacks that define the behavior of our custom model.
 */

/*
 * Since our model is a constant size, we can just return its size in
 * the column and row count fields.
 */

/* This function returns the number of columns in our ETableModel. */
static int
my_col_count (ETableModel *etc, void *data)
{
	return COLS;
}

/* This function returns the value at a particular point in our ETableModel. */
static void *
my_value_at (ETreeModel *etc, GNode *node, int col, void *data)
{
	switch (col) {
	case 0: return "Re: Two things";
	case 1: return "Chris Toshok";
	case 2: return "toshok@helixcode.com";
	case 3: return "Jun 07 2000";
	default: return NULL;
	}
}

/* This function sets the value at a particular point in our ETableModel. */
static void
my_set_value_at (ETableModel *etc, GNode *node, int col, const void *val, void *data)
{
}

/* This function returns whether a particular cell is editable. */
static gboolean
my_is_editable (ETableModel *etc, GNode *node, int col, void *data)
{
	return FALSE;
}

/* This function duplicates the value passed to it. */
static void *
my_duplicate_value (ETableModel *etc, int col, const void *value, void *data)
{
	return g_strdup (value);
}

/* This function frees the value passed to it. */
static void
my_free_value (ETableModel *etc, int col, void *value, void *data)
{
	g_free (value);
}

/* This function is for when the model is unfrozen.  This can mostly
   be ignored for simple models.  */
static void
my_thaw (ETableModel *etc, void *data)
{
}

/* We create a window containing our new tree. */
static void
create_tree (void)
{
	GtkWidget *e_table, *window, *frame;
	ECell *cell_left_just;
	ECell *cell_tree;
	ETableHeader *e_table_header;
	int i, j;
	ETreeModel *e_tree_model = NULL;
	GNode *root_node;

	/* create a root node with 5 children */
	root_node = g_node_new (NULL);
	for (i = 0; i < 5; i++){
		GNode *n = g_node_insert (root_node, 0, g_node_new(NULL));
		for (j = 0; j < 5; j ++) {
			g_node_insert (n, 0, g_node_new(NULL));
		}
	}

	/* Next we create our model.  This uses the functions we defined
	   earlier. */
	e_tree_model = e_tree_gnode_new (root_node,
					 my_value_at,
					 NULL);

	/*
	 * Next we create a header.  The ETableHeader is used in two
	 * different way.  The first is the full_header.  This is the
	 * list of possible columns in the view.  The second use is
	 * completely internal.  Many of the ETableHeader functions are
	 * for that purpose.  The only functions we really need are
	 * e_table_header_new and e_table_header_add_col.
	 *
	 * First we create the header.
	 */
	e_table_header = e_table_header_new ();
	
	/*
	 * Next we have to build renderers for all of the columns.
	 * Since all our columns are text columns, we can simply use
	 * the same renderer over and over again.  If we had different
	 * types of columns, we could use a different renderer for
	 * each column.
	 */
	cell_left_just = e_cell_text_new (E_TABLE_MODEL(e_tree_model), NULL, GTK_JUSTIFY_LEFT);
	
	/* 
	 * This renderer is used for the tree column (the leftmost one), and
	 * has as its subcell renderer the text renderer.  this means that
	 * text is displayed to the right of the tree pipes.
	 */
	cell_tree = e_cell_tree_new (E_TABLE_MODEL(e_tree_model), TRUE, cell_left_just);

	/*
	 * Next we create a column object for each view column and add
	 * them to the header.  We don't create a column object for
	 * the importance column since it will not be shown.
	 */
	for (i = 0; i < COLS; i++) {
		/* Create the column. */
		ETableCol *ecol = e_table_col_new (
						   i, headers [i],
						   80, 20,
						   i == 0 ? cell_tree
						   : cell_left_just,
						   g_str_compare, TRUE);
		/* Add it to the header. */
		e_table_header_add_column (e_table_header, ecol, i);
	}

	/*
	 * Here we create a window for our new table.  This window
	 * will get shown and the person will be able to test their
	 * item.
	 */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	/* This frame is simply to get a bevel around our table. */
	frame = gtk_frame_new (NULL);

	/*
	 * Here we create the table.  We give it the three pieces of
	 * the table we've created, the header, the model, and the
	 * initial layout.  It does the rest.
	 */
	e_table = e_table_new (e_table_header, E_TABLE_MODEL(e_tree_model), INITIAL_SPEC);

	if (!e_table) printf ("BAH!");

	/* Build the gtk widget hierarchy. */
	gtk_container_add (GTK_CONTAINER (frame), e_table);
	gtk_container_add (GTK_CONTAINER (window), frame);

	/* Size the initial window. */
	gtk_widget_set_usize (window, 200, 200);

	/* Show it all. */
	gtk_widget_show_all (window);
}

/* This is the main function which just initializes gnome and call our create_tree function */

int
main (int argc, char *argv [])
{
	gnome_init ("TableExample", "TableExample", argc, argv);
	e_cursors_init ();

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	create_tree ();
	
	gtk_main ();

	e_cursors_shutdown ();
	return 0;
}

