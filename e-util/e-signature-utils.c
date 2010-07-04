/*
 * e-signature-utils.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "e-signature-utils.h"

#include <errno.h>
#include <camel/camel.h>
#include <glib/gstdio.h>
#include <gconf/gconf-client.h>

#ifndef G_OS_WIN32
#include <sys/wait.h>
#endif

#include "e-util/e-util.h"

static ESignatureList *global_signature_list;

ESignatureList *
e_get_signature_list (void)
{
	if (G_UNLIKELY (global_signature_list == NULL)) {
		GConfClient *client;

		client = gconf_client_get_default ();
		global_signature_list = e_signature_list_new (client);
		g_object_unref (client);
	}

	g_return_val_if_fail (global_signature_list != NULL, NULL);

	return global_signature_list;
}

ESignature *
e_get_signature_by_name (const gchar *name)
{
	ESignatureList *signature_list;
	const ESignature *signature;
	e_signature_find_t find;

	g_return_val_if_fail (name != NULL, NULL);

	find = E_SIGNATURE_FIND_NAME;
	signature_list = e_get_signature_list ();
	signature = e_signature_list_find (signature_list, find, name);

	/* XXX ESignatureList misuses const. */
	return (ESignature *) signature;
}

ESignature *
e_get_signature_by_uid (const gchar *uid)
{
	ESignatureList *signature_list;
	const ESignature *signature;
	e_signature_find_t find;

	g_return_val_if_fail (uid != NULL, NULL);

	find = E_SIGNATURE_FIND_UID;
	signature_list = e_get_signature_list ();
	signature = e_signature_list_find (signature_list, find, uid);

	/* XXX ESignatureList misuses const. */
	return (ESignature *) signature;
}

gchar *
e_create_signature_file (GError **error)
{
	const gchar *data_dir;
	gchar basename[32];
	gchar *filename;
	gchar *pathname;
	gint32 ii;

	data_dir = e_get_user_data_dir ();
	pathname = g_build_filename (data_dir, "signatures", NULL);
	filename = NULL;

	if (g_mkdir_with_parents (pathname, 0700) < 0) {
		g_set_error (
			error, G_FILE_ERROR,
			g_file_error_from_errno (errno),
			"%s: %s", pathname, g_strerror (errno));
		g_free (pathname);
		return NULL;
	}

	for (ii = 0; ii < G_MAXINT32; ii++) {

		g_snprintf (
			basename, sizeof (basename),
			"signature-%" G_GINT32_FORMAT, ii);

		g_free (filename);
		filename = g_build_filename (pathname, basename, NULL);

		if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
			gint fd;

			fd = g_creat (filename, 0600);
			if (fd >= 0) {
				close (fd);
				break;
			}

			/* If we failed once we're probably going
			 * to continue failing, so just give up. */
			g_set_error (
				error, G_FILE_ERROR,
				g_file_error_from_errno (errno),
				"%s: %s", filename, g_strerror (errno));
			g_free (filename);
			filename = NULL;
			break;
		}
	}

	/* If there are actually G_MAXINT32 signature files, the
	 * most recent signature file we be overwritten.  Sorry. */

	return filename;
}

gchar *
e_read_signature_file (ESignature *signature,
                       gboolean convert_to_html,
                       GError **error)
{
	CamelStream *input_stream;
	CamelStream *output_stream;
	GByteArray *buffer;
	const gchar *filename;
	gboolean is_html;
	gchar *content;
	gsize length;
	gint fd;

	g_return_val_if_fail (E_IS_SIGNATURE (signature), NULL);

	filename = e_signature_get_filename (signature);
	is_html = e_signature_get_is_html (signature);

	fd = g_open (filename, O_RDONLY, 0);
	if (fd < 0) {
		g_set_error (
			error, G_FILE_ERROR,
			g_file_error_from_errno (errno),
			"%s: %s", filename, g_strerror (errno));
		return NULL;
	}

	input_stream = camel_stream_fs_new_with_fd (fd);

	if (!is_html && convert_to_html) {
		CamelStream *filtered_stream;
		CamelMimeFilter *filter;
		gint32 flags;

		filtered_stream =
			camel_stream_filter_new (input_stream);
		g_object_unref (input_stream);

		flags =
			CAMEL_MIME_FILTER_TOHTML_PRESERVE_8BIT |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES;
		filter = camel_mime_filter_tohtml_new (flags, 0);
		camel_stream_filter_add (
			CAMEL_STREAM_FILTER (filtered_stream), filter);
		g_object_unref (filter);

		input_stream = filtered_stream;
	}

	buffer = g_byte_array_new ();
	output_stream = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (
		CAMEL_STREAM_MEM (output_stream), buffer);
	camel_stream_write_to_stream (input_stream, output_stream, NULL);
	g_object_unref (output_stream);
	g_object_unref (input_stream);

	/* Make sure the buffer is nul-terminated. */
	length = (gsize) buffer->len;
	g_byte_array_append (buffer, (guint8 *) "", 1);
	content = (gchar *) g_byte_array_free (buffer, FALSE);

	/* Signatures are saved as UTF-8, but we still need to check that
	 * the signature is valid UTF-8 because the user may be opening
	 * a signature file that is in his/her locale character set.  If
	 * it's not in UTF-8 then try converting from the current locale. */
	if (!g_utf8_validate (content, length, NULL)) {
		gchar *utf8;

		utf8 = g_locale_to_utf8 (content, length, NULL, NULL, error);
		g_free (content);
		content = utf8;
	}

	return content;
}

