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
composer_update_gallery_visibility (EMsgComposer *composer)
{
	EHTMLEditor *editor;
	gboolean gallery_active;
	gboolean is_html;

	editor = e_msg_composer_get_editor (composer);
	is_html = e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_HTML;
	gallery_active = e_ui_action_get_active (ACTION (PICTURE_GALLERY));

	gtk_widget_set_visible (composer->priv->gallery_scrolled_window, is_html && gallery_active);
	gtk_widget_set_visible (composer->priv->gallery_icon_view, is_html && gallery_active);
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

static gchar *
e_composer_find_data_file (const gchar *basename)
{
	gchar *filename;

	g_return_val_if_fail (basename != NULL, NULL);

	/* Support running directly from the source tree. */
	filename = g_build_filename (".", basename, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free (filename);

	filename = g_build_filename (".", "data", "ui", basename, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free (filename);

	filename = g_build_filename ("..", "..", "..", "data", "ui", basename, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free (filename);

	filename = g_build_filename (EVOLUTION_UIDIR, basename, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		return filename;
	g_free (filename);

	g_critical ("Could not locate '%s'", basename);

	return NULL;
}

static gboolean
e_composer_ui_manager_create_item_cb (EUIManager *ui_manager,
				      EUIElement *elem,
				      EUIAction *action,
				      EUIElementKind for_kind,
				      GObject **out_item,
				      gpointer user_data)
{
	EMsgComposer *self = user_data;
	const gchar *name;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (self), FALSE);

	name = g_action_get_name (G_ACTION (action));

	if (!g_str_has_prefix (name, "EMsgComposer::"))
		return FALSE;

	#define is_action(_nm) (g_strcmp0 (name, (_nm)) == 0)

	if (for_kind == E_UI_ELEMENT_KIND_MENU) {
		if (is_action ("EMsgComposer::charset-menu"))
			*out_item = G_OBJECT (g_menu_item_new_submenu (e_ui_action_get_label (action), G_MENU_MODEL (self->priv->charset_menu)));
		else
			g_warning ("%s: Unhandled menu action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_TOOLBAR) {
		g_warning ("%s: Unhandled toolbar action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		if (is_action ("EMsgComposer::menu-button"))
			*out_item = G_OBJECT (g_object_ref (self->priv->menu_button));
		else
			g_warning ("%s: Unhandled headerbar action '%s'", G_STRFUNC, name);
	} else {
		g_warning ("%s: Unhandled element kind '%d' for action '%s'", G_STRFUNC, (gint) for_kind, name);
	}

	#undef is_action

	return TRUE;
}

static GIcon * /* (transfer full) */
e_composer_mix_icons (const gchar *action_icon_name,
		      const gchar *emblem_icon_name)
{
	GIcon *action_icon;
	GIcon *temp_icon;
	GEmblem *emblem;

	temp_icon = g_themed_icon_new (emblem_icon_name);
	emblem = g_emblem_new (temp_icon);
	g_object_unref (temp_icon);

	action_icon = g_themed_icon_new (action_icon_name);
	temp_icon = g_emblemed_icon_new (action_icon, emblem);
	g_object_unref (action_icon);

	g_object_unref (emblem);

	return temp_icon;
}

static gboolean
e_composer_ui_manager_create_gicon_cb (EUIManager *manager,
				       const gchar *name,
				       GIcon **out_gicon,
				       gpointer user_data)
{
	EMsgComposer *self = user_data;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (self), FALSE);

	if (g_strcmp0 (name, "EMsgComposer::pgp-sign") == 0) {
		/* Borrow a GnuPG icon from gcr to distinguish between GPG and S/MIME Sign/Encrypt actions */
		*out_gicon = e_composer_mix_icons ("stock_signature", "gcr-gnupg");
		return TRUE;
	} else if (g_strcmp0 (name, "EMsgComposer::pgp-encrypt") == 0) {
		/* Borrow a GnuPG icon from gcr to distinguish between GPG and S/MIME Sign/Encrypt actions */
		*out_gicon = e_composer_mix_icons ("security-high", "gcr-gnupg");
		return TRUE;
	}

	return FALSE;
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
	EUIManager *ui_manager;
	EUIAction *action;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWindow *window;
	GSettings *settings;
	GObject *ui_item;
	gchar *filename, *gallery_path;
	guint ii;
	GError *local_error = NULL;

	editor = e_msg_composer_get_editor (composer);
	ui_manager = e_html_editor_get_ui_manager (editor);
	cnt_editor = e_html_editor_get_content_editor (editor);

	g_signal_connect_object (ui_manager, "create-item",
		G_CALLBACK (e_composer_ui_manager_create_item_cb), composer, 0);
	g_signal_connect_object (ui_manager, "create-gicon",
		G_CALLBACK (e_composer_ui_manager_create_gicon_cb), composer, 0);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	shell = e_msg_composer_get_shell (composer);
	client_cache = e_shell_get_client_cache (shell);

	/* Each composer window gets its own window group. */
	window = GTK_WINDOW (composer);
	priv->window_group = gtk_window_group_new ();
	gtk_window_group_add_window (priv->window_group, window);

	priv->async_actions = e_ui_manager_get_action_group (ui_manager, "async");
	priv->composer_actions = e_ui_manager_get_action_group (ui_manager, "composer");

	priv->charset_menu = g_menu_new ();
	e_charset_add_to_g_menu (priv->charset_menu, "composer.EMsgComposer::charset-menu");

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

	filename = e_composer_find_data_file ("evolution-composer.eui");
	if (!e_ui_parser_merge_file (e_ui_manager_get_parser (ui_manager), filename, &local_error)) {
		g_critical ("%s: Failed to merge .eui data: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");
	}
	g_clear_error (&local_error);
	g_free (filename);

	action = e_ui_manager_get_action (ui_manager, "EMsgComposer::charset-menu");
	e_ui_action_set_state (action, g_variant_new_string (priv->charset));
	e_ui_action_set_usable_for_kinds (action, E_UI_ELEMENT_KIND_MENU);

	e_ui_manager_set_actions_usable_for_kinds (ui_manager, E_UI_ELEMENT_KIND_HEADERBAR,
		"EMsgComposer::menu-button",
		NULL);

	/* Configure an EFocusTracker to manage selection actions. */

	focus_tracker = e_focus_tracker_new (GTK_WINDOW (composer));

	e_html_editor_connect_focus_tracker (editor, focus_tracker);

	priv->focus_tracker = focus_tracker;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (composer), widget);
	gtk_widget_show (widget);

	container = widget;

	/* Construct the main menu and headerbar. */
	ui_item = e_html_editor_get_ui_object (editor, E_HTML_EDITOR_UI_OBJECT_MAIN_MENU);
	widget = gtk_menu_bar_new_from_model (G_MENU_MODEL (ui_item));

	priv->menu_bar = e_menu_bar_new (GTK_MENU_BAR (widget), window, &priv->menu_button);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	if (e_util_get_use_header_bar ()) {
		ui_item = e_ui_manager_create_item (ui_manager, "main-headerbar");
		widget = GTK_WIDGET (ui_item);
		gtk_window_set_titlebar (window, widget);

		e_ui_customizer_register (e_ui_manager_get_customizer (ui_manager), "main-headerbar", NULL);
	}

	ui_item = e_html_editor_get_ui_object (editor, E_HTML_EDITOR_UI_OBJECT_MAIN_TOOLBAR);
	widget = GTK_WIDGET (ui_item);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	e_binding_bind_property (
		ACTION (TOOLBAR_SHOW_MAIN), "active",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

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
		widget, "editable",
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

	widget = e_html_editor_get_content_box (editor);
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
		editor, "notify::mode",
		G_CALLBACK (composer_update_gallery_visibility), composer);

	g_signal_connect_swapped (
		ACTION (PICTURE_GALLERY), "notify::active",
		G_CALLBACK (composer_update_gallery_visibility), composer);

	/* Initial sync */
	composer_update_gallery_visibility (composer);

	/* Bind headers to their corresponding actions. */

	for (ii = 0; ii < E_COMPOSER_NUM_HEADERS; ii++) {
		EComposerHeaderTable *table;

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

			case E_COMPOSER_HEADER_MAIL_FOLLOWUP_TO:
				action = ACTION (VIEW_MAIL_FOLLOWUP_TO);
				e_widget_undo_attach (
					GTK_WIDGET (header->input_widget),
					focus_tracker);
				break;

			case E_COMPOSER_HEADER_MAIL_REPLY_TO:
				action = ACTION (VIEW_MAIL_REPLY_TO);
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

	e_binding_bind_property (
		E_HTML_EDITOR_ACTION (editor, "paragraph-style-menu"), "visible",
		ACTION (TOOLBAR_SHOW_EDIT), "sensitive",
		G_BINDING_SYNC_CREATE);

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

	g_clear_object (&composer->priv->editor);
	g_clear_object (&composer->priv->header_table);
	g_clear_object (&composer->priv->attachment_paned);
	g_clear_object (&composer->priv->focus_tracker);
	g_clear_object (&composer->priv->window_group);
	g_clear_object (&composer->priv->charset_menu);
	g_clear_object (&composer->priv->gallery_scrolled_window);
	g_clear_object (&composer->priv->redirect);
	g_clear_object (&composer->priv->menu_bar);

	composer->priv->async_actions = NULL;
	composer->priv->composer_actions = NULL;
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

	g_clear_object (&composer->priv->load_signature_cancellable);

	g_free (composer->priv->charset);
	g_free (composer->priv->mime_type);
	g_free (composer->priv->mime_body);
	g_free (composer->priv->previous_identity_uid);

	g_clear_pointer (&composer->priv->content_hash, e_content_editor_util_free_content_hash);
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

static gboolean
e_composer_selection_uri_is_image (const gchar *uri)
{
	GFile *file;
	GFileInfo *file_info;
	GdkPixbufLoader *loader;
	const gchar *attribute;
	const gchar *content_type;
	gchar *mime_type = NULL;

	file = g_file_new_for_uri (uri);
	attribute = G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE;

	/* XXX This blocks, but we're requesting the fast content
	 *     type (which only inspects filenames), so hopefully
	 *     it won't be noticeable.  Also, this is best effort
	 *     so we don't really care if it fails. */
	file_info = g_file_query_info (file, attribute, G_FILE_QUERY_INFO_NONE, NULL, NULL);

	if (file_info == NULL) {
		g_object_unref (file);
		return FALSE;
	}

	content_type = g_file_info_get_attribute_string (file_info, attribute);
	mime_type = g_content_type_get_mime_type (content_type);

	g_object_unref (file_info);
	g_object_unref (file);

	if (mime_type == NULL)
		return FALSE;

	/* Easy way to determine if a MIME type is a supported
	 * image format: try creating a GdkPixbufLoader for it. */
	loader = gdk_pixbuf_loader_new_with_mime_type (mime_type, NULL);

	g_free (mime_type);

	if (loader == NULL)
		return FALSE;

	gdk_pixbuf_loader_close (loader, NULL);
	g_object_unref (loader);

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

	if (!uris)
		return FALSE;

	for (ii = 0; uris[ii] != NULL; ii++) {
		all_image_uris = e_composer_selection_uri_is_image (uris[ii]);
		if (!all_image_uris)
			break;
	}

	g_strfreev (uris);

	return all_image_uris;
}

gboolean
e_composer_selection_is_moz_url_image (EMsgComposer *composer,
				       GtkSelectionData *selection,
				       gchar **out_moz_url)
{
	static GdkAtom moz_url_atom = GDK_NONE;
	const guchar *raw_data;
	gchar *utf8 = NULL;
	gint len;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);
	g_return_val_if_fail (selection != NULL, FALSE);

	if (moz_url_atom == GDK_NONE)
		moz_url_atom = gdk_atom_intern_static_string ("text/x-moz-url");

	if (gtk_selection_data_get_data_type (selection) != moz_url_atom)
		return FALSE;

	raw_data = gtk_selection_data_get_data_with_length (selection, &len);
	if (raw_data)
		utf8 = g_utf16_to_utf8 ((const gunichar2 *) raw_data, len, NULL, NULL, NULL);

	if (utf8) {
		gchar *ptr;

		ptr = strchr (utf8, '\n');
		if (ptr)
			*ptr = '\0';
	}

	if (!utf8 || !e_composer_selection_uri_is_image ((const gchar *) utf8)) {
		g_free (utf8);
		return FALSE;
	}

	if (out_moz_url)
		*out_moz_url = utf8;
	else
		g_free (utf8);

	return TRUE;
}

typedef struct _UpdateSignatureData {
	EMsgComposer *composer;
	gboolean can_reposition_caret;
} UpdateSignatureData;

static void
update_signature_data_free (gpointer ptr)
{
	UpdateSignatureData *usd = ptr;

	if (usd) {
		g_clear_object (&usd->composer);
		g_slice_free (UpdateSignatureData, usd);
	}
}

static void
composer_load_signature_cb (EMailSignatureComboBox *combo_box,
			    GAsyncResult *result,
			    gpointer user_data)
{
	UpdateSignatureData *usd = user_data;
	EMsgComposer *composer = usd->composer;
	gchar *contents = NULL, *new_signature_id;
	gsize length = 0;
	EContentEditorMode editor_mode = E_CONTENT_EDITOR_MODE_UNKNOWN;
	GError *error = NULL;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	e_mail_signature_combo_box_load_selected_finish (
		combo_box, result, &contents, &length, &editor_mode, &error);

	/* FIXME Use an EAlert here. */
	if (error != NULL) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		update_signature_data_free (usd);
		return;
	}

	g_clear_object (&composer->priv->load_signature_cancellable);

	if (composer->priv->ignore_next_signature_change) {
		composer->priv->ignore_next_signature_change = FALSE;
		update_signature_data_free (usd);
		return;
	}

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	new_signature_id = e_content_editor_insert_signature (
		cnt_editor,
		contents,
		editor_mode,
		usd->can_reposition_caret,
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
	update_signature_data_free (usd);
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
	UpdateSignatureData *usd;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (composer->priv->load_signature_cancellable) {
		g_cancellable_cancel (composer->priv->load_signature_cancellable);
		g_clear_object (&composer->priv->load_signature_cancellable);
	}

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

	composer->priv->load_signature_cancellable = g_cancellable_new ();

	usd = g_slice_new (UpdateSignatureData);
	usd->composer = g_object_ref (composer);
	usd->can_reposition_caret = e_msg_composer_get_is_reply_or_forward (composer) &&
		!gtk_widget_get_realized (GTK_WIDGET (composer));

	/* XXX Signature files should be local and therefore load quickly,
	 *     so while we do load them asynchronously we don't allow for
	 *     user cancellation and we keep the composer alive until the
	 *     asynchronous loading is complete. */
	e_mail_signature_combo_box_load_selected (
		combo_box, G_PRIORITY_DEFAULT, composer->priv->load_signature_cancellable,
		(GAsyncReadyCallback) composer_load_signature_cb,
		usd);
}
