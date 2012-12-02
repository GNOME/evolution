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

#include <e-util/e-editor-selection.h>
#include <e-util/e-spell-checker.h>

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

typedef enum {
	E_EDITOR_WIDGET_REPLACE_ANSWER_REPLACE,
	E_EDITOR_WIDGET_REPLACE_ANSWER_REPLACE_ALL,
	E_EDITOR_WIDGET_REPLACE_ANSWER_CANCEL,
	E_EDITOR_WIDGET_REPLACE_ANSWER_NEXT
} EEditorWidgetReplaceAnswer;


/**
 * EEditorWidgetCommand:
 * @E_EDITOR_WIDGET_COMMAND_BACKGROUND_COLOR: Sets background color to given value.
 * @E_EDITOR_WIDGET_COMMAND_BOLD: Toggles bold formatting of current selection.
 * @E_EDITOR_WIDGET_COMMAND_COPY: Copies current selection to clipboard.
 * @E_EDITOR_WIDGET_COMMAND_CREATE_LINK: Converts current selection to a link that points to URL in value
 * @E_EDITOR_WIDGET_COMMAND_CUT: Cuts current selection to clipboard.
 * @E_EDITOR_WIDGET_COMMAND_DEFAULT_PARAGRAPH_SEPARATOR:
 * @E_EDITOR_WIDGET_COMMAND_DELETE: Deletes current selection.
 * @E_EDITOR_WIDGET_COMMAND_FIND_STRING: Highlights given string.
 * @E_EDITOR_WIDGET_COMMAND_FONT_NAME: Sets font name to given value.
 * @E_EDITOR_WIDGET_COMMAND_FONT_SIZE: Sets font point size to given value (no units, just number)
 * @E_EDITOR_WIDGET_COMMAND_FONT_SIZE_DELTA: Changes font size by given delta value (no units, just number)
 * @E_EDITOR_WIDGET_COMMAND_FORE_COLOR: Sets font color to given value
 * @E_EDITOR_WIDGET_COMMAND_FORMAT_BLOCK: Sets block type of current paragraph to given format. Allowed formats
 * 	are "BLOCKQUOTE", "H1", "H2", "H3", "H4", "H5", "H6", "P", "PRE" and "ADDRESS".
 * @E_EDITOR_WIDGET_COMMAND_FORWARD_DELETE:
 * @E_EDITOR_WIDGET_COMMAND_HILITE_COLOR: Sets color in which results of "FindString" command should be highlighted to given value.
 * @E_EDITOR_WIDGET_COMMAND_INDENT: Indents current paragraph by one level.
 * @E_EDITOR_WIDGET_COMMAND_INSERTS_HTML: Inserts give HTML code into document.
 * @E_EDITOR_WIDGET_COMMAND_INSERT_HORIZONTAL_RULE: Inserts a horizontal rule (&lt;HR&gt;) on current line.
 * @E_EDITOR_WIDGET_COMMAND_INSERT_IMAGE: Inserts an image with given source file.
 * @E_EDITOR_WIDGET_COMMAND_INSERT_LINE_BREAK: Breaks line at current cursor position.
 * @E_EDITOR_WIDGET_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT: Breaks citation at current cursor position.
 * @E_EDITOR_WIDGET_COMMAND_INSERT_ORDERERED_LIST: Creates an ordered list environment at current cursor position.
 * @E_EDITOR_WIDGET_COMMAND_INSERT_PARAGRAPH: Inserts a new paragraph at current cursor position.
 * @E_EDITOR_WIDGET_COMMAND_INSERT_TEXT: Inserts given text at current cursor position.
 * @E_EDITOR_WIDGET_COMMAND_INSERT_UNORDERED_LIST: Creates an undordered list environment at current cursor position.
 * @E_EDITOR_WIDGET_COMMAND_ITALIC: Toggles italic formatting of current selection.
 * @E_EDITOR_WIDGET_COMMAND_JUSTIFY_CENTER: Aligns current paragraph to center.
 * @E_EDITOR_WIDGET_COMMAND_JUSTIFY_FULL: Justifies current paragraph to block.
 * @E_EDITOR_WIDGET_COMMAND_JUSTIFY_NONE: Removes any justification or alignment of current paragraph.
 * @E_EDITOR_WIDGET_COMMAND_JUSTIFY_RIGHT: Aligns current paragraph to right.
 * @E_EDITOR_WIDGET_COMMAND_OUTDENT: Outdents current paragraph by one level.
 * @E_EDITOR_WIDGET_COMMAND_PASTE: Pastes clipboard content at current cursor position.
 * @E_EDITOR_WIDGET_COMMAND_PASTE_AND_MATCH_STYLE: Pastes clipboard content and matches it's style to style at current cursor position.
 * @E_EDITOR_WIDGET_COMMAND_PASTE_AS_PLAIN_TEXT: Pastes clipboard content at current cursor position removing any HTML formatting.
 * @E_EDITOR_WIDGET_COMMAND_PRINT: Print current document.
 * @E_EDITOR_WIDGET_COMMAND_REDO: Redos last action.
 * @E_EDITOR_WIDGET_COMMAND_REMOVE_FORMAT: Removes any formatting of current selection.
 * @E_EDITOR_WIDGET_COMMAND_SELECT_ALL: Extends selects to the entire document.
 * @E_EDITOR_WIDGET_COMMAND_STRIKETHROUGH: Toggles strikethrough formatting.
 * @E_EDITOR_WIDGET_COMMAND_STYLE_WITH_CSS: Toggles whether style should be defined in CSS "style" attribute of elements or
 *	whether to use deprecated <FONT> tags. Depends on whether given value is "true" or "false".
 * @E_EDITOR_WIDGET_COMMAND_SUBSCRIPT: Toggles subscript of current selection.
 * @E_EDITOR_WIDGET_COMMAND_SUPERSCRIPT: Toggles superscript of current selection.
 * @E_EDITOR_WIDGET_COMMAND_TRANSPOSE:
 * @E_EDITOR_WIDGET_COMMAND_UNDERLINE: Toggles underline formatting of current selection.
 * @E_EDITOR_WIDGET_COMMAND_UNDO: Undos last action.
 * @E_EDITOR_WIDGET_COMMAND_UNLINK:  Removes active links (&lt;A&gt;) from current selection (if there's any).
 * @E_EDITOR_WIDGET_COMMAND_UNSELECT: Cancels current selection.
 * @E_EDITOR_WIDGET_COMMAND_USE_CSS: Whether to allow use of CSS or not depending on whether given value is "true" or "false".
 *
 * Used to identify DOM command to execute using #e_editor_widget_exec_command().
 * Some commands require value to be passed in, which is always stated in the documentation.
 */

