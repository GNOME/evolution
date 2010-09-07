/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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
	path = "/main-menu/options-menu/charset-menu";
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

static void
msg_composer_url_requested_cb (GtkHTML *html,
                               const gchar *uri,
                               GtkHTMLStream *stream,
                               EMsgComposer *composer)
{
	GByteArray *array;
	GHashTable *hash_table;
	CamelDataWrapper *wrapper;
	CamelStream *camel_stream;
	CamelMimePart *mime_part;

	hash_table = composer->priv->inline_images_by_url;
	mime_part = g_hash_table_lookup (hash_table, uri);

	if (mime_part == NULL) {
		hash_table = composer->priv->inline_images;
		mime_part = g_hash_table_lookup (hash_table, uri);
	}

	/* If this is not an inline image request,
	 * allow the signal emission to continue. */
	if (mime_part == NULL)
		return;

	array = g_byte_array_new ();
	camel_stream = camel_stream_mem_new_with_byte_array (array);
	wrapper = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
	camel_data_wrapper_decode_to_stream (wrapper, camel_stream, NULL);

	gtk_html_write (html, stream, (gchar *) array->data, array->len);

	gtk_html_end (html, stream, GTK_HTML_STREAM_OK);

	g_object_unref (camel_stream);

	/* gtk_html_end() destroys the GtkHTMLStream, so we need to
	 * stop the signal emission so nothing else tries to use it. */
	g_signal_stop_emission_by_name (html, "url-requested");
}

