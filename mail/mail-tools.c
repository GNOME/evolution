/*
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
 *		Dan Winship <danw@ximian.com>
 *		Peter Williams <peterw@ximian.com>
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <glib/gi18n.h>

#include "em-utils.h"
#include "mail-folder-cache.h"
#include "mail-session.h"
#include "mail-tools.h"

/* **************************************** */

CamelFolder *
mail_tool_get_inbox (const gchar *url, GError **error)
{
	CamelStore *store;
	CamelFolder *folder;

	store = camel_session_get_store (session, url, error);
	if (!store)
		return NULL;

	folder = camel_store_get_inbox (store, error);
	g_object_unref (store);

	return folder;
}

static gboolean
is_local_provider (CamelStore *store)
{
	CamelProvider *provider;

	g_return_val_if_fail (store != NULL, FALSE);

	provider = camel_service_get_provider (CAMEL_SERVICE (store));

	g_return_val_if_fail (provider != NULL, FALSE);

	return (provider->flags & CAMEL_PROVIDER_IS_LOCAL) != 0;
}

CamelFolder *
mail_tool_get_trash (const gchar *url,
                     gint connect,
                     GError **error)
{
	CamelStore *store;
	CamelFolder *trash;

	if (connect)
		store = camel_session_get_store (session, url, error);
	else
		store = (CamelStore *) camel_session_get_service (
			session, url, CAMEL_PROVIDER_STORE, error);

	if (!store)
		return NULL;

	if (connect ||
		(CAMEL_SERVICE (store)->status == CAMEL_SERVICE_CONNECTED ||
		is_local_provider (store)))
		trash = camel_store_get_trash (store, error);
	else
		trash = NULL;

	g_object_unref (store);

	return trash;
}

#ifndef G_OS_WIN32

static gchar *
mail_tool_get_local_movemail_path (const guchar *uri,
                                   GError **error)
{
	guchar *safe_uri, *c;
	const gchar *data_dir;
	gchar *path, *full;
	struct stat st;

	safe_uri = (guchar *)g_strdup ((const gchar *)uri);
	for (c = safe_uri; *c; c++)
		if (strchr("/:;=|%&#!*^()\\, ", *c) || !isprint((gint) *c))
			*c = '_';

	data_dir = mail_session_get_data_dir ();
	path = g_build_filename (data_dir, "spool", NULL);

	if (g_stat(path, &st) == -1 && g_mkdir_with_parents(path, 0700) == -1) {
		g_set_error (
			error, G_FILE_ERROR,
			g_file_error_from_errno (errno),
			_("Could not create spool directory '%s': %s"),
			path, g_strerror(errno));
		g_free(path);
		return NULL;
	}

	full = g_strdup_printf("%s/movemail.%s", path, safe_uri);
	g_free(path);
	g_free(safe_uri);

	return full;
}

#endif

gchar *
mail_tool_do_movemail (const gchar *source_url, GError **error)
{
#ifndef G_OS_WIN32
	gchar *dest_path;
	struct stat sb;
	CamelURL *uri;
	gboolean success;

	uri = camel_url_new(source_url, error);
	if (uri == NULL)
		return NULL;

	if (strcmp(uri->protocol, "mbox") != 0) {
		/* This is really only an internal error anyway */
		g_set_error (
			error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_URL_INVALID,
			_("Trying to movemail a non-mbox source '%s'"),
			source_url);
		camel_url_free(uri);
		return NULL;
	}

	/* Set up our destination. */
	dest_path = mail_tool_get_local_movemail_path (
		(guchar *) source_url, error);
	if (dest_path == NULL)
		return NULL;

	/* Movemail from source (source_url) to dest_path */
	success = camel_movemail (uri->path, dest_path, error) != -1;
	camel_url_free(uri);

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
	g_warning("%s: Not implemented", __FUNCTION__);
	return NULL;
#endif
}

gchar *
mail_tool_generate_forward_subject (CamelMimeMessage *msg)
{
	const gchar *subject;
	gchar *fwd_subj;
	const gint max_subject_length = 1024;

	subject = camel_mime_message_get_subject(msg);

	if (subject && *subject) {
		/* Truncate insanely long subjects */
		if (strlen (subject) < max_subject_length) {
			fwd_subj = g_strdup_printf ("[Fwd: %s]", subject);
		} else {
			/* We can't use %.*s because it depends on the locale being C/POSIX
			   or UTF-8 to work correctly in glibc */
			/*fwd_subj = g_strdup_printf ("[Fwd: %.*s...]", max_subject_length, subject);*/
			fwd_subj = g_malloc (max_subject_length + 11);
			memcpy (fwd_subj, "[Fwd: ", 6);
			memcpy (fwd_subj + 6, subject, max_subject_length);
			memcpy (fwd_subj + 6 + max_subject_length, "...]", 5);
		}
	} else {
		const CamelInternetAddress *from;
		gchar *fromstr;

		from = camel_mime_message_get_from (msg);
		if (from) {
			fromstr = camel_address_format (CAMEL_ADDRESS (from));
			fwd_subj = g_strdup_printf ("[Fwd: %s]", fromstr);
			g_free (fromstr);
		} else
			fwd_subj = g_strdup ("[Fwd: No Subject]");
	}

	return fwd_subj;
}

