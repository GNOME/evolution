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

#include "e-shell-constants.h"

#include "e-shell-utils.h"


static char *
get_icon_path (const char *icon_name)
{
	char *icon_path;

	if (g_path_is_absolute (icon_name)) {
		icon_path = g_strdup (icon_name);
	} else {
		icon_path = gnome_pixmap_file (icon_name);

		if (icon_path == NULL)
			icon_path = g_concat_dir_and_file (EVOLUTION_IMAGES,
							   icon_name);
	}

	if (g_file_exists (icon_path)) {
		return icon_path;
	} else {
		g_free (icon_path);
		return NULL;
	}
}

static char *
get_mini_name (const char *icon_name)
{
	const char *dot_ptr;
	const char *basename;
	char *name_without_extension;
	char *mini_name;

	basename = g_basename (icon_name);
	if (basename == NULL)
		return NULL;

	dot_ptr = strrchr (basename, '.');

	if (dot_ptr == NULL) {
		/* No extension.  */
		return g_strconcat (icon_name, E_SHELL_MINI_ICON_SUFFIX, NULL);
	}

	name_without_extension = g_strndup (icon_name, dot_ptr - icon_name);
	mini_name = g_strconcat (name_without_extension, E_SHELL_MINI_ICON_SUFFIX,
				 dot_ptr, NULL);
	g_free (name_without_extension);

	return mini_name;
}


char *
e_shell_get_icon_path (const char *icon_name,
		       gboolean try_mini)
{
	if (try_mini) {
		char *path;
		char *mini_name;

		mini_name = get_mini_name (icon_name);
		if (mini_name == NULL) {
			path = NULL;
		} else {
			path = get_icon_path (mini_name);
			g_free (mini_name);
		}

		if (path != NULL)
			return path;
	}

	return get_icon_path (icon_name);
}
