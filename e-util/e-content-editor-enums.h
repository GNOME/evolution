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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CONTENT_EDITOR_ENUMS_H
#define E_CONTENT_EDITOR_ENUMS_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	E_CONTENT_EDITOR_INSERT_CONVERT = 1 << 0,
	E_CONTENT_EDITOR_INSERT_QUOTE_CONTENT = 1 << 1,
	E_CONTENT_EDITOR_INSERT_REPLACE_ALL = 1 << 2,
	E_CONTENT_EDITOR_INSERT_TEXT_HTML = 1 << 3,
	E_CONTENT_EDITOR_INSERT_TEXT_PLAIN = 1 << 4,
} EContentEditorInsertContentFlags;

typedef enum {
	E_CONTENT_EDITOR_GET_BODY = 1 << 0,
	E_CONTENT_EDITOR_GET_INLINE_IMAGES = 1 << 1,
	E_CONTENT_EDITOR_GET_PROCESSED = 1 << 2, /* raw or processed */
	E_CONTENT_EDITOR_GET_TEXT_HTML = 1 << 3,
	E_CONTENT_EDITOR_GET_TEXT_PLAIN = 1 << 4,
	E_CONTENT_EDITOR_GET_EXCLUDE_SIGNATURE = 1 << 5
} EContentEditorGetContentFlags;

typedef enum {
	E_CONTENT_EDITOR_MESSAGE_DRAFT = 1 << 0,
	E_CONTENT_EDITOR_MESSAGE_EDIT_AS_NEW = 1 << 1,
	E_CONTENT_EDITOR_MESSAGE_EDITTING = 1 << 2,
	E_CONTENT_EDITOR_MESSAGE_FROM_SELECTION = 1 << 3,
	E_CONTENT_EDITOR_MESSAGE_NEW = 1 << 4
} EContentEditorContentFlags;

typedef enum {
	E_CONTENT_EDITOR_NODE_IS_ANCHOR = 1 << 0,
	E_CONTENT_EDITOR_NODE_IS_H_RULE = 1 << 1,
	E_CONTENT_EDITOR_NODE_IS_IMAGE = 1 << 2,
	E_CONTENT_EDITOR_NODE_IS_TABLE = 1 << 3,
	E_CONTENT_EDITOR_NODE_IS_TABLE_CELL = 1 << 4,
	E_CONTENT_EDITOR_NODE_IS_TEXT = 1 << 5,
	E_CONTENT_EDITOR_NODE_IS_TEXT_COLLAPSED = 1 << 6,
} EContentEditorNodeFlags;

typedef enum {
	E_CONTENT_EDITOR_BLOCK_FORMAT_NONE = 0,
	E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH,
	E_CONTENT_EDITOR_BLOCK_FORMAT_PRE,
	E_CONTENT_EDITOR_BLOCK_FORMAT_ADDRESS,
	E_CONTENT_EDITOR_BLOCK_FORMAT_BLOCKQUOTE,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H1,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H2,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H3,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H4,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H5,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H6,
	E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST,
	E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST,
	E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ROMAN,
	E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ALPHA
} EContentEditorBlockFormat;

/* The values match the actual size in <font size="n"> */
typedef enum {
	E_CONTENT_EDITOR_FONT_SIZE_TINY		= 1,
	E_CONTENT_EDITOR_FONT_SIZE_SMALL	= 2,
	E_CONTENT_EDITOR_FONT_SIZE_NORMAL	= 3,
	E_CONTENT_EDITOR_FONT_SIZE_BIG		= 4,
	E_CONTENT_EDITOR_FONT_SIZE_BIGGER	= 5,
	E_CONTENT_EDITOR_FONT_SIZE_LARGE	= 6,
	E_CONTENT_EDITOR_FONT_SIZE_VERY_LARGE	= 7
} EContentEditorFontSize;

typedef enum {
	E_CONTENT_EDITOR_ALIGNMENT_LEFT = 0,
	E_CONTENT_EDITOR_ALIGNMENT_CENTER,
	E_CONTENT_EDITOR_ALIGNMENT_RIGHT
} EContentEditorAlignment;

typedef enum {
	E_CONTENT_EDITOR_GRANULARITY_CHARACTER = 0,
	E_CONTENT_EDITOR_GRANULARITY_WORD
} EContentEditorGranularity;

