/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-tree-example-2.c - Test program for directory reading in the GNOME
   Virtual File System.

   Copyright (C) 1999 Free Software Foundation

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Ettore Perazzoli <ettore@comm2000.it> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include "e-hpaned.h"
#include "e-util/e-cursors.h"
#include "e-table-header.h"
#include "e-table-header-item.h"
#include "e-table-item.h"
#include "e-cell-text.h"
#include "e-cell-tree.h"
#include "e-cell-checkbox.h"
#include "e-table-scrolled.h"
#include "e-tree-simple.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "art/tree-expanded.xpm"
#include "art/tree-unexpanded.xpm"

GdkPixbuf *tree_expanded_pixbuf;
GdkPixbuf *tree_unexpanded_pixbuf;

#define MINI_ICON_SIZE 16

#define RIGHT_COLS 4
#define LEFT_COLS 4

#define RIGHT_E_TABLE_SPEC \
"<ETableSpecification>                    	                       \
	<columns-shown>                  			       \
		<column> 0 </column>     			       \
		<column> 1 </column>     			       \
		<column> 2 </column>     			       \
		<column> 3 </column>     			       \
	</columns-shown>                 			       \
	<grouping></grouping>                                          \
</ETableSpecification>"

#define LEFT_E_TABLE_SPEC \
"<ETableSpecification no-header=\"1\">         	                       \
	<columns-shown>                  			       \
		<column> 0 </column>     			       \
	</columns-shown>                 			       \
	<grouping></grouping>                                          \
</ETableSpecification>"

char *left_headers [LEFT_COLS] = {
  "Folder"
};

char *right_headers [RIGHT_COLS] = {
  "Name",
  "Size",
  "Type",
  "Mime Type"
};



GnomeVFSDirectoryFilter *left_filter;
GnomeVFSDirectoryFilter *right_filter;
GHashTable *mime_type_to_pixbuf;
GnomeVFSDirectoryList *right_list = NULL;
ETreePath *right_root;
ETreeModel *right_model = NULL;
ETreeModel *left_model = NULL;

typedef struct {
	char *node_uri;
	GnomeVFSFileInfo *info;

	/* next two used only if the node is a directory */
	/* used if the node is expanded */
	GnomeVFSDirectoryList *list;
	/* used if the node is collapsed */
	ETreePath *placeholder;
} VFSInfo;

/*
 * ETreeSimple callbacks
 * These are the callbacks that define the behavior of our custom model.
 */

static gchar *
type_to_string (GnomeVFSFileType type)
{
	switch (type) {
	case GNOME_VFS_FILE_TYPE_UNKNOWN:
		return "Unknown";
	case GNOME_VFS_FILE_TYPE_REGULAR:
		return "Regular";
	case GNOME_VFS_FILE_TYPE_DIRECTORY:
		return "Directory";
	case GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK:
		return "Symbolic Link";
	case GNOME_VFS_FILE_TYPE_FIFO:
		return "FIFO";
	case GNOME_VFS_FILE_TYPE_SOCKET:
		return "Socket";
	case GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE:
		return "Character device";
	case GNOME_VFS_FILE_TYPE_BLOCK_DEVICE:
		return "Block device";
	default:
		return "???";
	}
}

