/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* calendar-component.c
 *
 * Copyright (C) 2003  Ximian, Inc
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
 * Author: Rodrigo Moya <rodrigo@ximian.com>
 */

#include <bonobo/bonobo-i18n.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-xfer.h>
#include <gal/util/e-util.h>
#include "migration.h"


static gboolean
process_old_dir (ESourceGroup *source_group, const char *path,
		 const char *filename, const char *name, const char *base_uri)
{
	char *s;
	GnomeVFSURI *from, *to;
	GnomeVFSResult vres;
	ESource *source;
	GDir *dir;
	gboolean retval = TRUE;

	s = g_build_filename (path, filename, NULL);
	if (!g_file_test (s, G_FILE_TEST_EXISTS)) {
		g_free (s);
		return FALSE;
	}

	/* transfer the old file to its new location */
	from = gnome_vfs_uri_new (s);
	g_free (s);
	if (!from)
		return FALSE;

	s = g_build_filename (e_source_group_peek_base_uri (source_group), base_uri,
			      filename, NULL);
	if (e_mkdir_hier (s, 0700) != 0) {
		gnome_vfs_uri_unref (from);
		g_free (s);
		return FALSE;
	}
	to = gnome_vfs_uri_new (s);
	g_free (s);
	if (!to) {
		gnome_vfs_uri_unref (from);
		return FALSE;
	}

	vres = gnome_vfs_xfer_uri ((const GnomeVFSURI *) from,
				   (const GnomeVFSURI *) to,
				   GNOME_VFS_XFER_DEFAULT,
				   GNOME_VFS_XFER_ERROR_MODE_ABORT,
				   GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
				   NULL, NULL);
	gnome_vfs_uri_unref (from);
	gnome_vfs_uri_unref (to);

	if (vres != GNOME_VFS_OK)
		return FALSE;

	/* create the new source */
	source = e_source_new (name, base_uri);
	e_source_group_add_source (source_group, source, -1);

	/* process subfolders */
	s = g_build_filename (path, "subfolders", NULL);
	dir = g_dir_open (s, 0, NULL);
	if (dir) {
		const char *name, *tmp_s;

		while ((name = g_dir_read_name (dir))) {
			tmp_s = g_build_filename (s, name, NULL);
			if (g_file_test (tmp_s, G_FILE_TEST_IS_DIR)) {
				retval = process_old_dir (source_group, tmp_s, filename, name, name);
			}

			g_free (tmp_s);
		}

		g_dir_close (dir);
	}

	g_free (s);

	return retval;
}

gboolean
migrate_old_calendars (ESourceGroup *source_group)
{
	char *path;
	gboolean retval;

	g_return_val_if_fail (E_IS_SOURCE_GROUP (source_group), FALSE);

	path = g_build_filename (g_get_home_dir (), "evolution", NULL);
        if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
		g_free (path);
                return FALSE;
	}
	g_free (path);

	/* look for the top-level calendar */
	path = g_build_filename (g_get_home_dir (), "evolution/local/Calendar", NULL);
	retval = process_old_dir (source_group, path, "calendar.ics", _("Personal"), "Personal");
	g_free (path);
	
        return retval;
}

gboolean
migrate_old_tasks (ESourceGroup *source_group)
{
	char *path;
	gboolean retval;

	g_return_val_if_fail (E_IS_SOURCE_GROUP (source_group), FALSE);

	path = g_build_filename (g_get_home_dir (), "evolution", NULL);
        if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
		g_free (path);
                return FALSE;
	}
	g_free (path);

	/* look for the top-level calendar */
	path = g_build_filename (g_get_home_dir (), "evolution/local/Tasks", NULL);
	retval = process_old_dir (source_group, path, "tasks.ics", _("Personal"), "Personal");
	g_free (path);
	
        return retval;
}
