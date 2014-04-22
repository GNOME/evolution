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

#define UNICODE_ZERO_WIDTH_SPACE "\xe2\x80\x8b"

static void
composer_setup_charset_menu (EMsgComposer *composer)
{
	EEditor *editor;
	GtkUIManager *ui_manager;
	const gchar *path;
	GList *list;
	guint merge_id;

	editor = e_msg_composer_get_editor (composer);
	ui_manager = e_editor_get_ui_manager (editor);
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
composer_update_gallery_visibility (EMsgComposer *composer)
{
	EEditor *editor;
	EEditorWidget *editor_widget;
	GtkToggleAction *toggle_action;
	gboolean gallery_active;
	gboolean is_html;

	editor = e_msg_composer_get_editor (composer);
	editor_widget = e_editor_get_editor_widget (editor);
	is_html = e_editor_widget_get_html_mode (editor_widget);

	toggle_action = GTK_TOGGLE_ACTION (ACTION (PICTURE_GALLERY));
	gallery_active = gtk_toggle_action_get_active (toggle_action);

	if (is_html && gallery_active) {
		gtk_widget_show (composer->priv->gallery_scrolled_window);
		gtk_widget_show (composer->priv->gallery_icon_view);
	} else {
		gtk_widget_hide (composer->priv->gallery_scrolled_window);
		gtk_widget_hide (composer->priv->gallery_icon_view);
	}
}

void
e_composer_private_constructed (EMsgComposer *composer)
{
	EMsgComposerPrivate *priv = composer->priv;
	EFocusTracker *focus_tracker;
	EComposerHeader *header;
	EShell *shell;
	EClientCache *client_cache;
	EEditor *editor;
	EEditorWidget *editor_widget;
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

	editor = e_msg_composer_get_editor (composer);
	ui_manager = e_editor_get_ui_manager (editor);
	editor_widget = e_editor_get_editor_widget (editor);

	settings = g_settings_new ("org.gnome.evolution.mail");

	shell = e_msg_composer_get_shell (composer);
	client_cache = e_shell_get_client_cache (shell);

	/* Each composer window gets its own window group. */
	window = GTK_WINDOW (composer);
	priv->window_group = gtk_window_group_new ();
	gtk_window_group_add_window (priv->window_group, window);

	priv->async_actions = gtk_action_group_new ("async");
	priv->charset_actions = gtk_action_group_new ("charset");
	priv->composer_actions = gtk_action_group_new ("composer");

	priv->extra_hdr_names = g_ptr_array_new ();
	priv->extra_hdr_values = g_ptr_array_new ();

	priv->charset = e_composer_get_default_charset ();

	priv->is_from_message = FALSE;
	priv->is_from_new_message = FALSE;
	priv->set_signature_from_message = FALSE;
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

	action = e_editor_get_action (editor, "cut");
	e_focus_tracker_set_cut_clipboard_action (focus_tracker, action);

	action = e_editor_get_action (editor, "copy");
	e_focus_tracker_set_copy_clipboard_action (focus_tracker, action);

	action = e_editor_get_action (editor, "paste");
	e_focus_tracker_set_paste_clipboard_action (focus_tracker, action);

	action = e_editor_get_action (editor, "select-all");
	e_focus_tracker_set_select_all_action (focus_tracker, action);

	priv->focus_tracker = focus_tracker;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (composer), widget);
	gtk_widget_show (widget);

	container = widget;

	/* Construct the main menu and toolbar. */

	widget = e_editor_get_managed_widget (editor, "/main-menu");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = e_editor_get_managed_widget (editor, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/* Construct the header table. */

	widget = e_composer_header_table_new (client_cache);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->header_table = g_object_ref (widget);
	gtk_widget_show (widget);

	header = e_composer_header_table_get_header (
		E_COMPOSER_HEADER_TABLE (widget),
		E_COMPOSER_HEADER_SUBJECT);
	g_object_bind_property (
		editor_widget, "spell-checker",
		header->input_widget, "spell-checker",
		G_BINDING_SYNC_CREATE);

	/* Construct the editing toolbars.  We'll have to reparent
	 * the embedded EEditorWidget a little further down. */

	widget = GTK_WIDGET (editor);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/* Construct the attachment paned. */

	widget = e_attachment_paned_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->attachment_paned = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	g_object_bind_property (
		editor_widget, "editable",
		widget, "sensitive",
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
	priv->gallery_scrolled_window = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Reparent the scrolled window containing the web view
	 * widget into the content area of the top attachment pane. */

	widget = GTK_WIDGET (editor_widget);
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

	g_signal_connect_swapped (
		editor_widget, "notify::mode",
		G_CALLBACK (composer_update_gallery_visibility), composer);

	g_signal_connect_swapped (
		ACTION (PICTURE_GALLERY), "toggled",
		G_CALLBACK (composer_update_gallery_visibility), composer);

	/* Initial sync */
	composer_update_gallery_visibility (composer);

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

	/* Disable actions that start asynchronous activities while an
	 * asynchronous activity is in progress.  We enforce this with
	 * a simple inverted binding to EEditor's "busy" property. */

	g_object_bind_property (
		editor, "busy",
		priv->async_actions, "sensitive",
		G_BINDING_SYNC_CREATE |
		G_BINDING_INVERT_BOOLEAN);

	g_object_bind_property (
		editor, "busy",
		priv->header_table, "sensitive",
		G_BINDING_SYNC_CREATE |
		G_BINDING_INVERT_BOOLEAN);

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

	if (composer->priv->editor != NULL) {
		g_object_unref (composer->priv->editor);
		composer->priv->editor = NULL;
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

	if (composer->priv->gallery_scrolled_window != NULL) {
		g_object_unref (composer->priv->gallery_scrolled_window);
		composer->priv->gallery_scrolled_window = NULL;
	}

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

gboolean
e_composer_paste_html (EMsgComposer *composer,
                       GtkClipboard *clipboard)
{
	EEditor *editor;
	EEditorWidget *editor_widget;
	EEditorSelection *editor_selection;
	gchar *html;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	g_return_val_if_fail (GTK_IS_CLIPBOARD (clipboard), FALSE);

	html = e_clipboard_wait_for_html (clipboard);
	g_return_val_if_fail (html != NULL, FALSE);

	editor = e_msg_composer_get_editor (composer);
	editor_widget = e_editor_get_editor_widget (editor);
	editor_selection = e_editor_widget_get_selection (editor_widget);
	e_editor_selection_insert_html (editor_selection, html);

	e_editor_widget_check_magic_links (editor_widget, FALSE);
	e_editor_widget_force_spell_check (editor_widget);

	e_editor_selection_scroll_to_caret (editor_selection);
	g_free (html);

	return TRUE;
}

gboolean
e_composer_paste_image (EMsgComposer *composer,
                        GtkClipboard *clipboard)
{
	EEditor *editor;
	EEditorWidget *editor_widget;
	EAttachmentStore *store;
	EAttachmentView *view;
	GdkPixbuf *pixbuf = NULL;
	gchar *filename = NULL;
	gchar *uri = NULL;
	gboolean success = FALSE;
	GError *error = NULL;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	g_return_val_if_fail (GTK_IS_CLIPBOARD (clipboard), FALSE);

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
	editor = e_msg_composer_get_editor (composer);
	editor_widget = e_editor_get_editor_widget (editor);
	if (e_editor_widget_get_html_mode (editor_widget)) {
		EEditorSelection *selection;

		selection = e_editor_widget_get_selection (editor_widget);
		e_editor_selection_insert_image (selection, uri);
		e_editor_selection_scroll_to_caret (selection);
	} else {
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
	EEditor *editor;
	EEditorWidget *editor_widget;
	EEditorSelection *editor_selection;
	gchar *text;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	g_return_val_if_fail (GTK_IS_CLIPBOARD (clipboard), FALSE);

	text = gtk_clipboard_wait_for_text (clipboard);
	g_return_val_if_fail (text != NULL, FALSE);

	editor = e_msg_composer_get_editor (composer);
	editor_widget = e_editor_get_editor_widget (editor);
	editor_selection = e_editor_widget_get_selection (editor_widget);
	/* Paste to EEditorWidget when it has focus */
	if (gtk_widget_has_focus (GTK_WIDGET (editor_widget))) {
		e_editor_selection_insert_text (editor_selection, text);

		e_editor_widget_check_magic_links (editor_widget, FALSE);
		e_editor_widget_force_spell_check (editor_widget);
		e_editor_selection_scroll_to_caret (editor_selection);
	}

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
e_composer_selection_is_base64_uris (EMsgComposer *composer,
                                     GtkSelectionData *selection)
{
	gboolean all_base64_uris = TRUE;
	gchar **uris;
	guint ii;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	g_return_val_if_fail (selection != NULL, FALSE);

	uris = gtk_selection_data_get_uris (selection);

	if (!uris)
		return FALSE;

	for (ii = 0; uris[ii] != NULL; ii++) {
		if (!((g_str_has_prefix (uris[ii], "data:") || strstr (uris[ii], ";data:"))
		    && strstr (uris[ii], ";base64,"))) {
			all_base64_uris = FALSE;
			break;
		}
	}

	g_strfreev (uris);

	return all_base64_uris;
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

	if (!uris)
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
composer_size_allocate_cb (GtkWidget *widget,
			   gpointer user_data)
{
	GtkWidget *scrolled_window;
	GtkAdjustment *adj;

	scrolled_window = gtk_widget_get_parent (GTK_WIDGET (widget));
	adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window));

	/* Scroll only when there is some size allocated */
	if (gtk_adjustment_get_upper (adj) != 0.0) {
		/* Scroll web view down to caret */
		gtk_adjustment_set_value (adj, gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj));
		gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window), adj);
		/* Disconnect because we don't want to scroll down the view on every window size change */
		g_signal_handlers_disconnect_by_func (
			widget, G_CALLBACK (composer_size_allocate_cb), NULL);
	}
}

static void
insert_paragraph_with_input (WebKitDOMElement *paragraph,
                             WebKitDOMElement *body)
{
	WebKitDOMNode *node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

	if (node) {
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (body),
			WEBKIT_DOM_NODE (paragraph),
			node,
			NULL);
	} else {
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (body),
			WEBKIT_DOM_NODE (paragraph),
			NULL);
	}
}

