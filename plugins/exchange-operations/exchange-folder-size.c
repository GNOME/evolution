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
#include <gtk/gtkliststore.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>

#include "exchange-hierarchy-webdav.h"
#include "e-folder-exchange.h"
#include "exchange-folder-size.h"

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;

typedef struct {
        char *folder_name;
        gdouble folder_size;
} folder_info;


struct _ExchangeFolderSizePrivate {
	
	GHashTable *table;
	GtkListStore *model;
	GHashTable *row_refs;
};

enum {
        COLUMN_NAME,
        COLUMN_SIZE,
        NUM_COLUMNS
};

static gboolean
free_fsize_table (gpointer key, gpointer value, gpointer data)
{
	folder_info *f_info = (folder_info *) value;

	g_free (key);
	g_free (f_info->folder_name);
	g_free (f_info);
	return TRUE;
}

static gboolean
free_row_refs (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	gtk_tree_row_reference_free (value);
	return TRUE;
}

static void
finalize (GObject *object)
{
	ExchangeFolderSize *fsize = EXCHANGE_FOLDER_SIZE (object);

	g_hash_table_foreach_remove (fsize->priv->table, free_fsize_table, NULL);
	g_hash_table_destroy (fsize->priv->table);
	g_hash_table_foreach_remove (fsize->priv->row_refs, free_row_refs, NULL);
	g_hash_table_destroy (fsize->priv->row_refs);
	if (fsize->priv->model)
		g_object_unref (fsize->priv->model);
	g_free (fsize->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
dispose (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
class_init (GObjectClass *object_class)
{
	parent_class = g_type_class_ref (PARENT_TYPE);

	/* override virtual methods */
	object_class->dispose = dispose;
	object_class->finalize = finalize;

}

static void
init (GObject *object)
{
	ExchangeFolderSize *fsize = EXCHANGE_FOLDER_SIZE (object);

	fsize->priv = g_new0 (ExchangeFolderSizePrivate, 1);
	fsize->priv->table = g_hash_table_new (g_str_hash, g_str_equal);
        fsize->priv->model = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_DOUBLE);
	fsize->priv->row_refs = g_hash_table_new (g_str_hash, g_str_equal);
}

E2K_MAKE_TYPE (exchange_folder_size, ExchangeFolderSize, class_init, init, PARENT_TYPE)

/**
 * exchange_folder_size_new:
 * @display_name: the delegate's (UTF8) display name
 *
 * Return value: a foldersize object with the table initialized
 **/
ExchangeFolderSize *
exchange_folder_size_new (void)
{
	ExchangeFolderSize *fsize;

	fsize = g_object_new (EXCHANGE_TYPE_FOLDER_SIZE, NULL);

	return fsize;
}

void
exchange_folder_size_update (ExchangeFolderSize *fsize, 
				const char *folder_name,
				gdouble folder_size)
{
	folder_info *f_info, *cached_info;
	ExchangeFolderSizePrivate *priv;
	GHashTable *folder_size_table;
	GtkTreeRowReference *row;
	GtkTreeIter iter;
	GtkTreePath *path;

	g_return_if_fail (EXCHANGE_IS_FOLDER_SIZE (fsize));

	priv = fsize->priv;
	folder_size_table = priv->table;

	cached_info = g_hash_table_lookup (folder_size_table, folder_name);
	if (cached_info) {
		if (cached_info->folder_size == folder_size) {
			return;
		} else {
			cached_info->folder_size = folder_size;
			row = g_hash_table_lookup (priv->row_refs, folder_name);
			path = gtk_tree_row_reference_get_path (row);
			if (gtk_tree_model_get_iter (GTK_TREE_MODEL (fsize->priv->model), &iter, path)) {
				gtk_list_store_set (fsize->priv->model, &iter,
						      COLUMN_NAME, cached_info->folder_name,
						      COLUMN_SIZE, cached_info->folder_size,
						      -1);
			}
			gtk_tree_path_free (path);
			return;
		}
	} else {
		f_info = g_new0(folder_info, 1);
		f_info->folder_name = g_strdup (folder_name);
		f_info->folder_size = folder_size;
		g_hash_table_insert (folder_size_table, f_info->folder_name, f_info); 

		gtk_list_store_append (fsize->priv->model, &iter);
		gtk_list_store_set (fsize->priv->model, &iter,
				      COLUMN_NAME, f_info->folder_name,
				      COLUMN_SIZE, f_info->folder_size,
				      -1);
		
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (fsize->priv->model), &iter);
		row = gtk_tree_row_reference_new (GTK_TREE_MODEL (fsize->priv->model), path);
		gtk_tree_path_free (path);

		g_hash_table_insert (fsize->priv->row_refs, g_strdup (folder_name), row);
	}
}

void
exchange_folder_size_remove (ExchangeFolderSize *fsize, 
				const char *folder_name)
{
	ExchangeFolderSizePrivate *priv;
	GHashTable *folder_size_table;
	folder_info *cached_info;
	GtkTreeRowReference *row;
	GtkTreeIter iter;
	GtkTreePath *path;

	g_return_if_fail (EXCHANGE_IS_FOLDER_SIZE (fsize));
	g_return_if_fail (folder_name != NULL);

	priv = fsize->priv;
	folder_size_table = priv->table;

	cached_info = g_hash_table_lookup (folder_size_table, folder_name);
	if (cached_info)  {
		row = g_hash_table_lookup (priv->row_refs, folder_name);
		path = gtk_tree_row_reference_get_path (row);
		g_hash_table_remove (folder_size_table, folder_name);
		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (fsize->priv->model), &iter, path)) {
			gtk_list_store_remove (fsize->priv->model, &iter);
		}
		g_hash_table_remove (priv->row_refs, row);
		gtk_tree_path_free (path);
	}
}

