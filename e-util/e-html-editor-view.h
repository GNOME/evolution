/*
 * e-html-editor-view.h
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_HTML_EDITOR_VIEW_H
#define E_HTML_EDITOR_VIEW_H

#include <webkit2/webkit2.h>

#include <camel/camel.h>

#include <e-util/e-html-editor-selection.h>
#include <e-util/e-emoticon.h>
#include <e-util/e-spell-checker.h>
#include <e-util/e-util-enums.h>

/* Standard GObject macros */
#define E_TYPE_HTML_EDITOR_VIEW \
	(e_html_editor_view_get_type ())
#define E_HTML_EDITOR_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_HTML_EDITOR_VIEW, EHTMLEditorView))
#define E_HTML_EDITOR_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_HTML_EDITOR_VIEW, EHTMLEditorViewClass))
#define E_IS_HTML_EDITOR_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_HTML_EDITOR_VIEW))
#define E_IS_HTML_EDITOR_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_HTML_EDITOR_VIEW))
#define E_HTML_EDITOR_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_HTML_EDITOR_VIEW, EHTMLEditorViewClass))

G_BEGIN_DECLS

typedef struct _EHTMLEditorView EHTMLEditorView;
typedef struct _EHTMLEditorViewClass EHTMLEditorViewClass;
typedef struct _EHTMLEditorViewPrivate EHTMLEditorViewPrivate;

struct _EHTMLEditorView {
	WebKitWebView parent;
	EHTMLEditorViewPrivate *priv;
};

struct _EHTMLEditorViewClass {
	WebKitWebViewClass parent_class;

	void		(*paste_clipboard_quoted)
						(EHTMLEditorView *view);
	gboolean	(*popup_event)		(EHTMLEditorView *view,
						 GdkEventButton *event);
	void		(*paste_primary_clipboard)
						(EHTMLEditorView *view);
};

GType		e_html_editor_view_get_type	(void) G_GNUC_CONST;
EHTMLEditorView *
		e_html_editor_view_new		(void);
GDBusProxy *
		e_html_editor_view_get_web_extension_proxy
						(EHTMLEditorView *view);
void		e_html_editor_view_call_simple_extension_function
						(EHTMLEditorView *view,
						 const gchar *extension_function);
void		e_html_editor_view_call_simple_extension_function_sync
						(EHTMLEditorView *view,
						 const gchar *function);
GVariant *	e_html_editor_view_get_element_attribute
						(EHTMLEditorView *view,
						 const gchar *selector,
						 const gchar *attribute);
void		e_html_editor_view_set_element_attribute
						(EHTMLEditorView *view,
						 const gchar *selector,
						 const gchar *attribute,
						 const gchar *value);
void		e_html_editor_view_remove_element_attribute
						(EHTMLEditorView *view,
						 const gchar *selector,
						 const gchar *attribute);
EHTMLEditorSelection *
		e_html_editor_view_get_selection
						(EHTMLEditorView *view);
gboolean	e_html_editor_view_get_changed	(EHTMLEditorView *view);
void		e_html_editor_view_set_changed	(EHTMLEditorView *view,
						 gboolean changed);
gboolean	e_html_editor_view_get_html_mode
						(EHTMLEditorView *view);
void		e_html_editor_view_set_html_mode
						(EHTMLEditorView *view,
						 gboolean html_mode);
void		e_html_editor_view_force_spell_check
						(EHTMLEditorView *view);
void		e_html_editor_view_turn_spell_check_off
						(EHTMLEditorView *view);
gboolean	e_html_editor_view_get_inline_spelling
						(EHTMLEditorView *view);
void		e_html_editor_view_set_inline_spelling
						(EHTMLEditorView *view,
						 gboolean inline_spelling);
gboolean	e_html_editor_view_get_magic_links
						(EHTMLEditorView *view);
void		e_html_editor_view_set_magic_links
						(EHTMLEditorView *view,
						 gboolean magic_links);
void		e_html_editor_view_insert_smiley
						(EHTMLEditorView *view,
						 EEmoticon *emoticon);
gboolean	e_html_editor_view_get_magic_smileys
						(EHTMLEditorView *view);
void		e_html_editor_view_set_magic_smileys
						(EHTMLEditorView *view,
						 gboolean magic_smileys);
ESpellChecker *	e_html_editor_view_get_spell_checker
						(EHTMLEditorView *view);
gchar *		e_html_editor_view_get_text_html
						(EHTMLEditorView *view,
						 const gchar *from_domain,
						 GList **inline_images);
gchar *		e_html_editor_view_get_text_html_for_drafts
						(EHTMLEditorView *view);
gchar *		e_html_editor_view_get_text_plain
						(EHTMLEditorView *view);
void		e_html_editor_view_convert_and_insert_plain_text
						(EHTMLEditorView *view,
						 const gchar *text);
void		e_html_editor_view_convert_and_insert_html_to_plain_text
						(EHTMLEditorView *view,
						 const gchar *html);
void		e_html_editor_view_insert_text (EHTMLEditorView *view,
						const gchar *text);
void		e_html_editor_view_insert_html (EHTMLEditorView *view,
						const gchar *html_text);
void		e_html_editor_view_set_text_html
						(EHTMLEditorView *view,
						 const gchar *text);
void		e_html_editor_view_set_text_plain
						(EHTMLEditorView *view,
						 const gchar *text);
void		e_html_editor_view_paste_as_text
						(EHTMLEditorView *view);
void		e_html_editor_view_paste_clipboard_quoted
						(EHTMLEditorView *view);
void		e_html_editor_view_embed_styles	(EHTMLEditorView *view);
void		e_html_editor_view_remove_embed_styles
						(EHTMLEditorView *view);
void		e_html_editor_view_update_fonts	(EHTMLEditorView *view);
void		e_html_editor_view_dequote_plain_text
						(EHTMLEditorView *view);
void		e_html_editor_view_add_inline_image_from_mime_part
						(EHTMLEditorView *view,
                                                 CamelMimePart *part);
gboolean	e_html_editor_view_is_message_from_draft
						(EHTMLEditorView *view);
void		e_html_editor_view_set_is_message_from_draft
						(EHTMLEditorView *view,
						 gboolean value);
void		e_html_editor_view_set_is_message_from_selection
						(EHTMLEditorView *view,
						 gboolean value);
gboolean	e_html_editor_view_is_message_from_edit_as_new
						(EHTMLEditorView *view);
void		e_html_editor_view_set_is_message_from_edit_as_new
						(EHTMLEditorView *view,
						 gboolean value);
void		e_html_editor_view_set_remove_initial_input_line
						(EHTMLEditorView *view,
						 gboolean value);
void		e_html_editor_view_scroll_to_caret
						(EHTMLEditorView *view);
void		e_html_editor_view_replace_image_src
						(EHTMLEditorView *view,
						 const gchar *selector,
						 const gchar *image_uri);
void		e_html_editor_view_insert_image (EHTMLEditorView *view,
						 const gchar *image_uri);
void		e_html_editor_view_check_magic_links
						(EHTMLEditorView *view);
void		e_html_editor_view_restore_selection
						(EHTMLEditorView *view);
void		e_html_editor_view_save_selection
						(EHTMLEditorView *view);
G_END_DECLS

#endif /* E_HTML_EDITOR_VIEW_H */
