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

#include "evolution-config.h"

#include "e-composer-private.h"
#include "e-composer-from-header.h"
#include "e-composer-spell-header.h"
#include "e-util/e-util-private.h"

/* Initial height of the picture gallery. */
#define GALLERY_INITIAL_HEIGHT 150

static void
composer_setup_charset_menu (EMsgComposer *composer)
{
	EHTMLEditor *editor;
	GtkUIManager *ui_manager;
	const gchar *path;
	GList *list;
	guint merge_id;

	editor = e_msg_composer_get_editor (composer);
	ui_manager = e_html_editor_get_ui_manager (editor);
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
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GtkToggleAction *toggle_action;
	gboolean gallery_active;
	gboolean is_html;

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	is_html = e_content_editor_get_html_mode (cnt_editor);

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

static gchar *
e_composer_extract_lang_from_source (ESourceRegistry *registry,
				     const gchar *uid)
{
	ESource *source;
	gchar *lang = NULL;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	source = e_source_registry_ref_source (registry, uid);
	if (source && e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION)) {
		ESourceMailComposition *mail_composition;

		mail_composition = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION);
		lang = e_source_mail_composition_dup_language (mail_composition);

		if (lang && !*lang) {
			g_free (lang);
			lang = NULL;
		}
	}

	g_clear_object (&source);

	return lang;
}

