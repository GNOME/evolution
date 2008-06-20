/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-composer-autosave.h"

#include <errno.h>
#include <sys/stat.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <e-util/e-error.h>
#include <e-util/e-util.h>
#include <camel/camel-stream-fs.h>

#define AUTOSAVE_PREFIX		".evolution-composer.autosave"
#define AUTOSAVE_SEED		AUTOSAVE_PREFIX "-XXXXXX"
#define AUTOSAVE_INTERVAL	60000  /* 60 seconds */

typedef struct _AutosaveState AutosaveState;

struct _AutosaveState {
	gchar *filename;
	gboolean enabled;
	gboolean saved;
	gint fd;
};

static GList *autosave_registry;
static guint autosave_source_id;

static EMsgComposer *
composer_autosave_registry_lookup (const gchar *basename)
{
	GList *iter;

	/* Find the composer with the given autosave filename. */
	for (iter = autosave_registry; iter != NULL; iter = iter->next) {
		EMsgComposer *composer = iter->data;
		AutosaveState *state;

		state = g_object_get_data (G_OBJECT (composer), "autosave");
		if (state == NULL || state->filename == NULL)
			continue;

		if (g_str_has_suffix (state->filename, basename))
			return composer;
	}

	return NULL;
}

static AutosaveState *
composer_autosave_state_new (void)
{
	AutosaveState *state;

	state = g_slice_new (AutosaveState);
	state->filename = NULL;
	state->enabled = TRUE;
	state->fd = -1;

	return state;
}

static void
composer_autosave_state_free (AutosaveState *state)
{
	if (state->fd >= 0)
		close (state->fd);

	g_free (state->filename);
	g_slice_free (AutosaveState, state);
}

static gboolean
composer_autosave_state_open (AutosaveState *state,
                              GError **error)
{
	if (state->filename != NULL)
		return TRUE;

	state->filename = g_build_filename (
		e_get_user_data_dir (), AUTOSAVE_SEED, NULL);

	errno = 0;
	if ((state->fd = g_mkstemp (state->filename)) >= 0)
		return TRUE;

	g_set_error (
		error, G_FILE_ERROR,
		g_file_error_from_errno (errno),
		"%s: %s", state->filename, g_strerror (errno));

	g_free (state->filename);
	state->filename = NULL;

	return FALSE;
}

static void
composer_autosave_foreach (EMsgComposer *composer)
{
	/* Make sure the composer is still alive. */
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (e_composer_autosave_get_enabled (composer))
		e_composer_autosave_snapshot (composer);
}

static gint
composer_autosave_timeout (void)
{
	g_list_foreach (
		autosave_registry, (GFunc)
		composer_autosave_foreach, NULL);

	return TRUE;
}

static void
composer_autosave_notify (gpointer unused,
                          GObject *where_the_object_was)
{
	/* Remove the dead composer from the registry. */
	autosave_registry = g_list_remove (
		autosave_registry, where_the_object_was);

	/* Cancel timeouts if the registry is now empty. */
	if (autosave_registry == NULL && autosave_source_id != 0) {
		g_source_remove (autosave_source_id);
		autosave_source_id = 0;
	}
}

