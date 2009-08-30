/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef CAL_EDITOR_UTILS_H
#define CAL_EDITOR_UTILS_H

#include <glib.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-component.h>
#include <shell/e-shell.h>

G_BEGIN_DECLS

void		open_component_editor		(EShell *shell,
						 ECal *client,
						 ECalComponent *comp,
						 gboolean is_new,
						 GError **error);

G_END_DECLS

#endif /* CAL_EDITOR_UTILS_H */
