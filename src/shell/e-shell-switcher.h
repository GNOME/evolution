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

#include <shell/e-shell-common.h>

G_BEGIN_DECLS

#define E_TYPE_SHELL_SWITCHER (e_shell_switcher_get_type ())

G_DECLARE_FINAL_TYPE (EShellSwitcher, e_shell_switcher, E, SHELL_SWITCHER, GtkBin);

GtkWidget *     e_shell_switcher_new            (void);
void            e_shell_switcher_switch_to_view (EShellSwitcher *self,
                                                 const gchar    *view_name);
void            e_shell_switcher_add_action     (EShellSwitcher *self,
                                                 GtkAction      *action);

G_END_DECLS

#endif /* E_SHELL_SWITCHER_H */
