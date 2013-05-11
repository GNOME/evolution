/*
 * e-mail-print-config-headers.c
 *
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
 */

#include "e-mail-print-config-headers.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#include <em-format/e-mail-formatter-print.h>

#define E_MAIL_PRINT_CONFIG_HEADERS_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_PRINT_CONFIG_HEADERS, EMailPrintConfigHeadersPrivate))

struct _EMailPrintConfigHeadersPrivate {
	GtkTreeView *tree_view;       /* not referenced */
	GtkWidget *go_top_button;     /* not referenced */
	GtkWidget *go_up_button;      /* not referenced */
	GtkWidget *go_down_button;    /* not referenced */
	GtkWidget *go_bottom_button;  /* not referenced */
	GtkWidget *select_all_button; /* not referenced */
	GtkWidget *clear_button;      /* not referenced */

	EMailPartHeaders *part;
};

enum {
	PROP_0,
	PROP_PART
};

G_DEFINE_TYPE (
	EMailPrintConfigHeaders,
	e_mail_print_config_headers,
	GTK_TYPE_BOX)

static GtkToolItem *
mail_print_config_headers_new_tool_button (const gchar *icon_name)
{
	GIcon *icon;
	GtkWidget *image;

	icon = g_themed_icon_new_with_default_fallbacks (icon_name);
	image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_widget_show (image);
	g_object_unref (icon);

	return gtk_tool_button_new (image, NULL);
}

static gboolean
mail_print_config_headers_first_row_selected (EMailPrintConfigHeaders *config)
{
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	tree_model = gtk_tree_view_get_model (config->priv->tree_view);
	selection = gtk_tree_view_get_selection (config->priv->tree_view);

	if (!gtk_tree_model_iter_nth_child (tree_model, &iter, NULL, 0))
		return FALSE;

	return gtk_tree_selection_iter_is_selected (selection, &iter);
}

static gboolean
mail_print_config_headers_last_row_selected (EMailPrintConfigHeaders *config)
{
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gint last;

	tree_model = gtk_tree_view_get_model (config->priv->tree_view);
	selection = gtk_tree_view_get_selection (config->priv->tree_view);

	last = gtk_tree_model_iter_n_children (tree_model, NULL) - 1;
	if (last < 0)
		return FALSE;

	if (!gtk_tree_model_iter_nth_child (tree_model, &iter, NULL, last))
		return FALSE;

	return gtk_tree_selection_iter_is_selected (selection, &iter);
}

static gboolean
mail_print_config_headers_move_selection_up (EMailPrintConfigHeaders *config)
{
	GtkListStore *list_store;
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GList *list, *link;

	/* Move all selected rows up one, even
	 * if the selection is not contiguous. */

	if (mail_print_config_headers_first_row_selected (config))
		return FALSE;

	selection = gtk_tree_view_get_selection (config->priv->tree_view);
	list = gtk_tree_selection_get_selected_rows (selection, &tree_model);

	list_store = GTK_LIST_STORE (tree_model);

	for (link = list; link != NULL; link = g_list_next (link)) {
		GtkTreePath *path = link->data;
		GtkTreeIter iter;
		GtkTreeIter prev;

		if (!gtk_tree_model_get_iter (tree_model, &iter, path)) {
			g_warn_if_reached ();
			continue;
		}

		prev = iter;
		if (!gtk_tree_model_iter_previous (tree_model, &prev)) {
			g_warn_if_reached ();
			continue;
		}

		gtk_list_store_swap (list_store, &iter, &prev);
	}

	g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

	return TRUE;
}

