/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <webkit2/webkit-web-extension.h>

#include "web-extensions/e-dom-utils.h"

#include "e-editor-dom-functions.h"
#include "e-editor-web-extension.h"
#include "e-editor-undo-redo-manager.h"

#include "e-editor-page.h"

struct _EEditorPagePrivate {
	WebKitWebPage *web_page; /* not referenced */
	EEditorWebExtension *web_extension; /* not referenced */

	EEditorUndoRedoManager *undo_redo_manager;
	ESpellChecker *spell_checker;

	guint spell_check_on_scroll_event_source_id;

	EContentEditorAlignment alignment;
	EContentEditorBlockFormat block_format;
	guint32 style_flags; /* bit-OR of EContentEditorStyleFlags */
	gchar *background_color;
	gchar *font_color;
	gchar *font_name;
	gint font_size;

	guint selection_changed_blocked;
	gboolean selection_changed;

	gboolean force_image_load;
	gboolean html_mode;
	gboolean return_key_pressed;
	gboolean space_key_pressed;
	gboolean smiley_written;
	gint word_wrap_length;

	gboolean convert_in_situ;
	gboolean body_input_event_removed;
	gboolean dont_save_history_in_body_input;
	gboolean composition_in_progress;
	gboolean pasting_content_from_itself;
	gboolean renew_history_after_coordinates;

	GHashTable *inline_images;

	WebKitDOMNode *node_under_mouse_click;

	GSettings *mail_settings;
};

G_DEFINE_TYPE (EEditorPage, e_editor_page, G_TYPE_OBJECT)


static void
web_page_document_loaded_cb (WebKitWebPage *web_page,
			     EEditorPage *editor_page)
{
	g_return_if_fail (WEBKIT_IS_WEB_PAGE (web_page));
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->body_input_event_removed = TRUE;

	e_editor_undo_redo_manager_clean_history (editor_page->priv->undo_redo_manager);
	e_editor_dom_process_content_after_load (editor_page);
}

static gboolean
web_page_context_menu_cb (WebKitWebPage *web_page,
			  WebKitContextMenu *context_menu,
			  WebKitWebHitTestResult *hit_test_result,
			  EEditorPage *editor_page)
{
	WebKitDOMNode *node;
	EContentEditorNodeFlags flags = 0;
	GVariant *variant;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	node = webkit_web_hit_test_result_get_node (hit_test_result);
	editor_page->priv->node_under_mouse_click = node;

	if (WEBKIT_DOM_IS_HTML_HR_ELEMENT (node))
		flags |= E_CONTENT_EDITOR_NODE_IS_H_RULE;

	if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node) ||
	    (dom_node_find_parent_element (node, "A") != NULL))
		flags |= E_CONTENT_EDITOR_NODE_IS_ANCHOR;

	if (WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (node) ||
	    (dom_node_find_parent_element (node, "IMG") != NULL))
		flags |= E_CONTENT_EDITOR_NODE_IS_IMAGE;

	if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (node) ||
	    (dom_node_find_parent_element (node, "TD") != NULL) ||
	    (dom_node_find_parent_element (node, "TH") != NULL))
		flags |= E_CONTENT_EDITOR_NODE_IS_TABLE_CELL;

	if (flags & E_CONTENT_EDITOR_NODE_IS_TABLE_CELL &&
	    (WEBKIT_DOM_IS_HTML_TABLE_ELEMENT (node) ||
	    dom_node_find_parent_element (node, "TABLE") != NULL))
		flags |= E_CONTENT_EDITOR_NODE_IS_TABLE;

	if (flags == 0)
		flags |= E_CONTENT_EDITOR_NODE_IS_TEXT;

	variant = g_variant_new_int32 (flags);
	webkit_context_menu_set_user_data (context_menu, variant);

	return FALSE;
}

static void
e_editor_page_setup (EEditorPage *editor_page,
		     WebKitWebPage *web_page,
		     struct _EEditorWebExtension *web_extension)
{
	WebKitWebEditor *web_editor;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->web_page = web_page;
	editor_page->priv->web_extension = web_extension;
	editor_page->priv->undo_redo_manager = e_editor_undo_redo_manager_new (editor_page);

