/*
 * e-editor-selection.h
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

#ifndef E_EDITOR_SELECTION_H
#define E_EDITOR_SELECTION_H

#include <gtk/gtk.h>
#include <e-util/e-util-enums.h>
#include <webkit/webkit.h>

/* Standard GObject macros */
#define E_TYPE_EDITOR_SELECTION \
	(e_editor_selection_get_type ())
#define E_EDITOR_SELECTION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EDITOR_SELECTION, EEditorSelection))
#define E_EDITOR_SELECTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EDITOR_SELECTION, EEditorSelectionClass))
#define E_IS_EDITOR_SELECTION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EDITOR_SELECTION))
#define E_IS_EDITOR_SELECTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EDITOR_SELECTION))
#define E_EDITOR_SELECTION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EDITOR_SELECTION, EEditorSelectionClass))

G_BEGIN_DECLS

struct _EEditorWidget;

typedef struct _EEditorSelection EEditorSelection;
typedef struct _EEditorSelectionClass EEditorSelectionClass;
typedef struct _EEditorSelectionPrivate EEditorSelectionPrivate;

struct _EEditorSelection {
	GObject parent;
	EEditorSelectionPrivate *priv;
};

struct _EEditorSelectionClass {
	GObjectClass parent_class;
};

GType		e_editor_selection_get_type	(void) G_GNUC_CONST;
struct _EEditorWidget *
		e_editor_selection_ref_editor_widget
						(EEditorSelection *selection);
void		e_editor_selection_block_selection_changed
						(EEditorSelection *selection);
void		e_editor_selection_unblock_selection_changed
						(EEditorSelection *selection);
gint		e_editor_selection_get_word_wrap_length
						(EEditorSelection *selection);
gboolean	e_editor_selection_has_text	(EEditorSelection *selection);
gchar *		e_editor_selection_get_caret_word
						(EEditorSelection *selection);
void		e_editor_selection_replace_caret_word
						(EEditorSelection *selection,
						 const gchar *replacement);
EEditorSelectionAlignment
		e_editor_selection_get_alignment
						(EEditorSelection *selection);
void		e_editor_selection_set_alignment
						(EEditorSelection *selection,
						 EEditorSelectionAlignment alignment);
const gchar *	e_editor_selection_get_background_color
						(EEditorSelection *selection);
void		e_editor_selection_set_background_color
						(EEditorSelection *selection,
						 const gchar *color);
void		e_editor_selection_get_font_color
						(EEditorSelection *selection,
						 GdkRGBA *rgba);
void		e_editor_selection_set_font_color
						(EEditorSelection *selection,
						 const GdkRGBA *rgba);
const gchar *	e_editor_selection_get_font_name
						(EEditorSelection *selection);
void		e_editor_selection_set_font_name
						(EEditorSelection *selection,
						 const gchar *font_name);
guint		e_editor_selection_get_font_size
						(EEditorSelection *selection);
void		e_editor_selection_set_font_size
						(EEditorSelection *selection,
						 guint font_size);
EEditorSelectionBlockFormat
		e_editor_selection_get_block_format
						(EEditorSelection *selection);
void		e_editor_selection_set_block_format
						(EEditorSelection *selection,
						 EEditorSelectionBlockFormat format);
gboolean	e_editor_selection_is_citation	(EEditorSelection *selection);
gboolean	e_editor_selection_is_indented	(EEditorSelection *selection);
void		e_editor_selection_indent	(EEditorSelection *selection);
void		e_editor_selection_unindent	(EEditorSelection *selection);
gboolean	e_editor_selection_is_bold	(EEditorSelection *selection);
void		e_editor_selection_set_bold	(EEditorSelection *selection,
						 gboolean bold);
gboolean	e_editor_selection_is_italic	(EEditorSelection *selection);
void		e_editor_selection_set_italic	(EEditorSelection *selection,
						 gboolean italic);
