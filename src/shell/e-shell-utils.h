/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
