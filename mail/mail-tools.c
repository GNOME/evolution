/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-ops.c: callbacks for the mail toolbar/menus */

/* 
 * Authors: 
 *  Dan Winship <danw@helixcode.com>
 *  Peter Williams <peterw@helixcode.com>
 *  Jeffrey Stedfast <fejj@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include <ctype.h>
#include <errno.h>
#include "camel/camel.h"
#include "camel/providers/vee/camel-vee-folder.h"
#include "mail-vfolder.h"
#include "filter/vfolder-rule.h"
#include "filter/vfolder-context.h"
#include "filter/filter-option.h"
#include "filter/filter-input.h"
#include "filter/filter-driver.h"
#include "mail.h" /*session*/
#include "mail-tools.h"
#include "mail-local.h"
#include "e-util/e-html-utils.h"

/* **************************************** */

G_LOCK_DEFINE_STATIC (camel);
G_LOCK_DEFINE_STATIC (camel_locklevel);
static GPrivate *camel_locklevel = NULL;

#define LOCK_VAL (GPOINTER_TO_INT (g_private_get (camel_locklevel)))
#define LOCK_SET(val) g_private_set (camel_locklevel, (GINT_TO_POINTER (val)))

void mail_tool_camel_lock_up (void)
{
	G_LOCK (camel_locklevel);

	if (camel_locklevel == NULL)
		camel_locklevel = g_private_new (GINT_TO_POINTER (0));
	
        if (LOCK_VAL == 0) {
		G_UNLOCK (camel_locklevel);
                G_LOCK (camel);
		G_LOCK (camel_locklevel);
	}

        LOCK_SET (LOCK_VAL + 1);

        G_UNLOCK (camel_locklevel);
}

void mail_tool_camel_lock_down (void)
{
        G_LOCK (camel_locklevel);

        if (camel_locklevel == NULL) {
                g_warning ("mail_tool_camel_lock_down: lock down before a lock up?");
                camel_locklevel = g_private_new (GINT_TO_POINTER (0));
                return;
        }

        LOCK_SET (LOCK_VAL - 1);

        if (LOCK_VAL == 0)
                G_UNLOCK (camel);

        G_UNLOCK (camel_locklevel);
}

/* **************************************** */

CamelFolder *
mail_tool_get_folder_from_urlname (const gchar *url, const gchar *name,
				   guint32 flags, CamelException *ex)
{
	CamelStore *store;
	CamelFolder *folder;

	mail_tool_camel_lock_up();

	store = camel_session_get_store (session, url, ex);
	if (!store) {
		mail_tool_camel_lock_down();
		return NULL;
	}

	/*camel_service_connect (CAMEL_SERVICE (store), ex);
	 *if (camel_exception_is_set (ex)) {
	 *	camel_object_unref (CAMEL_OBJECT (store));
	 *	mail_tool_camel_lock_down();
	 *	return NULL;
	 *}
	 */

	folder = camel_store_get_folder (store, name, flags, ex);
	camel_object_unref (CAMEL_OBJECT (store));
	mail_tool_camel_lock_down();

	return folder;
}

const gchar *
mail_tool_get_folder_name (CamelFolder *folder)
{
	const char *name = camel_folder_get_full_name (folder);
	char *path;

	/* This is a kludge. */

	if (strcmp (name, "//mbox") && strcmp (name, "//mh"))
		return name;

	/* For mbox/mh, return the parent store's final path component. */
	path = CAMEL_SERVICE (folder->parent_store)->url->path;
	if (strchr (path, '/'))
		return strrchr (path, '/') + 1;
	else
		return path;
}

gchar *
mail_tool_get_local_movemail_path (void)
{
	return g_strdup_printf ("%s/local/Inbox/movemail", evolution_dir);
}

CamelFolder *
mail_tool_get_local_inbox (CamelException *ex)
{
	gchar *url;
	CamelFolder *folder;

	url = g_strdup_printf("file://%s/local/Inbox", evolution_dir);
	folder = mail_tool_uri_to_folder (url, ex);
	g_free (url);
	return folder;
}

CamelFolder *
mail_tool_get_inbox (const gchar *url, CamelException *ex)
{
	/* FIXME: should be smarter? get_default_folder, etc */
	return mail_tool_get_folder_from_urlname (url, "inbox", 0, ex);
}
	