/* This function returns the value at a particular point in our ETreeModel. */
static void *
tree_value_at (ETreeModel *etm, ETreePath *path, int col, void *model_data)
{
	VFSInfo *vfs_info = e_tree_model_node_get_data (etm, path);

	switch (col) {
	case 0: /* filename */ 
		if (vfs_info->info) 
			return vfs_info->info->name;
		else
			return vfs_info->node_uri;
	case 1: /* size */ {
		static char buf[15];
		if (vfs_info->info) {
			if (vfs_info->info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
				return "";
			else {
				g_snprintf (buf, sizeof(buf), "%ld", (glong) vfs_info->info->size);
				return buf;
			}
		}
		else
			return "";
	}
	case 2: /* file type */
		if (vfs_info->info)
			return type_to_string (vfs_info->info->type);
		else
			return "";
	case 3: /* mime type */ 
		if (vfs_info->info) {
			const char *mime_type = gnome_vfs_file_info_get_mime_type (vfs_info->info);
			if (mime_type == NULL)
				mime_type = "(Unknown)";
			return (void*)mime_type;
		}
		else {
			return "";
		}
	default: return "";
	}
}

/* This function returns the number of columns in our ETableModel. */
static int
tree_col_count (ETableModel *etc, void *data)
{
	return RIGHT_COLS;
}

/* This function duplicates the value passed to it. */
static void *
tree_duplicate_value (ETableModel *etc, int col, const void *value, void *data)
{
	return g_strdup (value);
}

/* This function frees the value passed to it. */
static void
tree_free_value (ETableModel *etc, int col, void *value, void *data)
{
	g_free (value);
}

/* This function creates an empty value. */
static void *
tree_initialize_value (ETableModel *etc, int col, void *data)
{
	return g_strdup ("");
}

/* This function reports if a value is empty. */
static gboolean
tree_value_is_empty (ETableModel *etc, int col, const void *value, void *data)
{
	return !(value && *(char *)value);
}

/* This function reports if a value is empty. */
static char *
tree_value_to_string (ETableModel *etc, int col, const void *value, void *data)
{
	return g_strdup(value);
}

static GdkPixbuf *
tree_icon_at (ETreeModel *etm, ETreePath *path, void *model_data)
{
	VFSInfo *vfs_info = e_tree_model_node_get_data (etm, path);
	const char *mime_type;
	GdkPixbuf *scaled_pixbuf = NULL;

	if (vfs_info->info == NULL)
		return NULL;

	mime_type = gnome_vfs_file_info_get_mime_type (vfs_info->info);
	if (mime_type == NULL)
		mime_type = "(Unknown)";

	scaled_pixbuf = g_hash_table_lookup (mime_type_to_pixbuf, mime_type);
	if (!scaled_pixbuf) {
		const char *icon_filename = gnome_vfs_mime_get_icon (mime_type);
		if (icon_filename) {
			GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file (icon_filename);

			if (pixbuf) {
				scaled_pixbuf = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (pixbuf),
								gdk_pixbuf_get_has_alpha (pixbuf),
								gdk_pixbuf_get_bits_per_sample (pixbuf),
								MINI_ICON_SIZE, MINI_ICON_SIZE);

				gdk_pixbuf_scale (pixbuf, scaled_pixbuf,
						  0, 0, MINI_ICON_SIZE, MINI_ICON_SIZE,
						  0.0, 0.0,
						  (double) MINI_ICON_SIZE / gdk_pixbuf_get_width (pixbuf),
						  (double) MINI_ICON_SIZE / gdk_pixbuf_get_height (pixbuf),
						  GDK_INTERP_HYPER);
				
				g_hash_table_insert (mime_type_to_pixbuf, (char*)mime_type, scaled_pixbuf);

				gdk_pixbuf_unref (pixbuf);
			}
		}
	}

	return scaled_pixbuf;
}

/* This function sets the value at a particular point in our ETreeModel. */
static void
tree_set_value_at (ETreeModel *etm, ETreePath *path, int col, const void *val, void *model_data)
{
}

/* This function returns whether a particular cell is editable. */
static gboolean
tree_is_editable (ETreeModel *etm, ETreePath *path, int col, void *model_data)
{
	return FALSE;
}

static void
sort_list (GnomeVFSDirectoryList *list)
{
	GnomeVFSDirectorySortRule rules[] = {
		GNOME_VFS_DIRECTORY_SORT_DIRECTORYFIRST,
		GNOME_VFS_DIRECTORY_SORT_BYNAME,
		GNOME_VFS_DIRECTORY_SORT_NONE
	};
		

	gnome_vfs_directory_list_sort (list, FALSE, rules);
}



static void
node_collapsed (ETreeModel *etm, ETreePath *path, void *data)
{
	VFSInfo *vfs_info = e_tree_model_node_get_data (etm, path);
	char *name;
	ETreePath **paths;
	int num_children, i;

	if (vfs_info->info)
		name = vfs_info->info->name;
	else
		name = vfs_info->node_uri;

	gnome_vfs_directory_list_destroy (vfs_info->list);

	/* remove the children of this node, and replace the placeholder */
	num_children = e_tree_model_node_get_children (etm, path, &paths);
	for (i = 0; i < num_children; i ++)
		e_tree_model_node_remove (etm, paths[i]);
	vfs_info->placeholder = e_tree_model_node_insert (etm, path, 0, NULL);
}

