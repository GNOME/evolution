/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-file-utils.h"

static void
file_replace_contents_cb (GObject *source_object,
                          GAsyncResult *result,
                          gpointer user_data)
{
	GFile *file;
	GTask *task;
	EActivity *activity;
	gchar *new_etag = NULL;
	GError *error = NULL;

	file = G_FILE (source_object);
	task = G_TASK (user_data);
	activity = g_task_get_task_data (task);

	g_file_replace_contents_finish (file, result, &new_etag, &error);

	if (!e_activity_handle_cancellation (activity, error))
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

	if (error == NULL)
		g_task_return_pointer (task, g_steal_pointer (&new_etag), g_free);
	else {
		g_warn_if_fail (new_etag == NULL);
		g_task_return_error (task, g_steal_pointer (&error));
	}

	g_object_unref (task);
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
	GTask *task;
	GCancellable *cancellable;
	EActivity *activity;
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
		format = _("Writing “%s”");
		description = g_strdup_printf (format, basename);
	} else {
		/* Translators: The first string value is the basename of a
		 * remote file, the second string value is the hostname. */
		format = _("Writing “%s” to %s");
		description = g_strdup_printf (format, basename, hostname);
	}

	cancellable = g_cancellable_new ();

	activity = e_activity_new ();

	e_activity_set_text (activity, description);
	e_activity_set_cancellable (activity, cancellable);

	task = g_task_new (file, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_file_replace_contents_async);
	g_task_set_task_data (task, activity, g_object_unref);

	g_file_replace_contents_async (
		file, contents, length, etag,
		make_backup, flags, cancellable,
		file_replace_contents_cb,
		g_steal_pointer (&task));

	g_object_unref (cancellable);

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
	gchar *given_new_etag;
	GError *local_error = NULL;

	g_return_val_if_fail (G_IS_FILE (file), FALSE);
	g_return_val_if_fail (g_task_is_valid (result, file), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_file_replace_contents_async), FALSE);

	given_new_etag = g_task_propagate_pointer (G_TASK (result), &local_error);
	if (new_etag != NULL)
		*new_etag = g_steal_pointer (&given_new_etag);
	else
		g_clear_pointer (&given_new_etag, g_free);

	if (local_error) {
		g_propagate_error (error, g_steal_pointer (&local_error));
		return FALSE;
	}

	return TRUE;
}

