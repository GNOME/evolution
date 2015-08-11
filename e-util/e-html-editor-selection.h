/*
 * e-html-editor-selection.h
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

#ifndef E_HTML_EDITOR_SELECTION_H
#define E_HTML_EDITOR_SELECTION_H

#include <gtk/gtk.h>
#include <e-util/e-util-enums.h>
#include <webkit/webkit.h>

/* Standard GObject macros */
#define E_TYPE_HTML_EDITOR_SELECTION \
	(e_html_editor_selection_get_type ())
#define E_HTML_EDITOR_SELECTION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_HTML_EDITOR_SELECTION, EHTMLEditorSelection))
#define E_HTML_EDITOR_SELECTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_HTML_EDITOR_SELECTION, EHTMLEditorSelectionClass))
#define E_IS_HTML_EDITOR_SELECTION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_HTML_EDITOR_SELECTION))
#define E_IS_HTML_EDITOR_SELECTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_HTML_EDITOR_SELECTION))
#define E_HTML_EDITOR_SELECTION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_HTML_EDITOR_SELECTION, EHTMLEditorSelectionClass))

G_BEGIN_DECLS

struct _EHTMLEditorView;

typedef struct _EHTMLEditorSelection EHTMLEditorSelection;
typedef struct _EHTMLEditorSelectionClass EHTMLEditorSelectionClass;
typedef struct _EHTMLEditorSelectionPrivate EHTMLEditorSelectionPrivate;

struct _EHTMLEditorSelection {
	GObject parent;
	EHTMLEditorSelectionPrivate *priv;
};

struct _EHTMLEditorSelectionClass {
	GObjectClass parent_class;
};

GType		e_html_editor_selection_get_type
						(void) G_GNUC_CONST;
struct _EHTMLEditorView *
		e_html_editor_selection_ref_html_editor_view
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_block_selection_changed
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_unblock_selection_changed
						(EHTMLEditorSelection *selection);
gint		e_html_editor_selection_get_word_wrap_length
						(EHTMLEditorSelection *selection);
gboolean	e_html_editor_selection_has_text
						(EHTMLEditorSelection *selection);
gchar *		e_html_editor_selection_get_caret_word
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_replace_caret_word
						(EHTMLEditorSelection *selection,
						 const gchar *replacement);
EHTMLEditorSelectionAlignment
		e_html_editor_selection_get_alignment
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_alignment
						(EHTMLEditorSelection *selection,
						 EHTMLEditorSelectionAlignment alignment);
const gchar *	e_html_editor_selection_get_background_color
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_background_color
						(EHTMLEditorSelection *selection,
						 const gchar *color);
void		e_html_editor_selection_get_font_color
						(EHTMLEditorSelection *selection,
						 GdkRGBA *rgba);
void		e_html_editor_selection_set_font_color
						(EHTMLEditorSelection *selection,
						 const GdkRGBA *rgba);
const gchar *	e_html_editor_selection_get_font_name
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_font_name
						(EHTMLEditorSelection *selection,
						 const gchar *font_name);
guint		e_html_editor_selection_get_font_size
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_font_size
						(EHTMLEditorSelection *selection,
						 guint font_size);
EHTMLEditorSelectionBlockFormat
		e_html_editor_selection_get_block_format
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_block_format
						(EHTMLEditorSelection *selection,
						 EHTMLEditorSelectionBlockFormat format);
gboolean	e_html_editor_selection_is_citation
						(EHTMLEditorSelection *selection);
gboolean	e_html_editor_selection_is_indented
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_indent	(EHTMLEditorSelection *selection);
void		e_html_editor_selection_unindent
						(EHTMLEditorSelection *selection);
