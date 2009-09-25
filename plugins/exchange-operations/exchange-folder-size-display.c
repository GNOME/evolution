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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* ExchangeFolderSize: Display the folder tree with the folder sizes */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>
#include <e-util/e-dialog-utils.h>
#include "exchange-folder-size-display.h"

enum {
        COLUMN_NAME,
        COLUMN_SIZE,
        NUM_COLUMNS
};

static gboolean
get_folder_size_func (GtkTreeModel *model,
		  GtkTreePath	    *path,
                  GtkTreeIter       *iter,
                 gpointer           user_data)
{
	GHashTable *info = (GHashTable *) user_data;
	gdouble folder_size;
	gchar *folder_name;

	gtk_tree_model_get(model, iter, COLUMN_SIZE, &folder_size, COLUMN_NAME, &folder_name, -1);

	g_hash_table_insert (info, g_strdup (folder_name), g_strdup_printf ("%.2f", folder_size));
	return FALSE;
}

gchar *
exchange_folder_size_get_val (GtkListStore *model, const gchar *folder_name)
{
	GHashTable *finfo;
	gchar *folder_size, *fsize;

	finfo = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	gtk_tree_model_foreach (GTK_TREE_MODEL (model), get_folder_size_func, finfo);

	if ((fsize = g_hash_table_lookup (finfo, folder_name)) != NULL)
		folder_size = g_strdup (fsize);
	else
		folder_size = g_strdup ("0");

	g_hash_table_destroy (finfo);

	return folder_size;
}

static void
format_size_func (GtkTreeViewColumn *col,
                  GtkCellRenderer   *renderer,
                  GtkTreeModel      *model,
                  GtkTreeIter       *iter,
                 gpointer           user_data)
{
	GtkCellRendererText *cell = (GtkCellRendererText *)renderer;
	gdouble folder_size;
	gchar * new_text;

	gtk_tree_model_get(model, iter, COLUMN_SIZE, &folder_size, -1);

	if (folder_size)
		new_text = g_strdup_printf ("%.2f", folder_size);
	else
		new_text = g_strdup ("0");

	g_object_set (cell, "text", new_text, NULL);
	g_free (new_text);
}

void
exchange_folder_size_display (GtkListStore *model, GtkWidget *parent)
{
        GtkTreeViewColumn *column;
	GtkTreeSortable *sortable;
	GtkCellRenderer *cell;
	GtkWidget *folder_tree;
	GtkWidget *dialog_vbox1;
	GtkWidget *folder_tree_hbox;
	GtkWidget *scrolledwindow1;
	GtkWidget *folder_treeview;
	GList *l;
	gchar *col_name;

        g_return_if_fail (GTK_IS_WIDGET (parent));

	folder_tree = gtk_dialog_new_with_buttons (
		_("Exchange Folder Tree"),
		NULL,
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	gtk_window_set_position (GTK_WINDOW (folder_tree), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_default_size (GTK_WINDOW (folder_tree), 250, 300);
	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (folder_tree), GTK_WINDOW (parent));

	dialog_vbox1 = gtk_dialog_get_content_area (GTK_DIALOG (folder_tree));
	gtk_widget_show (dialog_vbox1);

	folder_tree_hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (folder_tree_hbox);
	gtk_box_pack_start (GTK_BOX (dialog_vbox1), folder_tree_hbox, TRUE, TRUE, 0);

	scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow1);
	gtk_box_pack_start (GTK_BOX (folder_tree_hbox), scrolledwindow1, TRUE, TRUE, 0);

	folder_treeview = gtk_tree_view_new ();
	gtk_widget_show (folder_treeview);
	gtk_container_add (GTK_CONTAINER (scrolledwindow1), folder_treeview);

	/* fsize->parent = parent; */

        /* Set up the table */
	sortable = GTK_TREE_SORTABLE (model);
	gtk_tree_sortable_set_sort_column_id (sortable, COLUMN_SIZE, GTK_SORT_DESCENDING);

        column = gtk_tree_view_column_new_with_attributes (
                _("Folder Name"), gtk_cell_renderer_text_new (), "text", COLUMN_NAME, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (folder_treeview),
                                     column);

	col_name = g_strdup_printf ("%s (KB)", _("Folder Size"));
        column = gtk_tree_view_column_new_with_attributes (
                col_name, gtk_cell_renderer_text_new (), "text", COLUMN_SIZE, NULL);
	g_free (col_name);

	l = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	cell = (GtkCellRenderer *)l->data;
	gtk_tree_view_column_set_cell_data_func (column, cell, format_size_func, NULL, NULL );
	g_list_free (l);

        gtk_tree_view_append_column (GTK_TREE_VIEW (folder_treeview),
                                     column);
        gtk_tree_view_set_model (GTK_TREE_VIEW (folder_treeview),
                                 GTK_TREE_MODEL (model));
	gtk_dialog_run (GTK_DIALOG (folder_tree));
        gtk_widget_destroy (folder_tree);
}
