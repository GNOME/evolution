/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-url.c
 *
 * Authors: Iain Holmes <iain@helixcode.com>
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
 */

#include <e-summary-util.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>

/**
 * e_pixmap_file:
 * @filename: Filename of pixmap.
 *
 * Finds @filename in the Evolution or GNOME installation dir.
 *
 * Returns: A newly allocated absolute path to @filename, or NULL
 * if it cannot be found.
 */
char *
e_pixmap_file (const char *filename)
{
	char *ret;
	char *edir;

	if (g_file_exists (filename)) {
		ret = g_strdup (filename);

		return ret;
	}

	/* Try the evolution images dir */
	edir = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution",
				      filename);

	if (g_file_exists (edir)) {
		ret = g_strdup (edir);
		g_free (edir);

		return ret;
	}
	g_free (edir);

	/* Try the evolution button images dir */
	edir = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution/buttons",
				      filename);

	if (g_file_exists (edir)) {
		ret = g_strdup (edir);
		g_free (edir);
		
		return ret;
	}
	g_free (edir);

	/* Fall back to the gnome_pixmap_file */
	return gnome_pixmap_file (filename);
}
	
/**
 * e_summary_rm_dir:
 * @path: Full path to the directory or file to be removed.
 *
 * Deletes everything in fullpath.
 */
void
e_summary_rm_dir (const char *path)
{
	DIR *base;
	struct stat statbuf;
	struct dirent *contents;

	stat (path, &statbuf);
	if (!S_ISDIR (statbuf.st_mode)) {
		/* Not a directory */
		g_warning ("Removing: %s", path);
		unlink (path);
		return;
	} else {
		g_warning ("Opening: %s", path);
		base = opendir (path);

		if (base == NULL)
			return;

		contents = readdir (base);
		while (contents != NULL) {
			char *fullpath;

			if (strcmp (contents->d_name, ".") == 0|| 
			    strcmp (contents->d_name, "..") ==0) {
				contents = readdir (base);
				continue;
			}

			fullpath = g_concat_dir_and_file (path, contents->d_name);
			e_summary_rm_dir (fullpath);
			g_free (fullpath);

			contents = readdir (base);
		}

		closedir (base);
		rmdir (path);
	}
}

