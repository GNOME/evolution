/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Copyright (C) 2002-2004 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* ExchangeFolderSize: Display the folder tree with the folder sizes */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <e-util/e-dialog-utils.h>
#include <glade/glade-xml.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include "exchange-folder-size-display.h"

enum {
        COLUMN_NAME,
        COLUMN_SIZE,
        NUM_COLUMNS
};


static void
format_size_func (GtkTreeViewColumn *col,
                  GtkCellRenderer   *renderer,
                  GtkTreeModel      *model,
                  GtkTreeIter       *iter,
                 gpointer           user_data)
{
	GtkCellRendererText *cell = (GtkCellRendererText *)renderer;
	gdouble folder_size;
	char * new_text;
	
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
	char *col_name;
        int response;

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
                ("Folder Name"), gtk_cell_renderer_text_new (), "text", COLUMN_NAME, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (table),
                                     column);

	col_name = g_strdup_printf ("%s (KB)", ("Folder Size"));
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
	response = gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}
