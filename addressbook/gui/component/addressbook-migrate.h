/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 * Author: Chris Toshok (toshok@ximian.com)
 */

#ifndef _ADDRESSBOOK_MIGRATE_H_
#define _ADDRESSBOOK_MIGRATE_H_

#include <glib.h>
#include <e-shell-module.h>

G_BEGIN_DECLS

gboolean	addressbook_migrate		(EShellModule *shell_module,
						 gint major,
						 gint minor,
						 gint revision,
						 GError **error);

G_END_DECLS

#endif /* _ADDRESSBOOK_MIGRATE_H_ */