static gboolean
mail_print_config_headers_move_selection_down (EMailPrintConfigHeaders *config)
{
	GtkListStore *list_store;
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	GList *list, *link;

	/* Move all selected rows down one, even
	 * if the selection is not contiguous. */

	if (mail_print_config_headers_last_row_selected (config))
		return FALSE;

	selection = gtk_tree_view_get_selection (config->priv->tree_view);
	list = gtk_tree_selection_get_selected_rows (selection, &tree_model);

	/* Reverse the list so we don't disturb rows we've already moved. */
	list = g_list_reverse (list);

	list_store = GTK_LIST_STORE (tree_model);

	for (link = list; link != NULL; link = g_list_next (link)) {
		GtkTreePath *path = link->data;
		GtkTreeIter iter;
		GtkTreeIter next;

		if (!gtk_tree_model_get_iter (tree_model, &iter, path)) {
			g_warn_if_reached ();
			continue;
		}

		next = iter;
		if (!gtk_tree_model_iter_next (tree_model, &next)) {
			g_warn_if_reached ();
			continue;
		}

		gtk_list_store_swap (list_store, &iter, &next);
	}

	g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

	return TRUE;
}

static void
mail_print_config_headers_scroll_to_cursor (EMailPrintConfigHeaders *config)
{
	GtkTreePath *path = NULL;

	gtk_tree_view_get_cursor (config->priv->tree_view, &path, NULL);

	if (path != NULL) {
		gtk_tree_view_scroll_to_cell (
			config->priv->tree_view,
			path, NULL, FALSE, 0.0, 0.0);
		gtk_tree_path_free (path);
	}
}

static void
mail_print_config_headers_update_buttons (EMailPrintConfigHeaders *config)
{
	GtkWidget *widget;
	GtkTreeModel *tree_model;
	GtkTreeSelection *selection;
	gboolean first_row_selected;
	gboolean last_row_selected;
	gboolean sensitive;
	gint n_selected_rows;
	gint n_rows;

	tree_model = gtk_tree_view_get_model (config->priv->tree_view);
	selection = gtk_tree_view_get_selection (config->priv->tree_view);

	n_rows = gtk_tree_model_iter_n_children (tree_model, NULL);
	n_selected_rows = gtk_tree_selection_count_selected_rows (selection);

	first_row_selected =
		mail_print_config_headers_first_row_selected (config);
	last_row_selected =
		mail_print_config_headers_last_row_selected (config);

	widget = config->priv->go_top_button;
	sensitive = (n_selected_rows > 0 && !first_row_selected);
	gtk_widget_set_sensitive (widget, sensitive);

	widget = config->priv->go_up_button;
	sensitive = (n_selected_rows > 0 && !first_row_selected);
	gtk_widget_set_sensitive (widget, sensitive);

	widget = config->priv->go_down_button;
	sensitive = (n_selected_rows > 0 && !last_row_selected);
	gtk_widget_set_sensitive (widget, sensitive);

	widget = config->priv->go_bottom_button;
	sensitive = (n_selected_rows > 0 && !last_row_selected);
	gtk_widget_set_sensitive (widget, sensitive);

	widget = config->priv->select_all_button;
	sensitive = (n_selected_rows < n_rows);
	gtk_widget_set_sensitive (widget, sensitive);

	widget = config->priv->clear_button;
	sensitive = (n_selected_rows > 0);
	gtk_widget_set_sensitive (widget, sensitive);
}

static void
mail_print_config_headers_go_top_cb (GtkToolButton *tool_button,
                                     EMailPrintConfigHeaders *config)
{
	/* Not the most efficient method, but it's simple and works.
	 * There should not be so many headers that this is a major
	 * performance hit anyway. */
	while (mail_print_config_headers_move_selection_up (config))
		;

	mail_print_config_headers_scroll_to_cursor (config);
	mail_print_config_headers_update_buttons (config);
}

static void
mail_print_config_headers_go_up_cb (GtkToolButton *tool_button,
                                    EMailPrintConfigHeaders *config)
{
	mail_print_config_headers_move_selection_up (config);

	mail_print_config_headers_scroll_to_cursor (config);
	mail_print_config_headers_update_buttons (config);
}

static void
mail_print_config_headers_go_down_cb (GtkToolButton *tool_button,
                                      EMailPrintConfigHeaders *config)
{
	mail_print_config_headers_move_selection_down (config);

	mail_print_config_headers_scroll_to_cursor (config);
	mail_print_config_headers_update_buttons (config);
}

