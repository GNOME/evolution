/*
 * e-mail-print-config-headers.c
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

#include <em-format/e-mail-formatter-print.h>

#include "e-mail-print-config-headers.h"

struct _EMailPrintConfigHeadersPrivate {
	EMailPartHeaders *part;
};

enum {
	PROP_0,
	PROP_PART
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailPrintConfigHeaders, e_mail_print_config_headers, E_TYPE_TREE_VIEW_FRAME)

static void
mail_print_config_headers_toggled_cb (GtkCellRendererToggle *renderer,
                                      const gchar *path_string,
                                      ETreeViewFrame *tree_view_frame)
{
	GtkTreeView *tree_view;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean include;

	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);

	tree_model = gtk_tree_view_get_model (tree_view);
	gtk_tree_model_get_iter_from_string (tree_model, &iter, path_string);

	gtk_tree_model_get (
		tree_model, &iter,
		E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_INCLUDE, &include, -1);

	gtk_list_store_set (
		GTK_LIST_STORE (tree_model), &iter,
		E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_INCLUDE, !include, -1);

	/* XXX Maybe not needed? */
	e_tree_view_frame_update_toolbar_actions (tree_view_frame);
}

static void
mail_print_config_headers_set_part (EMailPrintConfigHeaders *config,
                                    EMailPartHeaders *part)
{
	g_return_if_fail (E_IS_MAIL_PART_HEADERS (part));
	g_return_if_fail (config->priv->part == NULL);

	config->priv->part = g_object_ref (part);
}

static void
mail_print_config_headers_set_property (GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PART:
			mail_print_config_headers_set_part (
				E_MAIL_PRINT_CONFIG_HEADERS (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_print_config_headers_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PART:
			g_value_take_object (
				value,
				e_mail_print_config_headers_ref_part (
				E_MAIL_PRINT_CONFIG_HEADERS (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_print_config_headers_dispose (GObject *object)
{
	EMailPrintConfigHeaders *self = E_MAIL_PRINT_CONFIG_HEADERS (object);

	g_clear_object (&self->priv->part);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_print_config_headers_parent_class)->dispose (object);
}

static void
mail_print_config_headers_constructed (GObject *object)
{
	EMailPrintConfigHeaders *config;
	ETreeViewFrame *tree_view_frame;
	EUIAction *action;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	EMailPartHeaders *part;
	GtkTreeModel *print_model;
	const gchar *tooltip;

	config = E_MAIL_PRINT_CONFIG_HEADERS (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_print_config_headers_parent_class)->constructed (object);

	tree_view_frame = E_TREE_VIEW_FRAME (object);
	tree_view = e_tree_view_frame_get_tree_view (tree_view_frame);

	gtk_tree_view_set_reorderable (tree_view, TRUE);

	/* Configure the toolbar actions. */

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_ADD);
	e_ui_action_set_visible (action, FALSE);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_REMOVE);
	e_ui_action_set_visible (action, FALSE);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_MOVE_TOP);
	tooltip = _("Move selected headers to top");
	e_ui_action_set_tooltip (action, tooltip);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_MOVE_UP);
	tooltip = _("Move selected headers up one row");
	e_ui_action_set_tooltip (action, tooltip);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_MOVE_DOWN);
	tooltip = _("Move selected headers down one row");
	e_ui_action_set_tooltip (action, tooltip);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_MOVE_BOTTOM);
	tooltip = _("Move selected headers to bottom");
	e_ui_action_set_tooltip (action, tooltip);

	action = e_tree_view_frame_lookup_toolbar_action (
		tree_view_frame, E_TREE_VIEW_FRAME_ACTION_SELECT_ALL);
	tooltip = _("Select all headers");
	e_ui_action_set_tooltip (action, tooltip);

	/* Configure the tree view columns. */

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "active",
		E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_INCLUDE);
	gtk_tree_view_append_column (tree_view, column);

	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (mail_print_config_headers_toggled_cb),
		tree_view_frame);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Header Name"));
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "text",
		E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_HEADER_NAME);
	gtk_tree_view_append_column (tree_view, column);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Header Value"));
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "text",
		E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_HEADER_VALUE);
	gtk_tree_view_append_column (tree_view, column);

	/* Set the tree model and selection mode. */

	part = e_mail_print_config_headers_ref_part (config);
	print_model = e_mail_part_headers_ref_print_model (part);
	gtk_tree_view_set_model (tree_view, print_model);
	g_object_unref (print_model);
	g_object_unref (part);

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
}

static void
e_mail_print_config_headers_class_init (EMailPrintConfigHeadersClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_print_config_headers_set_property;
	object_class->get_property = mail_print_config_headers_get_property;
	object_class->dispose = mail_print_config_headers_dispose;
	object_class->constructed = mail_print_config_headers_constructed;

	/**
	 * EMailPartConfigHeaders:part:
	 *
	 * The #EMailPartHeaders to configure.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_PART,
		g_param_spec_object (
			"part",
			"Part",
			"The EMailPartHeaders to configure",
			E_TYPE_MAIL_PART_HEADERS,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_print_config_headers_init (EMailPrintConfigHeaders *config)
{
	config->priv = e_mail_print_config_headers_get_instance_private (config);
}

GtkWidget *
e_mail_print_config_headers_new (EMailPartHeaders *part)
{
	g_return_val_if_fail (E_IS_MAIL_PART_HEADERS (part), NULL);

	return g_object_new (
		E_TYPE_MAIL_PRINT_CONFIG_HEADERS,
		"part", part, NULL);
}

EMailPartHeaders *
e_mail_print_config_headers_ref_part (EMailPrintConfigHeaders *config)
{
	g_return_val_if_fail (E_IS_MAIL_PRINT_CONFIG_HEADERS (config), NULL);

	return g_object_ref (config->priv->part);
}

