/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_COMP_EDITOR_TASK_H
#define E_COMP_EDITOR_TASK_H

#include <calendar/gui/e-comp-editor.h>

/* Standard GObject macros */

#define E_TYPE_COMP_EDITOR_TASK \
	(e_comp_editor_task_get_type ())
#define E_COMP_EDITOR_TASK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_TASK, ECompEditorTask))
#define E_COMP_EDITOR_TASK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_TASK, ECompEditorTaskClass))
#define E_IS_COMP_EDITOR_TASK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_TASK))
#define E_IS_COMP_EDITOR_TASK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_TASK))
#define E_COMP_EDITOR_TASK_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_TASK, ECompEditorTaskClass))

G_BEGIN_DECLS

typedef struct _ECompEditorTask ECompEditorTask;
typedef struct _ECompEditorTaskClass ECompEditorTaskClass;
typedef struct _ECompEditorTaskPrivate ECompEditorTaskPrivate;

struct _ECompEditorTask {
	ECompEditor parent;

	ECompEditorTaskPrivate *priv;
};

struct _ECompEditorTaskClass {
	ECompEditorClass parent_class;
};

GType		e_comp_editor_task_get_type	(void) G_GNUC_CONST;

G_END_DECLS

#endif /* E_COMP_EDITOR_TASK_H */