	g_signal_connect_swapped (
		editor_page->priv->undo_redo_manager, "notify::can-undo",
		G_CALLBACK (e_editor_page_emit_undo_redo_state_changed), editor_page);

	g_signal_connect_swapped (
		editor_page->priv->undo_redo_manager, "notify::can-redo",
		G_CALLBACK (e_editor_page_emit_undo_redo_state_changed), editor_page);

	web_editor = webkit_web_page_get_editor (web_page);

	g_signal_connect_swapped (
		web_editor, "selection-changed",
		G_CALLBACK (e_editor_page_emit_selection_changed), editor_page);

	g_signal_connect (
		web_page, "document-loaded",
		G_CALLBACK (web_page_document_loaded_cb), editor_page);

	g_signal_connect (
		web_page, "context-menu",
		G_CALLBACK (web_page_context_menu_cb), editor_page);
}

static void
e_editor_page_dispose (GObject *object)
{
	EEditorPage *editor_page = E_EDITOR_PAGE (object);

	if (editor_page->priv->spell_check_on_scroll_event_source_id > 0) {
		g_source_remove (editor_page->priv->spell_check_on_scroll_event_source_id);
		editor_page->priv->spell_check_on_scroll_event_source_id = 0;
	}

	if (editor_page->priv->background_color != NULL) {
		g_free (editor_page->priv->background_color);
		editor_page->priv->background_color = NULL;
	}

	if (editor_page->priv->font_color != NULL) {
		g_free (editor_page->priv->font_color);
		editor_page->priv->font_color = NULL;
	}

	if (editor_page->priv->font_name != NULL) {
		g_free (editor_page->priv->font_name);
		editor_page->priv->font_name = NULL;
	}

	if (editor_page->priv->mail_settings != NULL) {
		g_signal_handlers_disconnect_by_data (editor_page->priv->mail_settings, object);
		g_object_unref (editor_page->priv->mail_settings);
		editor_page->priv->mail_settings = NULL;
	}

	g_clear_object (&editor_page->priv->undo_redo_manager);
	g_clear_object (&editor_page->priv->spell_checker);

	g_hash_table_remove_all (editor_page->priv->inline_images);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_editor_page_parent_class)->dispose (object);
}

static void
e_editor_page_finalize (GObject *object)
{
	EEditorPage *editor_page = E_EDITOR_PAGE (object);

	g_hash_table_destroy (editor_page->priv->inline_images);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_editor_page_parent_class)->finalize (object);
}

static void
e_editor_page_class_init (EEditorPageClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EEditorPagePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = e_editor_page_dispose;
	object_class->finalize = e_editor_page_finalize;
}

static void
e_editor_page_init (EEditorPage *editor_page)
{
	editor_page->priv = G_TYPE_INSTANCE_GET_PRIVATE (editor_page, E_TYPE_EDITOR_PAGE, EEditorPagePrivate);
	editor_page->priv->style_flags = 0;
	editor_page->priv->selection_changed_blocked = 0;
	editor_page->priv->background_color = g_strdup ("");
	editor_page->priv->font_color = g_strdup ("");
	editor_page->priv->font_name = g_strdup ("");
	editor_page->priv->font_size = E_CONTENT_EDITOR_FONT_SIZE_NORMAL;
	editor_page->priv->alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;
	editor_page->priv->block_format = E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH;
	editor_page->priv->force_image_load = FALSE;
	editor_page->priv->html_mode = TRUE;
	editor_page->priv->return_key_pressed = FALSE;
	editor_page->priv->space_key_pressed = FALSE;
	editor_page->priv->smiley_written = FALSE;
	editor_page->priv->convert_in_situ = FALSE;
	editor_page->priv->body_input_event_removed = TRUE;
	editor_page->priv->dont_save_history_in_body_input = FALSE;
	editor_page->priv->pasting_content_from_itself = FALSE;
	editor_page->priv->composition_in_progress = FALSE;
	editor_page->priv->renew_history_after_coordinates = TRUE;
	editor_page->priv->spell_check_on_scroll_event_source_id = 0;
	editor_page->priv->mail_settings = e_util_ref_settings ("org.gnome.evolution.mail");
	editor_page->priv->word_wrap_length = g_settings_get_int (editor_page->priv->mail_settings, "composer-word-wrap-length");
	editor_page->priv->inline_images = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	editor_page->priv->spell_checker = e_spell_checker_new ();
}