static void
e_composer_from_changed_cb (EComposerFromHeader *header,
			    EMsgComposer *composer)
{
	gchar *current_uid;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	current_uid = e_composer_from_header_dup_active_id (header, NULL, NULL);

	if (current_uid && g_strcmp0 (composer->priv->previous_identity_uid, current_uid) != 0) {
		gchar *previous_lang = NULL, *current_lang = NULL;
		ESourceRegistry *registry;

		registry = e_composer_header_get_registry (E_COMPOSER_HEADER (header));

		if (composer->priv->previous_identity_uid)
			previous_lang = e_composer_extract_lang_from_source (registry, composer->priv->previous_identity_uid);

		current_lang = e_composer_extract_lang_from_source (registry, current_uid);

		if (g_strcmp0 (previous_lang, current_lang) != 0) {
			GSettings *settings;
			gchar **strv;
			gboolean have_previous, have_current;
			gint ii;

			settings = e_util_ref_settings ("org.gnome.evolution.mail");
			strv = g_settings_get_strv (settings, "composer-spell-languages");
			g_object_unref (settings);

			have_previous = !previous_lang;
			have_current = !current_lang;

			for (ii = 0; strv && strv[ii] && (!have_previous || !have_current); ii++) {
				have_previous = have_previous || g_strcmp0 (previous_lang, strv[ii]) == 0;
				have_current = have_current || g_strcmp0 (current_lang, strv[ii]) == 0;
			}

			g_strfreev (strv);

			if (!have_previous || !have_current) {
				ESpellChecker *spell_checker;
				EHTMLEditor *editor;

				editor = e_msg_composer_get_editor (composer);
				spell_checker = e_content_editor_ref_spell_checker (e_html_editor_get_content_editor (editor));

				if (!have_previous)
					e_spell_checker_set_language_active (spell_checker, previous_lang, FALSE);

				if (!have_current)
					e_spell_checker_set_language_active (spell_checker, current_lang, TRUE);

				g_clear_object (&spell_checker);

				e_html_editor_update_spell_actions (editor);
				g_signal_emit_by_name (editor, "spell-languages-changed");
			}
		}

		g_free (previous_lang);
		g_free (current_lang);

		g_free (composer->priv->previous_identity_uid);
		composer->priv->previous_identity_uid = current_uid;
	} else {
		g_free (current_uid);
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
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
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
	ui_manager = e_html_editor_get_ui_manager (editor);
	cnt_editor = e_html_editor_get_content_editor (editor);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

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

	priv->set_signature_from_message = FALSE;
	priv->disable_signature = FALSE;
	priv->soft_busy_count = 0;
	priv->had_activities = FALSE;
	priv->saved_editable = FALSE;
	priv->dnd_history_saved = FALSE;
	priv->check_if_signature_is_changed = FALSE;
	priv->ignore_next_signature_change = FALSE;

	priv->focused_entry = NULL;

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

	action = e_html_editor_get_action (editor, "cut");
	e_focus_tracker_set_cut_clipboard_action (focus_tracker, action);

	action = e_html_editor_get_action (editor, "copy");
	e_focus_tracker_set_copy_clipboard_action (focus_tracker, action);

	action = e_html_editor_get_action (editor, "paste");
	e_focus_tracker_set_paste_clipboard_action (focus_tracker, action);

	action = e_html_editor_get_action (editor, "select-all");
	e_focus_tracker_set_select_all_action (focus_tracker, action);

	action = e_html_editor_get_action (editor, "undo");
	e_focus_tracker_set_undo_action (focus_tracker, action);

	action = e_html_editor_get_action (editor, "redo");
	e_focus_tracker_set_redo_action (focus_tracker, action);

	priv->focus_tracker = focus_tracker;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (composer), widget);
	gtk_widget_show (widget);

	container = widget;

	/* Construct the main menu and toolbar. */

	widget = e_html_editor_get_managed_widget (editor, "/main-menu");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = e_html_editor_get_managed_widget (editor, "/main-toolbar");
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
	e_binding_bind_property (
		cnt_editor, "spell-checker",
		header->input_widget, "spell-checker",
		G_BINDING_SYNC_CREATE);

	/* Construct the editing toolbars.  We'll have to reparent
	 * the embedded EHTMLEditorView a little further down. */

	widget = GTK_WIDGET (editor);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/* Construct the attachment paned. */

	widget = e_attachment_paned_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->attachment_paned = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		cnt_editor, "editable",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	container = e_attachment_paned_get_content_area (
		E_ATTACHMENT_PANED (priv->attachment_paned));

	widget = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_paned_set_wide_handle (GTK_PANED (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (widget, -1, GALLERY_INITIAL_HEIGHT);
	gtk_paned_pack1 (GTK_PANED (container), widget, FALSE, FALSE);
	priv->gallery_scrolled_window = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = GTK_WIDGET (cnt_editor);
	if (GTK_IS_SCROLLABLE (cnt_editor)) {
		/* Scrollables are packed in a scrolled window */
		widget = gtk_widget_get_parent (widget);
		g_warn_if_fail (GTK_IS_SCROLLED_WINDOW (widget));
	}
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

	e_signal_connect_notify_swapped (
		cnt_editor, "notify::html-mode",
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

		table = E_COMPOSER_HEADER_TABLE (priv->header_table);
		header = e_composer_header_table_get_header (table, ii);

		switch (ii) {
			case E_COMPOSER_HEADER_FROM:
				e_widget_undo_attach (
					GTK_WIDGET (e_composer_from_header_get_name_entry (E_COMPOSER_FROM_HEADER (header))),
					focus_tracker);
				e_widget_undo_attach (
					GTK_WIDGET (e_composer_from_header_get_address_entry (E_COMPOSER_FROM_HEADER (header))),
					focus_tracker);

				action = ACTION (VIEW_FROM_OVERRIDE);
				e_binding_bind_property (
					header, "override-visible",
					action, "active",
					G_BINDING_BIDIRECTIONAL |
					G_BINDING_SYNC_CREATE);

				g_signal_connect (header, "changed",
					G_CALLBACK (e_composer_from_changed_cb), composer);
				continue;

			case E_COMPOSER_HEADER_BCC:
				action = ACTION (VIEW_BCC);
				break;

			case E_COMPOSER_HEADER_CC:
				action = ACTION (VIEW_CC);
				break;

			case E_COMPOSER_HEADER_REPLY_TO:
				action = ACTION (VIEW_REPLY_TO);
				e_widget_undo_attach (
					GTK_WIDGET (header->input_widget),
					focus_tracker);
				break;

			case E_COMPOSER_HEADER_SUBJECT:
				e_widget_undo_attach (
					GTK_WIDGET (header->input_widget),
					focus_tracker);
				continue;

			default:
				continue;
		}

		e_binding_bind_property (
			header, "sensitive",
			action, "sensitive",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);

		e_binding_bind_property (
			header, "visible",
			action, "active",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);
	}

	g_settings_bind (
		settings, "composer-visually-wrap-long-lines",
		cnt_editor, "visually-wrap-long-lines",
		G_SETTINGS_BIND_DEFAULT);


	/* Disable actions that start asynchronous activities while an
	 * asynchronous activity is in progress. We enforce this with
	 * a simple inverted binding to EMsgComposer's "busy" property. */

	e_binding_bind_property (
		composer, "soft-busy",
		priv->async_actions, "sensitive",
		G_BINDING_SYNC_CREATE |
		G_BINDING_INVERT_BOOLEAN);

	e_binding_bind_property (
		composer, "busy",
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
	g_free (composer->priv->previous_identity_uid);

	g_clear_pointer (&composer->priv->content_hash, e_content_editor_util_free_content_hash);
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

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	charset = g_settings_get_string (settings, "composer-charset");

	if (!charset || !*charset) {
		g_free (charset);
		charset = NULL;
	}

	g_object_unref (settings);

	if (!charset)
		charset = g_strdup ("UTF-8");

	return charset;
}

gboolean
e_composer_paste_image (EMsgComposer *composer,
                        GtkClipboard *clipboard)
{
	EAttachment *attachment;
	EAttachmentStore *store;
	EAttachmentView *view;
	gchar *uri;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	g_return_val_if_fail (GTK_IS_CLIPBOARD (clipboard), FALSE);

	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	if (!(uri = e_util_save_image_from_clipboard (clipboard)))
		return FALSE;

	attachment = e_attachment_new_for_uri (uri);
	e_attachment_store_add_attachment (store, attachment);
	e_attachment_load_async (
		attachment, (GAsyncReadyCallback)
		e_attachment_load_handle_error, composer);
	g_object_unref (attachment);


	g_free (uri);

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

static void
composer_load_signature_cb (EMailSignatureComboBox *combo_box,
                            GAsyncResult *result,
                            EMsgComposer *composer)
{
	gchar *contents = NULL, *new_signature_id;
	gsize length = 0;
	gboolean is_html;
	GError *error = NULL;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	e_mail_signature_combo_box_load_selected_finish (
		combo_box, result, &contents, &length, &is_html, &error);

	/* FIXME Use an EAlert here. */
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		g_object_unref (composer);
		return;
	}

	if (composer->priv->ignore_next_signature_change) {
		composer->priv->ignore_next_signature_change = FALSE;
		g_object_unref (composer);
		return;
	}

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	new_signature_id = e_content_editor_insert_signature (
		cnt_editor,
		contents,
		is_html,
		gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box)),
		&composer->priv->set_signature_from_message,
		&composer->priv->check_if_signature_is_changed,
		&composer->priv->ignore_next_signature_change);

	if (new_signature_id && *new_signature_id) {
		gboolean been_ignore = composer->priv->ignore_next_signature_change;
		gboolean signature_changed = g_strcmp0 (gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box)), new_signature_id) != 0;

		composer->priv->ignore_next_signature_change = been_ignore && signature_changed;

		if (!gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), new_signature_id)) {
			signature_changed = g_strcmp0 (gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box)), "none") != 0;

			composer->priv->ignore_next_signature_change = been_ignore && signature_changed;

			gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), "none");
		}

		if (!signature_changed && composer->priv->check_if_signature_is_changed) {
			composer->priv->set_signature_from_message = FALSE;
			composer->priv->check_if_signature_is_changed = FALSE;
			composer->priv->ignore_next_signature_change = FALSE;
		}
	}

	g_free (new_signature_id);
	g_free (contents);
	g_object_unref (composer);
}

static void
content_editor_load_finished_cb (EContentEditor *cnt_editor,
                                 EMsgComposer *composer)
{
	g_signal_handlers_disconnect_by_func (
		cnt_editor, G_CALLBACK (content_editor_load_finished_cb), composer);

	e_composer_update_signature (composer);
}

void
e_composer_update_signature (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	EMailSignatureComboBox *combo_box;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	/* Do nothing if we're redirecting a message or we disabled
	 * the signature on purpose */
	if (composer->priv->redirect || composer->priv->disable_signature)
		return;

	table = e_msg_composer_get_header_table (composer);
	combo_box = e_composer_header_table_get_signature_combo_box (table);
	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	if (!e_content_editor_is_ready (cnt_editor)) {
		g_signal_connect (
			cnt_editor, "load-finished",
			G_CALLBACK (content_editor_load_finished_cb),
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

