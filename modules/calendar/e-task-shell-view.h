/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef E_TASK_SHELL_VIEW_H
#define E_TASK_SHELL_VIEW_H

#include <e-util/e-util.h>
#include "e-cal-base-shell-view.h"

/* Standard GObject macros */
#define E_TYPE_TASK_SHELL_VIEW \
	(e_task_shell_view_get_type ())
#define E_TASK_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TASK_SHELL_VIEW, ETaskShellView))
#define E_TASK_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TASK_SHELL_VIEW, ETaskShellViewClass))
#define E_IS_TASK_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TASK_SHELL_VIEW))
#define E_IS_TASK_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TASK_SHELL_VIEW))
#define E_TASK_SHELL_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TASK_SHELL_VIEW, ETaskShellViewClass))

G_BEGIN_DECLS

typedef struct _ETaskShellView ETaskShellView;
typedef struct _ETaskShellViewClass ETaskShellViewClass;
typedef struct _ETaskShellViewPrivate ETaskShellViewPrivate;

struct _ETaskShellView {
	ECalBaseShellView parent;
	ETaskShellViewPrivate *priv;
};

struct _ETaskShellViewClass {
	ECalBaseShellViewClass parent_class;
};

GType		e_task_shell_view_get_type		(void);
void		e_task_shell_view_type_register		(GTypeModule *type_module);
gboolean	e_task_shell_view_get_confirm_purge	(ETaskShellView *task_shell_view);
void		e_task_shell_view_set_confirm_purge	(ETaskShellView *task_shell_view,
							 gboolean confirm_purge);

G_END_DECLS

#endif /* E_TASK_SHELL_VIEW_H */
