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

#ifndef E_SHELL_FOLDER_COMMANDS_H
#define E_SHELL_FOLDER_COMMANDS_H

#include "e-shell.h"
#include "e-shell-view.h"

void  e_shell_command_create_new_folder           (EShell *shell, EShellView *shell_view);

void  e_shell_command_open_folder_in_other_window (EShell *shell, EShellView *shell_view);

void  e_shell_command_copy_folder          	  (EShell *shell, EShellView *shell_view);
void  e_shell_command_move_folder          	  (EShell *shell, EShellView *shell_view);
void  e_shell_command_rename_folder        	  (EShell *shell, EShellView *shell_view);

void  e_shell_command_add_to_shortcut_bar         (EShell *shell, EShellView *shell_view);

void  e_shell_command_folder_properties           (EShell *shell, EShellView *shell_view);

#endif
