/*
 * Evolution calendar - Task editor dialog
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Miguel de Icaza <miguel@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __TASK_EDITOR_H__
#define __TASK_EDITOR_H__

#include <gtk/gtk.h>
#include "comp-editor.h"

/* Standard GObject macros */
#define TYPE_TASK_EDITOR \
	(task_editor_get_type ())
#define TASK_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), TYPE_TASK_EDITOR, TaskEditor))
#define TASK_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), TYPE_TASK_EDITOR, TaskEditorClass))
#define IS_TASK_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), TYPE_TASK_EDITOR))
#define IS_TASK_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), TYPE_TASK_EDITOR))
#define TASK_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), TYPE_TASK_EDITOR, TaskEditorClass))

G_BEGIN_DECLS

typedef struct _TaskEditor TaskEditor;
typedef struct _TaskEditorClass TaskEditorClass;
typedef struct _TaskEditorPrivate TaskEditorPrivate;

struct _TaskEditor {
	CompEditor parent;
	TaskEditorPrivate *priv;
};

struct _TaskEditorClass {
	CompEditorClass parent_class;
};

GType		task_editor_get_type		(void);
CompEditor *	task_editor_new			(ECalClient *client,
						 EShell *shell,
						 CompEditorFlags flags);
void		task_editor_show_assignment	(TaskEditor *te);

G_END_DECLS

#endif /* __TASK_EDITOR_H__ */
