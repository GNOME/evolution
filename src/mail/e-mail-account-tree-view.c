/*
 * e-mail-account-tree-view.c
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
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-mail-account-tree-view.h"

struct _EMailAccountTreeViewPrivate {
	gint placeholder;
};

enum {
	ENABLE,
	DISABLE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EMailAccountTreeView, e_mail_account_tree_view, GTK_TYPE_TREE_VIEW)

static void
mail_account_tree_view_enabled_toggled_cb (GtkCellRendererToggle *cell_renderer,
                                           const gchar *path_string,
                                           EMailAccountTreeView *tree_view)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	/* Change the selection first so we act on the correct service. */
	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

	if (gtk_cell_renderer_toggle_get_active (cell_renderer))
		g_signal_emit (tree_view, signals[DISABLE], 0);
	else
		g_signal_emit (tree_view, signals[ENABLE], 0);
}

static void
mail_account_tree_view_constructed (GObject *object)
{
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_account_tree_view_parent_class)->constructed (object);

	tree_view = GTK_TREE_VIEW (object);

	gtk_tree_view_set_reorderable (tree_view, TRUE);

	/* Column: Enabled */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_title (column, _("Enabled"));

	cell_renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);

	g_signal_connect (
		cell_renderer, "toggled",
		G_CALLBACK (mail_account_tree_view_enabled_toggled_cb),
		tree_view);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "active",
		E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "visible",
		E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED_VISIBLE);

	gtk_tree_view_append_column (tree_view, column);

	/* Column: Account Name */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_title (column, _("Account Name"));

	cell_renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (cell_renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "icon-name",
		E_MAIL_ACCOUNT_STORE_COLUMN_ICON_NAME);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "visible",
		E_MAIL_ACCOUNT_STORE_COLUMN_ONLINE_ACCOUNT);

	cell_renderer = gtk_cell_renderer_text_new ();
	g_object_set (cell_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "text",
		E_MAIL_ACCOUNT_STORE_COLUMN_DISPLAY_NAME);

	/* This renderer is just an empty space filler. */
	cell_renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);

	cell_renderer = gtk_cell_renderer_text_new ();
	g_object_set (cell_renderer, "text", _("Default"), NULL);
	gtk_tree_view_column_pack_end (column, cell_renderer, FALSE);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "visible",
		E_MAIL_ACCOUNT_STORE_COLUMN_DEFAULT);

	cell_renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (
		cell_renderer, "icon-name", "emblem-default",
		"stock-size", GTK_ICON_SIZE_MENU, NULL);
	gtk_tree_view_column_pack_end (column, cell_renderer, FALSE);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "visible",
		E_MAIL_ACCOUNT_STORE_COLUMN_DEFAULT);

	gtk_tree_view_append_column (tree_view, column);

	/* Column: Type */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_title (column, _("Type"));

	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);

	gtk_tree_view_column_add_attribute (
		column, cell_renderer, "text",
		E_MAIL_ACCOUNT_STORE_COLUMN_BACKEND_NAME);

	gtk_tree_view_append_column (tree_view, column);
}

static void
mail_account_tree_view_drag_end (GtkWidget *widget,
                                 GdkDragContext *context)
{
	GtkTreeModel *tree_model;

	/* Chain up to parent's drag_end() method. */
	GTK_WIDGET_CLASS (e_mail_account_tree_view_parent_class)->
		drag_end (widget, context);

	tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (tree_model));

	g_signal_emit_by_name (tree_model, "services-reordered", FALSE);
}

static void
e_mail_account_tree_view_class_init (EMailAccountTreeViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_account_tree_view_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->drag_end = mail_account_tree_view_drag_end;

	signals[ENABLE] = g_signal_new (
		"enable",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountTreeViewClass, enable),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[DISABLE] = g_signal_new (
		"disable",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountTreeViewClass, disable),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_mail_account_tree_view_init (EMailAccountTreeView *tree_view)
{
	tree_view->priv = e_mail_account_tree_view_get_instance_private (tree_view);
}

GtkWidget *
e_mail_account_tree_view_new (EMailAccountStore *store)
{
	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), NULL);

	return g_object_new (
		E_TYPE_MAIL_ACCOUNT_TREE_VIEW,
		"model", store, NULL);
}

CamelService *
e_mail_account_tree_view_get_selected_service (EMailAccountTreeView *tree_view)
{
	GtkTreeSelection *selection;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	CamelService *service;
	GValue value = G_VALUE_INIT;
	gint column;

	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_TREE_VIEW (tree_view), NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	if (!gtk_tree_selection_get_selected (selection, &tree_model, &iter))
		return NULL;

	/* By convention, "get" functions don't return a new object
	 * reference, so use gtk_tree_model_get_value() to avoid it.
	 * The caller can always reference the object if needed. */

	column = E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE;
	gtk_tree_model_get_value (tree_model, &iter, column, &value);
	service = g_value_get_object (&value);
	g_value_unset (&value);

	g_warn_if_fail (CAMEL_IS_SERVICE (service));

	return service;
}

void
e_mail_account_tree_view_set_selected_service (EMailAccountTreeView *tree_view,
                                               CamelService *service)
{
	GtkTreeSelection *selection;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean iter_set;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_TREE_VIEW (tree_view));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	iter_set = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (iter_set) {
		GValue value = G_VALUE_INIT;
		CamelService *candidate;
		gint column;

		column = E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE;
		gtk_tree_model_get_value (tree_model, &iter, column, &value);
		candidate = g_value_get_object (&value);
		g_value_unset (&value);

		g_warn_if_fail (CAMEL_IS_SERVICE (candidate));

		if (service == candidate) {
			gtk_tree_selection_select_iter (selection, &iter);
			break;
		}

		iter_set = gtk_tree_model_iter_next (tree_model, &iter);
	}
}
