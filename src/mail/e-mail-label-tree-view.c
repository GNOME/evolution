/*
 * e-mail-label-tree-view.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-mail-label-tree-view.h"

#include <glib/gi18n.h>
#include "e-mail-label-list-store.h"

#define E_MAIL_LABEL_TREE_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_LABEL_TREE_VIEW, EMailLabelTreeViewPrivate))

struct _EMailLabelTreeViewPrivate {
	gint placeholder;
};

G_DEFINE_TYPE (EMailLabelTreeView, e_mail_label_tree_view, GTK_TYPE_TREE_VIEW)

static void
mail_label_tree_view_render_pixbuf (GtkTreeViewColumn *column,
                                    GtkCellRenderer *renderer,
                                    GtkTreeModel *model,
                                    GtkTreeIter *iter,
                                    EMailLabelTreeView *tree_view)
{
	EMailLabelListStore *store;
	gchar *stock_id;

	store = E_MAIL_LABEL_LIST_STORE (model);
	stock_id = e_mail_label_list_store_get_stock_id (store, iter);
	g_object_set (renderer, "stock-id", stock_id, NULL);
	g_free (stock_id);
}

static void
mail_label_tree_view_render_text (GtkTreeViewColumn *column,
                                  GtkCellRenderer *renderer,
                                  GtkTreeModel *model,
                                  GtkTreeIter *iter,
                                  EMailLabelTreeView *tree_view)
{
	EMailLabelListStore *store;
	gchar *name;

	store = E_MAIL_LABEL_LIST_STORE (model);
	name = e_mail_label_list_store_get_name (store, iter);
	g_object_set (renderer, "text", name, NULL);
	g_free (name);
}

static void
e_mail_label_tree_view_class_init (EMailLabelTreeViewClass *class)
{
	g_type_class_add_private (class, sizeof (EMailLabelTreeViewPrivate));
}

static void
e_mail_label_tree_view_init (EMailLabelTreeView *tree_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	tree_view->priv = E_MAIL_LABEL_TREE_VIEW_GET_PRIVATE (tree_view);

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_set_title (column, _("Color"));
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	gtk_tree_view_column_set_cell_data_func (
		column, renderer, (GtkTreeCellDataFunc)
		mail_label_tree_view_render_pixbuf, tree_view, NULL);

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_set_title (column, _("Name"));
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	gtk_tree_view_column_set_cell_data_func (
		column, renderer, (GtkTreeCellDataFunc)
		mail_label_tree_view_render_text, tree_view, NULL);
}

GtkWidget *
e_mail_label_tree_view_new (void)
{
	return g_object_new (E_TYPE_MAIL_LABEL_TREE_VIEW, NULL);
}
