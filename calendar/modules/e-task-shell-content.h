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
#include <shell/e-shell-view.h>

#include <widgets/menus/gal-view-instance.h>

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

struct _ETaskShellContent {
	EShellContent parent;
	ETaskShellContentPrivate *priv;
};

struct _ETaskShellContentClass {
	EShellContentClass parent_class;
};

GType		e_task_shell_content_get_type	(void);
GtkWidget *	e_task_shell_content_new	(EShellView *shell_view);
GalViewInstance *
		e_task_shell_content_get_view_instance
						(ETaskShellContent *task_shell_content);
gboolean	e_task_shell_content_get_preview_visible
						(ETaskShellContent *task_shell_content);
void		e_task_shell_content_set_preview_visible
						(ETaskShellContent *task_shell_content,
						 gboolean preview_visible);

G_END_DECLS

#endif /* E_TASK_SHELL_CONTENT_H */
