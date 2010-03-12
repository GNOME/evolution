/*
 *
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
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* This isn't as portable as, say, the stuff in GNU coreutils.
 * But I care not for OSF1. */
#ifdef HAVE_STATVFS
# ifdef HAVE_SYS_STATVFS_H
#  include <sys/statvfs.h>
# endif
#else
#ifdef HAVE_STATFS
# ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>	/* bsd interface */
# endif
# ifdef HAVE_SYS_MOUNT_H
#  include <sys/mount.h>
# endif
#endif
#endif

#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "e-file-utils.h"
#include "e-io-activity.h"

static void
file_replace_contents_cb (GFile *file,
                          GAsyncResult *result,
                          EActivity *activity)
{
	gchar *new_etag;
	gboolean success;
	GError *error = NULL;

	success = g_file_replace_contents_finish (
		file, result, &new_etag, &error);

	result = e_io_activity_get_async_result (E_IO_ACTIVITY (activity));

	if (error == NULL) {
		g_object_set_data_full (
			G_OBJECT (result),
			"__new_etag__", new_etag,
			(GDestroyNotify) g_free);
	} else {
		g_simple_async_result_set_from_error (
			G_SIMPLE_ASYNC_RESULT (result), error);
		g_error_free (error);
	}

	g_simple_async_result_set_op_res_gboolean (
		G_SIMPLE_ASYNC_RESULT (result), success);

	e_activity_complete (activity);

	g_object_unref (activity);
}

/**
 * e_file_replace_contents_async:
 * @file: input #GFile
 * @contents: string of contents to replace the file with
 * @length: the length of @contents in bytes
 * @etag: a new entity tag for the @file, or %NULL
 * @make_backup: %TRUE if a backup should be created
 * @flags: a set of #GFileCreateFlags
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to the callback function
 *
 * This is a wrapper for g_file_replace_contents_async() that also returns
 * an #EActivity to track the file operation.  Cancelling the activity will
 * cancel the file operation.  See g_file_replace_contents_async() for more
 * details.
 *
 * Returns: an #EActivity for the file operation
 **/
EActivity *
e_file_replace_contents_async (GFile *file,
                               const gchar *contents,
                               gsize length,
                               const gchar *etag,
                               gboolean make_backup,
                               GFileCreateFlags flags,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	EActivity *activity;
	GSimpleAsyncResult *simple;
	GCancellable *cancellable;
	const gchar *format;
	gchar *description;
	gchar *basename;
	gchar *filename;
	gchar *hostname;
	gchar *uri;

	g_return_val_if_fail (G_IS_FILE (file), NULL);
	g_return_val_if_fail (contents != NULL, NULL);

	uri = g_file_get_uri (file);
	filename = g_filename_from_uri (uri, &hostname, NULL);
	if (filename != NULL)
		basename = g_filename_display_basename (filename);
	else
		basename = g_strdup (_("(Unknown Filename)"));

	if (hostname == NULL) {
		/* Translators: The string value is the basename of a file. */
		format = _("Writing \"%s\"");
		description = g_strdup_printf (format, basename);
	} else {
		/* Translators: The first string value is the basename of a
		 * remote file, the second string value is the hostname. */
		format = _("Writing \"%s\" to %s");
		description = g_strdup_printf (format, basename, hostname);
	}

	cancellable = g_cancellable_new ();

	simple = g_simple_async_result_new (
		G_OBJECT (file), callback, user_data,
		e_file_replace_contents_async);

	activity = e_io_activity_new (
		description, G_ASYNC_RESULT (simple), cancellable);

	g_file_replace_contents_async (
		file, contents, length, etag,
		make_backup, flags, cancellable,
		(GAsyncReadyCallback) file_replace_contents_cb,
		activity);

	g_object_unref (cancellable);
	g_object_unref (simple);

	g_free (description);
	g_free (basename);
	g_free (filename);
	g_free (hostname);
	g_free (uri);

	return activity;
}

/**
 * e_file_replace_contents_finish:
 * @file: input #GFile
 * @result: a #GAsyncResult
 * @new_etag: return location for a new entity tag
 * @error: return location for a #GError, or %NULL
 *
 * Finishes an asynchronous replace of the given @file.  See
 * e_file_replace_contents_async().  Sets @new_etag to the new entity
 * tag for the document, if present.  Free it with g_free() when it is
 * no longer needed.
 *
 * Returns: %TRUE on success, %FALSE on failure
 **/
gboolean
e_file_replace_contents_finish (GFile *file,
                                GAsyncResult *result,
                                gchar **new_etag,
                                GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (G_IS_FILE (file), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	if (new_etag != NULL)
		*new_etag = g_object_steal_data (
			G_OBJECT (result), "__new_etag__");

	return TRUE;
}

/**
 * e_fsutils_usage:
 * @path: a file path
 *
 * Calculate the amount of disk space used by a given path.
 *
 * Return value: The number of 1024 byte blocks used by the
 * filesystem.
 **/
glong e_fsutils_usage(const gchar *inpath)
{
	GDir *dir;
	const gchar *d;
	long size = 0;
	GSList *paths;

	/* iterative, depth-first scan, because i can ... */
	paths = g_slist_prepend(NULL, g_strdup(inpath));

	while (paths) {
		gchar *path = paths->data;

		paths = g_slist_remove_link(paths, paths);

		dir = g_dir_open(path, 0, NULL);
		if (dir == NULL) {
			g_free(path);
			goto fail;
		}

		while ((d = g_dir_read_name(dir))) {
			gchar *full_path;
			struct stat st;

			full_path = g_build_filename(path, d, NULL);
			if (g_stat(full_path, &st) == -1) {
				g_free(full_path);
				g_dir_close(dir);
				g_free(path);
				goto fail;
			} else if (S_ISDIR(st.st_mode)) {
				paths = g_slist_prepend(paths, full_path);
				full_path = NULL;
			} else if (S_ISREG(st.st_mode)) {
				/* This is in 512 byte blocks.  st_blksize is page size on linux,
				   on *BSD it might be significant. */
#ifndef G_OS_WIN32
				size += st.st_blocks/2;
#endif
			}

			g_free(full_path);
		}

		g_dir_close(dir);
		g_free(path);
	}

	return size;

fail:
	g_slist_foreach(paths, (GFunc)g_free, NULL);
	g_slist_free(paths);

	return -1;
}

/**
 * e_fsutils_avail:
 * @path: a file path
 *
 * Find the available disk space at the given path.
 *
 * Return value: -1 if it could not be determined, otherwise the
 * number of disk blocks, expressed as system-independent, 1024 byte
 * blocks.
 **/
glong
e_fsutils_avail(const gchar *path)
{
#if defined(HAVE_STATVFS)
	struct statvfs stfs;

	if (statvfs(path, &stfs) == -1)
		return -1;

	/* Assumes that frsize === power of 2 */
	if (stfs.f_frsize >= 1024)
		return stfs.f_bavail * (stfs.f_frsize / 1024);
	else
		return stfs.f_bavail / (1024 / stfs.f_frsize);
#elif defined(HAVE_STATFS)
	struct statfs stfs;

	if (statfs(path, &stfs) == -1)
		return -1;

	/* For BSD this isn't clear, it may be dependent on f_bsize */
	return stfs.f_bavail / 2;
#else
	errno = ENOSYS;
	return -1;
#endif
}