static void
composer_move_caret (EMsgComposer *composer)
{
	EEditor *editor;
	EEditorWidget *editor_widget;
	EEditorSelection *editor_selection;
	GSettings *settings;
	gboolean start_bottom, html_mode, top_signature;
	gboolean has_paragraphs_in_body = TRUE;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMElement *input_start, *element, *signature;
	WebKitDOMHTMLElement *body;
	WebKitDOMNodeList *list, *blockquotes;
	WebKitDOMRange *new_range;

	/* When there is an option composer-reply-start-bottom set we have
	 * to move the caret between reply and signature. */
	settings = g_settings_new ("org.gnome.evolution.mail");
	start_bottom = g_settings_get_boolean (settings, "composer-reply-start-bottom");
	g_object_unref (settings);

	top_signature =
		use_top_signature (composer) &&
		!composer->priv->is_from_message &&
		!composer->priv->is_from_new_message;

	editor = e_msg_composer_get_editor (composer);
	editor_widget = e_editor_get_editor_widget (editor);
	editor_selection = e_editor_widget_get_selection (editor_widget);
	html_mode = e_editor_widget_get_html_mode (editor_widget);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (editor_widget));
	window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (window);

	body = webkit_dom_document_get_body (document);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-message", "", NULL);
	new_range = webkit_dom_document_create_range (document);

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-caret-position");
	/* Caret position found => composer mode changed */
	if (element) {
		e_editor_selection_restore_caret_position (editor_selection);
		/* We want to force spellcheck just in case that we switched to plain
		 * text mode (when switching to html mode, the underlined words are
		 * preserved */
		if (!html_mode)
			e_editor_widget_force_spell_check (editor_widget);
		return;
	}

	/* If editing message as new don't handle with caret */
	if (composer->priv->is_from_message) {
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (body), "data-edit-as-new", "", NULL);
		e_editor_selection_restore_caret_position (editor_selection);

		e_editor_widget_force_spell_check (editor_widget);
		return;
	}

	e_editor_selection_block_selection_changed (editor_selection);

	/* When the new message is written from the beginning - note it into body */
	if (composer->priv->is_from_new_message) {
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (body), "data-new-message", "", NULL);
	}

	list = webkit_dom_document_get_elements_by_class_name (document, "-x-evo-paragraph");
	signature = webkit_dom_document_query_selector (document, ".-x-evo-signature", NULL);
	/* Situation when wrapped paragraph is just in signature and not in message body */
	if (webkit_dom_node_list_get_length (list) == 1) {
		if (signature && webkit_dom_element_query_selector (signature, ".-x-evo-paragraph", NULL))
			has_paragraphs_in_body = FALSE;
	}

	if (webkit_dom_node_list_get_length (list) == 0)
		has_paragraphs_in_body = FALSE;

	blockquotes = webkit_dom_document_get_elements_by_tag_name (document, "blockquote");

	if (!has_paragraphs_in_body) {
		element = e_editor_selection_get_paragraph_element (
			editor_selection, document, -1, 0);
		webkit_dom_element_set_id (element, "-x-evo-input-start");
		webkit_dom_html_element_set_inner_html (
			WEBKIT_DOM_HTML_ELEMENT (element), UNICODE_ZERO_WIDTH_SPACE, NULL);
		if (top_signature)
			element_add_class (element, "-x-evo-top-signature");
	}

	if (start_bottom) {
		if (webkit_dom_node_list_get_length (blockquotes) != 0) {
			if (!has_paragraphs_in_body) {
				if (!top_signature) {
					webkit_dom_node_insert_before (
						WEBKIT_DOM_NODE (body),
						WEBKIT_DOM_NODE (element),
						signature ?
							webkit_dom_node_get_parent_node (
								WEBKIT_DOM_NODE (signature)) :
							webkit_dom_node_get_next_sibling (
								webkit_dom_node_list_item (
									blockquotes, 0)),
						NULL);
				} else {
					webkit_dom_node_append_child (
						WEBKIT_DOM_NODE (body),
						WEBKIT_DOM_NODE (element),
						NULL);
				}
			}

			e_editor_selection_restore_caret_position (editor_selection);
			if (!html_mode)
				e_editor_widget_quote_plain_text (editor_widget);
			e_editor_widget_force_spell_check (editor_widget);

			input_start = webkit_dom_document_get_element_by_id (
				document, "-x-evo-input-start");
			if (input_start)
				webkit_dom_range_select_node_contents (
					new_range, WEBKIT_DOM_NODE (input_start), NULL);

			webkit_dom_range_collapse (new_range, FALSE, NULL);
		} else {
			if (!has_paragraphs_in_body)
				insert_paragraph_with_input (
					element, WEBKIT_DOM_ELEMENT (body));

			webkit_dom_range_select_node_contents (
				new_range,
				webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (body)),
				NULL);
			webkit_dom_range_collapse (new_range, TRUE, NULL);
		}

		g_signal_connect (
			editor_widget, "size-allocate",
			G_CALLBACK (composer_size_allocate_cb), NULL);
	} else {
		/* Move caret on the beginning of message */
		if (!has_paragraphs_in_body) {
			insert_paragraph_with_input (
				element, WEBKIT_DOM_ELEMENT (body));

			if (webkit_dom_node_list_get_length (blockquotes) != 0) {
				if (!html_mode) {
					WebKitDOMNode *blockquote;

					blockquote = webkit_dom_node_list_item (blockquotes, 0);

					/* FIXME determine when we can skip this */
					e_editor_selection_wrap_paragraph (
						editor_selection,
						WEBKIT_DOM_ELEMENT (blockquote));

					e_editor_selection_restore_caret_position (editor_selection);
					e_editor_widget_quote_plain_text (editor_widget);
					body = webkit_dom_document_get_body (document);
				}
			}
		}

		e_editor_widget_force_spell_check (editor_widget);

		webkit_dom_range_select_node_contents (
			new_range,
			WEBKIT_DOM_NODE (
				webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body))),
			NULL);
		webkit_dom_range_collapse (new_range, TRUE, NULL);
	}

	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, new_range);

	e_editor_selection_unblock_selection_changed (editor_selection);
}

