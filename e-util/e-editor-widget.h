/*
 * e-editor-widget.h
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

#ifndef E_EDITOR_WIDGET_H
#define E_EDITOR_WIDGET_H

#include <webkit/webkit.h>

#include <camel/camel.h>

#include <e-util/e-editor-selection.h>
#include <e-util/e-emoticon.h>
#include <e-util/e-spell-checker.h>
#include <e-util/e-util-enums.h>

/* Standard GObject macros */
#define E_TYPE_EDITOR_WIDGET \
	(e_editor_widget_get_type ())
#define E_EDITOR_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EDITOR_WIDGET, EEditorWidget))
#define E_EDITOR_WIDGET_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EDITOR_WIDGET, EEditorWidgetClass))
#define E_IS_EDITOR_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EDITOR_WIDGET))
#define E_IS_EDITOR_WIDGET_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EDITOR_WIDGET))
#define E_EDITOR_WIDGET_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EDITOR_WIDGET, EEditorWidgetClass))

G_BEGIN_DECLS

typedef struct _EEditorWidget EEditorWidget;
typedef struct _EEditorWidgetClass EEditorWidgetClass;
typedef struct _EEditorWidgetPrivate EEditorWidgetPrivate;

struct _EEditorWidget {
	WebKitWebView parent;
	EEditorWidgetPrivate *priv;
};

struct _EEditorWidgetClass {
	WebKitWebViewClass parent_class;

	void		(*paste_clipboard_quoted)
						(EEditorWidget *widget);
	gboolean	(*popup_event)		(EEditorWidget *widget,
						 GdkEventButton *event);
};

GType		e_editor_widget_get_type	(void) G_GNUC_CONST;
EEditorWidget *	e_editor_widget_new		(void);
EEditorSelection *
		e_editor_widget_get_selection	(EEditorWidget *widget);
gboolean	e_editor_widget_exec_command	(EEditorWidget *widget,
						 EEditorWidgetCommand command,
						 const gchar *value);
gboolean	e_editor_widget_get_changed	(EEditorWidget *widget);
void		e_editor_widget_set_changed	(EEditorWidget *widget,
						 gboolean changed);
gboolean	e_editor_widget_get_html_mode	(EEditorWidget *widget);
void		e_editor_widget_set_html_mode	(EEditorWidget *widget,
						 gboolean html_mode);
gboolean	e_editor_widget_get_inline_spelling
						(EEditorWidget *widget);
void		e_editor_widget_set_inline_spelling
						(EEditorWidget *widget,
						 gboolean inline_spelling);
gboolean	e_editor_widget_get_magic_links	(EEditorWidget *widget);
void		e_editor_widget_set_magic_links	(EEditorWidget *widget,
						 gboolean magic_links);
void		e_editor_widget_insert_smiley	(EEditorWidget *widget,
						 EEmoticon *emoticon);
gboolean	e_editor_widget_get_magic_smileys
						(EEditorWidget *widget);
void		e_editor_widget_set_magic_smileys
						(EEditorWidget *widget,
						 gboolean magic_smileys);
ESpellChecker *	e_editor_widget_get_spell_checker
						(EEditorWidget *widget);
gchar *		e_editor_widget_get_text_html	(EEditorWidget *widget);
gchar *		e_editor_widget_get_text_html_for_drafts
						(EEditorWidget *widget);
gchar *		e_editor_widget_get_text_plain	(EEditorWidget *widget);
void		e_editor_widget_convert_and_insert_html_to_plain_text
						(EEditorWidget *widget,
						 const gchar *html);
void		e_editor_widget_set_text_html	(EEditorWidget *widget,
						 const gchar *text);
void		e_editor_widget_set_text_plain	(EEditorWidget *widget,
						 const gchar *text);
void		e_editor_widget_paste_clipboard_quoted
						(EEditorWidget *widget);
void		e_editor_widget_embed_styles	(EEditorWidget *widget);
void		e_editor_widget_remove_embed_styles
						(EEditorWidget *widget);
void		e_editor_widget_update_fonts	(EEditorWidget *widget);
WebKitDOMElement *
		e_editor_widget_get_element_under_mouse_click
						(EEditorWidget *widget);
void		e_editor_widget_check_magic_links
						(EEditorWidget *widget,
						 gboolean while_typing);
WebKitDOMElement *
		e_editor_widget_quote_plain_text_element
						(EEditorWidget *widget,
                                                 WebKitDOMElement *element);
WebKitDOMElement *
		e_editor_widget_quote_plain_text
						(EEditorWidget *widget);
void		e_editor_widget_dequote_plain_text
						(EEditorWidget *widget);
void		e_editor_widget_force_spellcheck
						(EEditorWidget *widget);
void		e_editor_widget_add_inline_image_from_mime_part
						(EEditorWidget *widget,
                                                 CamelMimePart *part);
GList *		e_editor_widget_get_parts_for_inline_images
						(EEditorWidget *widget);
G_END_DECLS

#endif /* E_EDITOR_WIDGET_H */