static void
node_expanded (ETreeModel *etm, ETreePath *path, gboolean *allow_expand, void *data)
{
	VFSInfo *vfs_info = e_tree_model_node_get_data (etm, path);
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	char *name;

	if (vfs_info->info)
		name = vfs_info->info->name;
	else
		name = vfs_info->node_uri;

	/* Load with no filters and without requesting any metadata.  */
	result = gnome_vfs_directory_list_load
		(&vfs_info->list, vfs_info->node_uri,
		 (GNOME_VFS_FILE_INFO_GET_MIME_TYPE
		  | GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE
		  | GNOME_VFS_FILE_INFO_FOLLOW_LINKS),
		 left_filter);

	*allow_expand = (result == GNOME_VFS_OK);

	if (!*allow_expand) {
		char *buf = g_strdup_printf ("Cannot open directory %s : %s\n",
					     vfs_info->info->name, gnome_vfs_result_to_string (result));
		gnome_error_dialog (buf);
		g_free (buf);
		return;
	}

	sort_list (vfs_info->list);

	/* remove the placeholder and insert all the children of this node */
	e_tree_model_node_remove (etm, vfs_info->placeholder);

	info = gnome_vfs_directory_list_first (vfs_info->list);

	if (info == NULL) {
		/* no files */
		return;
	}

	while (info != NULL) {
		if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
			ETreePath *new_node;
			VFSInfo *new_vfs_info = g_new0(VFSInfo, 1);
			new_vfs_info->info = info;

			new_node = e_tree_model_node_insert (etm, path, -1, new_vfs_info);
			new_vfs_info->placeholder = e_tree_model_node_insert (etm, new_node, -1, NULL);
			new_vfs_info->node_uri = g_strdup_printf("%s%s/", vfs_info->node_uri, info->name);
		}

		info = gnome_vfs_directory_list_next (vfs_info->list);
	}
}

static void
on_cursor_change (ETable *table,
		  int row,
		  gpointer user_data)
{
	int num_children, i;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	ETreePath **paths;
	ETreePath *left_path = e_tree_model_node_at_row (left_model, row);
	VFSInfo *vfs_info = e_tree_model_node_get_data (left_model, left_path);

	if (right_list) {
		gnome_vfs_directory_list_destroy (right_list);
		right_list = NULL;
	}

	/* remove the children of this node, and replace the placeholder */
	num_children = e_tree_model_node_get_children (right_model, right_root, &paths);
	for (i = 0; i < num_children; i ++)
		e_tree_model_node_remove (right_model, paths[i]);

	/* Load with no filters and without requesting any metadata.  */
	result = gnome_vfs_directory_list_load
		(&right_list, vfs_info->node_uri,
		 (GNOME_VFS_FILE_INFO_GET_MIME_TYPE
		  | GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE
		  | GNOME_VFS_FILE_INFO_FOLLOW_LINKS),
		 right_filter);

	if (result != GNOME_VFS_OK)
		return;

	info = gnome_vfs_directory_list_first (right_list);
	if (!info)
		return;

	while (info != NULL) {
		ETreePath *new_node;
		VFSInfo *new_vfs_info = g_new0(VFSInfo, 1);
		new_vfs_info->info = info;

		new_node = e_tree_model_node_insert (right_model, right_root, -1, new_vfs_info);

		new_vfs_info->node_uri = g_strdup_printf("%s%s", vfs_info->node_uri, info->name);
			
		info = gnome_vfs_directory_list_next (right_list);
	}
	
}

static void
on_double_click (ETable *etable,
		 int row,
		 void *data)
{
	GnomeVFSMimeApplication *app;
	ETreePath *path = e_tree_model_node_at_row (right_model, row);
	VFSInfo *vfs_info = e_tree_model_node_get_data (right_model, path);
	const char *mime_type = gnome_vfs_file_info_get_mime_type (vfs_info->info);
	if (mime_type == NULL)
		mime_type = "(Unknown)";

	app = gnome_vfs_mime_get_default_application (mime_type);

	if (app)
		printf ("exec %s\n", app->command);
	else
		printf ("no command for mime type %s\n", mime_type);
}