static void
composer_load_signature_cb (EMailSignatureComboBox *combo_box,
                            GAsyncResult *result,
                            EMsgComposer *composer)
{
	GString *html_buffer = NULL;
	gchar *contents = NULL;
	gsize length = 0;
	const gchar *active_id;
	gboolean top_signature;
	gboolean is_html;
	GError *error = NULL;
	EEditor *editor;
	EEditorWidget *editor_widget;
	WebKitDOMDocument *document;
	WebKitDOMNodeList *signatures;
	gulong list_length, ii;
	GSettings *settings;
	gboolean start_bottom;

	e_mail_signature_combo_box_load_selected_finish (
		combo_box, result, &contents, &length, &is_html, &error);

	/* FIXME Use an EAlert here. */
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		goto exit;
	}

	/* "Edit as New Message" sets "priv->is_from_message".
	 * Always put the signature at the bottom for that case. */
	top_signature =
		use_top_signature (composer) &&
		!composer->priv->is_from_message &&
		!composer->priv->is_from_new_message;

	settings = g_settings_new ("org.gnome.evolution.mail");
	start_bottom = g_settings_get_boolean (settings, "composer-reply-start-bottom");
	g_object_unref (settings);

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

	g_string_append_printf (
		html_buffer,
		"<SPAN class=\"-x-evo-signature\" id=\"1\" name=\"%s\">",
		(active_id != NULL) ? active_id : "");

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

	g_string_append (html_buffer, "</SPAN>");
	g_free (contents);

