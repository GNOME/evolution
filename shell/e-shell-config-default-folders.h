/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-config-default-folders.h - Configuration page for specifying default
 * folders.
 *
 * Copyright (C) 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
 * Author: Dan Winship <danw@ximian.com>
 */

#ifndef E_SHELL_CONFIG_DEFAULT_FOLDERS_H
#define E_SHELL_CONFIG_DEFAULT_FOLDERS_H

#include "e-shell.h"

#include <bonobo/bonobo-object.h>

BonoboObject *e_shell_config_default_folders_create_control (EShell *shell);

#endif /* E_SHELL_CONFIG_DEFAULT_FOLDERS_H */
