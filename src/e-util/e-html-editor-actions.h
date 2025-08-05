/* e-html-editor-actions.h
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_HTML_EDITOR_ACTIONS_H
#define E_HTML_EDITOR_ACTIONS_H

#define E_HTML_EDITOR_ACTION(editor, name) \
	(e_html_editor_get_action (E_HTML_EDITOR (editor), (name)))

#define E_HTML_EDITOR_ACTION_BOLD(editor) \
	E_HTML_EDITOR_ACTION ((editor), "bold")
#define E_HTML_EDITOR_ACTION_CONTEXT_COPY_LINK(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-copy-link")
#define E_HTML_EDITOR_ACTION_CONTEXT_DELETE_CELL(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-delete-cell")
#define E_HTML_EDITOR_ACTION_CONTEXT_DELETE_COLUMN(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-delete-column")
#define E_HTML_EDITOR_ACTION_CONTEXT_DELETE_HRULE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-delete-hrule")
#define E_HTML_EDITOR_ACTION_CONTEXT_DELETE_IMAGE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-delete-image")
#define E_HTML_EDITOR_ACTION_CONTEXT_DELETE_ROW(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-delete-row")
#define E_HTML_EDITOR_ACTION_CONTEXT_DELETE_TABLE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-delete-table")
#define E_HTML_EDITOR_ACTION_CONTEXT_INSERT_COLUMN_AFTER(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-insert-column-after")
#define E_HTML_EDITOR_ACTION_CONTEXT_INSERT_COLUMN_BEFORE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-insert-column-before")
#define E_HTML_EDITOR_ACTION_CONTEXT_INSERT_LINK(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-insert-link")
#define E_HTML_EDITOR_ACTION_CONTEXT_INSERT_ROW_ABOVE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-insert-row-above")
#define E_HTML_EDITOR_ACTION_CONTEXT_INSERT_ROW_BELOW(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-insert-row-below")
#define E_HTML_EDITOR_ACTION_CONTEXT_OPEN_LINK(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-open-link")
#define E_HTML_EDITOR_ACTION_CONTEXT_PROPERTIES_CELL(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-properties-cell")
#define E_HTML_EDITOR_ACTION_CONTEXT_PROPERTIES_IMAGE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-properties-image")
#define E_HTML_EDITOR_ACTION_CONTEXT_PROPERTIES_LINK(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-properties-link")
#define E_HTML_EDITOR_ACTION_CONTEXT_PROPERTIES_PARAGRAPH(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-properties-paragraph")
#define E_HTML_EDITOR_ACTION_CONTEXT_PROPERTIES_RULE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-properties-rule")
#define E_HTML_EDITOR_ACTION_CONTEXT_PROPERTIES_TABLE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-properties-table")
#define E_HTML_EDITOR_ACTION_CONTEXT_PROPERTIES_TEXT(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-properties-text")
#define E_HTML_EDITOR_ACTION_CONTEXT_REMOVE_LINK(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-remove-link")
#define E_HTML_EDITOR_ACTION_CONTEXT_SPELL_ADD(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-spell-add")
#define E_HTML_EDITOR_ACTION_CONTEXT_SPELL_ADD_MENU(editor) \
	E_HTML_EDITOR_ACTION ((editor), "EHTMLEditor::context-spell-add-menu")
#define E_HTML_EDITOR_ACTION_CONTEXT_SPELL_IGNORE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "context-spell-ignore")
#define E_HTML_EDITOR_ACTION_COPY(editor) \
	E_HTML_EDITOR_ACTION ((editor), "copy")
#define E_HTML_EDITOR_ACTION_CUT(editor) \
	E_HTML_EDITOR_ACTION ((editor), "cut")
#define E_HTML_EDITOR_ACTION_EDIT_MENU(editor) \
	E_HTML_EDITOR_ACTION ((editor), "edit-menu")
#define E_HTML_EDITOR_ACTION_FIND_AGAIN(editor) \
	E_HTML_EDITOR_ACTION ((editor), "find-again")
#define E_HTML_EDITOR_ACTION_FONT_SIZE_GROUP(editor) \
	E_HTML_EDITOR_ACTION ((editor), "size-plus-zero")
#define E_HTML_EDITOR_ACTION_FORMAT_MENU(editor) \
	E_HTML_EDITOR_ACTION ((editor), "format-menu")
#define E_HTML_EDITOR_ACTION_FORMAT_TEXT(editor) \
	E_HTML_EDITOR_ACTION ((editor), "format-text")
#define E_HTML_EDITOR_ACTION_INDENT(editor) \
	E_HTML_EDITOR_ACTION ((editor), "indent")
#define E_HTML_EDITOR_ACTION_INSERT_EMOTICON(editor) \
	E_HTML_EDITOR_ACTION ((editor), "insert-emoticon")
#define E_HTML_EDITOR_ACTION_INSERT_EMOJI(editor) \
	E_HTML_EDITOR_ACTION ((editor), "insert-emoji")
#define E_HTML_EDITOR_ACTION_INSERT_IMAGE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "insert-image")
#define E_HTML_EDITOR_ACTION_INSERT_LINK(editor) \
	E_HTML_EDITOR_ACTION ((editor), "insert-link")
#define E_HTML_EDITOR_ACTION_INSERT_MENU(editor) \
	E_HTML_EDITOR_ACTION ((editor), "insert-menu")
#define E_HTML_EDITOR_ACTION_INSERT_RULE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "insert-rule")
#define E_HTML_EDITOR_ACTION_INSERT_TABLE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "insert-table")
#define E_HTML_EDITOR_ACTION_ITALIC(editor) \
	E_HTML_EDITOR_ACTION ((editor), "italic")
#define E_HTML_EDITOR_ACTION_JUSTIFY_CENTER(editor) \
	E_HTML_EDITOR_ACTION ((editor), "justify-center")
#define E_HTML_EDITOR_ACTION_JUSTIFY_FILL(editor) \
	E_HTML_EDITOR_ACTION ((editor), "justify-fill")
#define E_HTML_EDITOR_ACTION_JUSTIFY_LEFT(editor) \
	E_HTML_EDITOR_ACTION ((editor), "justify-left")
#define E_HTML_EDITOR_ACTION_JUSTIFY_RIGHT(editor) \
	E_HTML_EDITOR_ACTION ((editor), "justify-right")
#define E_HTML_EDITOR_ACTION_LANGUAGE_MENU(editor) \
	E_HTML_EDITOR_ACTION ((editor), "language-menu")
#define E_HTML_EDITOR_ACTION_MODE_HTML(editor) \
	E_HTML_EDITOR_ACTION ((editor), "mode-html")
#define E_HTML_EDITOR_ACTION_MODE_PLAIN(editor) \
	E_HTML_EDITOR_ACTION ((editor), "mode-plain")
#define E_HTML_EDITOR_ACTION_PASTE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "paste")
#define E_HTML_EDITOR_ACTION_PASTE_QUOTE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "paste-quote")
#define E_HTML_EDITOR_ACTION_PROPERTIES_RULE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "properties-rule")
#define E_HTML_EDITOR_ACTION_PROPERTIES_TABLE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "properties-table")
#define E_HTML_EDITOR_ACTION_REDO(editor) \
	E_HTML_EDITOR_ACTION ((editor), "redo")
#define E_HTML_EDITOR_ACTION_SELECT_ALL(editor) \
	E_HTML_EDITOR_ACTION ((editor), "select-all")
#define E_HTML_EDITOR_ACTION_SHOW_FIND(editor) \
	E_HTML_EDITOR_ACTION ((editor), "show-find")
#define E_HTML_EDITOR_ACTION_SHOW_REPLACE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "show-replace")
#define E_HTML_EDITOR_ACTION_SIZE_PLUS_ZERO(editor) \
	E_HTML_EDITOR_ACTION ((editor), "size-plus-zero")
#define E_HTML_EDITOR_ACTION_SPELL_CHECK(editor) \
	E_HTML_EDITOR_ACTION ((editor), "spell-check")
#define E_HTML_EDITOR_ACTION_STRIKETHROUGH(editor) \
	E_HTML_EDITOR_ACTION ((editor), "strikethrough")
#define E_HTML_EDITOR_ACTION_STYLE_ADDRESS(editor) \
	E_HTML_EDITOR_ACTION ((editor), "style-address")
#define E_HTML_EDITOR_ACTION_STYLE_H1(editor) \
	E_HTML_EDITOR_ACTION ((editor), "style-h1")
#define E_HTML_EDITOR_ACTION_STYLE_H2(editor) \
	E_HTML_EDITOR_ACTION ((editor), "style-h2")
#define E_HTML_EDITOR_ACTION_STYLE_H3(editor) \
	E_HTML_EDITOR_ACTION ((editor), "style-h3")
#define E_HTML_EDITOR_ACTION_STYLE_H4(editor) \
	E_HTML_EDITOR_ACTION ((editor), "style-h4")
#define E_HTML_EDITOR_ACTION_STYLE_H5(editor) \
	E_HTML_EDITOR_ACTION ((editor), "style-h5")
#define E_HTML_EDITOR_ACTION_STYLE_H6(editor) \
	E_HTML_EDITOR_ACTION ((editor), "style-h6")
#define E_HTML_EDITOR_ACTION_STYLE_NORMAL(editor) \
	E_HTML_EDITOR_ACTION ((editor), "style-normal")
#define E_HTML_EDITOR_ACTION_SUBSCRIPT(editor) \
	E_HTML_EDITOR_ACTION ((editor), "subscript")
#define E_HTML_EDITOR_ACTION_SUPERSCRIPT(editor) \
	E_HTML_EDITOR_ACTION ((editor), "superscript")
#define E_HTML_EDITOR_ACTION_TEST_URL(editor) \
	E_HTML_EDITOR_ACTION ((editor), "test-url")
#define E_HTML_EDITOR_ACTION_UNDERLINE(editor) \
	E_HTML_EDITOR_ACTION ((editor), "underline")
#define E_HTML_EDITOR_ACTION_UNDO(editor) \
	E_HTML_EDITOR_ACTION ((editor), "undo")
#define E_HTML_EDITOR_ACTION_UNINDENT(editor) \
	E_HTML_EDITOR_ACTION ((editor), "unindent")
#define E_HTML_EDITOR_ACTION_WRAP_LINES(editor) \
	E_HTML_EDITOR_ACTION ((editor), "wrap-lines")
#define E_HTML_EDITOR_ACTION_ZOOM_MENU(composer) \
	E_HTML_EDITOR_ACTION ((composer), "zoom-menu")
#define E_HTML_EDITOR_ACTION_ZOOM_IN(composer) \
	E_HTML_EDITOR_ACTION ((composer), "zoom-in")
#define E_HTML_EDITOR_ACTION_ZOOM_OUT(composer) \
	E_HTML_EDITOR_ACTION ((composer), "zoom-out")
#define E_HTML_EDITOR_ACTION_ZOOM_100(composer) \
	E_HTML_EDITOR_ACTION ((composer), "zoom-100")

#endif /* E_HTML_EDITOR_ACTIONS_H */