/* why is this function so stupidly complex when allthe work is done elsehwere? */
char *
mail_tool_do_movemail (const gchar *source_url, CamelException *ex)
{
	gchar *dest_path;
	const gchar *source;
	struct stat sb;
#ifndef MOVEMAIL_PATH
	int tmpfd;
#endif
	g_return_val_if_fail (strncmp (source_url, "mbox:", 5) == 0, NULL);

	/* Set up our destination. */

	dest_path = mail_tool_get_local_movemail_path();

	/* Create a new movemail mailbox file of 0 size */

#ifndef MOVEMAIL_PATH
	tmpfd = open (dest_path, O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);

	if (tmpfd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Couldn't create temporary "
				      "mbox `%s': %s"), dest_path, g_strerror (errno));
		g_free (dest_path);
		return NULL;
	}

	close (tmpfd);
#endif

	/* Skip over "mbox:" plus host part (if any) of url. */

	source = source_url + 5;
	if (!strncmp (source, "//", 2))
		source = strchr (source + 2, '/');


	/* Movemail from source (source_url) to dest_path */

	mail_tool_camel_lock_up();
	camel_movemail (source, dest_path, ex);
	mail_tool_camel_lock_down();

	if (stat (dest_path, &sb) < 0 || sb.st_size == 0) {
		g_free (dest_path);
		return NULL;
	}

	if (camel_exception_is_set (ex)) {
		g_free (dest_path);
		return NULL;
	}

	return dest_path;
}

void
mail_tool_move_folder_contents (CamelFolder *source, CamelFolder *dest, gboolean use_cache, CamelException *ex)
{
	CamelUIDCache *cache;
	GPtrArray *uids;
	int i;
	gboolean summary_capability;

	mail_tool_camel_lock_up();

	camel_object_ref (CAMEL_OBJECT (source));
	camel_object_ref (CAMEL_OBJECT (dest));

	/* Get all uids of source */

	mail_op_set_message (_("Examining %s"), source->full_name);

	uids = camel_folder_get_uids (source);
	printf ("mail_tool_move_folder: got %d messages in source\n", uids->len);

	/* If we're using the cache, ... use it */

	if (use_cache) {
		GPtrArray *new_uids;
		char *url, *p, *filename;

		url = camel_url_to_string (
			CAMEL_SERVICE (source->parent_store)->url, FALSE);
		for (p = url; *p; p++) {
			if (!isascii ((unsigned char)*p) ||
			    strchr (" /'\"`&();|<>${}!", *p))
				*p = '_';
		}
		filename = g_strdup_printf ("%s/config/cache-%s",
					    evolution_dir, url);
		g_free (url);

		cache = camel_uid_cache_new (filename);

		if (cache) {
			new_uids = camel_uid_cache_get_new_uids (cache, uids);
			camel_folder_free_uids (source, uids);
			uids = new_uids;
		} else {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Could not read UID "
					      "cache file \"%s\". You may "
					      "receive duplicate "
					      "messages."), filename);
		}

		g_free (filename);
	} else
		cache = NULL;

	printf ("mail_tool_move_folder: %d of those messages are new\n", uids->len);

	summary_capability = camel_folder_has_summary_capability (source);

	/* Copy the messages */
	for (i = 0; i < uids->len; i++) {
		CamelMimeMessage *msg;
		const CamelMessageInfo *info = NULL;

		/* Info */

		mail_op_set_message (_("Retrieving message %d of %d"),
				     i + 1, uids->len);

		/* Get the message */

		msg = camel_folder_get_message (source, uids->pdata[i], ex);
		if (camel_exception_is_set (ex)) {
			camel_object_unref (CAMEL_OBJECT (msg));
			goto cleanup;
		}
		
		/* Append it to dest */

		mail_op_set_message (_("Writing message %d of %d"),
				     i + 1, uids->len);

		if (summary_capability)
			info = camel_folder_get_message_info (source, uids->pdata[i]);
		camel_folder_append_message (dest, msg, info, ex);
		if (camel_exception_is_set (ex)) {
			camel_object_unref (CAMEL_OBJECT (msg));
			goto cleanup;
		}

		/* (Maybe) get rid of the message */

		camel_object_unref (CAMEL_OBJECT (msg));
		if (!use_cache)
			camel_folder_delete_message (source, uids->pdata[i]);
	}

	/* All done. Sync n' free. */

	if (cache) {
		camel_uid_cache_free_uids (uids);
		
		if (!camel_exception_is_set (ex))
			camel_uid_cache_save (cache);
		camel_uid_cache_destroy (cache);
	} else
		camel_folder_free_uids (source, uids);

	mail_op_set_message (_("Saving changes to %s"), source->full_name);

	camel_folder_sync (source, TRUE, ex);

 cleanup:
	camel_object_unref (CAMEL_OBJECT (source));
	camel_object_unref (CAMEL_OBJECT (dest));
	mail_tool_camel_lock_down();
}

