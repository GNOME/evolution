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
#include <glade/glade-xml.h>
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
        GladeXML *xml;
        GtkWidget *dialog, *table;
	GList *l;
	gchar *col_name;

	printf ("exchange_folder_size_display called\n");
        g_return_if_fail (GTK_IS_WIDGET (parent));

        xml = glade_xml_new (CONNECTOR_GLADEDIR "/exchange-folder-tree.glade", NULL, NULL);
        g_return_if_fail (xml != NULL);
        dialog = glade_xml_get_widget (xml, "folder_tree");
        table = glade_xml_get_widget (xml, "folder_treeview");
	g_object_unref (xml);

        e_dialog_set_transient_for (GTK_WINDOW (dialog), parent);
	/* fsize->parent = parent; */

        /* Set up the table */
	sortable = GTK_TREE_SORTABLE (model);
	gtk_tree_sortable_set_sort_column_id (sortable, COLUMN_SIZE, GTK_SORT_DESCENDING);

        column = gtk_tree_view_column_new_with_attributes (
                _("Folder Name"), gtk_cell_renderer_text_new (), "text", COLUMN_NAME, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (table),
                                     column);

	col_name = g_strdup_printf ("%s (KB)", _("Folder Size"));
        column = gtk_tree_view_column_new_with_attributes (
                col_name, gtk_cell_renderer_text_new (), "text", COLUMN_SIZE, NULL);
	g_free (col_name);

	l = gtk_tree_view_column_get_cell_renderers (column);
	cell = (GtkCellRenderer *)l->data;
	gtk_tree_view_column_set_cell_data_func (column, cell, format_size_func, NULL, NULL );
	g_list_free (l);

        gtk_tree_view_append_column (GTK_TREE_VIEW (table),
                                     column);
        gtk_tree_view_set_model (GTK_TREE_VIEW (table),
                                 GTK_TREE_MODEL (model));
	gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}
