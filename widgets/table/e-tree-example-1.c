/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This code is GPL. */
#include <stdio.h>
#include <string.h>
#include <gnome.h>
#include "e-util/e-cursors.h"
#include "e-table-header.h"
#include "e-table-header-item.h"
#include "e-table-item.h"
#include "e-cell-text.h"
#include "e-cell-tree.h"
#include "e-cell-checkbox.h"
#include "e-table.h"
#include "e-tree-simple.h"
#include "libgnomeprint/gnome-print.h"
#include "libgnomeprint/gnome-print-preview.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "tree-expanded.xpm"
#include "tree-unexpanded.xpm"

GdkPixbuf *tree_expanded_pixbuf;
GdkPixbuf *tree_unexpanded_pixbuf;

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

GtkWidget *e_table;

/*
 * ETreeSimple callbacks
 * These are the callbacks that define the behavior of our custom model.
 */

/* This function returns the value at a particular point in our ETreeModel. */
static void *
my_value_at (ETreeModel *etm, ETreePath *path, int col, void *model_data)
{
	switch (col) {
	case 0: return e_tree_model_node_get_data (etm, path);
	case 1: return "Chris Toshok";
	case 2: return "toshok@helixcode.com";
	case 3: return "Jun 07 2000";
	default: return NULL;
	}
}

/* This function sets the value at a particular point in our ETreeModel. */
static void
my_set_value_at (ETreeModel *etm, ETreePath *path, int col, const void *val, void *model_data)
{
	if (col == 0) {
		char *str = e_tree_model_node_get_data (etm, path);
		g_free (str);
		e_tree_model_node_set_data (etm, path, g_strdup(val));
	}
}

/* This function returns whether a particular cell is editable. */
static gboolean
my_is_editable (ETreeModel *etm, ETreePath *path, int col, void *model_data)
{
	if (col == 0)
		return TRUE;
	else
		return FALSE;
}

static void
toggle_root (GtkButton *button, gpointer data)
{
	ETreeModel *e_tree_model = (ETreeModel*)data;
	e_tree_model_root_node_set_visible (e_tree_model, !e_tree_model_root_node_is_visible (e_tree_model));
}

static void
add_sibling (GtkButton *button, gpointer data)
{
	ETreeModel *e_tree_model = E_TREE_MODEL (data);
	int selected_row;
	ETreePath *selected_node;
	ETreePath *parent_node;

	selected_row = e_table_get_selected_view_row (E_TABLE (e_table));
	if (selected_row == -1)
		return;

	selected_node = e_tree_model_node_at_row (e_tree_model, selected_row);
	g_assert (selected_node);

	parent_node = e_tree_model_node_get_parent (e_tree_model, selected_node);

	e_tree_model_node_insert_before (e_tree_model, parent_node,
					 selected_node,
					 NULL, NULL,
					 g_strdup("User added sibling"));

}

static void
add_child (GtkButton *button, gpointer data)
{
	ETreeModel *e_tree_model = E_TREE_MODEL (data);
	int selected_row;
	ETreePath *selected_node;

	selected_row = e_table_get_selected_view_row (E_TABLE (e_table));
	if (selected_row == -1)
		return;

	selected_node = e_tree_model_node_at_row (e_tree_model, selected_row);
	g_assert (selected_node);

	e_tree_model_node_insert (e_tree_model, selected_node,
				  0, NULL, NULL,
				  g_strdup("User added child"));
}

static void
remove_node (GtkButton *button, gpointer data)
{
	ETreeModel *e_tree_model = E_TREE_MODEL (data);
	int selected_row;
	char *str;
	ETreePath *selected_node;

	selected_row = e_table_get_selected_view_row (E_TABLE (e_table));
	if (selected_row == -1)
		return;

	selected_node = e_tree_model_node_at_row (e_tree_model, selected_row);
	g_assert (selected_node);

	if (e_tree_model_node_get_children (e_tree_model, selected_node, NULL) > 0)
		return;

	str = (char*)e_tree_model_node_remove (e_tree_model, selected_node);
	printf ("removed node %s\n", str);
	g_free (str);
}