void
mail_tool_set_uid_flags (CamelFolder *folder, const char *uid, guint32 mask, guint32 set)
{
	mail_tool_camel_lock_up ();
	camel_folder_set_message_flags (folder, uid, mask, set);
	mail_tool_camel_lock_down ();
}

char *
mail_tool_generate_forward_subject (CamelMimeMessage *msg)
{
	const char *subject;
	char *fwd_subj, *fromstr;
	const CamelInternetAddress *from;

	/* we need to lock around the whole function, as we are
	   only getting references to the message's data */
	mail_tool_camel_lock_up();

	from = camel_mime_message_get_from(msg);
	subject = camel_mime_message_get_subject(msg);

	if (from) {
		fromstr = camel_address_format((CamelAddress *)from);
		if (subject && *subject) {
			fwd_subj = g_strdup_printf ("[%s] %s", fromstr, subject);
		} else {
			fwd_subj = g_strdup_printf (_("[%s] (forwarded message)"),
						    fromstr);
		}
		g_free(fromstr);
	} else {
		if (subject && *subject) {
			if (strncmp (subject, "Fwd: ", 5) == 0)
				subject += 4;
			fwd_subj = g_strdup_printf ("Fwd: %s", subject);
		} else
			fwd_subj = g_strdup (_("Fwd: (no subject)"));
	}

	mail_tool_camel_lock_down();

	return fwd_subj;
}

void
mail_tool_send_via_transport (CamelTransport *transport, CamelMedium *medium, CamelException *ex)
{
	mail_tool_camel_lock_up();

	/*camel_service_connect (CAMEL_SERVICE (transport), ex);*/

	if (camel_exception_is_set (ex))
		goto cleanup;

	camel_transport_send (transport, medium, ex);

	/*camel_service_disconnect (CAMEL_SERVICE (transport),
	 *camel_exception_is_set (ex) ? NULL : ex);*/

 cleanup:
	mail_tool_camel_lock_down();
}

CamelMimePart *
mail_tool_make_message_attachment (CamelMimeMessage *message)
{
	CamelMimePart *part;
	const char *subject;
	gchar *desc;

	mail_tool_camel_lock_up();
	/*camel_object_ref (CAMEL_OBJECT (message));*/

	subject = camel_mime_message_get_subject (message);
	if (subject)
		desc = g_strdup_printf (_("Forwarded message - %s"), subject);
	else
		desc = g_strdup (_("Forwarded message (no subject)"));

	part = camel_mime_part_new ();
	camel_mime_part_set_disposition (part, "inline");
	camel_mime_part_set_description (part, desc);
	camel_medium_set_content_object (CAMEL_MEDIUM (part),
					 CAMEL_DATA_WRAPPER (message));
	camel_mime_part_set_content_type (part, "message/rfc822");
	/*camel_object_unref (CAMEL_OBJECT (message));*/
	mail_tool_camel_lock_down();
	return part;
}

CamelFolder *
mail_tool_filter_get_folder_func (FilterDriver *d, const char *uri, void *data)
{
	return mail_tool_uri_to_folder_noex (uri);
}

CamelFolder *
mail_tool_get_root_of_store (const char *source_uri, CamelException *ex)
{
	CamelStore *store;
	CamelFolder *folder;

	mail_tool_camel_lock_up();

	store = camel_session_get_store (session, source_uri, ex);
	if (!store) {
		mail_tool_camel_lock_down ();
		return NULL;
	}

	/*camel_service_connect (CAMEL_SERVICE (store), ex);
	 *if (camel_exception_is_set (ex)) {
	 *	camel_object_unref (CAMEL_OBJECT (store));
	 *	mail_tool_camel_lock_down();
	 *	return NULL;
	 *}
	 */

	folder = camel_store_get_root_folder (store, ex);
	camel_object_unref (CAMEL_OBJECT (store));
	mail_tool_camel_lock_down();

	return folder;
}

