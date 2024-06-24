/*
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
 *		Dan Winship <danw@ximian.com>
 *		Peter Williams <peterw@ximian.com>
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <glib/gstdio.h>

#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "e-mail-session.h"
#include "mail-folder-cache.h"
#include "mail-tools.h"

/* **************************************** */

#ifndef G_OS_WIN32

static gchar *
mail_tool_get_local_movemail_path (CamelStore *store,
                                   GError **error)
{
	const gchar *uid;
	guchar *safe_uid, *c;
	const gchar *data_dir;
	gchar *path, *full;
	struct stat st;

	uid = camel_service_get_uid (CAMEL_SERVICE (store));
	safe_uid = (guchar *) g_strdup ((const gchar *) uid);
	for (c = safe_uid; *c; c++)
		if (strchr ("/:;=|%&#!*^()\\, ", *c) || !isprint ((gint) *c))
			*c = '_';

	data_dir = mail_session_get_data_dir ();
	path = g_build_filename (data_dir, "spool", NULL);

	if (g_stat (path, &st) == -1 && g_mkdir_with_parents (path, 0700) == -1) {
		g_set_error (
			error, G_FILE_ERROR,
			g_file_error_from_errno (errno),
			_("Could not create spool directory “%s”: %s"),
			path, g_strerror (errno));
		g_free (path);
		return NULL;
	}

	full = g_strdup_printf ("%s/movemail.%s", path, safe_uid);
	g_free (path);
	g_free (safe_uid);

	return full;
}

#endif

gchar *
mail_tool_do_movemail (CamelStore *store,
                       GError **error)
{
#ifndef G_OS_WIN32
	CamelService *service;
	CamelProvider *provider;
	CamelSettings *settings;
	gchar *src_path;
	gchar *dest_path;
	struct stat sb;
	gboolean success;

	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);

	service = CAMEL_SERVICE (store);
	provider = camel_service_get_provider (service);

	g_return_val_if_fail (provider != NULL, NULL);

	if (g_strcmp0 (provider->protocol, "mbox") != 0) {
		/* This is really only an internal error anyway */
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_URL_INVALID,
			_("Trying to movemail a non-mbox source “%s”"),
			camel_service_get_uid (CAMEL_SERVICE (store)));
		return NULL;
	}

	settings = camel_service_ref_settings (service);

	src_path = camel_local_settings_dup_path (
		CAMEL_LOCAL_SETTINGS (settings));

	g_object_unref (settings);

	/* Set up our destination. */
	dest_path = mail_tool_get_local_movemail_path (store, error);
	if (dest_path == NULL)
		return NULL;

	/* Movemail from source to dest_path */
	success = camel_movemail (src_path, dest_path, error) != -1;

	g_free (src_path);

	if (g_stat (dest_path, &sb) < 0 || sb.st_size == 0) {
		g_unlink (dest_path); /* Clean up the movemail.foo file. */
		g_free (dest_path);
		return NULL;
	}

	if (!success) {
		g_free (dest_path);
		return NULL;
	}

	return dest_path;
#else
	/* Unclear yet whether camel-movemail etc makes any sense on
	 * Win32, at least it is not ported yet.
	 */
	g_warning ("%s: Not implemented", G_STRFUNC);
	return NULL;
#endif
}

