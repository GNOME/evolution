/* Evolution calendar - Task editor dialog
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __TASK_EDITOR_H__
#define __TASK_EDITOR_H__

#include <gtk/gtkobject.h>
#include "comp-editor.h"



#define TYPE_TASK_EDITOR            (task_editor_get_type ())
#define TASK_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_TASK_EDITOR, TaskEditor))
#define TASK_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_TASK_EDITOR,	\
				      TaskEditorClass))
#define IS_TASK_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_TASK_EDITOR))
#define IS_TASK_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_TASK_EDITOR))

typedef struct _TaskEditor TaskEditor;
typedef struct _TaskEditorClass TaskEditorClass;
typedef struct _TaskEditorPrivate TaskEditorPrivate;

struct _TaskEditor {
	CompEditor parent;

	/* Private data */
	TaskEditorPrivate *priv;
};

struct _TaskEditorClass {
	CompEditorClass parent_class;
};

GtkType     task_editor_get_type       (void);
TaskEditor *task_editor_construct      (TaskEditor *te,
					ECal  *client, gboolean is_assigned);
TaskEditor *task_editor_new            (ECal  *client, gboolean is_assigned);
void        task_editor_show_assignment(TaskEditor *te);


#endif /* __TASK_EDITOR_H__ */