typedef enum {
	E_EDITOR_WIDGET_COMMAND_BACKGROUND_COLOR,
	E_EDITOR_WIDGET_COMMAND_BOLD,
	E_EDITOR_WIDGET_COMMAND_COPY,
	E_EDITOR_WIDGET_COMMAND_CREATE_LINK,
	E_EDITOR_WIDGET_COMMAND_CUT,
	E_EDITOR_WIDGET_COMMAND_DEFAULT_PARAGRAPH_SEPARATOR,
	E_EDITOR_WIDGET_COMMAND_DELETE,
	E_EDITOR_WIDGET_COMMAND_FIND_STRING,
	E_EDITOR_WIDGET_COMMAND_FONT_NAME,
	E_EDITOR_WIDGET_COMMAND_FONT_SIZE,
	E_EDITOR_WIDGET_COMMAND_FONT_SIZE_DELTA,
	E_EDITOR_WIDGET_COMMAND_FORE_COLOR,
	E_EDITOR_WIDGET_COMMAND_FORMAT_BLOCK,
	E_EDITOR_WIDGET_COMMAND_FORWARD_DELETE,
	E_EDITOR_WIDGET_COMMAND_HILITE_COLOR,
	E_EDITOR_WIDGET_COMMAND_INDENT,
	E_EDITOR_WIDGET_COMMAND_INSERT_HTML,
	E_EDITOR_WIDGET_COMMAND_INSERT_HORIZONTAL_RULE,
	E_EDITOR_WIDGET_COMMAND_INSERT_IMAGE,
	E_EDITOR_WIDGET_COMMAND_INSERT_LINE_BREAK,
	E_EDITOR_WIDGET_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT,
	E_EDITOR_WIDGET_COMMAND_INSERT_ORDERED_LIST,
	E_EDITOR_WIDGET_COMMAND_INSERT_PARAGRAPH,
	E_EDITOR_WIDGET_COMMAND_INSERT_TEXT,
	E_EDITOR_WIDGET_COMMAND_INSERT_UNORDERED_LIST,
	E_EDITOR_WIDGET_COMMAND_ITALIC,
	E_EDITOR_WIDGET_COMMAND_JUSTIFY_CENTER,
	E_EDITOR_WIDGET_COMMAND_JUSTIFY_FULL,
	E_EDITOR_WIDGET_COMMAND_JUSTIFY_LEFT,
	E_EDITOR_WIDGET_COMMAND_JUSTIFY_NONE,
	E_EDITOR_WIDGET_COMMAND_JUSTIFY_RIGHT,
	E_EDITOR_WIDGET_COMMAND_OUTDENT,
	E_EDITOR_WIDGET_COMMAND_PASTE,
	E_EDITOR_WIDGET_COMMAND_PASTE_AND_MATCH_STYLE,
	E_EDITOR_WIDGET_COMMAND_PASTE_AS_PLAIN_TEXT,
	E_EDITOR_WIDGET_COMMAND_PRINT,
	E_EDITOR_WIDGET_COMMAND_REDO,
	E_EDITOR_WIDGET_COMMAND_REMOVE_FORMAT,
	E_EDITOR_WIDGET_COMMAND_SELECT_ALL,
	E_EDITOR_WIDGET_COMMAND_STRIKETHROUGH,
	E_EDITOR_WIDGET_COMMAND_STYLE_WITH_CSS,
	E_EDITOR_WIDGET_COMMAND_SUBSCRIPT,
	E_EDITOR_WIDGET_COMMAND_SUPERSCRIPT,
	E_EDITOR_WIDGET_COMMAND_TRANSPOSE,
	E_EDITOR_WIDGET_COMMAND_UNDERLINE,
	E_EDITOR_WIDGET_COMMAND_UNDO,
	E_EDITOR_WIDGET_COMMAND_UNLINK,
	E_EDITOR_WIDGET_COMMAND_UNSELECT,
	E_EDITOR_WIDGET_COMMAND_USE_CSS
} EEditorWidgetCommand;