EEditorPage *
e_editor_page_new (WebKitWebPage *web_page,
		   struct _EEditorWebExtension *web_extension)
{
	EEditorPage *editor_page;

	g_return_val_if_fail (WEBKIT_IS_WEB_PAGE (web_page), NULL);
	g_return_val_if_fail (E_IS_EDITOR_WEB_EXTENSION (web_extension), NULL);

	editor_page = g_object_new (E_TYPE_EDITOR_PAGE, NULL);
	e_editor_page_setup (editor_page, web_page, web_extension);

	return editor_page;
}

WebKitWebPage *
e_editor_page_get_web_page (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	return editor_page->priv->web_page;
}

struct _EEditorWebExtension *
e_editor_page_get_web_extension (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	return editor_page->priv->web_extension;
}

guint64
e_editor_page_get_page_id (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), 0);

	if (!editor_page->priv->web_page)
		return 0;

	return webkit_web_page_get_id (editor_page->priv->web_page);
}

WebKitDOMDocument *
e_editor_page_get_document (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	if (!editor_page->priv->web_page)
		return NULL;

	return webkit_web_page_get_dom_document (editor_page->priv->web_page);
}

struct _EEditorUndoRedoManager *
e_editor_page_get_undo_redo_manager (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	return editor_page->priv->undo_redo_manager;
}

void
e_editor_page_block_selection_changed (EEditorPage *editor_page)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->selection_changed_blocked++;
}

void
e_editor_page_unblock_selection_changed (EEditorPage *editor_page)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));
	g_return_if_fail (editor_page->priv->selection_changed_blocked > 0);

	editor_page->priv->selection_changed_blocked--;

	if (!editor_page->priv->selection_changed_blocked &&
	    editor_page->priv->selection_changed) {
		editor_page->priv->selection_changed = FALSE;
		e_editor_page_emit_selection_changed (editor_page);
	}
}

gboolean
e_editor_page_get_html_mode (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return editor_page->priv->html_mode;
}

void
e_editor_page_set_html_mode (EEditorPage *editor_page,
			     gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->html_mode = value;
}

gboolean
e_editor_page_get_force_image_load (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return editor_page->priv->force_image_load;
}

void
e_editor_page_set_force_image_load (EEditorPage *editor_page,
				    gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->force_image_load = value;
}

gint
e_editor_page_get_word_wrap_length (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), 0);

	return editor_page->priv->word_wrap_length;
}

static gboolean
e_editor_page_check_style_flag (EEditorPage *editor_page,
				EContentEditorStyleFlags flag)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return (editor_page->priv->style_flags & flag) != 0;
}

static gboolean
e_editor_page_set_style_flag (EEditorPage *editor_page,
			      EContentEditorStyleFlags flag,
			      gboolean value)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	if ((((editor_page->priv->style_flags & flag) != 0) ? 1 : 0) == (value ? 1 : 0))
		return FALSE;

	editor_page->priv->style_flags = (editor_page->priv->style_flags & ~flag) | (value ? flag : 0);

	return TRUE;
}

gboolean
e_editor_page_get_bold (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return e_editor_page_check_style_flag (editor_page, E_CONTENT_EDITOR_STYLE_IS_BOLD);
}

void
e_editor_page_set_bold (EEditorPage *editor_page,
			gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_page_get_bold (editor_page) != value) {
		e_editor_dom_selection_set_bold (editor_page, value);
	        e_editor_page_set_style_flag (editor_page, E_CONTENT_EDITOR_STYLE_IS_BOLD, value);
	}
}