gchar *
mail_tool_generate_forward_subject (CamelMimeMessage *msg,
				    const gchar *orig_subject)
{
	gchar *subject = NULL;
	gchar *fwd_subj;
	const gint max_subject_length = 1024;
	const gchar *format;
	GSettings *settings;

	if ((!orig_subject || !*orig_subject) && msg)
		orig_subject = camel_mime_message_get_subject (msg);

	if (orig_subject && *orig_subject) {
		gchar *utf8;

		utf8 = e_util_utf8_make_valid (orig_subject);
		if (utf8 && *utf8) {
			/* Truncate insanely long subjects */
			if (g_utf8_strlen (utf8, -1) < max_subject_length) {
				subject = utf8;
				utf8 = NULL;
			} else {
				gchar *end = g_utf8_offset_to_pointer (utf8, max_subject_length);

				if (end) {
					*end = '\0';

					subject = g_strconcat (utf8, "...", NULL);
				}
			}
		}

		g_free (utf8);
	}

	if (!subject && msg) {
		const CamelInternetAddress *from;

		from = camel_mime_message_get_from (msg);
		if (from)
			subject = camel_address_format (CAMEL_ADDRESS (from));
	}

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	if (g_settings_get_boolean (settings, "composer-use-localized-fwd-re")) {
		/* Translators: This is a subject attribution for forwarded messages. The %s is replaced with subject of the original message. */
		format = _("Fwd: %s");
	} else {
		/* Do not localize this string */
		format = "Fwd: %s";
	}
	g_clear_object (&settings);

	fwd_subj = g_strdup_printf (format,
		/* Translators: This is a subject attribution for forwarded messages, used when there could not be used any subject.
	           It results in "[Fwd: No Subject]" being used as a subject of the forwarded message. */
		(subject && *subject) ? subject : _("No Subject"));

	g_free (subject);

	return fwd_subj;
}

CamelNameValueArray *
mail_tool_remove_xevolution_headers (CamelMimeMessage *message)
{
	CamelNameValueArray *orig_headers, *removed_headers = NULL;
	CamelMedium *medium;
	guint ii, len;

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	medium = CAMEL_MEDIUM (message);
	orig_headers = camel_medium_dup_headers (medium);
	len = camel_name_value_array_get_length (orig_headers);

	for (ii = 0; ii < len; ii++) {
		const gchar *header_name = NULL, *header_value = NULL;

		if (!camel_name_value_array_get (orig_headers, ii, &header_name, &header_value) || !header_name)
			continue;

		if (g_ascii_strncasecmp (header_name, "X-Evolution", 11) == 0) {
			if (!removed_headers)
				removed_headers = camel_name_value_array_new ();

			camel_name_value_array_append (removed_headers, header_name, header_value);

			camel_medium_remove_header (medium, header_name);
		}
	}

	camel_name_value_array_free (orig_headers);

	return removed_headers;
}

void
mail_tool_restore_xevolution_headers (CamelMimeMessage *message,
                                      CamelNameValueArray *headers)
{
	CamelMedium *medium;
	guint ii, len;

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	if (!headers)
		return;

	medium = CAMEL_MEDIUM (message);
	len = camel_name_value_array_get_length (headers);

	for (ii = 0; ii < len; ii++) {
		const gchar *header_name = NULL, *header_value = NULL;

		if (!camel_name_value_array_get (headers, ii, &header_name, &header_value) || !header_name)
			continue;

		camel_medium_add_header (medium, header_name, header_value);
	}
}

CamelMimePart *
mail_tool_make_message_attachment (CamelMimeMessage *message)
{
	CamelMimePart *part;
	const gchar *subject;
	gchar *desc;

	subject = camel_mime_message_get_subject (message);
	if (subject)
		desc = g_strdup_printf (_("Forwarded message — %s"), subject);
	else
		desc = g_strdup (_("Forwarded message"));

	/* rip off the X-Evolution headers */
	camel_name_value_array_free (mail_tool_remove_xevolution_headers (message));

	/* remove Bcc headers */
	camel_medium_remove_header (CAMEL_MEDIUM (message), "Bcc");

	part = camel_mime_part_new ();
	camel_mime_part_set_disposition (part, "inline");
	camel_mime_part_set_description (part, desc);
	if (!g_str_has_suffix (desc, ".eml")) {
		gchar *fnm;

		fnm = g_strconcat (desc, ".eml", NULL);
		camel_mime_part_set_filename (part, fnm);
		g_free (fnm);
	}
	camel_medium_set_content (
		CAMEL_MEDIUM (part), CAMEL_DATA_WRAPPER (message));
	camel_mime_part_set_content_type (part, "message/rfc822");
	g_free (desc);

	return part;
}
