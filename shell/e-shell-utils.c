/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-utils.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
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

#include <gnome.h>

#include "e-shell-utils.h"


char *
e_shell_get_icon_path (const char *icon_name)
{
	char *icon_path;

	g_return_val_if_fail (icon_name != NULL, NULL);

	if (g_path_is_absolute (icon_name)) {
		icon_path = g_strdup (icon_name);
	} else {
		icon_path = gnome_pixmap_file (icon_name);

		if (icon_path == NULL)
			icon_path = g_concat_dir_and_file (EVOLUTION_IMAGES,
							   icon_name);
	}

	return icon_path;
}
