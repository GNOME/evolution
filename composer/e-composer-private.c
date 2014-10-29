/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-composer-private.h"
#include "e-composer-spell-header.h"
#include "e-util/e-util-private.h"

/* Initial height of the picture gallery. */
#define GALLERY_INITIAL_HEIGHT 150

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
	camel_data_wrapper_decode_to_stream_sync (
		wrapper, camel_stream, NULL, NULL);

	gtk_html_write (html, stream, (gchar *) array->data, array->len);

	gtk_html_end (html, stream, GTK_HTML_STREAM_OK);

	g_object_unref (camel_stream);

	/* gtk_html_end() destroys the GtkHTMLStream, so we need to
	 * stop the signal emission so nothing else tries to use it. */
	g_signal_stop_emission_by_name (html, "url-requested");
}

static void
composer_update_gallery_visibility (EMsgComposer *composer)
{
	GtkhtmlEditor *editor;
	GtkToggleAction *toggle_action;
	gboolean gallery_active;
	gboolean html_mode;

	editor = GTKHTML_EDITOR (composer);
	html_mode = gtkhtml_editor_get_html_mode (editor);

	toggle_action = GTK_TOGGLE_ACTION (ACTION (PICTURE_GALLERY));
	gallery_active = gtk_toggle_action_get_active (toggle_action);

	if (html_mode && gallery_active) {
		gtk_widget_show (composer->priv->gallery_scrolled_window);
		gtk_widget_show (composer->priv->gallery_icon_view);
	} else {
		gtk_widget_hide (composer->priv->gallery_scrolled_window);
		gtk_widget_hide (composer->priv->gallery_icon_view);
	}
}

static void
composer_spell_languages_changed (EMsgComposer *composer,
                                  GList *languages)
{
	EComposerHeader *header;
	EComposerHeaderTable *table;

	table = e_msg_composer_get_header_table (composer);
	header = e_composer_header_table_get_header (
		table, E_COMPOSER_HEADER_SUBJECT);

	e_composer_spell_header_set_languages (
		E_COMPOSER_SPELL_HEADER (header), languages);
}