GList *
e_composer_autosave_find_orphans (GError **error)
{
	GDir *dir;
	const gchar *dirname;
	const gchar *basename;
	GList *orphans = NULL;

	dirname = e_get_user_data_dir ();
	dir = g_dir_open (dirname, 0, error);
	if (dir == NULL)
		return NULL;

	/* Scan the user directory for autosave files. */
	while ((basename = g_dir_read_name (dir)) != NULL) {
		const gchar *errmsg;
		gchar *filename;
		struct stat st;

		/* Is this an autosave file? */
		if (!g_str_has_prefix (basename, AUTOSAVE_PREFIX))
			continue;

		/* Is this an orphaned autosave file? */
		if (composer_autosave_registry_lookup (basename) != NULL)
			continue;

		filename = g_build_filename (dirname, basename, NULL);

		/* Try to examine the autosave file.  Failure here
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

		orphans = g_list_prepend (orphans, filename);
	}

	g_dir_close (dir);

	return g_list_reverse (orphans);
}

void
e_composer_autosave_register (EMsgComposer *composer)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	g_object_set_data_full (
		G_OBJECT (composer), "autosave",
		composer_autosave_state_new (),
		(GDestroyNotify) composer_autosave_state_free);

	autosave_registry = g_list_prepend (autosave_registry, composer);

	g_object_weak_ref (
		G_OBJECT (composer), (GWeakNotify)
		composer_autosave_notify, NULL);

	if (autosave_source_id == 0)
		autosave_source_id = g_timeout_add (
			AUTOSAVE_INTERVAL, (GSourceFunc)
			composer_autosave_timeout, NULL);
}

void
e_composer_autosave_unregister (EMsgComposer *composer,
                                gboolean delete_file)
{
	AutosaveState *state;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	state = g_object_get_data (G_OBJECT (composer), "autosave");
	if (state == NULL || state->filename == NULL)
		return;

	close (state->fd);

	if (delete_file)
		g_unlink (state->filename);

	g_object_set_data (G_OBJECT (composer), "autosave", NULL);
}

gboolean
e_composer_autosave_snapshot (EMsgComposer *composer)
{
	GtkhtmlEditor *editor;
	CamelMimeMessage *message;
	AutosaveState *state;
	CamelStream *stream;
	gint camelfd;
	const gchar *errmsg;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	editor = GTKHTML_EDITOR (composer);

	/* If the contents are unchanged, exit early. */
	if (!gtkhtml_editor_get_changed (editor))
		return TRUE;

	state = g_object_get_data (G_OBJECT (composer), "autosave");
	g_return_val_if_fail (state != NULL, FALSE);

	/* Open the autosave file on-demand. */
	if (!composer_autosave_state_open (state, NULL)) {
		errmsg = _("Could not open autosave file");
		goto fail;
	}

	/* Extract a MIME message from the composer. */
	message = e_msg_composer_get_message_draft (composer);
	if (message == NULL) {
		errmsg = _("Unable to retrieve message from editor");
		goto fail;
	}

	/* Move to the beginning of the autosave file. */
	if (lseek (state->fd, (off_t) 0, SEEK_SET) < 0) {
		camel_object_unref (message);
		errmsg = g_strerror (errno);
		goto fail;
	}

	/* Destroy the contents of the autosave file. */
	if (ftruncate (state->fd, (off_t) 0) < 0) {
		camel_object_unref (message);
		errmsg = g_strerror (errno);
		goto fail;
	}

	/* Duplicate the file descriptor for Camel. */
	if ((camelfd = dup (state->fd)) < 0) {
		camel_object_unref (message);
		errmsg = g_strerror (errno);
		goto fail;
	}

	/* Open a CamelStream to the autosave file. */
	stream = camel_stream_fs_new_with_fd (camelfd);

	/* Write the message to the CamelStream. */
	if (camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream) < 0) {
		camel_object_unref (message);
		camel_object_unref (stream);
		errmsg = g_strerror (errno);
		goto fail;
	}

	/* Close the CamelStream. */
	if (camel_stream_close (CAMEL_STREAM (stream)) < 0) {
		camel_object_unref (message);
		camel_object_unref (stream);
		errmsg = g_strerror (errno);
		goto fail;
	}

	/* Snapshot was successful; set various flags. */
	gtkhtml_editor_set_changed (editor, FALSE);
	e_composer_autosave_set_saved (composer, TRUE);

	camel_object_unref (message);
	camel_object_unref (stream);

	return TRUE;

fail:
	e_error_run (
		GTK_WINDOW (composer), "mail-composer:no-autosave",
		(state->filename != NULL) ? state->filename : "",
		errmsg, NULL);

	return FALSE;
}

gint
e_composer_autosave_get_fd (EMsgComposer *composer)
{
	AutosaveState *state;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), -1);

	state = g_object_get_data (G_OBJECT (composer), "autosave");
	g_return_val_if_fail (state != NULL, -1);

	return state->fd;
}

const gchar *
e_composer_autosave_get_filename (EMsgComposer *composer)
{
	AutosaveState *state;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	state = g_object_get_data (G_OBJECT (composer), "autosave");
	g_return_val_if_fail (state != NULL, NULL);

	return state->filename;
}

gboolean
e_composer_autosave_get_enabled (EMsgComposer *composer)
{
	AutosaveState *state;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	state = g_object_get_data (G_OBJECT (composer), "autosave");
	g_return_val_if_fail (state != NULL, FALSE);

	return state->enabled;
}

void
e_composer_autosave_set_enabled (EMsgComposer *composer,
                                 gboolean enabled)
{
	AutosaveState *state;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	state = g_object_get_data (G_OBJECT (composer), "autosave");
	g_return_if_fail (state != NULL);

	state->enabled = enabled;
}

gboolean
e_composer_autosave_get_saved (EMsgComposer *composer)
{
	AutosaveState *state;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	state = g_object_get_data (G_OBJECT (composer), "autosave");
	g_return_val_if_fail (state != NULL, FALSE);

	return state->saved;
}

void
e_composer_autosave_set_saved (EMsgComposer *composer,
                               gboolean saved)
{
	AutosaveState *state;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	state = g_object_get_data (G_OBJECT (composer), "autosave");
	g_return_if_fail (state != NULL);

	state->saved = saved;
}