void
e_composer_private_constructed (EMsgComposer *composer)
{
	EMsgComposerPrivate *priv = composer->priv;
	EShell *shell;
	EFocusTracker *focus_tracker;
	GtkhtmlEditor *editor;
	GtkUIManager *ui_manager;
	GtkAction *action;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *send_widget;
	GtkWindow *window;
	GtkHTML *html;
	const gchar *path;
	gboolean small_screen_mode;
	gchar *filename;
	gint ii;
	GError *error = NULL;

	editor = GTKHTML_EDITOR (composer);
	html = gtkhtml_editor_get_html (editor);
	ui_manager = gtkhtml_editor_get_ui_manager (editor);

	shell = e_msg_composer_get_shell (composer);
	small_screen_mode = e_shell_get_small_screen_mode (shell);

	if (small_screen_mode) {
#if 0
		/* In the lite composer, for small screens, we are not
		 * ready yet to hide the menubar.  It still has useful
		 * items like the ones to show/hide the various header
		 * fields, plus the security options.
		 *
		 * When we move those options out of the menu and into
		 * the composer's toplevel, we can probably get rid of
		 * the menu.
		 */
		widget = gtkhtml_editor_get_managed_widget (editor, "/main-menu");
		gtk_widget_hide (widget);
#endif
		widget = gtkhtml_editor_get_managed_widget (editor, "/main-toolbar");
		gtk_toolbar_set_style (GTK_TOOLBAR (widget), GTK_TOOLBAR_BOTH_HORIZ);
		gtk_widget_hide (widget);

	}

	/* Each composer window gets its own window group. */
	window = GTK_WINDOW (composer);
	priv->window_group = gtk_window_group_new ();
	gtk_window_group_add_window (priv->window_group, window);

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
		(GDestroyNotify) g_object_unref);

	priv->charset = e_composer_get_default_charset ();

	priv->is_from_message = FALSE;

	e_composer_actions_init (composer);

	filename = e_composer_find_data_file ("evolution-composer.ui");
	gtk_ui_manager_add_ui_from_file (ui_manager, filename, &error);
	g_free (filename);

	/* We set the send button as important to have a label */
	path = "/main-toolbar/pre-main-toolbar/send";
	send_widget = gtk_ui_manager_get_widget (ui_manager, path);
	gtk_tool_item_set_is_important (GTK_TOOL_ITEM (send_widget), TRUE);

	/* Tooltips for all buttons in toolbar that don't yet have one */
	path = "/main-toolbar/undo";
	widget = gtk_ui_manager_get_widget (ui_manager, path);
	gtk_widget_set_tooltip_text (widget, _("Undo the last action"));

	path = "/main-toolbar/redo";
	widget = gtk_ui_manager_get_widget (ui_manager, path);
	gtk_widget_set_tooltip_text (widget, _("Redo the last undone action"));

	path = "/main-toolbar/show-find";
	widget = gtk_ui_manager_get_widget (ui_manager, path);
	gtk_widget_set_tooltip_text (widget, _("Search for text"));

	path = "/main-toolbar/show-replace";
	widget = gtk_ui_manager_get_widget (ui_manager, path);
	gtk_widget_set_tooltip_text (widget, _("Search for and replace text"));

	composer_setup_charset_menu (composer);

	if (error != NULL) {
		/* Henceforth, bad things start happening. */
		g_critical ("%s", error->message);
		g_clear_error (&error);
	}

	/* Configure an EFocusTracker to manage selection actions. */

	focus_tracker = e_focus_tracker_new (GTK_WINDOW (composer));

	action = gtkhtml_editor_get_action (editor, "cut");
	e_focus_tracker_set_cut_clipboard_action (focus_tracker, action);

	action = gtkhtml_editor_get_action (editor, "copy");
	e_focus_tracker_set_copy_clipboard_action (focus_tracker, action);

	action = gtkhtml_editor_get_action (editor, "paste");
	e_focus_tracker_set_paste_clipboard_action (focus_tracker, action);

	action = gtkhtml_editor_get_action (editor, "select-all");
	e_focus_tracker_set_select_all_action (focus_tracker, action);

	priv->focus_tracker = focus_tracker;

	/* Construct the header table. */

	container = editor->vbox;

	widget = e_composer_header_table_new (shell);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	gtk_box_pack_start (GTK_BOX (editor->vbox), widget, FALSE, FALSE, 0);
	if (small_screen_mode)
		gtk_box_reorder_child (GTK_BOX (editor->vbox), widget, 1);
	else
		gtk_box_reorder_child (GTK_BOX (editor->vbox), widget, 2);

	priv->header_table = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Construct the attachment paned. */

	if (small_screen_mode) {
		e_attachment_paned_set_default_height (75); /* short attachment bar for Anjal */
		e_attachment_icon_view_set_default_icon_size (GTK_ICON_SIZE_BUTTON);
	}
	widget = e_attachment_paned_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->attachment_paned = g_object_ref (widget);
	gtk_widget_show (widget);

	if (small_screen_mode) {
		GtkWidget *tmp, *tmp1, *tmp_box, *container;
		GtkWidget *combo;

		combo = e_attachment_paned_get_view_combo (
			E_ATTACHMENT_PANED (widget));
		gtk_widget_hide (combo);
		container = e_attachment_paned_get_controls_container (
			E_ATTACHMENT_PANED (widget));

		tmp_box = gtk_hbox_new (FALSE, 0);

		tmp = gtk_hbox_new (FALSE, 0);
		tmp1 = gtk_image_new_from_icon_name (
			"mail-send", GTK_ICON_SIZE_BUTTON);
		gtk_box_pack_start ((GtkBox *)tmp, tmp1, FALSE, FALSE, 0);
		tmp1 = gtk_label_new_with_mnemonic (_("S_end"));
		gtk_box_pack_start ((GtkBox *)tmp, tmp1, FALSE, FALSE, 6);
		gtk_widget_show_all (tmp);
		gtk_widget_reparent (send_widget, tmp_box);
		gtk_box_set_child_packing (
			GTK_BOX (tmp_box), send_widget,
			FALSE, FALSE, 6, GTK_PACK_END);
		gtk_tool_item_set_is_important (GTK_TOOL_ITEM (send_widget), TRUE);
		send_widget = gtk_bin_get_child ((GtkBin *)send_widget);
		gtk_container_remove (
			GTK_CONTAINER (send_widget),
			gtk_bin_get_child (GTK_BIN (send_widget)));
		gtk_container_add ((GtkContainer *)send_widget, tmp);
		gtk_button_set_relief ((GtkButton *)send_widget, GTK_RELIEF_NORMAL);
		path = "/main-toolbar/pre-main-toolbar/save-draft";
		send_widget = gtk_ui_manager_get_widget (ui_manager, path);
		tmp = gtk_hbox_new (FALSE, 0);
		tmp1 = gtk_image_new_from_stock (
			GTK_STOCK_SAVE, GTK_ICON_SIZE_BUTTON);
		gtk_box_pack_start ((GtkBox *)tmp, tmp1, FALSE, FALSE, 0);
		tmp1 = gtk_label_new_with_mnemonic (_("Save draft"));
		gtk_box_pack_start ((GtkBox *)tmp, tmp1, FALSE, FALSE, 3);
		gtk_widget_show_all (tmp);
		gtk_widget_reparent (send_widget, tmp_box);
		gtk_box_set_child_packing (
			GTK_BOX (tmp_box), send_widget,
			FALSE, FALSE, 6, GTK_PACK_END);
		gtk_tool_item_set_is_important (GTK_TOOL_ITEM (send_widget), TRUE);
		send_widget = gtk_bin_get_child ((GtkBin *)send_widget);
		gtk_container_remove (
			GTK_CONTAINER (send_widget),
			gtk_bin_get_child (GTK_BIN (send_widget)));
		gtk_container_add ((GtkContainer *)send_widget, tmp);
		gtk_button_set_relief ((GtkButton *)send_widget, GTK_RELIEF_NORMAL);

		gtk_widget_show (tmp_box);
		gtk_box_pack_end (GTK_BOX (container), tmp_box, FALSE, FALSE, 3);
	}

	g_object_set_data ((GObject *)composer, "vbox", editor->vbox);

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

			case E_COMPOSER_HEADER_REPLY_TO:
				action = ACTION (VIEW_REPLY_TO);
				break;

			default:
				continue;
		}

		e_mutual_binding_new (
			header, "sensitive",
			action, "sensitive");

		e_mutual_binding_new (
			header, "visible",
			action, "active");
	}

	/* Install a handler for inline images. */

	/* XXX We no longer use GtkhtmlEditor::uri-requested because it
	 *     conflicts with EWebView's url_requested() method, which
	 *     unconditionally launches an async operation.  I changed
	 *     GtkHTML::url-requested to be a G_SIGNAL_RUN_LAST so that
	 *     our handler runs first.  If we can handle the request
	 *     we'll stop the signal emission to prevent EWebView from
	 *     launching an async operation.  Messy, but works until we
	 *     switch to WebKit.  --mbarnes */

	g_signal_connect (
		html, "url-requested",
		G_CALLBACK (msg_composer_url_requested_cb), composer);

	priv->mail_sent = FALSE;
}

