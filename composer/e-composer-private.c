/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-composer-private.h"

static void
composer_setup_charset_menu (EMsgComposer *composer)
{
	GtkUIManager *manager;
	const gchar *path;
	GList *list;
	guint merge_id;

	manager = gtkhtml_editor_get_ui_manager (GTKHTML_EDITOR (composer));
	list = gtk_action_group_list_actions (composer->priv->charset_actions);
	path = "/main-menu/edit-menu/pre-spell-check/charset-menu";
	merge_id = gtk_ui_manager_new_merge_id (manager);

	while (list != NULL) {
		GtkAction *action = list->data;

		gtk_ui_manager_add_ui (
			manager, merge_id, path,
			gtk_action_get_name (action),
			gtk_action_get_name (action),
			GTK_UI_MANAGER_AUTO, FALSE);

		list = g_list_delete_link (list, list);
	}

	gtk_ui_manager_ensure_update (manager);
}

static void
composer_setup_recent_menu (EMsgComposer *composer)
{
	GtkUIManager *manager;
	GtkAction *action = NULL;
	const gchar *path, *action_name;
	guint merge_id;

	manager = gtkhtml_editor_get_ui_manager (GTKHTML_EDITOR (composer));
	action_name = "recent-menu";
	path = "/main-menu/insert-menu/insert-menu-top/recent-placeholder";
	merge_id = gtk_ui_manager_new_merge_id (manager);

	action = e_attachment_bar_recent_action_new (
			e_msg_composer_get_attachment_bar (composer), 
			action_name, _("Recent _Documents"));

	if (action != NULL) {
		gtk_action_group_add_action (composer->priv->composer_actions, action);

		gtk_ui_manager_add_ui ( 
			manager, merge_id, path,
			action_name, 
			action_name, 
			GTK_UI_MANAGER_AUTO, FALSE);
	}

	gtk_ui_manager_ensure_update (manager);
}

void
e_composer_private_init (EMsgComposer *composer)
{
	EMsgComposerPrivate *priv = composer->priv;

	GtkhtmlEditor *editor;
	GtkUIManager *manager;
	GtkWidget *widget;
	GtkWidget *expander;
	GtkWidget *container;
	gchar *filename;
	GError *error = NULL;

	editor = GTKHTML_EDITOR (composer);
	manager = gtkhtml_editor_get_ui_manager (editor);

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
	gtk_ui_manager_add_ui_from_file (manager, filename, &error);
	g_free (filename);

	composer_setup_charset_menu (composer);

	if (error != NULL) {
		/* Henceforth, bad things start happening. */
		g_critical ("%s", error->message);
		g_clear_error (&error);
	}

	/* Construct the header table. */

	widget = e_composer_header_table_new ();
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	gtk_box_pack_start (GTK_BOX (editor->vbox), widget, FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (editor->vbox), widget, 2);
	priv->header_table = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Construct attachment widgets.
	 * XXX Move this stuff into a new custom widget. */

	widget = gtk_expander_new (NULL);
	gtk_expander_set_expanded (GTK_EXPANDER (widget), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	gtk_box_pack_start (GTK_BOX (editor->vbox), widget, FALSE, FALSE, 0);
	priv->attachment_expander = g_object_ref (widget);
	gtk_widget_show (widget);
	expander = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (expander), widget);
	priv->attachment_scrolled_window = g_object_ref (widget);
	gtk_widget_show (widget);
	container = widget;

	widget = e_attachment_bar_new (NULL);
	GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->attachment_bar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_hbox_new (FALSE, 0);
	gtk_expander_set_label_widget (GTK_EXPANDER (expander), widget);
	gtk_widget_show (widget);
	container = widget;

	widget = gtk_label_new_with_mnemonic (_("Show _Attachment Bar"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 6);
	priv->attachment_expander_label = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_image_new_from_icon_name (
		"mail-attachment", GTK_ICON_SIZE_MENU);
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_widget_set_size_request (widget, 100, -1);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->attachment_expander_icon = g_object_ref (widget);
	gtk_widget_hide (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 6);
	priv->attachment_expander_num = g_object_ref (widget);
	gtk_widget_show (widget);

	composer_setup_recent_menu (composer);
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

