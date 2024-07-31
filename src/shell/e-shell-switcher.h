/*
 * e-shell-switcher.h
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

#ifndef E_SHELL_SWITCHER_H
#define E_SHELL_SWITCHER_H

#include <e-util/e-util.h>
#include <shell/e-shell-common.h>

/* Standard GObject macros */
#define E_TYPE_SHELL_SWITCHER \
	(e_shell_switcher_get_type ())
#define E_SHELL_SWITCHER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_SWITCHER, EShellSwitcher))
#define E_SHELL_SWITCHER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_SWITCHER, EShellSwitcherClass))
#define E_IS_SHELL_SWITCHER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_SWITCHER))
#define E_IS_SHELL_SWITCHER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_SHELL_SWITCHER))
#define E_SHELL_SWITCHER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_SWITCHER, EShellSwitcherClass))

#define E_SHELL_SWITCHER_DEFAULT_TOOLBAR_STYLE		GTK_TOOLBAR_BOTH_HORIZ

G_BEGIN_DECLS

typedef struct _EShellSwitcher EShellSwitcher;
typedef struct _EShellSwitcherClass EShellSwitcherClass;
typedef struct _EShellSwitcherPrivate EShellSwitcherPrivate;

/**
 * EShellSwitcher:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EShellSwitcher {
	GtkBin parent;
	EShellSwitcherPrivate *priv;
};

struct _EShellSwitcherClass {
	GtkBinClass parent_class;

	void		(*style_changed)	(EShellSwitcher *switcher,
						 GtkToolbarStyle style);
};

GType		e_shell_switcher_get_type	(void);
GtkWidget *	e_shell_switcher_new		(void);
void		e_shell_switcher_add_action	(EShellSwitcher *switcher,
						 EUIAction      *switch_action,
						 EUIAction      *new_window_action);
GtkToolbarStyle	e_shell_switcher_get_style	(EShellSwitcher *switcher);
void		e_shell_switcher_set_style	(EShellSwitcher *switcher,
						 GtkToolbarStyle style);
void		e_shell_switcher_unset_style	(EShellSwitcher *switcher);
gboolean	e_shell_switcher_get_visible	(EShellSwitcher *switcher);
void		e_shell_switcher_set_visible	(EShellSwitcher *switcher,
						 gboolean visible);

G_END_DECLS

#endif /* E_SHELL_SWITCHER_H */
