/*
 * e-task-shell-content.h
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_TASK_SHELL_CONTENT_H
#define E_TASK_SHELL_CONTENT_H

#include <shell/e-shell-content.h>
#include <shell/e-shell-searchbar.h>
#include <shell/e-shell-view.h>

#include <calendar/gui/e-cal-model.h>
#include <calendar/gui/e-task-table.h>

#include <menus/gal-view-instance.h>
#include <misc/e-preview-pane.h>

/* Standard GObject macros */
#define E_TYPE_TASK_SHELL_CONTENT \
	(e_task_shell_content_get_type ())
#define E_TASK_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TASK_SHELL_CONTENT, ETaskShellContent))
#define E_TASK_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TASK_SHELL_CONTENT, ETaskShellContentClass))
#define E_IS_TASK_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TASK_SHELL_CONTENT))
#define E_IS_TASK_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TASK_SHELL_CONTENT))
#define E_TASK_SHELL_CONTENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TASK_SHELL_CONTENT, ETaskShellContentClass))

G_BEGIN_DECLS

typedef struct _ETaskShellContent ETaskShellContent;
typedef struct _ETaskShellContentClass ETaskShellContentClass;
typedef struct _ETaskShellContentPrivate ETaskShellContentPrivate;

enum {
	E_TASK_SHELL_CONTENT_SELECTION_SINGLE		= 1 << 0,
	E_TASK_SHELL_CONTENT_SELECTION_MULTIPLE		= 1 << 1,
	E_TASK_SHELL_CONTENT_SELECTION_CAN_ASSIGN	= 1 << 2,
	E_TASK_SHELL_CONTENT_SELECTION_CAN_EDIT		= 1 << 3,
	E_TASK_SHELL_CONTENT_SELECTION_HAS_COMPLETE	= 1 << 4,
	E_TASK_SHELL_CONTENT_SELECTION_HAS_INCOMPLETE	= 1 << 5,
	E_TASK_SHELL_CONTENT_SELECTION_HAS_URL		= 1 << 6
};

struct _ETaskShellContent {
	EShellContent parent;
	ETaskShellContentPrivate *priv;
};

struct _ETaskShellContentClass {
	EShellContentClass parent_class;
};

GType		e_task_shell_content_get_type	(void);
void		e_task_shell_content_register_type
					(GTypeModule *type_module);
GtkWidget *	e_task_shell_content_new
					(EShellView *shell_view);
ECalModel *	e_task_shell_content_get_task_model
					(ETaskShellContent *task_shell_content);
ETaskTable *	e_task_shell_content_get_task_table
					(ETaskShellContent *task_shell_content);
EPreviewPane *	e_task_shell_content_get_preview_pane
					(ETaskShellContent *task_shell_content);
gboolean	e_task_shell_content_get_preview_visible
					(ETaskShellContent *task_shell_content);
void		e_task_shell_content_set_preview_visible
					(ETaskShellContent *task_shell_content,
					 gboolean preview_visible);
EShellSearchbar *
		e_task_shell_content_get_searchbar
					(ETaskShellContent *task_shell_content);
GalViewInstance *
		e_task_shell_content_get_view_instance
					(ETaskShellContent *task_shell_content);

G_END_DECLS

#endif /* E_TASK_SHELL_CONTENT_H */