/**
 * EContentEditorCommand:
 * @E_CONTENT_EDITOR_COMMAND_BACKGROUND_COLOR:
 *   Sets background color to given value.
 * @E_CONTENT_EDITOR_COMMAND_BOLD:
 *   Toggles bold formatting of current selection.
 * @E_CONTENT_EDITOR_COMMAND_COPY:
 *   Copies current selection to clipboard.
 * @E_CONTENT_EDITOR_COMMAND_CREATE_LINK:
 *   Converts current selection to a link that points to URL in value
 * @E_CONTENT_EDITOR_COMMAND_CUT:
 *   Cuts current selection to clipboard.
 * @E_CONTENT_EDITOR_COMMAND_DEFAULT_PARAGRAPH_SEPARATOR:
 *   (XXX Explain me!)
 * @E_CONTENT_EDITOR_COMMAND_DELETE:
 *   Deletes current selection.
 * @E_CONTENT_EDITOR_COMMAND_FIND_STRING:
 *   Highlights given string.
 * @E_CONTENT_EDITOR_COMMAND_FONT_NAME:
 *   Sets font name to given value.
 * @E_CONTENT_EDITOR_COMMAND_FONT_SIZE:
 *   Sets font point size to given value (no units, just number)
 * @E_CONTENT_EDITOR_COMMAND_FONT_SIZE_DELTA:
 *   Changes font size by given delta value (no units, just number)
 * @E_CONTENT_EDITOR_COMMAND_FORE_COLOR:
 *   Sets font color to given value
 * @E_CONTENT_EDITOR_COMMAND_FORMAT_BLOCK:
 *   Sets block type of current paragraph to given format. Allowed formats
 *   are "BLOCKQUOTE", "H1", "H2", "H3", "H4", "H5", "H6", "P", "PRE" and
 *   "ADDRESS".
 * @E_CONTENT_EDITOR_COMMAND_FORWARD_DELETE:
 *   (XXX Explain me!)
 * @E_CONTENT_EDITOR_COMMAND_HILITE_COLOR:
 *   Sets color in which results of "FindString" command should be
 *   highlighted to given value.
 * @E_CONTENT_EDITOR_COMMAND_INDENT:
 *   Indents current paragraph by one level.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_HTML:
 *   Inserts give HTML code into document.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_HORIZONTAL_RULE:
 *   Inserts a horizontal rule (&lt;HR&gt;) on current line.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_IMAGE:
 *   Inserts an image with given source file.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_LINE_BREAK:
 *   Breaks line at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT:
 *   Breaks citation at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_ORDERED_LIST:
 *   Creates an ordered list environment at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_PARAGRAPH:
 *   Inserts a new paragraph at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_TEXT:
 *   Inserts given text at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_UNORDERED_LIST:
 *   Creates an undordered list environment at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_ITALIC:
 *   Toggles italic formatting of current selection.
 * @E_CONTENT_EDITOR_COMMAND_JUSTIFY_CENTER:
 *   Aligns current paragraph to center.
 * @E_CONTENT_EDITOR_COMMAND_JUSTIFY_FULL:
 *   Justifies current paragraph to block.
 * @E_CONTENT_EDITOR_COMMAND_JUSTIFY_NONE:
 *   Removes any justification or alignment of current paragraph.
 * @E_CONTENT_EDITOR_COMMAND_JUSTIFY_RIGHT:
 *   Aligns current paragraph to right.
 * @E_CONTENT_EDITOR_COMMAND_OUTDENT:
 *   Outdents current paragraph by one level.
 * @E_CONTENT_EDITOR_COMMAND_PASTE:
 *   Pastes clipboard content at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_PASTE_AND_MATCH_STYLE:
 *   Pastes clipboard content and matches its style to style at current
 *   cursor position.
 * @E_CONTENT_EDITOR_COMMAND_PASTE_AS_PLAIN_TEXT:
 *   Pastes clipboard content at current cursor position removing any HTML
 *   formatting.
 * @E_CONTENT_EDITOR_COMMAND_PRINT:
 *   Print current document.
 * @E_CONTENT_EDITOR_COMMAND_REDO:
 *   Redoes last action.
 * @E_CONTENT_EDITOR_COMMAND_REMOVE_FORMAT:
 *   Removes any formatting of current selection.
 * @E_CONTENT_EDITOR_COMMAND_SELECT_ALL:
 *   Extends selects to the entire document.
 * @E_CONTENT_EDITOR_COMMAND_STRIKETHROUGH:
 *   Toggles strikethrough formatting.
 * @E_CONTENT_EDITOR_COMMAND_STYLE_WITH_CSS:
 *   Toggles whether style should be defined in CSS "style" attribute of
 *   elements or whether to use deprecated &lt;FONT&gt; tags. Depends on
 *   whether given value is "true" or "false".
 * @E_CONTENT_EDITOR_COMMAND_SUBSCRIPT:
 *   Toggles subscript of current selection.
 * @E_CONTENT_EDITOR_COMMAND_SUPERSCRIPT:
 *   Toggles superscript of current selection.
 * @E_CONTENT_EDITOR_COMMAND_TRANSPOSE:
 *   (XXX Explain me!)
 * @E_CONTENT_EDITOR_COMMAND_UNDERLINE:
 *   Toggles underline formatting of current selection.
 * @E_CONTENT_EDITOR_COMMAND_UNDO:
 *   Undoes last action.
 * @E_CONTENT_EDITOR_COMMAND_UNLINK:
 *   Removes active links (&lt;A&gt;) from current selection (if there's any).
 * @E_CONTENT_EDITOR_COMMAND_UNSELECT:
 *   Cancels current selection.
 * @E_CONTENT_EDITOR_COMMAND_USE_CSS:
 *   Whether to allow use of CSS or not depending on whether given value is
 *   "true" or "false".
 *
 * Specifies the DOM command to execute in e_editor_widget_exec_command().
 * Some commands require value to be passed in, which is always stated in the
 * documentation.
 */