/* create the table on the right */
static GtkWidget*
create_right_tree(GtkWidget *paned)
{
	GtkWidget *e_table;
	GtkWidget *frame;
	ECell *cell_left_just;
	ECell *cell_tree;
	ETableHeader *e_table_header;
	int i;

	right_filter = gnome_vfs_directory_filter_new (GNOME_VFS_DIRECTORY_FILTER_NONE,
						      GNOME_VFS_DIRECTORY_FILTER_NODIRS,
						      NULL);

	/* here we create our model.  This uses the functions we defined
	   earlier. */
	right_model = e_tree_simple_new (tree_col_count,
					 tree_duplicate_value,
					 tree_free_value,
					 tree_initialize_value,
					 tree_value_is_empty,
					 tree_value_to_string,
					 tree_icon_at,
					 tree_value_at,
					 tree_set_value_at,
					 tree_is_editable,
					 NULL);

	/* create the unexpanded root node and it's placeholder child. */
	right_root = e_tree_model_node_insert (right_model, NULL,
					       0, NULL);
	e_tree_model_root_node_set_visible (right_model, FALSE);

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
	cell_left_just = e_cell_text_new (E_TABLE_MODEL(right_model), NULL, GTK_JUSTIFY_LEFT);

	/* 
	 * This renderer is used for the tree column (the leftmost one), and
	 * has as its subcell renderer the text renderer.  this means that
	 * text is displayed to the right of the tree pipes.
	 */
	cell_tree = e_cell_tree_new (E_TABLE_MODEL(right_model),
				     tree_expanded_pixbuf, tree_unexpanded_pixbuf,
				     TRUE, cell_left_just);

	/*
	 * Next we create a column object for each view column and add
	 * them to the header.  We don't create a column object for
	 * the importance column since it will not be shown.
	 */
	for (i = 0; i < RIGHT_COLS; i++) {
		/* Create the column. */
		ETableCol *ecol = e_table_col_new (
						   i, right_headers [i],
						   80, 20,
						   i == 0 ? cell_tree
						   : cell_left_just,
						   g_str_compare, TRUE);
		/* Add it to the header. */
		e_table_header_add_column (e_table_header, ecol, i);
	}

	/* This frame is simply to get a bevel around our table. */
	frame = gtk_frame_new (NULL);

	/*
	 * Here we create the table.  We give it the three pieces of
	 * the table we've created, the header, the model, and the
	 * initial layout.  It does the rest.
	 */
	e_table = e_table_scrolled_new (e_table_header, E_TABLE_MODEL(right_model), RIGHT_E_TABLE_SPEC);

	gtk_signal_connect (GTK_OBJECT (e_table), "double_click", GTK_SIGNAL_FUNC (on_double_click), NULL);

	/* Build the gtk widget hierarchy. */
	gtk_container_add (GTK_CONTAINER (frame), e_table);
	gtk_container_add (GTK_CONTAINER (paned), frame);

	return e_table;
}