gboolean
e_editor_page_get_italic (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return e_editor_page_check_style_flag (editor_page, E_CONTENT_EDITOR_STYLE_IS_ITALIC);
}

void
e_editor_page_set_italic (EEditorPage *editor_page,
			  gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_page_get_italic (editor_page) != value) {
		e_editor_dom_selection_set_italic (editor_page, value);
		e_editor_page_set_style_flag (editor_page, E_CONTENT_EDITOR_STYLE_IS_ITALIC, value);
	}
}

gboolean
e_editor_page_get_underline (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return e_editor_page_check_style_flag (editor_page, E_CONTENT_EDITOR_STYLE_IS_UNDERLINE);
}

void
e_editor_page_set_underline (EEditorPage *editor_page,
			     gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_page_get_underline (editor_page) != value) {
		e_editor_dom_selection_set_underline (editor_page, value);
		e_editor_page_set_style_flag (editor_page, E_CONTENT_EDITOR_STYLE_IS_UNDERLINE, value);
	}
}

gboolean
e_editor_page_get_monospace (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return e_editor_page_check_style_flag (editor_page, E_CONTENT_EDITOR_STYLE_IS_MONOSPACE);
}

void
e_editor_page_set_monospace (EEditorPage *editor_page,
			     gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_page_get_monospace (editor_page) != value) {
		e_editor_dom_selection_set_monospace (editor_page, value);
		e_editor_page_set_style_flag (editor_page, E_CONTENT_EDITOR_STYLE_IS_MONOSPACE, value);
	}
}

gboolean
e_editor_page_get_strikethrough (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return e_editor_page_check_style_flag (editor_page, E_CONTENT_EDITOR_STYLE_IS_STRIKETHROUGH);
}

void
e_editor_page_set_strikethrough (EEditorPage *editor_page,
				 gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_page_get_strikethrough (editor_page) != value) {
		e_editor_dom_selection_set_strikethrough (editor_page, value);
		e_editor_page_set_style_flag (editor_page, E_CONTENT_EDITOR_STYLE_IS_STRIKETHROUGH, value);
	}
}

guint
e_editor_page_get_font_size (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), 0);

	return editor_page->priv->font_size;
}

void
e_editor_page_set_font_size (EEditorPage *editor_page,
			     guint value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (editor_page->priv->font_size == value)
		return;

	editor_page->priv->font_size = value;
}

const gchar *
e_editor_page_get_font_color (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	return editor_page->priv->font_color;
}

EContentEditorAlignment
e_editor_page_get_alignment (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), E_CONTENT_EDITOR_ALIGNMENT_LEFT);

	return editor_page->priv->alignment;
}

void
e_editor_page_set_alignment (EEditorPage *editor_page,
			     EContentEditorAlignment value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->alignment = value;
}

gboolean
e_editor_page_get_return_key_pressed (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return editor_page->priv->return_key_pressed;
}

void
e_editor_page_set_return_key_pressed (EEditorPage *editor_page,
				      gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->return_key_pressed = value;
}

gboolean
e_editor_page_get_space_key_pressed (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return editor_page->priv->space_key_pressed;
}

void
e_editor_page_set_space_key_pressed (EEditorPage *editor_page,
				     gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->space_key_pressed = value;
}

gboolean
e_editor_page_get_magic_links_enabled (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return g_settings_get_boolean (editor_page->priv->mail_settings, "composer-magic-links");
}

gboolean
e_editor_page_get_magic_smileys_enabled (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return g_settings_get_boolean (editor_page->priv->mail_settings, "composer-magic-smileys");
}

gboolean
e_editor_page_get_unicode_smileys_enabled (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return g_settings_get_boolean (editor_page->priv->mail_settings, "composer-unicode-smileys");
}

EImageLoadingPolicy
e_editor_page_get_image_loading_policy (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), E_IMAGE_LOADING_POLICY_NEVER);

	return g_settings_get_enum (editor_page->priv->mail_settings, "image-loading-policy");
}

