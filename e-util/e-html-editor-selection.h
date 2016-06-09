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
void		e_html_editor_selection_activate_properties_changed
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_block_selection_changed
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_unblock_selection_changed
						(EHTMLEditorSelection *selection);
gint		e_html_editor_selection_get_word_wrap_length
						(EHTMLEditorSelection *selection);
EContentEditorAlignment
		e_html_editor_selection_get_alignment
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_alignment
						(EHTMLEditorSelection *selection,
						 EContentEditorAlignment alignment);
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
EContentEditorBlockFormat
		e_html_editor_selection_get_block_format
						(EHTMLEditorSelection *selection);
void		e_html_editor_selection_set_block_format
						(EHTMLEditorSelection *selection,
						 EContentEditorBlockFormat format);
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
G_END_DECLS

#endif /* E_HTML_EDITOR_SELECTION_H */