insert:
	/* Remove the old signature and insert the new one. */

	editor = e_msg_composer_get_editor (composer);
	editor_widget = e_editor_get_editor_widget (editor);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (editor_widget));

	signatures = webkit_dom_document_get_elements_by_class_name (
		document, "-x-evo-signature");
	list_length = webkit_dom_node_list_get_length (signatures);
	for (ii = 0; ii < list_length; ii++) {
		WebKitDOMNode *node;
		gchar *id;

		node = webkit_dom_node_list_item (signatures, ii);
		id = webkit_dom_element_get_id (WEBKIT_DOM_ELEMENT (node));

		/* When we are editing a message with signature we need to set active
		 * signature id in signature combo box otherwise no signature will be
		 * added but we have to do it just once when the composer opens */
		if (composer->priv->is_from_message && composer->priv->set_signature_from_message) {
			gchar *name = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "name");
			gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), name);
			g_free (name);
			composer->priv->set_signature_from_message = FALSE;
		}

		if (id && (strlen (id) == 1) && (*id == '1')) {
			/* We have to remove the div containing the span with signature */
			WebKitDOMNode *next_sibling;
			WebKitDOMNode *parent;

			parent = webkit_dom_node_get_parent_node (node);
			next_sibling = webkit_dom_node_get_next_sibling (parent);

			if (WEBKIT_DOM_IS_HTMLBR_ELEMENT (next_sibling))
				webkit_dom_node_remove_child (
					webkit_dom_node_get_parent_node (next_sibling),
					next_sibling,
					NULL);

			webkit_dom_node_remove_child (
				webkit_dom_node_get_parent_node (parent),
				parent,
				NULL);

			g_free (id);
			break;
		}

		g_free (id);
	}

	if (html_buffer != NULL) {
		if (*html_buffer->str) {
			WebKitDOMElement *element;
			WebKitDOMHTMLElement *body;

			body = webkit_dom_document_get_body (document);
			element = webkit_dom_document_create_element (document, "DIV", NULL);

			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (element), html_buffer->str, NULL);

			if (top_signature) {
				WebKitDOMNode *signature_inserted;
				WebKitDOMNode *child =
					webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));
				WebKitDOMElement *br =
					webkit_dom_document_create_element (
						document, "br", NULL);

				if (start_bottom) {
					signature_inserted = webkit_dom_node_insert_before (
						WEBKIT_DOM_NODE (body),
						WEBKIT_DOM_NODE (element),
						child,
						NULL);
				} else {
					WebKitDOMElement *input_start =
						webkit_dom_document_get_element_by_id (
							document, "-x-evo-input-start");
					/* When we are using signature on top the caret
					 * should be before the signature */
					signature_inserted = webkit_dom_node_insert_before (
						WEBKIT_DOM_NODE (body),
						WEBKIT_DOM_NODE (element),
						input_start ?
							webkit_dom_node_get_next_sibling (
								WEBKIT_DOM_NODE (input_start)) :
							child,
						NULL);
				}

				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (body),
					WEBKIT_DOM_NODE (br),
					webkit_dom_node_get_next_sibling (signature_inserted),
					NULL);
			} else {
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (body),
					WEBKIT_DOM_NODE (element),
					NULL);
			}
		}

		g_string_free (html_buffer, TRUE);
	}

	composer_move_caret (composer);