static void
expand_all (GtkButton *button, gpointer data)
{
	ETreeModel *e_tree_model = E_TREE_MODEL (data);
	int selected_row;
	ETreePath *selected_node;

	selected_row = e_table_get_selected_view_row (E_TABLE (e_table));
	if (selected_row == -1)
		return;

	selected_node = e_tree_model_node_at_row (e_tree_model, selected_row);
	g_assert (selected_node);

	e_tree_model_node_set_expanded_recurse (e_tree_model, selected_node, TRUE);
}

static void
collapse_all (GtkButton *button, gpointer data)
{
	ETreeModel *e_tree_model = E_TREE_MODEL (data);
	int selected_row;
	ETreePath *selected_node;

	selected_row = e_table_get_selected_view_row (E_TABLE (e_table));
	if (selected_row == -1)
		return;

	selected_node = e_tree_model_node_at_row (e_tree_model, selected_row);
	g_assert (selected_node);

	e_tree_model_node_set_expanded_recurse (e_tree_model, selected_node, FALSE);
}

static void
print_tree (GtkButton *button, gpointer data)
{
	EPrintable *printable = e_table_get_printable (E_TABLE (e_table));
	GnomePrintContext *gpc;

	gpc = gnome_print_context_new (gnome_printer_new_generic_ps ("tree-out.ps"));

	e_printable_print_page (printable, gpc, 8*72, 10*72, FALSE);

	gnome_print_context_close (gpc);
}

/* We create a window containing our new tree. */
static void
create_tree (void)
{
	GtkWidget *window, *frame, *button, *vbox;
	ECell *cell_left_just;
	ECell *cell_tree;
	ETableHeader *e_table_header;
	int i, j;
	ETreeModel *e_tree_model = NULL;
	ETreePath *root_node;

	/* here we create our model.  This uses the functions we defined
	   earlier. */
	e_tree_model = e_tree_simple_new (my_value_at,
					  my_set_value_at,
					  my_is_editable,
					  NULL);

	/* create a root node with 5 children */
	root_node = e_tree_model_node_insert (e_tree_model, NULL,
					      0, NULL, NULL,
					      g_strdup("Root Node"));

	for (i = 0; i < 5; i++){
		ETreePath *n = e_tree_model_node_insert (e_tree_model,
							 root_node, 0,
							 tree_expanded_pixbuf, tree_unexpanded_pixbuf,
							 g_strdup("First level of children"));
		for (j = 0; j < 5; j ++) {
			e_tree_model_node_insert (e_tree_model,
						  n, 0,
						  NULL, NULL,
						  g_strdup("Second level of children"));
		}
	}

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
	cell_tree = e_cell_tree_new (E_TABLE_MODEL(e_tree_model),
				     tree_expanded_pixbuf, tree_unexpanded_pixbuf,
				     TRUE, cell_left_just);

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
	vbox = gtk_vbox_new (FALSE, 0);
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
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

	button = gtk_button_new_with_label ("Toggle Root Node");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", toggle_root, e_tree_model);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_with_label ("Add Sibling");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", add_sibling, e_tree_model);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_with_label ("Add Child");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", add_child, e_tree_model);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_with_label ("Remove Node");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", remove_node, e_tree_model);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_with_label ("Expand All Below");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", expand_all, e_tree_model);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_with_label ("Collapse All Below");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", collapse_all, e_tree_model);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

	button = gtk_button_new_with_label ("Print Tree");
	gtk_signal_connect (GTK_OBJECT (button), "clicked", print_tree, e_tree_model);
	gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	
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

	/*
	 * Create our pixbuf for expanding/unexpanding
	 */
	tree_expanded_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)tree_expanded_xpm);
	tree_unexpanded_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)tree_unexpanded_xpm);
	
	create_tree ();
	
	gtk_main ();

	e_cursors_shutdown ();
	return 0;
}