gboolean
e_editor_page_get_inline_spelling_enabled (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return g_settings_get_boolean (editor_page->priv->mail_settings, "composer-inline-spelling");
}

gboolean
e_editor_page_check_word_spelling (EEditorPage *editor_page,
				   const gchar *word,
				   const gchar * const *languages)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), TRUE);

	if (!word || !languages || !*languages)
		return TRUE;

	e_spell_checker_set_active_languages (editor_page->priv->spell_checker, languages);

	return e_spell_checker_check_word (editor_page->priv->spell_checker, word, -1);
}

gboolean
e_editor_page_get_body_input_event_removed (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return editor_page->priv->body_input_event_removed;
}

void
e_editor_page_set_body_input_event_removed (EEditorPage *editor_page,
					    gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->body_input_event_removed = value;
}

gboolean
e_editor_page_get_convert_in_situ (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return editor_page->priv->convert_in_situ;
}

void
e_editor_page_set_convert_in_situ (EEditorPage *editor_page,
				   gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->convert_in_situ = value;
}

GHashTable *
e_editor_page_get_inline_images (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	return editor_page->priv->inline_images;
}

void
e_editor_page_add_new_inline_image_into_list (EEditorPage *editor_page,
					      const gchar *cid_src,
					      const gchar *src)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	g_hash_table_insert (editor_page->priv->inline_images, g_strdup (cid_src), g_strdup (src));
}

gboolean
e_editor_page_get_is_smiley_written (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return editor_page->priv->smiley_written;
}

void
e_editor_page_set_is_smiley_written (EEditorPage *editor_page,
				     gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->smiley_written = value;
}

gboolean
e_editor_page_get_dont_save_history_in_body_input (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return editor_page->priv->dont_save_history_in_body_input;
}

void
e_editor_page_set_dont_save_history_in_body_input (EEditorPage *editor_page,
						   gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->dont_save_history_in_body_input = value;
}

gboolean
e_editor_page_is_pasting_content_from_itself (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return editor_page->priv->pasting_content_from_itself;
}

void
e_editor_page_set_pasting_content_from_itself (EEditorPage *editor_page,
					       gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->pasting_content_from_itself = value;
}

gboolean
e_editor_page_get_renew_history_after_coordinates (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return editor_page->priv->renew_history_after_coordinates;
}

void
e_editor_page_set_renew_history_after_coordinates (EEditorPage *editor_page,
						   gboolean renew_history_after_coordinates)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->renew_history_after_coordinates = renew_history_after_coordinates;
}

gboolean
e_editor_page_is_composition_in_progress (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return editor_page->priv->composition_in_progress;
}

void
e_editor_page_set_composition_in_progress (EEditorPage *editor_page,
					   gboolean value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->composition_in_progress = value;
}

guint
e_editor_page_get_spell_check_on_scroll_event_source_id (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), 0);

	return editor_page->priv->spell_check_on_scroll_event_source_id;
}

void
e_editor_page_set_spell_check_on_scroll_event_source_id (EEditorPage *editor_page,
							 guint value)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	editor_page->priv->spell_check_on_scroll_event_source_id = value;
}

WebKitDOMNode *
e_editor_page_get_node_under_mouse_click (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	return editor_page->priv->node_under_mouse_click;
}