gboolean	e_html_editor_selection_is_bold	(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_bold
						(EHTMLEditorSelection *selection,
						 gboolean bold);
gboolean	e_html_editor_selection_is_italic
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_italic
						(EHTMLEditorSelection *selection,
						 gboolean italic);
gboolean	e_html_editor_selection_is_monospaced
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_monospaced
						(EHTMLEditorSelection *selection,
						 gboolean monospaced);
gboolean	e_html_editor_selection_is_strikethrough
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_strikethrough
						(EHTMLEditorSelection *selection,
						 gboolean strikethrough);
gboolean	e_html_editor_selection_is_superscript
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_superscript
						(EHTMLEditorSelection *selection,
						 gboolean superscript);
gboolean	e_html_editor_selection_is_subscript
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_subscript
						(EHTMLEditorSelection *selection,
						 gboolean subscript);
gboolean	e_html_editor_selection_is_underline
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_underline
						(EHTMLEditorSelection *selection,
						 gboolean underline);
void		e_html_editor_selection_unlink	(EHTMLEditorSelection *selection);
void		e_html_editor_selection_create_link
						(EHTMLEditorSelection *selection,
						 const gchar *uri);
gboolean	e_html_editor_selection_is_collapsed
						(EHTMLEditorSelection *selection);
const gchar *	e_html_editor_selection_get_string
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_replace	(EHTMLEditorSelection *selection,
						 const gchar *new_string);
void		e_html_editor_selection_insert_text
						(EHTMLEditorSelection *selection,
						 const gchar *plain_text);
void		e_html_editor_selection_insert_html
						(EHTMLEditorSelection *selection,
						 const gchar *html_text);
void		e_html_editor_selection_insert_as_text
						(EHTMLEditorSelection *selection,
						 const gchar *html_text);
void		e_html_editor_selection_replace_image_src
						(EHTMLEditorSelection *selection,
						 WebKitDOMElement *element,
						 const gchar *image_uri);
void		e_html_editor_selection_insert_image
						(EHTMLEditorSelection *selection,
						 const gchar *image_uri);
void		e_html_editor_selection_move_caret_into_element
						(WebKitDOMDocument *document,
						 WebKitDOMElement *element,
						 gboolean to_start);
void		e_html_editor_selection_set_indented_style
						(EHTMLEditorSelection *selection,
						 WebKitDOMElement *element,
						 gint width);
WebKitDOMElement *
		e_html_editor_selection_get_indented_element
						(EHTMLEditorSelection *selection,
						 WebKitDOMDocument *document,
						 gint width);
void		e_html_editor_selection_set_paragraph_style
						(EHTMLEditorSelection *selection,
						 WebKitDOMElement *element,
						 gint width,
						 gint offset,
						 const gchar *style_to_add);
WebKitDOMElement *
		e_html_editor_selection_get_paragraph_element
						(EHTMLEditorSelection *selection,
						 WebKitDOMDocument *document,
						 gint width,
						 gint offset);
WebKitDOMElement *
		e_html_editor_selection_put_node_into_paragraph
						(EHTMLEditorSelection *selection,
						 WebKitDOMDocument *document,
						 WebKitDOMNode *node,
						 gboolean with_input);
void		e_html_editor_selection_wrap_lines
						(EHTMLEditorSelection *selection);
WebKitDOMElement *
		e_html_editor_selection_wrap_paragraph_length
						(EHTMLEditorSelection *selection,
						 WebKitDOMElement *paragraph,
						 gint length);
void		e_html_editor_selection_wrap_paragraphs_in_document
						(EHTMLEditorSelection *selection,
						 WebKitDOMDocument *document);
WebKitDOMElement *
		e_html_editor_selection_wrap_paragraph
						(EHTMLEditorSelection *selection,
						 WebKitDOMElement *paragraph);
void		e_html_editor_selection_save	(EHTMLEditorSelection *selection);
void		e_html_editor_selection_restore	(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_on_point
						(EHTMLEditorSelection *selection,
						 guint x,
						 guint y);
void		e_html_editor_selection_move	(EHTMLEditorSelection *selection,
						 gboolean forward,
						 EHTMLEditorSelectionGranularity granularity);
void		e_html_editor_selection_extend	(EHTMLEditorSelection *selection,
						 gboolean forward,
						 EHTMLEditorSelectionGranularity granularity);
void		e_html_editor_selection_scroll_to_caret
						(EHTMLEditorSelection *selection);
EHTMLEditorSelectionAlignment
		e_html_editor_selection_get_list_alignment_from_node
						(WebKitDOMNode *node);
void		remove_wrapping_from_element	(WebKitDOMElement *element);
void		remove_quoting_from_element	(WebKitDOMElement *element);
void		e_html_editor_selection_get_selection_coordinates
						(EHTMLEditorSelection *selection,
						 guint *start_x,
						 guint *start_y,
						 guint *end_x,
						 guint *end_y);
G_END_DECLS

#endif /* E_HTML_EDITOR_SELECTION_H */
