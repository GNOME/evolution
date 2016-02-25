/*
 * e-html-editor-history-event.h
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

#ifndef E_HTML_EDITOR_HISTORY_EVENT_H
#define E_HTML_EDITOR_HISTORY_EVENT_H

#include "config.h"

G_BEGIN_DECLS

enum EHTMLEditorHistoryEventType {
	HISTORY_ALIGNMENT,
	HISTORY_AND,
	HISTORY_BLOCK_FORMAT,
	HISTORY_BLOCKQUOTE,
	HISTORY_BOLD,
	HISTORY_CELL_DIALOG,
	HISTORY_DELETE, /* BackSpace, Delete, with and without selection */
	HISTORY_FONT_COLOR,
	HISTORY_FONT_SIZE,
	HISTORY_HRULE_DIALOG,
	HISTORY_INDENT,
	HISTORY_INPUT,
	HISTORY_IMAGE,
	HISTORY_IMAGE_DIALOG,
	HISTORY_INSERT_HTML,
	HISTORY_ITALIC,
	HISTORY_MONOSPACE,
	HISTORY_PAGE_DIALOG,
	HISTORY_PASTE,
	HISTORY_PASTE_AS_TEXT,
	HISTORY_PASTE_QUOTED,
	HISTORY_REMOVE_LINK,
	HISTORY_REPLACE,
	HISTORY_REPLACE_ALL,
	HISTORY_CITATION_SPLIT,
	HISTORY_SMILEY,
	HISTORY_START, /* Start of history */
	HISTORY_STRIKETHROUGH,
	HISTORY_TABLE_DIALOG,
	HISTORY_TABLE_INPUT,
	HISTORY_UNDERLINE,
	HISTORY_WRAP,
	HISTORY_UNQUOTE
};

typedef struct {
	gint from; /* From what format we are changing. */
	gint to; /* To what format we are changing. */
} EHTMLEditorStyleChange;

/* This is used for e-html-editor-*-dialogs */
typedef struct {
	WebKitDOMNode *from; /* From what node we are changing. */
	WebKitDOMNode *to; /* To what node we are changing. */
} EHTMLEditorDOMChange;

typedef struct {
	gchar *from; /* From what format we are changing. */
	gchar *to; /* To what format we are changing. */
} EHTMLEditorStringChange;

typedef struct {
	guint x;
	guint y;
} EHTMLEditorSelectionPoint;

typedef struct {
	EHTMLEditorSelectionPoint start;
	EHTMLEditorSelectionPoint end;
} EHTMLEditorSelection;

typedef struct {
	enum EHTMLEditorHistoryEventType type;
	EHTMLEditorSelection before;
	EHTMLEditorSelection after;
	union {
		WebKitDOMDocumentFragment *fragment;
		EHTMLEditorStyleChange style;
		EHTMLEditorStringChange string;
		EHTMLEditorDOMChange dom;
	} data;
} EHTMLEditorHistoryEvent;

G_END_DECLS

#endif /* E_HTML_EDITOR_HISTORY_EVENT_H */