static void
mail_print_config_headers_go_bottom_cb (GtkToolButton *tool_button,
                                        EMailPrintConfigHeaders *config)
{
	/* Not the most efficient method, but it's simple and works.
	 * There should not be so many headers that this is a major
	 * performance hit anyway. */
	while (mail_print_config_headers_move_selection_down (config))
		;

	mail_print_config_headers_scroll_to_cursor (config);
	mail_print_config_headers_update_buttons (config);
}

static void
mail_print_config_headers_select_all_cb (GtkToolButton *tool_button,
                                         EMailPrintConfigHeaders *config)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (config->priv->tree_view);
	gtk_tree_selection_select_all (selection);

	mail_print_config_headers_update_buttons (config);
}

static void
mail_print_config_headers_unselect_all_cb (GtkToolButton *tool_button,
                                           EMailPrintConfigHeaders *config)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (config->priv->tree_view);
	gtk_tree_selection_unselect_all (selection);

	mail_print_config_headers_update_buttons (config);
}

static void
mail_print_config_headers_toggled_cb (GtkCellRendererToggle *renderer,
                                      const gchar *path_string,
                                      EMailPrintConfigHeaders *config)
{
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean include;

	tree_model = gtk_tree_view_get_model (config->priv->tree_view);
	gtk_tree_model_get_iter_from_string (tree_model, &iter, path_string);

	gtk_tree_model_get (
		tree_model, &iter,
		E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_INCLUDE, &include, -1);

	gtk_list_store_set (
		GTK_LIST_STORE (tree_model), &iter,
		E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_INCLUDE, !include, -1);

	mail_print_config_headers_update_buttons (config);
}

static void
mail_print_config_headers_selection_changed_cb (GtkTreeSelection *selection,
                                                EMailPrintConfigHeaders *config)
{
	mail_print_config_headers_update_buttons (config);
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
	EMailPrintConfigHeadersPrivate *priv;

	priv = E_MAIL_PRINT_CONFIG_HEADERS_GET_PRIVATE (object);

	g_clear_object (&priv->part);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_print_config_headers_parent_class)->
		dispose (object);
}

