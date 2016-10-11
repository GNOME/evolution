/*
 * e-shell-taskbar.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_SHELL_TASKBAR_H
#define E_SHELL_TASKBAR_H

#include <shell/e-shell-common.h>

/* Standard GObject macros */
#define E_TYPE_SHELL_TASKBAR \
	(e_shell_taskbar_get_type ())
#define E_SHELL_TASKBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_TASKBAR, EShellTaskbar))
#define E_SHELL_TASKBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_TASKBAR, EShellTaskbarClass))
#define E_IS_SHELL_TASKBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_TASKBAR))
#define E_IS_SHELL_TASKBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SHELL_TASKBAR))
#define E_SHELL_TASKBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_TASKBAR, EShellTaskbarClass))

G_BEGIN_DECLS

/* Avoid including <e-shell-view.h>, because it includes us! */
struct _EShellView;

typedef struct _EShellTaskbar EShellTaskbar;
typedef struct _EShellTaskbarClass EShellTaskbarClass;
typedef struct _EShellTaskbarPrivate EShellTaskbarPrivate;

/**
 * EShellTaskbar:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EShellTaskbar {
	GtkBox parent;
	EShellTaskbarPrivate *priv;
};

struct _EShellTaskbarClass {
	GtkBoxClass parent_class;
};

GType		e_shell_taskbar_get_type	(void);
GtkWidget *	e_shell_taskbar_new		(struct _EShellView *shell_view);
struct _EShellView *
		e_shell_taskbar_get_shell_view	(EShellTaskbar *shell_taskbar);
const gchar *	e_shell_taskbar_get_message	(EShellTaskbar *shell_taskbar);
void		e_shell_taskbar_set_message	(EShellTaskbar *shell_taskbar,
						 const gchar *message);
void		e_shell_taskbar_unset_message	(EShellTaskbar *shell_taskbar);
guint		e_shell_taskbar_get_activity_count
						(EShellTaskbar *shell_taskbar);

G_END_DECLS

#endif /* E_SHELL_TASKBAR_H */