/* We create a window containing our new tree. */
static GtkWidget *
create_left_tree (GtkWidget *paned, char *root_uri)
{
	GtkWidget *scrolled;
	GtkWidget *e_table;
	GtkWidget *frame;
	ECell *cell_left_just;
	ECell *cell_tree;
	ETableHeader *e_table_header;
	ETreePath *root_node;
	VFSInfo *root_vfs_info;
	ETableCol *ecol;

	left_filter = gnome_vfs_directory_filter_new (GNOME_VFS_DIRECTORY_FILTER_NONE,
						      /* putting DIRSONLY doesn't work here, so we filter
							 files out in node_expanded. */
						      (GNOME_VFS_DIRECTORY_FILTER_NOSELFDIR
						       | GNOME_VFS_DIRECTORY_FILTER_NOPARENTDIR), 
						      NULL);


	/* here we create our model.  This uses the functions we defined
	   earlier. */
	left_model = e_tree_simple_new (tree_col_count,
					tree_duplicate_value,
					tree_free_value,
					tree_initialize_value,
					tree_value_is_empty,
					tree_value_to_string,
					tree_icon_at,
					tree_value_at,
					tree_set_value_at,
					tree_is_editable,
					NULL);

	/* catch collapsed/expanded signals */
	gtk_signal_connect (GTK_OBJECT (left_model), "node_expanded",
			    GTK_SIGNAL_FUNC (node_expanded), NULL);

	gtk_signal_connect (GTK_OBJECT (left_model), "node_collapsed",
			    GTK_SIGNAL_FUNC (node_collapsed), NULL);

	/* create the unexpanded root node and it's placeholder child. */
	root_vfs_info = g_new0 (VFSInfo, 1);
	root_vfs_info->node_uri = g_strdup (root_uri);
	root_vfs_info->info = gnome_vfs_file_info_new();

	gnome_vfs_get_file_info (root_uri, root_vfs_info->info,
				 GNOME_VFS_FILE_INFO_GET_MIME_TYPE
				 | GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE
				 | GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

	root_node = e_tree_model_node_insert (left_model, NULL,
					      0,
					      root_vfs_info);
	e_tree_model_node_set_expanded (left_model, root_node, FALSE);

	root_vfs_info->placeholder = e_tree_model_node_insert (left_model, root_node, 0, NULL);

	e_tree_model_root_node_set_visible (left_model, TRUE);

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
	cell_left_just = e_cell_text_new (E_TABLE_MODEL(left_model), NULL, GTK_JUSTIFY_LEFT);

	/* 
	 * This renderer is used for the tree column (the leftmost one), and
	 * has as its subcell renderer the text renderer.  this means that
	 * text is displayed to the right of the tree pipes.
	 */
	cell_tree = e_cell_tree_new (E_TABLE_MODEL(left_model),
				     tree_expanded_pixbuf, tree_unexpanded_pixbuf,
				     TRUE, cell_left_just);

	/* Create the column. */
	ecol = e_table_col_new (0, left_headers [0],
				80, 20,
				cell_tree,
				g_str_compare, TRUE);
	
	e_table_header_add_column (e_table_header, ecol, 0);

	/* This frame is simply to get a bevel around our table. */
	frame = gtk_frame_new (NULL);

	/*
	 * Here we create the table.  We give it the three pieces of
	 * the table we've created, the header, the model, and the
	 * initial layout.  It does the rest.
	 */
	e_table = e_table_new (e_table_header, E_TABLE_MODEL(left_model), LEFT_E_TABLE_SPEC);

	gtk_object_set (GTK_OBJECT (e_table),
			"cursor_mode", E_TABLE_CURSOR_LINE,
			NULL);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	/* Build the gtk widget hierarchy. */
	gtk_container_add (GTK_CONTAINER (scrolled), e_table);
	gtk_container_add (GTK_CONTAINER (frame), scrolled);
	gtk_container_add (GTK_CONTAINER (paned), frame);

	return e_table;
}

static void
create_window(char *root_uri)
{
	GtkWidget *paned;
	GtkWidget *window, *left_tree, *right_tree;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	paned = e_hpaned_new ();

	gtk_container_add (GTK_CONTAINER (window), paned);

	left_tree = create_left_tree (paned, root_uri);
	right_tree = create_right_tree (paned);

	/* Show it all. */
	gtk_widget_show_all (window);

	gtk_signal_connect (GTK_OBJECT (left_tree), "cursor_change", GTK_SIGNAL_FUNC (on_cursor_change), NULL);
	gtk_signal_connect (GTK_OBJECT (window), "delete-event", gtk_main_quit, NULL);

	/* kick things off by selecting the root node */
	e_table_set_cursor_row (E_TABLE (left_tree), 0);
	on_cursor_change (E_TABLE(left_tree), 0, NULL);
}


int
main (int argc, char **argv)
{
	gchar *root_uri;

	if (argv[1] == NULL)
		root_uri = "file:///";
	else
		root_uri = argv[1];

	gnome_init ("TableExample", "TableExample", argc, argv);
	e_cursors_init ();
	gnome_vfs_init ();

	mime_type_to_pixbuf = g_hash_table_new (g_str_hash, g_str_equal);

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	/*
	 * Create our pixbuf for expanding/unexpanding
	 */
	tree_expanded_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)tree_expanded_xpm);
	tree_unexpanded_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)tree_unexpanded_xpm);

	create_window (root_uri);

	gtk_main ();

	e_cursors_shutdown ();
	return 0;
}