void
e_composer_private_dispose (EMsgComposer *composer)
{
	if (composer->priv->gconf_bridge_binding_ids) {
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

		g_array_free (composer->priv->gconf_bridge_binding_ids, TRUE);
		composer->priv->gconf_bridge_binding_ids = NULL;
	}

	if (composer->priv->shell != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (composer->priv->shell),
			&composer->priv->shell);
		composer->priv->shell = NULL;
	}

	if (composer->priv->header_table != NULL) {
		g_object_unref (composer->priv->header_table);
		composer->priv->header_table = NULL;
	}

	if (composer->priv->attachment_paned != NULL) {
		g_object_unref (composer->priv->attachment_paned);
		composer->priv->attachment_paned = NULL;
	}

	if (composer->priv->focus_tracker != NULL) {
		g_object_unref (composer->priv->focus_tracker);
		composer->priv->focus_tracker = NULL;
	}

	if (composer->priv->window_group != NULL) {
		g_object_unref (composer->priv->window_group);
		composer->priv->window_group = NULL;
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
		g_object_unref (composer->priv->redirect);
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

gboolean
e_composer_paste_html (EMsgComposer *composer,
                       GtkClipboard *clipboard)
{
	GtkhtmlEditor *editor;
	gchar *html;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	g_return_val_if_fail (GTK_IS_CLIPBOARD (clipboard), FALSE);

	html = e_clipboard_wait_for_html (clipboard);
	g_return_val_if_fail (html != NULL, FALSE);

	editor = GTKHTML_EDITOR (composer);
	gtkhtml_editor_insert_html (editor, html);

	g_free (html);

	return TRUE;
}

gboolean
e_composer_paste_image (EMsgComposer *composer,
                        GtkClipboard *clipboard)
{
	GtkhtmlEditor *editor;
	EAttachmentStore *store;
	EAttachmentView *view;
	GdkPixbuf *pixbuf = NULL;
	gchar *filename = NULL;
	gchar *uri = NULL;
	gboolean success = FALSE;
	GError *error = NULL;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	g_return_val_if_fail (GTK_IS_CLIPBOARD (clipboard), FALSE);

	editor = GTKHTML_EDITOR (composer);
	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	/* Extract the image data from the clipboard. */
	pixbuf = gtk_clipboard_wait_for_image (clipboard);
	g_return_val_if_fail (pixbuf != NULL, FALSE);

	/* Reserve a temporary file. */
	filename = e_mktemp (NULL);
	if (filename == NULL) {
		g_set_error (
			&error, G_FILE_ERROR,
			g_file_error_from_errno (errno),
			"Could not create temporary file: %s",
			g_strerror (errno));
		goto exit;
	}

	/* Save the pixbuf as a temporary file in image/png format. */
	if (!gdk_pixbuf_save (pixbuf, filename, "png", &error, NULL))
		goto exit;

	/* Convert the filename to a URI. */
	uri = g_filename_to_uri (filename, NULL, &error);
	if (uri == NULL)
		goto exit;

	/* In HTML mode, paste the image into the message body.
	 * In text mode, add the image to the attachment store. */
	if (gtkhtml_editor_get_html_mode (editor))
		gtkhtml_editor_insert_image (editor, uri);
	else {
		EAttachment *attachment;

		attachment = e_attachment_new_for_uri (uri);
		e_attachment_store_add_attachment (store, attachment);
		e_attachment_load_async (
			attachment, (GAsyncReadyCallback)
			e_attachment_load_handle_error, composer);
		g_object_unref (attachment);
	}

	success = TRUE;

exit:
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_object_unref (pixbuf);
	g_free (filename);
	g_free (uri);

	return success;
}

gboolean
e_composer_paste_text (EMsgComposer *composer,
                       GtkClipboard *clipboard)
{
	GtkhtmlEditor *editor;
	gchar *text;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	g_return_val_if_fail (GTK_IS_CLIPBOARD (clipboard), FALSE);

	text = gtk_clipboard_wait_for_text (clipboard);
	g_return_val_if_fail (text != NULL, FALSE);

	editor = GTKHTML_EDITOR (composer);
	gtkhtml_editor_insert_text (editor, text);

	g_free (text);

	return TRUE;
}

gboolean
e_composer_paste_uris (EMsgComposer *composer,
                       GtkClipboard *clipboard)
{
	EAttachmentStore *store;
	EAttachmentView *view;
	gchar **uris;
	gint ii;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	g_return_val_if_fail (GTK_IS_CLIPBOARD (clipboard), FALSE);

	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	/* Extract the URI data from the clipboard. */
	uris = gtk_clipboard_wait_for_uris (clipboard);
	g_return_val_if_fail (uris != NULL, FALSE);

	/* Add the URIs to the attachment store. */
	for (ii = 0; uris[ii] != NULL; ii++) {
		EAttachment *attachment;

		attachment = e_attachment_new_for_uri (uris[ii]);
		e_attachment_store_add_attachment (store, attachment);
		e_attachment_load_async (
			attachment, (GAsyncReadyCallback)
			e_attachment_load_handle_error, composer);
		g_object_unref (attachment);
	}

	return TRUE;
}