struct _camel_header_raw *
mail_tool_remove_xevolution_headers (CamelMimeMessage *message)
{
	struct _camel_header_raw *scan, *list = NULL;

	for (scan = ((CamelMimePart *)message)->headers;scan;scan=scan->next)
		if (!strncmp(scan->name, "X-Evolution", 11))
			camel_header_raw_append(&list, scan->name, scan->value, scan->offset);

	for (scan=list;scan;scan=scan->next)
		camel_medium_remove_header((CamelMedium *)message, scan->name);

	return list;
}

void
mail_tool_restore_xevolution_headers (CamelMimeMessage *message,
                                      struct _camel_header_raw *xev)
{
	CamelMedium *medium;

	medium = CAMEL_MEDIUM (message);

	for (;xev;xev=xev->next)
		camel_medium_add_header (medium, xev->name, xev->value);
}

CamelMimePart *
mail_tool_make_message_attachment (CamelMimeMessage *message)
{
	CamelMimePart *part;
	const gchar *subject;
	struct _camel_header_raw *xev;
	gchar *desc;

	subject = camel_mime_message_get_subject (message);
	if (subject)
		desc = g_strdup_printf (_("Forwarded message - %s"), subject);
	else
		desc = g_strdup (_("Forwarded message"));

	/* rip off the X-Evolution headers */
	xev = mail_tool_remove_xevolution_headers (message);
	camel_header_raw_clear(&xev);

	/* remove Bcc headers */
	camel_medium_remove_header (CAMEL_MEDIUM (message), "Bcc");

	part = camel_mime_part_new ();
	camel_mime_part_set_disposition (part, "inline");
	camel_mime_part_set_description (part, desc);
	camel_medium_set_content (
		CAMEL_MEDIUM (part), CAMEL_DATA_WRAPPER (message));
	camel_mime_part_set_content_type (part, "message/rfc822");
	g_free (desc);

	return part;
}

CamelFolder *
mail_tool_uri_to_folder (const gchar *uri, guint32 flags, GError **error)
{
	CamelURL *url;
	CamelStore *store = NULL;
	CamelFolder *folder = NULL;
	gint offset = 0;
	gchar *curi = NULL;

	g_return_val_if_fail (uri != NULL, NULL);

	/* TODO: vtrash and vjunk are no longer used for these uri's */
	if (!strncmp (uri, "vtrash:", 7))
		offset = 7;
	else if (!strncmp (uri, "vjunk:", 6))
		offset = 6;
	else if (!strncmp(uri, "email:", 6)) {
		/* FIXME?: the filter:get_folder callback should do this itself? */
		curi = em_uri_to_camel(uri);
		if (uri == NULL) {
			g_set_error (
				error,
				CAMEL_ERROR, CAMEL_ERROR_GENERIC,
				_("Invalid folder: '%s'"), uri);
			return NULL;
		}
		uri = curi;
	}

	url = camel_url_new (uri + offset, error);
	if (!url) {
		g_free(curi);
		return NULL;
	}

	store = (CamelStore *) camel_session_get_service (
		session, uri + offset, CAMEL_PROVIDER_STORE, error);
	if (store) {
		const gchar *name;

		/* if we have a fragment, then the path is actually used by the store,
		   so the fragment is the path to the folder instead */
		if (url->fragment) {
			name = url->fragment;
		} else {
			if (url->path && *url->path)
				name = url->path + 1;
			else
				name = "";
		}

		if (offset) {
			if (offset == 7)
				folder = camel_store_get_trash (store, error);
			else if (offset == 6)
				folder = camel_store_get_junk (store, error);
		} else
			folder = camel_store_get_folder (store, name, flags, error);
		g_object_unref (store);
	}

	if (folder)
		mail_folder_cache_note_folder (mail_folder_cache_get_default (), folder);

	camel_url_free (url);
	g_free(curi);

	return folder;
}

/**
 * mail_tools_x_evolution_message_parse:
 * @in: GtkSelectionData->data
 * @inlen: GtkSelectionData->length
 * @uids: pointer to a gptrarray that will be filled with uids on success
 *
 * Parses the GtkSelectionData and returns a CamelFolder and a list of
 * UIDs specified by the selection.
 **/
CamelFolder *
mail_tools_x_evolution_message_parse (gchar *in, guint inlen, GPtrArray **uids)
{
	/* format: "uri\0uid1\0uid2\0uid3\0...\0uidn" */
	gchar *inptr, *inend;
	CamelFolder *folder;

	if (in == NULL)
		return NULL;

	folder = mail_tool_uri_to_folder (in, 0, NULL);

	if (!folder)
		return NULL;

	/* split the uids */
	inend = in + inlen;
	inptr = in + strlen (in) + 1;
	*uids = g_ptr_array_new ();
	while (inptr < inend) {
		gchar *start = inptr;

		while (inptr < inend && *inptr)
			inptr++;

		g_ptr_array_add (*uids, g_strndup (start, inptr - start));
		inptr++;
	}

	return folder;
}

/* FIXME: This should be a property on CamelFolder */
gchar *
mail_tools_folder_to_url (CamelFolder *folder)
{
	CamelService *service;
	CamelStore *parent_store;
	const gchar *full_name;
	CamelURL *url;
	gchar *out;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);
	service = CAMEL_SERVICE (parent_store);

	url = camel_url_copy (service->url);
	if (service->provider->url_flags  & CAMEL_URL_FRAGMENT_IS_PATH) {
		camel_url_set_fragment(url, full_name);
	} else {
		gchar *name = g_alloca(strlen(full_name)+2);

		sprintf(name, "/%s", full_name);
		camel_url_set_path(url, name);
	}

	out = camel_url_to_string(url, CAMEL_URL_HIDE_ALL);
	camel_url_free(url);

	return out;
}