gboolean	e_editor_selection_is_monospaced
						(EEditorSelection *selection);
void		e_editor_selection_set_monospaced
						(EEditorSelection *selection,
						 gboolean monospaced);
gboolean	e_editor_selection_is_strikethrough
						(EEditorSelection *selection);
void		e_editor_selection_set_strikethrough
						(EEditorSelection *selection,
						 gboolean strikethrough);
gboolean	e_editor_selection_is_superscript
						(EEditorSelection *selection);
void		e_editor_selection_set_superscript
						(EEditorSelection *selection,
						 gboolean superscript);
gboolean	e_editor_selection_is_subscript
						(EEditorSelection *selection);
void		e_editor_selection_set_subscript
						(EEditorSelection *selection,
						 gboolean subscript);
gboolean	e_editor_selection_is_underline
						(EEditorSelection *selection);
void		e_editor_selection_set_underline
						(EEditorSelection *selection,
						 gboolean underline);
void		e_editor_selection_unlink	(EEditorSelection *selection);
void		e_editor_selection_create_link	(EEditorSelection *selection,
						 const gchar *uri);
const gchar *	e_editor_selection_get_string	(EEditorSelection *selection);
void		e_editor_selection_replace	(EEditorSelection *selection,
						 const gchar *new_string);
void		e_editor_selection_insert_html	(EEditorSelection *selection,
						 const gchar *html_text);
void		e_editor_selection_insert_image	(EEditorSelection *selection,
						 const gchar *image_uri);
void		e_editor_selection_insert_text	(EEditorSelection *selection,
						 const gchar *plain_text);
void 		e_editor_selection_clear_caret_position_marker
						(EEditorSelection *selection);
WebKitDOMNode *
		e_editor_selection_get_caret_position_node
						(WebKitDOMDocument *document);
WebKitDOMElement *
		e_editor_selection_save_caret_position
						(EEditorSelection *selection);
void		e_editor_selection_restore_caret_position
						(EEditorSelection *selection);
void		e_editor_selection_set_indented_style
						(EEditorSelection *selection,
						 WebKitDOMElement *element,
						 gint width);
WebKitDOMElement *
		e_editor_selection_get_indented_element
						(EEditorSelection *selection,
						 WebKitDOMDocument *document,
						 gint width);
void		e_editor_selection_set_paragraph_style
						(EEditorSelection *selection,
						 WebKitDOMElement *element,
						 gint width,
						 gint offset,
						 const gchar *style_to_add);
WebKitDOMElement *
		e_editor_selection_get_paragraph_element
						(EEditorSelection *selection,
						 WebKitDOMDocument *document,
						 gint width,
						 gint offset);
WebKitDOMElement *
		e_editor_selection_put_node_into_paragraph
						(EEditorSelection *selection,
						 WebKitDOMDocument *document,
						 WebKitDOMNode *node,
						 WebKitDOMNode *caret_position);
void		e_editor_selection_wrap_lines	(EEditorSelection *selection);
WebKitDOMElement *
		e_editor_selection_wrap_paragraph_length
						(EEditorSelection *selection,
						 WebKitDOMElement *paragraph,
						 gint length);
void		e_editor_selection_wrap_paragraphs_in_document
						(EEditorSelection *selection,
						 WebKitDOMDocument *document);
WebKitDOMElement *
		e_editor_selection_wrap_paragraph
						(EEditorSelection *selection,
						 WebKitDOMElement *paragraph);
void		e_editor_selection_save		(EEditorSelection *selection);
void		e_editor_selection_restore	(EEditorSelection *selection);
void		e_editor_selection_move		(EEditorSelection *selection,
						 gboolean forward,
						 EEditorSelectionGranularity granularity);
void		e_editor_selection_extend	(EEditorSelection *selection,
						 gboolean forward,
						 EEditorSelectionGranularity granularity);

G_END_DECLS

#endif /* E_EDITOR_SELECTION_H */
