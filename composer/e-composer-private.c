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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "e-composer-private.h"
#include "e-util/e-util-private.h"

static void
composer_setup_charset_menu (EMsgComposer *composer)
{
	GtkUIManager *ui_manager;
	const gchar *path;
	GList *list;
	guint merge_id;

	ui_manager = gtkhtml_editor_get_ui_manager (GTKHTML_EDITOR (composer));
	path = "/main-menu/edit-menu/pre-spell-check/charset-menu";
	merge_id = gtk_ui_manager_new_merge_id (ui_manager);

	list = gtk_action_group_list_actions (composer->priv->charset_actions);
	list = g_list_sort (list, (GCompareFunc) e_action_compare_by_label);

	while (list != NULL) {
		GtkAction *action = list->data;

		gtk_ui_manager_add_ui (
			ui_manager, merge_id, path,
			gtk_action_get_name (action),
			gtk_action_get_name (action),
			GTK_UI_MANAGER_AUTO, FALSE);

		list = g_list_delete_link (list, list);
	}

	gtk_ui_manager_ensure_update (ui_manager);
}

static void
composer_setup_recent_menu (EMsgComposer *composer)
{
	EAttachmentView *view;
	GtkUIManager *ui_manager;
	GtkAction *action;
	const gchar *action_name;
	const gchar *path;
	guint merge_id;

	view = e_msg_composer_get_attachment_view (composer);
	ui_manager = gtkhtml_editor_get_ui_manager (GTKHTML_EDITOR (composer));
	path = "/main-menu/insert-menu/insert-menu-top/recent-placeholder";
	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	action_name = "recent-menu";

	action = e_attachment_view_recent_action_new (
		view, action_name, _("Recent _Documents"));

	if (action != NULL) {
		gtk_action_group_add_action (
			composer->priv->composer_actions, action);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id, path,
			action_name, action_name,
			GTK_UI_MANAGER_AUTO, FALSE);
	}

	gtk_ui_manager_ensure_update (ui_manager);
}

void
e_composer_private_init (EMsgComposer *composer)
{
	EMsgComposerPrivate *priv = composer->priv;

	GtkhtmlEditor *editor;
	GtkUIManager *ui_manager;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *send_widget;
	const gchar *path;
	gchar *filename;
	gint ii;
	GError *error = NULL;

	editor = GTKHTML_EDITOR (composer);
	ui_manager = gtkhtml_editor_get_ui_manager (editor);

	priv->charset_actions = gtk_action_group_new ("charset");
	priv->composer_actions = gtk_action_group_new ("composer");

	priv->extra_hdr_names = g_ptr_array_new ();
	priv->extra_hdr_values = g_ptr_array_new ();

	priv->gconf_bridge_binding_ids = g_array_new (
		FALSE, FALSE, sizeof (guint));

	priv->inline_images = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	priv->inline_images_by_url = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) camel_object_unref);

	priv->charset = e_composer_get_default_charset ();

	e_composer_actions_init (composer);

	filename = e_composer_find_data_file ("evolution-composer.ui");
	gtk_ui_manager_add_ui_from_file (ui_manager, filename, &error);
	g_free (filename);

	/* We set the send button as important to have a label */
	path = "/main-toolbar/pre-main-toolbar/send";
	send_widget = gtk_ui_manager_get_widget (ui_manager, path);
	gtk_tool_item_set_is_important (GTK_TOOL_ITEM (send_widget), TRUE);

	composer_setup_charset_menu (composer);

	if (error != NULL) {
		/* Henceforth, bad things start happening. */
		g_critical ("%s", error->message);
		g_clear_error (&error);
	}

	/* Construct the header table. */

	container = editor->vbox;

	widget = e_composer_header_table_new ();
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (container), widget, 2);
	priv->header_table = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Construct the attachment paned. */

	widget = e_attachment_paned_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->attachment_paned = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Reparent the scrolled window containing the GtkHTML widget
	 * into the content area of the top attachment pane. */

	widget = GTK_WIDGET (gtkhtml_editor_get_html (editor));
	widget = gtk_widget_get_parent (widget);
	container = e_attachment_paned_get_content_area (
		E_ATTACHMENT_PANED (priv->attachment_paned));
	gtk_widget_reparent (widget, container);
	gtk_box_set_child_packing (
		GTK_BOX (container), widget, TRUE, TRUE, 0, GTK_PACK_START);

	composer_setup_recent_menu (composer);

	/* Bind headers to their corresponding actions. */

	for (ii = 0; ii < E_COMPOSER_NUM_HEADERS; ii++) {
		EComposerHeaderTable *table;
		EComposerHeader *header;
		GtkAction *action;

		table = E_COMPOSER_HEADER_TABLE (priv->header_table);
		header = e_composer_header_table_get_header (table, ii);

		switch (ii) {
			case E_COMPOSER_HEADER_BCC:
				action = ACTION (VIEW_BCC);
				break;

			case E_COMPOSER_HEADER_CC:
				action = ACTION (VIEW_CC);
				break;

			case E_COMPOSER_HEADER_FROM:
				action = ACTION (VIEW_FROM);
				break;

			case E_COMPOSER_HEADER_REPLY_TO:
				action = ACTION (VIEW_REPLY_TO);
				break;

			default:
				continue;
		}

		e_mutual_binding_new (
			G_OBJECT (header), "sensitive",
			G_OBJECT (action), "sensitive");

		e_mutual_binding_new (
			G_OBJECT (header), "visible",
			G_OBJECT (action), "active");
	}
}

