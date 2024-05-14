/*
 * e-autosave-utils.c
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
 */

#include "evolution-config.h"

#include "e-autosave-utils.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <camel/camel.h>

#include <e-util/e-util.h>

#define SNAPSHOT_FILE_KEY	"e-composer-snapshot-file"
#define SNAPSHOT_FILE_PREFIX	".evolution-composer.autosave"
#define SNAPSHOT_FILE_SEED	SNAPSHOT_FILE_PREFIX "-XXXXXX"

typedef struct _SaveContext SaveContext;

struct _SaveContext {
	GCancellable *cancellable;
	GFile *snapshot_file;
};

static void
save_context_free (SaveContext *context)
{
	g_clear_object (&context->cancellable);
	g_clear_object (&context->snapshot_file);

	g_free (context);
}

static void
delete_snapshot_file (GFile *snapshot_file)
{
	g_file_delete (snapshot_file, NULL, NULL);
	g_object_unref (snapshot_file);
}

static GFile *
create_snapshot_file (EMsgComposer *composer,
                      GError **error)
{
	GFile *snapshot_file;
	const gchar *user_data_dir;
	gchar *path;
	gint fd;

	snapshot_file = e_composer_get_snapshot_file (composer);

	if (G_IS_FILE (snapshot_file))
		return snapshot_file;

	user_data_dir = e_get_user_data_dir ();
	path = g_build_filename (user_data_dir, SNAPSHOT_FILE_SEED, NULL);

	/* g_mkstemp() modifies the XXXXXX part of the
	 * template string to form the actual filename. */
	errno = 0;
	fd = g_mkstemp (path);
	if (fd == -1) {
		g_set_error (
			error, G_FILE_ERROR,
			g_file_error_from_errno (errno),
			"%s", g_strerror (errno));
		g_free (path);
		return NULL;
	}

	close (fd);

	snapshot_file = g_file_new_for_path (path);

	/* Save the GFile for subsequent snapshots. */
	g_object_set_data_full (
		G_OBJECT (composer),
		SNAPSHOT_FILE_KEY, snapshot_file,
		(GDestroyNotify) delete_snapshot_file);

	g_free (path);

	return snapshot_file;
}

typedef struct _CreateComposerData {
	CamelMimeMessage *message;
	GFile *snapshot_file;
} CreateComposerData;

static void
create_composer_data_free (CreateComposerData *ccd)
{
	g_clear_object (&ccd->message);
	g_clear_object (&ccd->snapshot_file);

	g_free (ccd);
}

static void
autosave_composer_created_cb (GObject *source_object,
			      GAsyncResult *result,
			      gpointer user_data)
{
	GTask *task;
	EMsgComposer *composer;
	GError *error = NULL;

	task = G_TASK (user_data);
	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, error->message);
		g_task_return_error (task, g_steal_pointer (&error));
	} else {
		CreateComposerData *ccd;

		ccd = g_task_get_task_data (task);
		e_msg_composer_setup_with_message (composer, ccd->message, TRUE, NULL, NULL, NULL, NULL);
		g_object_set_data_full (
			G_OBJECT (composer),
			SNAPSHOT_FILE_KEY, g_object_ref (ccd->snapshot_file),
			(GDestroyNotify) delete_snapshot_file);
		g_task_return_pointer (task, g_object_ref_sink (composer), g_object_unref);
	}

	g_object_unref (task);
}

static void
load_snapshot_loaded_cb (GObject *source_object,
                         GAsyncResult *result,
                         gpointer user_data)
{
	GFile *snapshot_file;
	GTask *task;
	EShell *shell;
	CamelMimeMessage *message;
	CamelStream *camel_stream;
	gchar *contents = NULL;
	gsize length;
	CreateComposerData *ccd;
	GError *local_error = NULL;

	snapshot_file = G_FILE (source_object);
	task = G_TASK (user_data);

	g_file_load_contents_finish (
		snapshot_file, result, &contents, &length, NULL, &local_error);

	if (local_error != NULL) {
		g_warn_if_fail (contents == NULL);
		g_task_return_error (task, g_steal_pointer (&local_error));
		g_object_unref (task);
		return;
	}

	/* Create an in-memory buffer for the MIME parser to read from.
	 * We have to do this because CamelStreams are syncrhonous-only,
	 * and feeding the parser a direct file stream would block. */
	message = camel_mime_message_new ();
	camel_stream = camel_stream_mem_new_with_buffer (contents, length);
	camel_data_wrapper_construct_from_stream_sync (
		CAMEL_DATA_WRAPPER (message), camel_stream, NULL, &local_error);
	g_object_unref (camel_stream);
	g_free (contents);

	if (local_error != NULL) {
		g_task_return_error (task, g_steal_pointer (&local_error));
		g_object_unref (message);
		g_object_unref (task);
		return;
	}

	/* Create a new composer window from the loaded message and
	 * restore its snapshot file so it continues auto-saving to
	 * the same file. */
	shell = E_SHELL (g_task_get_source_object (task));

	ccd = g_new0 (CreateComposerData, 1);
	ccd->message = g_steal_pointer (&message);
	ccd->snapshot_file = g_object_ref (snapshot_file);
	g_task_set_task_data (task, g_steal_pointer (&ccd),
		(GDestroyNotify) create_composer_data_free);

	e_msg_composer_new (shell, autosave_composer_created_cb, task);
}

