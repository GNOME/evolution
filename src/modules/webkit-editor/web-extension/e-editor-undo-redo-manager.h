/*
 * e-editor-undo-redo-manager.h
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

#ifndef E_EDITOR_UNDO_REDO_MANAGER_H
#define E_EDITOR_UNDO_REDO_MANAGER_H

#include <glib-object.h>
#include <webkitdom/webkitdom.h>

#define E_TYPE_EDITOR_UNDO_REDO_MANAGER \
	(e_editor_undo_redo_manager_get_type ())
#define E_EDITOR_UNDO_REDO_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EDITOR_UNDO_REDO_MANAGER, EEditorUndoRedoManager))
#define E_EDITOR_UNDO_REDO_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EDITOR_UNDO_REDO_MANAGER, EEditorUndoRedoManagerClass))
#define E_IS_EDITOR_UNDO_REDO_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EDITOR_UNDO_REDO_MANAGER))
#define E_IS_EDITOR_UNDO_REDO_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EDITOR_UNDO_REDO_MANAGER))
#define E_EDITOR_UNDO_REDO_MANAGER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EDITOR_UNDO_REDO_MANAGER, EEditorUndoRedoManagerClass))

G_BEGIN_DECLS

struct _EEditorPage;

enum EEditorHistoryEventType {
	HISTORY_ALIGNMENT,
	HISTORY_AND,
	HISTORY_BLOCK_FORMAT,
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
	HISTORY_LINK_DIALOG,
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
} EEditorStyleChange;

/* This is used for e-html-editor-*-dialogs */
typedef struct {
	WebKitDOMNode *from; /* From what node we are changing. */
	WebKitDOMNode *to; /* To what node we are changing. */
} EEditorDOMChange;

typedef struct {
	gchar *from; /* From what format we are changing. */
	gchar *to; /* To what format we are changing. */
} EEditorStringChange;

typedef struct {
	guint x;
	guint y;
} EEditorSelectionPoint;

typedef struct {
	EEditorSelectionPoint start;
	EEditorSelectionPoint end;
} EEditorSelection;

typedef struct {
	enum EEditorHistoryEventType type;
	EEditorSelection before;
	EEditorSelection after;
	union {
		WebKitDOMDocumentFragment *fragment;
		EEditorStyleChange style;
		EEditorStringChange string;
		EEditorDOMChange dom;
	} data;
} EEditorHistoryEvent;

typedef struct _EEditorUndoRedoManager EEditorUndoRedoManager;
typedef struct _EEditorUndoRedoManagerClass EEditorUndoRedoManagerClass;
typedef struct _EEditorUndoRedoManagerPrivate EEditorUndoRedoManagerPrivate;

struct _EEditorUndoRedoManager {
	GObject parent;
	EEditorUndoRedoManagerPrivate *priv;
};

struct _EEditorUndoRedoManagerClass
{
	GObjectClass parent_class;
};

GType		e_editor_undo_redo_manager_get_type
						(void) G_GNUC_CONST;

EEditorUndoRedoManager *
		e_editor_undo_redo_manager_new	(struct _EEditorPage *editor_page);
gboolean	e_editor_undo_redo_manager_is_operation_in_progress
						(EEditorUndoRedoManager *manager);

void		e_editor_undo_redo_manager_set_operation_in_progress
						(EEditorUndoRedoManager *manager,
						 gboolean value);

void		e_editor_undo_redo_manager_insert_history_event
						(EEditorUndoRedoManager *manager,
						 EEditorHistoryEvent *event);

EEditorHistoryEvent *
		e_editor_undo_redo_manager_get_current_history_event
						(EEditorUndoRedoManager *manager);

EEditorHistoryEvent *
		e_editor_undo_redo_manager_get_next_history_event_for
						(EEditorUndoRedoManager *manager,
						 EEditorHistoryEvent *event);

void		e_editor_undo_redo_manager_remove_current_history_event
						(EEditorUndoRedoManager *manager);

void		e_editor_undo_redo_manager_insert_dash_history_event
						(EEditorUndoRedoManager *manager);

void		e_editor_undo_redo_manager_last_drop_operation_did_copy
						(EEditorUndoRedoManager *manager);

gboolean	e_editor_undo_redo_manager_can_undo
						(EEditorUndoRedoManager *manager);

void		e_editor_undo_redo_manager_undo	(EEditorUndoRedoManager *manager);

gboolean	e_editor_undo_redo_manager_can_redo
						(EEditorUndoRedoManager *manager);

void		e_editor_undo_redo_manager_redo	(EEditorUndoRedoManager *manager);

void		e_editor_undo_redo_manager_clean_history
						(EEditorUndoRedoManager *manager);

G_END_DECLS

#endif /* E_EDITOR_UNDO_REDO_MANAGER_H */
