/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-utils.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib.h>

#include <libgnome/gnome-util.h>
#include <libgnome/gnome-i18n.h>

#include "e-util/e-util-private.h"

#include "e-shell-constants.h"
#include "e-shell-utils.h"


static char *
get_icon_path (const char *icon_name)
{
	char *icon_path;

	if (g_path_is_absolute (icon_name))
		icon_path = g_strdup (icon_name);
	else
		icon_path = g_build_filename (EVOLUTION_IMAGESDIR, icon_name, NULL);

	if (g_file_test (icon_path, G_FILE_TEST_EXISTS)) {
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
	char *basename;
	char *name_without_extension;
	char *mini_name;

	basename = g_path_get_basename (icon_name);
	if (basename == NULL)
		return NULL;

	dot_ptr = strrchr (basename, '.');

	if (dot_ptr == NULL) {
		/* No extension.  */
		g_free (basename);
		return g_strconcat (icon_name, E_SHELL_MINI_ICON_SUFFIX, NULL);
	}

	name_without_extension = g_strndup (icon_name, dot_ptr - basename);
	mini_name = g_strconcat (name_without_extension, E_SHELL_MINI_ICON_SUFFIX,
				 dot_ptr, NULL);
	g_free (name_without_extension);

	g_free (basename);
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


gboolean
e_shell_folder_name_is_valid (const char *name,
			      const char **reason_return)
{
	if (name == NULL || *name == '\0') {
		if (reason_return != NULL)
			*reason_return = _("No folder name specified.");
		return FALSE;
	}
	
	/* GtkEntry is broken - if you hit KP_ENTER you get a \r inserted... */
	if (strchr (name, '\r')) {
		if (reason_return != NULL)
			*reason_return = _("Folder name cannot contain the Return character.");
		return FALSE;
	}
	
	if (strchr (name, E_PATH_SEPARATOR) != NULL) {
		if (reason_return != NULL)
			*reason_return = _("Folder name cannot contain the character \"/\".");
		return FALSE;
	}

	if (strchr (name, '#') != NULL) {
		if (reason_return != NULL)
			*reason_return = _("Folder name cannot contain the character \"#\".");
		return FALSE;
	}

	if (strcmp (name, ".") == 0 || strcmp (name, "..") == 0) {
		if (reason_return != NULL)
			*reason_return = _("'.' and '..' are reserved folder names.");
		return FALSE;
	}

	*reason_return = NULL;
	
	return TRUE;
}

