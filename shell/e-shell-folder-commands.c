/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-folder-commands.h
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-folder-commands.h"

#include "e-shell-constants.h"
#include "e-shell-folder-creation-dialog.h"


/* Create new folder.  */

void
e_shell_command_create_new_folder (EShell *shell,
				   EShellView *shell_view)
{
	const char *current_uri;
	const char *default_folder;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	current_uri = e_shell_view_get_current_uri (shell_view);

	if (strncmp (current_uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) == 0)
		default_folder = current_uri + E_SHELL_URI_PREFIX_LEN;
	else
		default_folder = NULL;

	e_shell_show_folder_creation_dialog (shell, GTK_WINDOW (shell_view), default_folder,
					     NULL /* result_callback */,
					     NULL /* result_callback_data */);
}


/* Open folder in other window.   */

void
e_shell_command_open_folder_in_other_window (EShell *shell,
					     EShellView *shell_view)
{
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	e_shell_new_view (shell, e_shell_view_get_current_uri (shell_view));
}


/* Copy folder.  */

void
e_shell_command_copy_folder (EShell *shell,
			     EShellView *shell_view)
{
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	g_warning ("To be implemented");
}


/* Move folder.  */

void
e_shell_command_move_folder (EShell *shell,
			     EShellView *shell_view)
{
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));
}


void
e_shell_command_rename_folder (EShell *shell,
			       EShellView *shell_view)
{
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	g_warning ("To be implemented");
}


void
e_shell_command_add_to_shortcut_bar (EShell *shell,
				     EShellView *shell_view)
{
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	g_warning ("To be implemented");
}


void
e_shell_command_folder_properties (EShell *shell,
				   EShellView *shell_view)
{
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (shell_view != NULL && E_IS_SHELL_VIEW (shell_view));

	g_warning ("To be implemented");
}