void
e_composer_private_dispose (EMsgComposer *composer)
{
	GConfBridge *bridge;
	GArray *array;
	guint binding_id;

	bridge = gconf_bridge_get ();
	array = composer->priv->gconf_bridge_binding_ids;

	while (array->len > 0) {
		binding_id = g_array_index (array, guint, 0);
		gconf_bridge_unbind (bridge, binding_id);
		g_array_remove_index_fast (array, 0);
	}

	if (composer->priv->header_table != NULL) {
		g_object_unref (composer->priv->header_table);
		composer->priv->header_table = NULL;
	}

	if (composer->priv->attachment_paned != NULL) {
		g_object_unref (composer->priv->attachment_paned);
		composer->priv->attachment_paned = NULL;
	}

	if (composer->priv->charset_actions != NULL) {
		g_object_unref (composer->priv->charset_actions);
		composer->priv->charset_actions = NULL;
	}

	if (composer->priv->composer_actions != NULL) {
		g_object_unref (composer->priv->composer_actions);
		composer->priv->composer_actions = NULL;
	}

	g_hash_table_remove_all (composer->priv->inline_images);
	g_hash_table_remove_all (composer->priv->inline_images_by_url);

	if (composer->priv->redirect != NULL) {
		camel_object_unref (composer->priv->redirect);
		composer->priv->redirect = NULL;
	}
}

void
e_composer_private_finalize (EMsgComposer *composer)
{
	GPtrArray *array;

	array = composer->priv->extra_hdr_names;
	g_ptr_array_foreach (array, (GFunc) g_free, NULL);
	g_ptr_array_free (array, TRUE);

	array = composer->priv->extra_hdr_values;
	g_ptr_array_foreach (array, (GFunc) g_free, NULL);
	g_ptr_array_free (array, TRUE);

	g_free (composer->priv->charset);
	g_free (composer->priv->mime_type);
	g_free (composer->priv->mime_body);

	g_hash_table_destroy (composer->priv->inline_images);
	g_hash_table_destroy (composer->priv->inline_images_by_url);
}

gchar *
e_composer_find_data_file (const gchar *basename)
{
	gchar *filename;

	g_return_val_if_fail (basename != NULL, NULL);

	/* Support running directly from the source tree. */
	filename = g_build_filename (".", basename, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free (filename);

	/* XXX This is kinda broken. */
	filename = g_build_filename (EVOLUTION_UIDIR, basename, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free (filename);

	g_critical ("Could not locate '%s'", basename);

	return NULL;
}

gchar *
e_composer_get_default_charset (void)
{
	GConfClient *client;
	gchar *charset;
	GError *error = NULL;

	client = gconf_client_get_default ();

	charset = gconf_client_get_string (
		client, COMPOSER_GCONF_CHARSET_KEY, &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	/* See what charset the mailer is using.
	 * XXX We should not have to know where this lives in GConf.
	 *     Need a mail_config_get_default_charset() that does this. */
	if (!charset || charset[0] == '\0') {
		g_free (charset);
		charset = gconf_client_get_string (
			client, MAIL_GCONF_CHARSET_KEY, NULL);
		if (charset != NULL && *charset == '\0') {
			g_free (charset);
			charset = NULL;
		} else if (error != NULL) {
			g_warning ("%s", error->message);
			g_clear_error (&error);
		}
	}

	g_object_unref (client);

	if (charset == NULL)
		charset = g_strdup (camel_iconv_locale_charset ());

	if (charset == NULL)
		charset = g_strdup ("us-ascii");

	return charset;
}