void
e_composer_private_constructed (EMsgComposer *composer)
{
	EMsgComposerPrivate *priv = composer->priv;
	EFocusTracker *focus_tracker;
	EShell *shell;
	EWebViewGtkHTML *web_view;
	EClientCache *client_cache;
	GtkhtmlEditor *editor;
	GtkUIManager *ui_manager;
	GtkAction *action;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *send_widget;
	GtkWindow *window;
	GSettings *settings;
	const gchar *path;
	gchar *filename, *gallery_path;
	gint ii;
	GError *error = NULL;

	editor = GTKHTML_EDITOR (composer);
	ui_manager = gtkhtml_editor_get_ui_manager (editor);

	settings = g_settings_new ("org.gnome.evolution.mail");

	shell = e_msg_composer_get_shell (composer);
	client_cache = e_shell_get_client_cache (shell);
	web_view = e_msg_composer_get_web_view (composer);

	/* Each composer window gets its own window group. */
	window = GTK_WINDOW (composer);
	priv->window_group = gtk_window_group_new ();
	gtk_window_group_add_window (priv->window_group, window);

	priv->async_actions = gtk_action_group_new ("async");
	priv->charset_actions = gtk_action_group_new ("charset");
	priv->composer_actions = gtk_action_group_new ("composer");

	priv->extra_hdr_names = g_ptr_array_new ();
	priv->extra_hdr_values = g_ptr_array_new ();

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
	priv->disable_signature = FALSE;

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

	container = editor->vbox;

	/* Construct the activity bar. */

	widget = e_activity_bar_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->activity_bar = g_object_ref_sink (widget);
	/* EActivityBar controls its own visibility. */

	/* Construct the alert bar for errors. */

	widget = e_alert_bar_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->alert_bar = g_object_ref_sink (widget);
	/* EAlertBar controls its own visibility. */

	/* Construct the header table. */

	widget = e_composer_header_table_new (client_cache);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (container), widget, 2);
	priv->header_table = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	g_signal_connect (
		G_OBJECT (composer), "spell-languages-changed",
		G_CALLBACK (composer_spell_languages_changed), NULL);

	/* Construct the attachment paned. */

	widget = e_attachment_paned_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->attachment_paned = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	g_object_bind_property (
		web_view, "editable",
		widget, "editable",
		G_BINDING_SYNC_CREATE);

	container = e_attachment_paned_get_content_area (
		E_ATTACHMENT_PANED (priv->attachment_paned));

	widget = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_widget_set_size_request (widget, -1, GALLERY_INITIAL_HEIGHT);
	gtk_paned_pack1 (GTK_PANED (container), widget, FALSE, FALSE);
	priv->gallery_scrolled_window = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	/* Reparent the scrolled window containing the GtkHTML widget
	 * into the content area of the top attachment pane. */

	widget = GTK_WIDGET (web_view);
	widget = gtk_widget_get_parent (widget);
	gtk_widget_reparent (widget, container);

	/* Construct the picture gallery. */

	container = priv->gallery_scrolled_window;

	/* FIXME This should be an EMsgComposer property. */
	gallery_path = g_settings_get_string (
		settings, "composer-gallery-path");
	widget = e_picture_gallery_new (gallery_path);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->gallery_icon_view = g_object_ref_sink (widget);
	g_free (gallery_path);

	e_signal_connect_notify (
		composer, "notify::html-mode",
		G_CALLBACK (composer_update_gallery_visibility), NULL);

	g_signal_connect_swapped (
		ACTION (PICTURE_GALLERY), "toggled",
		G_CALLBACK (composer_update_gallery_visibility), composer);

	/* XXX What is this for? */
	g_object_set_data (G_OBJECT (composer), "vbox", editor->vbox);

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

		g_object_bind_property (
			header, "sensitive",
			action, "sensitive",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);

		g_object_bind_property (
			header, "visible",
			action, "active",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);
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
		web_view, "url-requested",
		G_CALLBACK (msg_composer_url_requested_cb), composer);

	g_object_unref (settings);
}