static void
save_snapshot_splice_cb (GObject *source_object,
                         GAsyncResult *result,
                         gpointer user_data)
{
	GTask *parent_task;
	CamelDataWrapper *data_wrapper;
	GError *local_error = NULL;

	parent_task = G_TASK (user_data);
	data_wrapper = CAMEL_DATA_WRAPPER (source_object);

	g_return_if_fail (g_task_is_valid (result, data_wrapper));

	g_task_propagate_int (G_TASK (result), &local_error);

	if (local_error != NULL)
		g_task_return_error (parent_task, g_steal_pointer (&local_error));
	else
		g_task_return_boolean (parent_task, TRUE);

	g_object_unref (parent_task);
}

static void
write_message_to_stream_thread (GTask *task,
				gpointer source_object,
				gpointer task_data,
				GCancellable *cancellable)
{
	GFileOutputStream *file_output_stream;
	GOutputStream *output_stream;
	GFile *snapshot_file;
	gssize bytes_written;
	GError *local_error = NULL;

	snapshot_file = task_data;

	file_output_stream = g_file_replace (snapshot_file, NULL, FALSE,
		G_FILE_CREATE_PRIVATE, cancellable, &local_error);

	if (!file_output_stream) {
		if (local_error)
			g_task_return_error (task, local_error);
		else
			g_task_return_int (task, 0);

		return;
	}

	output_stream = G_OUTPUT_STREAM (file_output_stream);

	bytes_written = camel_data_wrapper_decode_to_output_stream_sync (
		CAMEL_DATA_WRAPPER (source_object),
		output_stream, cancellable, &local_error);

	g_output_stream_close (output_stream, cancellable, local_error ? NULL : &local_error);

	g_object_unref (file_output_stream);

	if (local_error != NULL) {
		g_task_return_error (task, local_error);
	} else {
		g_task_return_int (task, bytes_written);
	}
}

static void
save_snapshot_get_message_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	EMsgComposer *composer;
	SaveContext *context;
	CamelMimeMessage *message;
	GTask *parent_task, *task;
	GError *local_error = NULL;

	composer = E_MSG_COMPOSER (source_object);
	parent_task = G_TASK (user_data);
	context = g_task_get_task_data (parent_task);

	message = e_msg_composer_get_message_draft_finish (
		composer, result, &local_error);

	if (local_error != NULL) {
		g_warn_if_fail (message == NULL);
		g_task_return_error (parent_task, g_steal_pointer (&local_error));
		g_object_unref (parent_task);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	task = g_task_new (message, g_task_get_cancellable (parent_task),
		save_snapshot_splice_cb, parent_task);

	g_task_set_task_data (task, g_object_ref (context->snapshot_file), g_object_unref);

	g_task_run_in_thread (task, write_message_to_stream_thread);

	g_object_unref (task);
	g_object_unref (message);
}

static EMsgComposer *
composer_registry_lookup (GQueue *registry,
                          const gchar *basename)
{
	GList *iter;

	/* Find the composer with the given snapshot filename. */
	for (iter = registry->head; iter != NULL; iter = iter->next) {
		EMsgComposer *composer;
		GFile *snapshot_file;
		gchar *snapshot_name;

		composer = E_MSG_COMPOSER (iter->data);
		snapshot_file = e_composer_get_snapshot_file (composer);

		if (!G_IS_FILE (snapshot_file))
			continue;

		snapshot_name = g_file_get_basename (snapshot_file);
		if (g_strcmp0 (basename, snapshot_name) == 0) {
			g_free (snapshot_name);
			return composer;
		}

		g_free (snapshot_name);
	}

	return NULL;
}

