/*
 * e-shell-utils.h
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

#ifndef E_SHELL_UTILS_H
#define E_SHELL_UTILS_H

#include <shell/e-shell.h>

G_BEGIN_DECLS

typedef void (* EShellOepnSaveCustomizeFunc)	(GtkFileChooserNative *file_chooser_native,
						 gpointer user_data);

GFile *		e_shell_run_open_dialog		(EShell *shell,
						 const gchar *title,
						 EShellOepnSaveCustomizeFunc customize_func,
						 gpointer customize_data);

GFile *		e_shell_run_save_dialog		(EShell *shell,
						 const gchar *title,
						 const gchar *suggestion,
						 const gchar *filters,
						 EShellOepnSaveCustomizeFunc customize_func,
						 gpointer customize_data);

guint		e_shell_utils_import_uris	(EShell *shell,
						 const gchar * const *uris);

void		e_shell_utils_run_preferences	(EShell *shell);
void		e_shell_utils_run_help_about	(EShell *shell);
void		e_shell_utils_run_help_contents	(EShell *shell);
EAlertSink *	e_shell_utils_find_alternate_alert_sink
						(GtkWidget *widget);

G_END_DECLS

#endif /* E_SHELL_UTILS_H */
