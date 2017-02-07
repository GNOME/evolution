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

#ifndef E_EDITOR_PAGE_H
#define E_EDITOR_PAGE_H

#include <glib-object.h>
#include <webkit2/webkit-web-extension.h>

#define E_UTIL_INCLUDE_WITHOUT_WEBKIT
#include <e-util/e-util.h>
#undef E_UTIL_INCLUDE_WITHOUT_WEBKIT

/* Standard GObject macros */
#define E_TYPE_EDITOR_PAGE \
	(e_editor_page_get_type ())
#define E_EDITOR_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EDITOR_PAGE, EEditorPage))
#define E_EDITOR_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EDITOR_PAGE, EEditorPageClass))
#define E_IS_EDITOR_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EDITOR_PAGE))
#define E_IS_EDITOR_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EDITOR_PAGE))
#define E_EDITOR_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EDITOR_PAGE, EEditorPageClass))

G_BEGIN_DECLS

struct _EEditorWebExtension;
struct _EEditorUndoRedoManager;

typedef struct _EEditorPage EEditorPage;
typedef struct _EEditorPageClass EEditorPageClass;
typedef struct _EEditorPagePrivate EEditorPagePrivate;

struct _EEditorPage {
	GObject parent;
	EEditorPagePrivate *priv;
};

struct _EEditorPageClass {
	GObjectClass parent_class;
};

GType		e_editor_page_get_type		(void) G_GNUC_CONST;
EEditorPage *	e_editor_page_new		(WebKitWebPage *web_page,
						 struct _EEditorWebExtension *web_extension);
WebKitWebPage *	e_editor_page_get_web_page	(EEditorPage *editor_page);
struct _EEditorWebExtension *
		e_editor_page_get_web_extension	(EEditorPage *editor_page);
guint64		e_editor_page_get_page_id	(EEditorPage *editor_page);
WebKitDOMDocument *
		e_editor_page_get_document	(EEditorPage *editor_page);
struct _EEditorUndoRedoManager *
		e_editor_page_get_undo_redo_manager
						(EEditorPage *editor_page);

void		e_editor_page_block_selection_changed
						(EEditorPage *editor_page);
void		e_editor_page_unblock_selection_changed
						(EEditorPage *editor_page);
gboolean	e_editor_page_get_html_mode	(EEditorPage *editor_page);
void		e_editor_page_set_html_mode	(EEditorPage *editor_page,
						 gboolean value);
gboolean	e_editor_page_get_force_image_load
						(EEditorPage *editor_page);
void		e_editor_page_set_force_image_load
						(EEditorPage *editor_page,
						 gboolean value);
gboolean	e_editor_page_get_bold		(EEditorPage *editor_page);
void		e_editor_page_set_bold		(EEditorPage *editor_page,
						 gboolean value);
gboolean	e_editor_page_get_italic	(EEditorPage *editor_page);
void		e_editor_page_set_italic	(EEditorPage *editor_page,
						 gboolean value);
gboolean	e_editor_page_get_underline	(EEditorPage *editor_page);
void		e_editor_page_set_underline	(EEditorPage *editor_page,
						 gboolean value);
gboolean	e_editor_page_get_monospace	(EEditorPage *editor_page);
void		e_editor_page_set_monospace	(EEditorPage *editor_page,
						 gboolean value);
gboolean	e_editor_page_get_strikethrough	(EEditorPage *editor_page);
void		e_editor_page_set_strikethrough	(EEditorPage *editor_page,
						 gboolean value);
guint		e_editor_page_get_font_size	(EEditorPage *editor_page);
void		e_editor_page_set_font_size	(EEditorPage *editor_page,
						 guint value);
const gchar *	e_editor_page_get_font_color	(EEditorPage *editor_page);
EContentEditorAlignment
		e_editor_page_get_alignment	(EEditorPage *editor_page);
void		e_editor_page_set_alignment	(EEditorPage *editor_page,
						 EContentEditorAlignment value);