typedef struct _EEditorWidget EEditorWidget;
typedef struct _EEditorWidgetClass EEditorWidgetClass;
typedef struct _EEditorWidgetPrivate EEditorWidgetPrivate;

struct _EEditorWidget {
	WebKitWebView parent;

	EEditorWidgetPrivate *priv;
};

struct _EEditorWidgetClass {
	WebKitWebViewClass parent_class;

	void		(*paste_clipboard_quoted)	(EEditorWidget *widget);

	gboolean	(*popup_event)			(EEditorWidget *widget,
							 GdkEventButton *event);
};

GType		e_editor_widget_get_type 	(void);

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
gboolean	e_editor_widget_get_magic_smileys
						(EEditorWidget *widget);
void		e_editor_widget_set_magic_smileys
						(EEditorWidget *widget,
						 gboolean magic_smileys);

GList *		e_editor_widget_get_spell_languages
						(EEditorWidget *widget);
void		e_editor_widget_set_spell_languages
						(EEditorWidget *widget,
						 GList *spell_languages);

ESpellChecker *	e_editor_widget_get_spell_checker
						(EEditorWidget *widget);

gchar *		e_editor_widget_get_text_html	(EEditorWidget *widget);
gchar *		e_editor_widget_get_text_plain	(EEditorWidget *widget);
void		e_editor_widget_set_text_html	(EEditorWidget *widget,
						 const gchar *text);
void		e_editor_widget_set_text_plain	(EEditorWidget *widget,
						 const gchar *text);

void		e_editor_widget_paste_clipboard_quoted
						(EEditorWidget *widget);

void		e_editor_widget_update_fonts	(EEditorWidget *widget);

G_END_DECLS

#endif /* E_EDITOR_WIDGET_H */