gchar *
e_run_signature_script (const gchar *filename)
{
	/* FIXME Make this cross-platform, prefer GLib functions over
	 *       POSIX, and report errors via GError instead of dumping
	 *       messages to the terminal where users won't see them. */

#ifndef G_OS_WIN32
	gint in_fds[2];
	pid_t pid;

	g_return_val_if_fail (filename != NULL, NULL);

	if (pipe (in_fds) == -1) {
		g_warning (
			"Failed to create pipe to '%s': %s",
			filename, g_strerror (errno));
		return NULL;
	}

	pid = fork ();

	/* Child Process */
	if (pid == 0) {
		gint maxfd, ii;

		close (in_fds[0]);
		if (dup2 (in_fds[1], STDOUT_FILENO) < 0)
			_exit (255);
		close (in_fds[1]);

		setsid ();

		maxfd = sysconf (_SC_OPEN_MAX);
		for (ii = 3; ii < maxfd; ii++) {
			if (ii == STDIN_FILENO)
				continue;
			if (ii == STDOUT_FILENO)
				continue;
			if (ii == STDERR_FILENO)
				continue;
			fcntl (ii, F_SETFD, FD_CLOEXEC);
		}

		execlp ("/bin/sh", "/bin/sh", "-c", filename, NULL);

		g_warning (
			"Could not execute '%s': %s",
			filename, g_strerror (errno));

		_exit (255);

	/* Parent Process */
	} else if (pid > 0) {
		CamelStream *output_stream;
		CamelStream *input_stream;
		GByteArray *buffer;
		gchar *content;
		gsize length;
		gint result;
		gint status;

		close (in_fds[1]);

		buffer = g_byte_array_new ();
		output_stream = camel_stream_mem_new ();
		camel_stream_mem_set_byte_array (
			CAMEL_STREAM_MEM (output_stream), buffer);

		input_stream = camel_stream_fs_new_with_fd (in_fds[0]);
		camel_stream_write_to_stream (input_stream, output_stream, NULL);
		g_object_unref (input_stream);

		g_object_unref (output_stream);

		/* Make sure the buffer is nul-terminated. */
		length = (gsize) buffer->len;
		g_byte_array_append (buffer, (guchar *) "", 1);
		content = (gchar *) g_byte_array_free (buffer, FALSE);

		/* Signature scripts are supposed to generate UTF-8 content,
		 * but because users are known to never read the manual, we
		 * try to do our best if the content isn't valid UTF-8 by
		 * assuming that the content is in the user's locale
		 * character set. */
		if (!g_utf8_validate (content, length, NULL)) {
			gchar *utf8;

			/* XXX Should pass a GError here. */
			utf8 = g_locale_to_utf8 (
				content, length, NULL, NULL, NULL);
			g_free (content);
			content = utf8;
		}

		/* Wait for the script process to terminate. */
		result = waitpid (pid, &status, 0);

		if (result == -1 && errno == EINTR) {
			/* Child process is hanging... */
			kill (pid, SIGTERM);
			sleep (1);
			result = waitpid (pid, &status, WNOHANG);
			if (result == 0) {
				/* ...still hanging, set phasers to KILL. */
				kill (pid, SIGKILL);
				sleep (1);
				waitpid (pid, &status, WNOHANG);
			}
		}

		return content;

	/* Forking Failed */
	} else {
		g_warning (
			"Failed to create child process '%s': %s",
			filename, g_strerror (errno));
		close (in_fds[0]);
		close (in_fds[1]);
	}
#endif

	return NULL;
}