gint		e_editor_page_get_word_wrap_length
						(EEditorPage *editor_page);
gboolean	e_editor_page_get_return_key_pressed
						(EEditorPage *editor_page);
void		e_editor_page_set_return_key_pressed
						(EEditorPage *editor_page,
						 gboolean value);
gboolean	e_editor_page_get_space_key_pressed
						(EEditorPage *editor_page);
void		e_editor_page_set_space_key_pressed
						(EEditorPage *editor_page,
						 gboolean value);
gboolean	e_editor_page_get_magic_links_enabled
						(EEditorPage *editor_page);
gboolean	e_editor_page_get_magic_smileys_enabled
						(EEditorPage *editor_page);
gboolean	e_editor_page_get_unicode_smileys_enabled
						(EEditorPage *editor_page);
EImageLoadingPolicy
		e_editor_page_get_image_loading_policy
						(EEditorPage *editor_page);
gboolean	e_editor_page_get_inline_spelling_enabled
						(EEditorPage *editor_page);
gboolean	e_editor_page_check_word_spelling
						(EEditorPage *editor_page,
						 const gchar *word,
						 const gchar * const *languages);
gboolean	e_editor_page_get_body_input_event_removed
						(EEditorPage *editor_page);
void		e_editor_page_set_body_input_event_removed
						(EEditorPage *editor_page,
						 gboolean value);
gboolean	e_editor_page_get_convert_in_situ
						(EEditorPage *editor_page);
void		e_editor_page_set_convert_in_situ
						(EEditorPage *editor_page,
						 gboolean value);
GHashTable *	e_editor_page_get_inline_images
						(EEditorPage *editor_page);
void		e_editor_page_add_new_inline_image_into_list
						(EEditorPage *editor_page,
						 const gchar *cid_src,
						 const gchar *src);
gboolean	e_editor_page_get_is_smiley_written
						(EEditorPage *editor_page);
void		e_editor_page_set_is_smiley_written
						(EEditorPage *editor_page,
						 gboolean value);
gboolean	e_editor_page_get_dont_save_history_in_body_input
						(EEditorPage *editor_page);
void		e_editor_page_set_dont_save_history_in_body_input
						(EEditorPage *editor_page,
						 gboolean value);
gboolean	e_editor_page_is_pasting_content_from_itself
						(EEditorPage *editor_page);
void		e_editor_page_set_pasting_content_from_itself
						(EEditorPage *editor_page,
						 gboolean value);
gboolean	e_editor_page_get_renew_history_after_coordinates
						(EEditorPage *editor_page);
void		e_editor_page_set_renew_history_after_coordinates
						(EEditorPage *editor_page,
						 gboolean renew_history_after_coordinates);
gboolean	e_editor_page_is_composition_in_progress
						(EEditorPage *editor_page);
void		e_editor_page_set_composition_in_progress
						(EEditorPage *editor_page,
						 gboolean value);
gboolean	e_editor_page_get_allow_top_signature
						(EEditorPage *editor_page);
void		e_editor_page_set_allow_top_signature
						(EEditorPage *editor_page,
						 gboolean value);
guint		e_editor_page_get_spell_check_on_scroll_event_source_id
						(EEditorPage *editor_page);
void		e_editor_page_set_spell_check_on_scroll_event_source_id
						(EEditorPage *editor_page,
						 guint value);
WebKitDOMNode *	e_editor_page_get_node_under_mouse_click
						(EEditorPage *editor_page);

void		e_editor_page_emit_selection_changed
						(EEditorPage *editor_page);
void		e_editor_page_emit_content_changed
						(EEditorPage *editor_page);
void		e_editor_page_emit_undo_redo_state_changed
						(EEditorPage *editor_page);
void		e_editor_page_emit_user_changed_default_colors
						(EEditorPage *editor_page,
						 gboolean suppress_color_changes);

G_END_DECLS

#endif /* E_EDITOR_PAGE_H */