void
e_editor_page_emit_selection_changed (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMRange *range = NULL;
	GDBusConnection *connection;
	GError *error = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (!editor_page->priv->web_extension ||
	    editor_page->priv->selection_changed_blocked) {
		editor_page->priv->selection_changed = TRUE;
		return;
	}

	document = e_editor_page_get_document (editor_page);
	if (!document)
		return;

	connection = e_editor_web_extension_get_connection (editor_page->priv->web_extension);
	if (!connection)
		return;

	range = e_editor_dom_get_current_range (editor_page);
	if (!range)
		return;

	g_clear_object (&range);

	editor_page->priv->alignment = e_editor_dom_selection_get_alignment (editor_page);
	editor_page->priv->block_format = e_editor_dom_selection_get_block_format (editor_page);

	if (editor_page->priv->html_mode) {
		guint32 style_flags = E_CONTENT_EDITOR_STYLE_NONE;

		#define set_flag_if(tst, flg) G_STMT_START { \
			if (tst (editor_page)) \
				style_flags |= flg; \
			} G_STMT_END

		set_flag_if (e_editor_dom_selection_is_bold, E_CONTENT_EDITOR_STYLE_IS_BOLD);
		set_flag_if (e_editor_dom_selection_is_italic, E_CONTENT_EDITOR_STYLE_IS_ITALIC);
		set_flag_if (e_editor_dom_selection_is_underline, E_CONTENT_EDITOR_STYLE_IS_UNDERLINE);
		set_flag_if (e_editor_dom_selection_is_strikethrough, E_CONTENT_EDITOR_STYLE_IS_STRIKETHROUGH);
		set_flag_if (e_editor_dom_selection_is_monospace, E_CONTENT_EDITOR_STYLE_IS_MONOSPACE);
		set_flag_if (e_editor_dom_selection_is_subscript, E_CONTENT_EDITOR_STYLE_IS_SUBSCRIPT);
		set_flag_if (e_editor_dom_selection_is_superscript, E_CONTENT_EDITOR_STYLE_IS_SUPERSCRIPT);

		#undef set_flag_if

		editor_page->priv->style_flags = style_flags;
		editor_page->priv->font_size = e_editor_dom_selection_get_font_size (editor_page);
		g_free (editor_page->priv->font_color);
		editor_page->priv->font_color = e_editor_dom_selection_get_font_color (editor_page);
	}

	g_dbus_connection_emit_signal (
		connection,
		NULL,
		E_WEBKIT_EDITOR_WEB_EXTENSION_OBJECT_PATH,
		E_WEBKIT_EDITOR_WEB_EXTENSION_INTERFACE,
		"SelectionChanged",
		g_variant_new ("(tiibiis)",
			e_editor_page_get_page_id (editor_page),
			(gint32) editor_page->priv->alignment,
			(gint32) editor_page->priv->block_format,
			e_editor_dom_selection_is_indented (editor_page),
			editor_page->priv->style_flags,
			(gint32) editor_page->priv->font_size,
			editor_page->priv->font_color ? editor_page->priv->font_color : ""),
		&error);

	if (error) {
		g_warning ("%s: Failed to emit signal: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}

void
e_editor_page_emit_content_changed (EEditorPage *editor_page)
{
	GDBusConnection *connection;
	GError *error = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (!editor_page->priv->web_extension)
		return;

	connection = e_editor_web_extension_get_connection (editor_page->priv->web_extension);
	if (!connection)
		return;

	g_dbus_connection_emit_signal (
		connection,
		NULL,
		E_WEBKIT_EDITOR_WEB_EXTENSION_OBJECT_PATH,
		E_WEBKIT_EDITOR_WEB_EXTENSION_INTERFACE,
		"ContentChanged",
		g_variant_new ("(t)", e_editor_page_get_page_id (editor_page)),
		&error);

	if (error) {
		g_warning ("%s: Failed to emit signal: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}

void
e_editor_page_emit_undo_redo_state_changed (EEditorPage *editor_page)
{
	GDBusConnection *connection;
	GError *error = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (!editor_page->priv->web_extension)
		return;

	connection = e_editor_web_extension_get_connection (editor_page->priv->web_extension);
	if (!connection)
		return;

	g_dbus_connection_emit_signal (
		connection,
		NULL,
		E_WEBKIT_EDITOR_WEB_EXTENSION_OBJECT_PATH,
		E_WEBKIT_EDITOR_WEB_EXTENSION_INTERFACE,
		"UndoRedoStateChanged",
		g_variant_new ("(tbb)",
			e_editor_page_get_page_id (editor_page),
			e_editor_undo_redo_manager_can_undo (editor_page->priv->undo_redo_manager),
			e_editor_undo_redo_manager_can_redo (editor_page->priv->undo_redo_manager)),
		&error);

	if (error) {
		g_warning ("%s: Failed to emit signal: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}