typedef enum {
	E_CONTENT_EDITOR_COMMAND_BACKGROUND_COLOR,
	E_CONTENT_EDITOR_COMMAND_BOLD,
	E_CONTENT_EDITOR_COMMAND_COPY,
	E_CONTENT_EDITOR_COMMAND_CREATE_LINK,
	E_CONTENT_EDITOR_COMMAND_CUT,
	E_CONTENT_EDITOR_COMMAND_DEFAULT_PARAGRAPH_SEPARATOR,
	E_CONTENT_EDITOR_COMMAND_DELETE,
	E_CONTENT_EDITOR_COMMAND_FIND_STRING,
	E_CONTENT_EDITOR_COMMAND_FONT_NAME,
	E_CONTENT_EDITOR_COMMAND_FONT_SIZE,
	E_CONTENT_EDITOR_COMMAND_FONT_SIZE_DELTA,
	E_CONTENT_EDITOR_COMMAND_FORE_COLOR,
	E_CONTENT_EDITOR_COMMAND_FORMAT_BLOCK,
	E_CONTENT_EDITOR_COMMAND_FORWARD_DELETE,
	E_CONTENT_EDITOR_COMMAND_HILITE_COLOR,
	E_CONTENT_EDITOR_COMMAND_INDENT,
	E_CONTENT_EDITOR_COMMAND_INSERT_HTML,
	E_CONTENT_EDITOR_COMMAND_INSERT_HORIZONTAL_RULE,
	E_CONTENT_EDITOR_COMMAND_INSERT_IMAGE,
	E_CONTENT_EDITOR_COMMAND_INSERT_LINE_BREAK,
	E_CONTENT_EDITOR_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT,
	E_CONTENT_EDITOR_COMMAND_INSERT_ORDERED_LIST,
	E_CONTENT_EDITOR_COMMAND_INSERT_PARAGRAPH,
	E_CONTENT_EDITOR_COMMAND_INSERT_TEXT,
	E_CONTENT_EDITOR_COMMAND_INSERT_UNORDERED_LIST,
	E_CONTENT_EDITOR_COMMAND_ITALIC,
	E_CONTENT_EDITOR_COMMAND_JUSTIFY_CENTER,
	E_CONTENT_EDITOR_COMMAND_JUSTIFY_FULL,
	E_CONTENT_EDITOR_COMMAND_JUSTIFY_LEFT,
	E_CONTENT_EDITOR_COMMAND_JUSTIFY_NONE,
	E_CONTENT_EDITOR_COMMAND_JUSTIFY_RIGHT,
	E_CONTENT_EDITOR_COMMAND_OUTDENT,
	E_CONTENT_EDITOR_COMMAND_PASTE,
	E_CONTENT_EDITOR_COMMAND_PASTE_AND_MATCH_STYLE,
	E_CONTENT_EDITOR_COMMAND_PASTE_AS_PLAIN_TEXT,
	E_CONTENT_EDITOR_COMMAND_PRINT,
	E_CONTENT_EDITOR_COMMAND_REDO,
	E_CONTENT_EDITOR_COMMAND_REMOVE_FORMAT,
	E_CONTENT_EDITOR_COMMAND_SELECT_ALL,
	E_CONTENT_EDITOR_COMMAND_STRIKETHROUGH,
	E_CONTENT_EDITOR_COMMAND_STYLE_WITH_CSS,
	E_CONTENT_EDITOR_COMMAND_SUBSCRIPT,
	E_CONTENT_EDITOR_COMMAND_SUPERSCRIPT,
	E_CONTENT_EDITOR_COMMAND_TRANSPOSE,
	E_CONTENT_EDITOR_COMMAND_UNDERLINE,
	E_CONTENT_EDITOR_COMMAND_UNDO,
	E_CONTENT_EDITOR_COMMAND_UNLINK,
	E_CONTENT_EDITOR_COMMAND_UNSELECT,
	E_CONTENT_EDITOR_COMMAND_USE_CSS
} EContentEditorCommand;

typedef enum {
	E_CONTENT_EDITOR_SCOPE_CELL = 0,
	E_CONTENT_EDITOR_SCOPE_ROW,
	E_CONTENT_EDITOR_SCOPE_COLUMN,
	E_CONTENT_EDITOR_SCOPE_TABLE
} EContentEditorScope;

typedef enum {
	E_CONTENT_EDITOR_UNIT_AUTO = 0,
	E_CONTENT_EDITOR_UNIT_PIXEL,
	E_CONTENT_EDITOR_UNIT_PERCENTAGE
} EContentEditorUnit;

G_END_DECLS

#endif /* E_CONTENT_EDITOR_ENUMS_H */