GList *
e_composer_find_orphans (GQueue *registry,
                         GError **error)
{
	GDir *dir;
	const gchar *dirname;
	const gchar *basename;
	GList *orphans = NULL;

	g_return_val_if_fail (registry != NULL, NULL);

	dirname = e_get_user_data_dir ();
	dir = g_dir_open (dirname, 0, error);
	if (dir == NULL)
		return NULL;

	/* Scan the user data directory for snapshot files. */
	while ((basename = g_dir_read_name (dir)) != NULL) {
		const gchar *errmsg;
		gchar *filename;
		struct stat st;

		/* Is this a snapshot file? */
		if (!g_str_has_prefix (basename, SNAPSHOT_FILE_PREFIX))
			continue;

		/* Is this an orphaned snapshot file? */
		if (composer_registry_lookup (registry, basename) != NULL)
			continue;

		filename = g_build_filename (dirname, basename, NULL);

		/* Try to examine the snapshot file.  Failure here
		 * is non-fatal; just emit a warning and move on. */
		errno = 0;
		if (g_stat (filename, &st) < 0) {
			errmsg = g_strerror (errno);
			g_warning ("%s: %s", filename, errmsg);
			g_free (filename);
			continue;
		}

		/* If the file is empty, delete it.  Failure here
		 * is non-fatal; just emit a warning and move on. */
		if (st.st_size == 0) {
			errno = 0;
			if (g_unlink (filename) < 0) {
				errmsg = g_strerror (errno);
				g_warning ("%s: %s", filename, errmsg);
			}
			g_free (filename);
			continue;
		}

		orphans = g_list_prepend (
			orphans, g_file_new_for_path (filename));

		g_free (filename);
	}

	g_dir_close (dir);

	return g_list_reverse (orphans);
}

void
e_composer_load_snapshot (EShell *shell,
                          GFile *snapshot_file,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
	GTask *task;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (G_IS_FILE (snapshot_file));

	task = g_task_new (shell, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_composer_load_snapshot);

	g_file_load_contents_async (
		snapshot_file, cancellable,
		load_snapshot_loaded_cb, g_steal_pointer (&task));
}

EMsgComposer *
e_composer_load_snapshot_finish (EShell *shell,
                                 GAsyncResult *result,
                                 GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, shell), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_composer_load_snapshot), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

void
e_composer_save_snapshot (EMsgComposer *composer,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
	GTask *task;
	SaveContext *context;
	GFile *snapshot_file;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	snapshot_file = e_composer_get_snapshot_file (composer);

	if (!G_IS_FILE (snapshot_file))
		snapshot_file = create_snapshot_file (composer, &local_error);

	if (local_error != NULL) {
		g_warn_if_fail (snapshot_file == NULL);
		g_task_report_error (composer, callback, user_data,
			e_composer_save_snapshot, g_steal_pointer (&local_error));
		return;
	}

	g_return_if_fail (G_IS_FILE (snapshot_file));

	context = g_new0 (SaveContext, 1);

	context->snapshot_file = g_object_ref (snapshot_file);
	if (G_IS_CANCELLABLE (cancellable))
		context->cancellable = g_object_ref (cancellable);

	task = g_task_new (composer, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_composer_save_snapshot);
	g_task_set_task_data (task, context, (GDestroyNotify) save_context_free);

	e_msg_composer_get_message_draft (
		composer, G_PRIORITY_DEFAULT,
		cancellable, (GAsyncReadyCallback)
		save_snapshot_get_message_cb, g_steal_pointer (&task));
}

gboolean
e_composer_save_snapshot_finish (EMsgComposer *composer,
                                 GAsyncResult *result,
                                 GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, composer), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_composer_save_snapshot), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

GFile *
e_composer_get_snapshot_file (EMsgComposer *composer)
{
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	return g_object_get_data (G_OBJECT (composer), SNAPSHOT_FILE_KEY);
}

void
e_composer_prevent_snapshot_file_delete (EMsgComposer *composer)
{
	GFile *snapshot_file;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	snapshot_file = g_object_steal_data (G_OBJECT (composer), SNAPSHOT_FILE_KEY);
	if (snapshot_file) {
		g_object_set_data_full (G_OBJECT (composer), SNAPSHOT_FILE_KEY,
			snapshot_file, g_object_unref);
	}
}

void
e_composer_allow_snapshot_file_delete (EMsgComposer *composer)
{
	GFile *snapshot_file;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	snapshot_file = g_object_steal_data (G_OBJECT (composer), SNAPSHOT_FILE_KEY);
	if (snapshot_file) {
		g_object_set_data_full (G_OBJECT (composer), SNAPSHOT_FILE_KEY,
			snapshot_file, (GDestroyNotify) delete_snapshot_file);
	}
}