static void
mail_print_config_headers_constructed (GObject *object)
{
	EMailPrintConfigHeaders *config;
	GtkStyleContext *style_context;
	GtkWidget *widget;
	GtkWidget *container;
	GtkToolItem *tool_item;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	EMailPartHeaders *part;
	GtkTreeModel *print_model;
	const gchar *icon_name;
	const gchar *text;

	config = E_MAIL_PRINT_CONFIG_HEADERS (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_print_config_headers_parent_class)->
		constructed (object);

	gtk_container_set_border_width (GTK_CONTAINER (object), 12);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (object), GTK_ORIENTATION_VERTICAL);

	container = GTK_WIDGET (object);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_tree_view_new ();
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (widget), TRUE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	config->priv->tree_view = GTK_TREE_VIEW (widget);
	gtk_widget_show (widget);

	container = GTK_WIDGET (object);

	widget = gtk_toolbar_new ();
	gtk_toolbar_set_icon_size (GTK_TOOLBAR (widget), GTK_ICON_SIZE_MENU);
	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_set_junction_sides (
		style_context, GTK_JUNCTION_TOP);
	gtk_style_context_add_class (
		style_context, GTK_STYLE_CLASS_INLINE_TOOLBAR);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	icon_name = "go-top-symbolic";
	tool_item = mail_print_config_headers_new_tool_button (icon_name);
	gtk_toolbar_insert (GTK_TOOLBAR (container), tool_item, -1);
	config->priv->go_top_button = GTK_WIDGET (tool_item);
	gtk_widget_show (GTK_WIDGET (tool_item));

	text = _("Move selection to top");
	gtk_widget_set_tooltip_text (GTK_WIDGET (tool_item), text);

	g_signal_connect (
		tool_item, "clicked",
		G_CALLBACK (mail_print_config_headers_go_top_cb),
		config);

	icon_name = "go-up-symbolic";
	tool_item = mail_print_config_headers_new_tool_button (icon_name);
	gtk_toolbar_insert (GTK_TOOLBAR (container), tool_item, -1);
	config->priv->go_up_button = GTK_WIDGET (tool_item);
	gtk_widget_show (GTK_WIDGET (tool_item));

	text = _("Move selection up one row");
	gtk_widget_set_tooltip_text (GTK_WIDGET (tool_item), text);

	g_signal_connect (
		tool_item, "clicked",
		G_CALLBACK (mail_print_config_headers_go_up_cb),
		config);

	icon_name = "go-down-symbolic";
	tool_item = mail_print_config_headers_new_tool_button (icon_name);
	gtk_toolbar_insert (GTK_TOOLBAR (container), tool_item, -1);
	config->priv->go_down_button = GTK_WIDGET (tool_item);
	gtk_widget_show (GTK_WIDGET (tool_item));

	text = _("Move selection down one row");
	gtk_widget_set_tooltip_text (GTK_WIDGET (tool_item), text);

	g_signal_connect (
		tool_item, "clicked",
		G_CALLBACK (mail_print_config_headers_go_down_cb),
		config);

	icon_name = "go-bottom-symbolic";
	tool_item = mail_print_config_headers_new_tool_button (icon_name);
	gtk_toolbar_insert (GTK_TOOLBAR (container), tool_item, -1);
	config->priv->go_bottom_button = GTK_WIDGET (tool_item);
	gtk_widget_show (GTK_WIDGET (tool_item));

	text = _("Move selection to bottom");
	gtk_widget_set_tooltip_text (GTK_WIDGET (tool_item), text);

	g_signal_connect (
		tool_item, "clicked",
		G_CALLBACK (mail_print_config_headers_go_bottom_cb),
		config);

	icon_name = "edit-select-all-symbolic";
	tool_item = mail_print_config_headers_new_tool_button (icon_name);
	gtk_toolbar_insert (GTK_TOOLBAR (container), tool_item, -1);
	config->priv->select_all_button = GTK_WIDGET (tool_item);
	gtk_widget_show (GTK_WIDGET (tool_item));

	text = _("Select all headers");
	gtk_widget_set_tooltip_text (GTK_WIDGET (tool_item), text);

	g_signal_connect (
		tool_item, "clicked",
		G_CALLBACK (mail_print_config_headers_select_all_cb),
		config);

	icon_name = "edit-clear-symbolic";
	tool_item = mail_print_config_headers_new_tool_button (icon_name);
	gtk_toolbar_insert (GTK_TOOLBAR (container), tool_item, -1);
	config->priv->clear_button = GTK_WIDGET (tool_item);
	gtk_widget_show (GTK_WIDGET (tool_item));

	text = _("Unselect all headers");
	gtk_widget_set_tooltip_text (GTK_WIDGET (tool_item), text);

	g_signal_connect (
		tool_item, "clicked",
		G_CALLBACK (mail_print_config_headers_unselect_all_cb),
		config);

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "active",
		E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_INCLUDE);
	gtk_tree_view_append_column (config->priv->tree_view, column);

	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (mail_print_config_headers_toggled_cb), config);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Header Name"));
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "text",
		E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_HEADER_NAME);
	gtk_tree_view_append_column (config->priv->tree_view, column);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Header Value"));
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "text",
		E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_HEADER_VALUE);
	gtk_tree_view_append_column (config->priv->tree_view, column);

	part = e_mail_print_config_headers_ref_part (config);
	print_model = e_mail_part_headers_ref_print_model (part);
	gtk_tree_view_set_model (config->priv->tree_view, print_model);
	g_object_unref (print_model);
	g_object_unref (part);

	selection = gtk_tree_view_get_selection (config->priv->tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	g_signal_connect (
		selection, "changed",
		G_CALLBACK (mail_print_config_headers_selection_changed_cb),
		config);

	mail_print_config_headers_update_buttons (config);
}

static void
e_mail_print_config_headers_class_init (EMailPrintConfigHeadersClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (
		class, sizeof (EMailPrintConfigHeadersPrivate));

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
	config->priv = E_MAIL_PRINT_CONFIG_HEADERS_GET_PRIVATE (config);
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

