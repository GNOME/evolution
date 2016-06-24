/*
 * e-html-editor-undo-redo-manager.h
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

#ifndef E_HTML_EDITOR_UNDO_REDO_MANAGER_H
#define E_HTML_EDITOR_UNDO_REDO_MANAGER_H

#include <glib-object.h>
#include <webkitdom/webkitdom.h>

#include "e-html-editor-history-event.h"

#define E_TYPE_HTML_EDITOR_UNDO_REDO_MANAGER \
	(e_html_editor_undo_redo_manager_get_type ())
#define E_HTML_EDITOR_UNDO_REDO_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_HTML_EDITOR_UNDO_REDO_MANAGER, EHTMLEditorUndoRedoManager))
#define E_HTML_EDITOR_UNDO_REDO_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_HTML_EDITOR_UNDO_REDO_MANAGER, EHTMLEditorUndoRedoManagerClass))
#define E_IS_HTML_EDITOR_UNDO_REDO_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_HTML_EDITOR_UNDO_REDO_MANAGER))
#define E_IS_HTML_EDITOR_UNDO_REDO_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_HTML_EDITOR_UNDO_REDO_MANAGER))
#define E_HTML_EDITOR_UNDO_REDO_MANAGER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_HTML_EDITOR_UNDO_REDO_MANAGER, EHTMLEditorUndoRedoManagerClass))

G_BEGIN_DECLS

struct _EHTMLEditorWebExtension;

typedef struct _EHTMLEditorUndoRedoManager EHTMLEditorUndoRedoManager;
typedef struct _EHTMLEditorUndoRedoManagerClass EHTMLEditorUndoRedoManagerClass;
typedef struct _EHTMLEditorUndoRedoManagerPrivate EHTMLEditorUndoRedoManagerPrivate;

struct _EHTMLEditorUndoRedoManager {
	GObject parent;
	EHTMLEditorUndoRedoManagerPrivate *priv;
};

struct _EHTMLEditorUndoRedoManagerClass
{
	GObjectClass parent_class;
};

GType		e_html_editor_undo_redo_manager_get_type
						(void) G_GNUC_CONST;

EHTMLEditorUndoRedoManager *
		e_html_editor_undo_redo_manager_new
						(struct _EHTMLEditorWebExtension *extension);
void		e_html_editor_undo_redo_manager_set_document
						(EHTMLEditorUndoRedoManager *manager,
						 WebKitDOMDocument *document);

gboolean	e_html_editor_undo_redo_manager_is_operation_in_progress
						(EHTMLEditorUndoRedoManager *manager);

void		e_html_editor_undo_redo_manager_set_operation_in_progress
						(EHTMLEditorUndoRedoManager *manager,
						 gboolean value);

void		e_html_editor_undo_redo_manager_insert_history_event
						(EHTMLEditorUndoRedoManager *manager,
						 EHTMLEditorHistoryEvent *event);

EHTMLEditorHistoryEvent *
		e_html_editor_undo_redo_manager_get_current_history_event
						(EHTMLEditorUndoRedoManager *manager);
void		e_html_editor_undo_redo_manager_remove_current_history_event
						(EHTMLEditorUndoRedoManager *manager);

void
e_html_editor_undo_redo_manager_insert_dash_history_event
						(EHTMLEditorUndoRedoManager *manager);

gboolean	e_html_editor_undo_redo_manager_can_undo
						(EHTMLEditorUndoRedoManager *manager);

void		e_html_editor_undo_redo_manager_undo
						(EHTMLEditorUndoRedoManager *manager);

gboolean	e_html_editor_undo_redo_manager_can_redo
						(EHTMLEditorUndoRedoManager *manager);

void		e_html_editor_undo_redo_manager_redo
						(EHTMLEditorUndoRedoManager *manager);

void		e_html_editor_undo_redo_manager_clean_history
						(EHTMLEditorUndoRedoManager *manager);

G_END_DECLS

#endif /* E_HTML_EDITOR_UNDO_REDO_MANAGER_H */
