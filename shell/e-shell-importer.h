/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* importer.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Iain Holmes  <iain@ximian.com>
 */

#ifndef E_SHELL_IMPORTER_H
#define E_SHELL_IMPORTER_H

#include "e-shell-common.h"
#include "e-shell-window.h"

G_BEGIN_DECLS

void		e_shell_importer_start_import	(EShellWindow *shell_window);

G_END_DECLS

#endif /* E_SHELL_IMPORTER_H */