exit:
	g_object_unref (composer);
}

static void
composer_web_view_load_status_changed_cb (WebKitWebView *webkit_web_view,
					  GParamSpec *pspec,
					  EMsgComposer *composer)
{
	WebKitLoadStatus status;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	status = webkit_web_view_get_load_status (webkit_web_view);

	if (status != WEBKIT_LOAD_FINISHED)
		return;

	g_signal_handlers_disconnect_by_func (
		webkit_web_view,
		G_CALLBACK (composer_web_view_load_status_changed_cb),
		NULL);

	e_composer_update_signature (composer);
}

void
e_composer_update_signature (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	EMailSignatureComboBox *combo_box;
	EEditor *editor;
	EEditorWidget *editor_widget;
	WebKitLoadStatus status;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	/* Do nothing if we're redirecting a message or we disabled the signature * on purpose */
	if (composer->priv->redirect || composer->priv->disable_signature)
		return;

	table = e_msg_composer_get_header_table (composer);
	combo_box = e_composer_header_table_get_signature_combo_box (table);
	editor = e_msg_composer_get_editor (composer);
	editor_widget = e_editor_get_editor_widget (editor);

	status = webkit_web_view_get_load_status (WEBKIT_WEB_VIEW (editor_widget));
	/* If document is not loaded, we will wait for him */
	if (status != WEBKIT_LOAD_FINISHED) {
		/* Disconnect previous handlers */
		g_signal_handlers_disconnect_by_func (
			WEBKIT_WEB_VIEW (editor_widget),
			G_CALLBACK (composer_web_view_load_status_changed_cb),
			composer);
		g_signal_connect (
			WEBKIT_WEB_VIEW(editor_widget), "notify::load-status",
			G_CALLBACK (composer_web_view_load_status_changed_cb),
			composer);
		return;
	}

	/* XXX Signature files should be local and therefore load quickly,
	 *     so while we do load them asynchronously we don't allow for
	 *     user cancellation and we keep the composer alive until the
	 *     asynchronous loading is complete. */
	e_mail_signature_combo_box_load_selected (
		combo_box, G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) composer_load_signature_cb,
		g_object_ref (composer));
}

