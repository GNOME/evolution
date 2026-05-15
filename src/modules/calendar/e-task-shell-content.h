/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 2014 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_TASK_SHELL_CONTENT_H
#define E_TASK_SHELL_CONTENT_H

#include <shell/e-shell-content.h>
#include <shell/e-shell-searchbar.h>
#include <shell/e-shell-view.h>

#include <calendar/gui/e-task-table.h>

#include "e-cal-base-shell-content.h"

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
	ECalBaseShellContent parent;
	ETaskShellContentPrivate *priv;
};

struct _ETaskShellContentClass {
	ECalBaseShellContentClass parent_class;
};

GType		e_task_shell_content_get_type		(void);
void		e_task_shell_content_type_register	(GTypeModule *type_module);
GtkWidget *	e_task_shell_content_new		(EShellView *shell_view);
ETaskTable *	e_task_shell_content_get_task_table	(ETaskShellContent *task_shell_content);
EPreviewPane *	e_task_shell_content_get_preview_pane	(ETaskShellContent *task_shell_content);
gboolean	e_task_shell_content_get_preview_visible
							(ETaskShellContent *task_shell_content);
void		e_task_shell_content_set_preview_visible
							(ETaskShellContent *task_shell_content,
							 gboolean preview_visible);
EShellSearchbar *
		e_task_shell_content_get_searchbar	(ETaskShellContent *task_shell_content);

G_END_DECLS

#endif /* E_TASK_SHELL_CONTENT_H */