void
e_composer_private_dispose (EMsgComposer *composer)
{
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

	if (composer->priv->activity_bar != NULL) {
		g_object_unref (composer->priv->activity_bar);
		composer->priv->activity_bar = NULL;
	}

	if (composer->priv->alert_bar != NULL) {
		g_object_unref (composer->priv->alert_bar);
		composer->priv->alert_bar = NULL;
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

	if (composer->priv->async_actions != NULL) {
		g_object_unref (composer->priv->async_actions);
		composer->priv->async_actions = NULL;
	}

	if (composer->priv->charset_actions != NULL) {
		g_object_unref (composer->priv->charset_actions);
		composer->priv->charset_actions = NULL;
	}

	if (composer->priv->composer_actions != NULL) {
		g_object_unref (composer->priv->composer_actions);
		composer->priv->composer_actions = NULL;
	}

	g_clear_object (&composer->priv->gallery_icon_view);
	g_clear_object (&composer->priv->gallery_scrolled_window);

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
	g_free (composer->priv->selected_signature_uid);

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
	GSettings *settings;
	gchar *charset;

	settings = g_settings_new ("org.gnome.evolution.mail");

	charset = g_settings_get_string (settings, "composer-charset");

	/* See what charset the mailer is using.
	 * XXX We should not have to know where this lives in GSettings.
	 *     Need a mail_config_get_default_charset() that does this. */
	if (!charset || charset[0] == '\0') {
		g_free (charset);
		charset = g_settings_get_string (settings, "charset");
		if (charset != NULL && *charset == '\0') {
			g_free (charset);
			charset = NULL;
		}
	}

	g_object_unref (settings);

	if (charset == NULL)
		charset = g_strdup (camel_iconv_locale_charset ());

	if (charset == NULL)
		charset = g_strdup ("us-ascii");

	return charset;
}

gchar *
e_composer_decode_clue_value (const gchar *encoded_value)
{
	GString *buffer;
	const gchar *cp;

	/* Decode a GtkHtml "ClueFlow" value. */

	g_return_val_if_fail (encoded_value != NULL, NULL);

	buffer = g_string_sized_new (strlen (encoded_value));

	/* Copy the value, decoding escaped characters as we go. */
	cp = encoded_value;
	while (*cp != '\0') {
		if (*cp == '.') {
			cp++;
			switch (*cp) {
				case '.':
					g_string_append_c (buffer, '.');
					break;
				case '1':
					g_string_append_c (buffer, '"');
					break;
				case '2':
					g_string_append_c (buffer, '=');
					break;
				default:
					/* Invalid escape sequence. */
					g_string_free (buffer, TRUE);
					return NULL;
			}
		} else
			g_string_append_c (buffer, *cp);
		cp++;
	}

	return g_string_free (buffer, FALSE);
}

gchar *
e_composer_encode_clue_value (const gchar *decoded_value)
{
	gchar *encoded_value;
	gchar **strv;

	/* Encode a GtkHtml "ClueFlow" value. */

	g_return_val_if_fail (decoded_value != NULL, NULL);

	/* XXX This is inefficient but easy to understand. */

	encoded_value = g_strdup (decoded_value);

	/* Substitution: '.' --> '..' (do this first) */
	if (strchr (encoded_value, '.') != NULL) {
		strv = g_strsplit (encoded_value, ".", 0);
		g_free (encoded_value);
		encoded_value = g_strjoinv ("..", strv);
		g_strfreev (strv);
	}

	/* Substitution: '"' --> '.1' */
	if (strchr (encoded_value, '"') != NULL) {
		strv = g_strsplit (encoded_value, """", 0);
		g_free (encoded_value);
		encoded_value = g_strjoinv (".1", strv);
		g_strfreev (strv);
	}

	/* Substitution: '=' --> '.2' */
	if (strchr (encoded_value, '=') != NULL) {
		strv = g_strsplit (encoded_value, "=", 0);
		g_free (encoded_value);
		encoded_value = g_strjoinv (".2", strv);
		g_strfreev (strv);
	}

	return encoded_value;
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

gboolean
e_composer_selection_is_image_uris (EMsgComposer *composer,
                                    GtkSelectionData *selection)
{
	gboolean all_image_uris = TRUE;
	gchar **uris;
	guint ii;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	g_return_val_if_fail (selection != NULL, FALSE);

	uris = gtk_selection_data_get_uris (selection);

	if (uris == NULL)
		return FALSE;

	for (ii = 0; uris[ii] != NULL; ii++) {
		GFile *file;
		GFileInfo *file_info;
		GdkPixbufLoader *loader;
		const gchar *attribute;
		const gchar *content_type;
		gchar *mime_type = NULL;

		file = g_file_new_for_uri (uris[ii]);
		attribute = G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE;

		/* XXX This blocks, but we're requesting the fast content
		 *     type (which only inspects filenames), so hopefully
		 *     it won't be noticeable.  Also, this is best effort
		 *     so we don't really care if it fails. */
		file_info = g_file_query_info (
			file, attribute, G_FILE_QUERY_INFO_NONE, NULL, NULL);

		if (file_info == NULL) {
			g_object_unref (file);
			all_image_uris = FALSE;
			break;
		}

		content_type = g_file_info_get_attribute_string (
			file_info, attribute);
		mime_type = g_content_type_get_mime_type (content_type);

		g_object_unref (file_info);
		g_object_unref (file);

		if (mime_type == NULL) {
			all_image_uris = FALSE;
			break;
		}

		/* Easy way to determine if a MIME type is a supported
		 * image format: try creating a GdkPixbufLoader for it. */
		loader = gdk_pixbuf_loader_new_with_mime_type (mime_type, NULL);

		g_free (mime_type);

		if (loader == NULL) {
			all_image_uris = FALSE;
			break;
		}

		gdk_pixbuf_loader_close (loader, NULL);
		g_object_unref (loader);
	}

	g_strfreev (uris);

	return all_image_uris;
}

static gboolean
add_signature_delimiter (EMsgComposer *composer)
{
	GSettings *settings;
	gboolean signature_delim;

	/* FIXME This should be an EMsgComposer property. */
	settings = g_settings_new ("org.gnome.evolution.mail");
	signature_delim = !g_settings_get_boolean (
		settings, "composer-no-signature-delim");
	g_object_unref (settings);

	return signature_delim;
}

static gboolean
use_top_signature (EMsgComposer *composer)
{
	GSettings *settings;
	gboolean top_signature;

	/* FIXME This should be an EMsgComposer property. */
	settings = g_settings_new ("org.gnome.evolution.mail");
	top_signature = g_settings_get_boolean (
		settings, "composer-top-signature");
	g_object_unref (settings);

	return top_signature;
}

static void
composer_load_signature_cb (EMailSignatureComboBox *combo_box,
                            GAsyncResult *result,
                            EMsgComposer *composer)
{
	GString *html_buffer = NULL;
	GtkhtmlEditor *editor;
	gchar *contents = NULL;
	gsize length = 0;
	const gchar *active_id;
	gchar *encoded_uid = NULL;
	gboolean top_signature;
	gboolean is_html;
	GError *error = NULL;

	e_mail_signature_combo_box_load_selected_finish (
		combo_box, result, &contents, &length, &is_html, &error);

	/* FIXME Use an EAlert here. */
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		goto exit;
	}

	if (composer->priv->disable_signature)
		goto exit;

	/* "Edit as New Message" sets "priv->is_from_message".
	 * Always put the signature at the bottom for that case. */
	top_signature =
		use_top_signature (composer) &&
		!composer->priv->is_from_message;

	if (contents == NULL)
		goto insert;

	if (!is_html) {
		gchar *html;

		html = camel_text_to_html (contents, 0, 0);
		if (html) {
			g_free (contents);

			contents = html;
			length = strlen (contents);
		}
	}

	/* Generate HTML code for the signature. */

	html_buffer = g_string_sized_new (1024);

	/* The combo box active ID is the signature's ESource UID. */
	active_id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box));

	if (active_id != NULL && *active_id != '\0')
		encoded_uid = e_composer_encode_clue_value (active_id);

	g_string_append_printf (
		html_buffer,
		"<!--+GtkHTML:<DATA class=\"ClueFlow\" "
		"    key=\"signature\" value=\"1\">-->"
		"<!--+GtkHTML:<DATA class=\"ClueFlow\" "
		"    key=\"signature_name\" value=\"uid:%s\">-->",
		(encoded_uid != NULL) ? encoded_uid : "");

	g_string_append (
		html_buffer,
		"<TABLE WIDTH=\"100%%\" CELLSPACING=\"0\""
		" CELLPADDING=\"0\"><TR><TD>");

	if (!is_html)
		g_string_append (html_buffer, "<PRE>\n");

	/* The signature dash convention ("-- \n") is specified
	 * in the "Son of RFC 1036", section 4.3.2.
	 * http://www.chemie.fu-berlin.de/outerspace/netnews/son-of-1036.html
	 */
	if (add_signature_delimiter (composer)) {
		const gchar *delim;
		const gchar *delim_nl;

		if (is_html) {
			delim = "-- \n<BR>";
			delim_nl = "\n-- \n<BR>";
		} else {
			delim = "-- \n";
			delim_nl = "\n-- \n";
		}

		/* Skip the delimiter if the signature already has one. */
		if (g_ascii_strncasecmp (contents, delim, strlen (delim)) == 0)
			;  /* skip */
		else if (e_util_strstrcase (contents, delim_nl) != NULL)
			;  /* skip */
		else
			g_string_append (html_buffer, delim);
	}

	g_string_append_len (html_buffer, contents, length);

	if (!is_html)
		g_string_append (html_buffer, "</PRE>\n");

	if (top_signature)
		g_string_append (html_buffer, "<BR>");

	g_string_append (html_buffer, "</TD></TR></TABLE>");

	g_free (encoded_uid);
	g_free (contents);

insert:
	/* Remove the old signature and insert the new one. */

	editor = GTKHTML_EDITOR (composer);

	/* This prevents our command before/after callbacks from
	 * screwing around with the signature as we insert it. */
	composer->priv->in_signature_insert = TRUE;

	gtkhtml_editor_freeze (editor);
	gtkhtml_editor_run_command (editor, "cursor-position-save");
	gtkhtml_editor_undo_begin (editor, "Set signature", "Reset signature");

	gtkhtml_editor_run_command (editor, "block-selection");
	gtkhtml_editor_run_command (editor, "cursor-bod");
	if (gtkhtml_editor_search_by_data (editor, 1, "ClueFlow", "signature", "1")) {
		gtkhtml_editor_run_command (editor, "select-paragraph");
		gtkhtml_editor_run_command (editor, "delete");
		gtkhtml_editor_set_paragraph_data (editor, "signature", "0");
		gtkhtml_editor_run_command (editor, "delete-back");
	}
	gtkhtml_editor_run_command (editor, "unblock-selection");

	if (html_buffer != NULL) {
		gtkhtml_editor_run_command (editor, "insert-paragraph");
		if (!gtkhtml_editor_run_command (editor, "cursor-backward"))
			gtkhtml_editor_run_command (editor, "insert-paragraph");
		else
			gtkhtml_editor_run_command (editor, "cursor-forward");

		gtkhtml_editor_set_paragraph_data (editor, "orig", "0");
		gtkhtml_editor_run_command (editor, "indent-zero");
		gtkhtml_editor_run_command (editor, "style-normal");
		gtkhtml_editor_insert_html (editor, html_buffer->str);

		g_string_free (html_buffer, TRUE);

	} else if (top_signature) {
		/* Insert paragraph after the signature ClueFlow stuff. */
		if (gtkhtml_editor_run_command (editor, "cursor-forward"))
			gtkhtml_editor_run_command (editor, "insert-paragraph");
	}

	gtkhtml_editor_undo_end (editor);
	gtkhtml_editor_run_command (editor, "cursor-position-restore");
	gtkhtml_editor_thaw (editor);

	composer->priv->in_signature_insert = FALSE;

exit:
	g_object_unref (composer);
}

static gboolean
is_null_or_none (const gchar *text)
{
	return !text || g_strcmp0 (text, "none") == 0;
}

void
e_composer_update_signature (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	EMailSignatureComboBox *combo_box;
	const gchar *signature_uid;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	/* Do nothing if we're redirecting a message or we disabled
	 * the signature on purpose */
	if (composer->priv->redirect || composer->priv->disable_signature)
		return;

	table = e_msg_composer_get_header_table (composer);
	signature_uid = e_composer_header_table_get_signature_uid (table);

	/* this is a case when the signature combo cleared itself for a reload */
	if (!signature_uid)
		return;

	if (g_strcmp0 (signature_uid, composer->priv->selected_signature_uid) == 0 ||
	    (is_null_or_none (signature_uid) && is_null_or_none (composer->priv->selected_signature_uid)))
		return;

	g_free (composer->priv->selected_signature_uid);
	composer->priv->selected_signature_uid = g_strdup (signature_uid);

	combo_box = e_composer_header_table_get_signature_combo_box (table);

	/* XXX Signature files should be local and therefore load quickly,
	 *     so while we do load them asynchronously we don't allow for
	 *     user cancellation and we keep the composer alive until the
	 *     asynchronous loading is complete. */
	e_mail_signature_combo_box_load_selected (
		combo_box, G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) composer_load_signature_cb,
		g_object_ref (composer));
}