gdouble
exchange_folder_size_get (ExchangeFolderSize *fsize,
			  const char *folder_name)
{
	ExchangeFolderSizePrivate *priv;
	GHashTable *folder_size_table;
	folder_info *cached_info;

	g_return_val_if_fail (EXCHANGE_IS_FOLDER_SIZE (fsize), -1);
	
	priv = fsize->priv;
	folder_size_table = priv->table;

	cached_info = g_hash_table_lookup (folder_size_table, folder_name);
	if (cached_info)  {
		return cached_info->folder_size;
	}
	return -1;
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
	char * new_text;
	
	gtk_tree_model_get(model, iter, COLUMN_SIZE, &folder_size, -1);
	
	if (folder_size)
		new_text = g_strdup_printf ("%.2f", folder_size);
	else
		new_text = g_strdup ("0");

	g_object_set (cell, "text", new_text, NULL);
	g_free (new_text);
}

static void
parent_destroyed (gpointer dialog, GObject *ex_parent)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_CANCEL);
}

void
exchange_folder_size_display (EFolder *folder, GtkWidget *parent)
{
        ExchangeFolderSizePrivate *priv;
	ExchangeFolderSize *fsize;
        ExchangeHierarchy *hier;
        GtkTreeViewColumn *column;
	GtkTreeSortable *sortable;
	GtkCellRenderer *cell;
        GHashTable *folder_size_table;
        GladeXML *xml;
        GtkWidget *dialog, *table;
	GList *l;
	char *col_name;
        int response;

        g_return_if_fail (GTK_IS_WIDGET (parent));

        hier = e_folder_exchange_get_hierarchy (folder);
	if (!hier)
		return;
	/* FIXME: This should be a more generic query and not just 
	specifically for webdav */
        fsize = exchange_hierarchy_webdav_get_folder_size (EXCHANGE_HIERARCHY_WEBDAV (hier));
	if (!fsize)
		return;
	priv = fsize->priv;
	folder_size_table = priv->table;

	if (!g_hash_table_size (folder_size_table))
		return;

        xml = glade_xml_new (CONNECTOR_GLADEDIR "/exchange-folder-tree.glade", NULL, NULL);
        g_return_if_fail (xml != NULL);
        dialog = glade_xml_get_widget (xml, "folder_tree");
        table = glade_xml_get_widget (xml, "folder_treeview");

        e_dialog_set_transient_for (GTK_WINDOW (dialog), parent);
	/* fsize->parent = parent; */
        g_object_weak_ref (G_OBJECT (parent), parent_destroyed, dialog);

        /* Set up the table */
	sortable = GTK_TREE_SORTABLE (priv->model);
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
                                 GTK_TREE_MODEL (priv->model));
	response = gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
        g_object_unref (xml);
}