CamelFolder *
mail_tool_uri_to_folder (const char *uri, CamelException *ex)
{
	CamelURL *url;
	CamelStore *store = NULL;
	CamelFolder *folder = NULL;

	url = camel_url_new (uri, ex);
	if (!url)
		return NULL;

	if (!strcmp (url->protocol, "vfolder")) {
		folder = vfolder_uri_to_folder (uri, ex);
	} else {
		mail_tool_camel_lock_up ();
		store = camel_session_get_store (session, uri, ex);
		if (store) {
			char *name;

			if (url->path && *url->path)
				name = url->path + 1;
			else
				name = "";
			folder = camel_store_get_folder (
				store, name, CAMEL_STORE_FOLDER_CREATE, ex);
		}
		mail_tool_camel_lock_down ();
	}

	if (camel_exception_is_set (ex)) {
		if (folder) {
			camel_object_unref (CAMEL_OBJECT (folder));
			folder = NULL;
		}
	}
	if (store)
		camel_object_unref (CAMEL_OBJECT (store));
	camel_url_free (url);

	return folder;
}

CamelFolder *
mail_tool_uri_to_folder_noex (const char *uri)
{
	CamelException ex;
	CamelFolder *result;

	camel_exception_init (&ex);
	result = mail_tool_uri_to_folder (uri, &ex);

	if (camel_exception_is_set (&ex)) {
		gchar *msg;
		GtkWidget *dialog;

		msg = g_strdup_printf (_("Cannot open location `%s':\n"
				       "%s"),
				       uri,
				       camel_exception_get_description (&ex));
		dialog = gnome_error_dialog (msg);
		g_free (msg);
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}

	return result;
}

/**
 * mail_tool_quote_message:
 * @message: mime message to quote
 * @fmt: credits format - example: "On %s, %s wrote:\n"
 * @Varargs: arguments
 *
 * Returns an allocated buffer containing the quoted message.
 */
gchar *
mail_tool_quote_message (CamelMimeMessage *message, const char *fmt, ...)
{
	CamelDataWrapper *contents;
	gboolean want_plain, is_html;
	gchar *text;
	
	want_plain = !mail_config_send_html ();
	contents = camel_medium_get_content_object (CAMEL_MEDIUM (message));
	text = mail_get_message_body (contents, want_plain, &is_html);
	
	/* Set the quoted reply text. */
	if (text) {
		gchar *ret_text, *credits = NULL;
		
		/* create credits */
		if (fmt) {
			gchar *buf;
			va_list ap;
			
			va_start (ap, fmt);
			credits = g_strdup_vprintf (fmt, ap);
			va_end (ap);
		}
		
		if (is_html) {
			if (credits) {
				ret_text = g_strdup_printf ("<blockquote><i>\n%s\n%s\n"
							    "</i></blockquote>\n",
							    credits, text);
			} else {
				ret_text = g_strdup_printf ("<blockquote><i>\n%s\n"
							    "</i></blockquote>\n",
							    text);
			}
		} else {
			gchar *s, *d, *quoted_text;
			gint lines, len, offset = 0;
			
			/* Count the number of lines in the body. If
			 * the text ends with a \n, this will be one
			 * too high, but that's ok. Allocate enough
			 * space for the text and the "> "s.
			 */
			for (s = text, lines = 0; s; s = strchr (s + 1, '\n'))
				lines++;
			
			offset = credits ? strlen (credits) : 0;
			quoted_text = g_malloc (offset + strlen (text) + lines * 2);
			
			if (credits)
				memcpy (quoted_text, credits, offset);
			
			s = text;
			d = quoted_text + offset;
			
			/* Copy text to quoted_text line by line,
			 * prepending "> ".
			 */
			while (1) {
				len = strcspn (s, "\n");
				if (len == 0 && !*s)
					break;
				sprintf (d, "> %.*s\n", len, s);
				s += len;
				if (!*s++)
					break;
				d += len + 3;
			}
			*d = '\0';
			
			/* Now convert that to HTML. */
			ret_text = e_text_to_html (quoted_text, E_TEXT_TO_HTML_PRE);
			g_free (quoted_text);
		}
		
		g_free (text);
		return ret_text;
	}
	
	return NULL;
}
