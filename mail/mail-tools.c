/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Authors: 
 *  Dan Winship <danw@ximian.com>
 *  Peter Williams <peterw@ximian.com>
 *  Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <camel/camel.h>
#include <camel/camel-vee-folder.h>

#include <filter/vfolder-rule.h>
#include <filter/vfolder-context.h>
#include <filter/filter-option.h>
#include <filter/filter-input.h>

#include "e-util/e-meta.h"

#include "mail.h" /*session*/
#include "mail-component.h"
#include "mail-config.h"
#include "mail-vfolder.h"
#include "mail-tools.h"
#include "mail-mt.h"
#include "mail-folder-cache.h"


/* **************************************** */

CamelFolder *
mail_tool_get_local_inbox (CamelException *ex)
{
	CamelFolder *folder;
	char *url;
	
	url = g_strdup_printf("file://%s/local/Inbox",
			      mail_component_peek_base_directory (mail_component_peek ()));
	folder = mail_tool_uri_to_folder (url, 0, ex);
	g_free (url);
	
	return folder;
}

CamelFolder *
mail_tool_get_inbox (const gchar *url, CamelException *ex)
{
	CamelStore *store;
	CamelFolder *folder;
	
	store = camel_session_get_store (session, url, ex);
	if (!store)
		return NULL;
	
	folder = camel_store_get_inbox (store, ex);
	camel_object_unref (store);
	
	return folder;
}

CamelFolder *
mail_tool_get_trash (const gchar *url, int connect, CamelException *ex)
{
	CamelStore *store;
	CamelFolder *trash;
	
	if (connect)
		store = camel_session_get_store (session, url, ex);
	else
		store = (CamelStore *) camel_session_get_service (session, url, CAMEL_PROVIDER_STORE, ex);
	
	if (!store)
		return NULL;
	
	if (connect || ((CamelService *) store)->status == CAMEL_SERVICE_CONNECTED)
		trash = camel_store_get_trash (store, ex);
	else
		trash = NULL;
	
	camel_object_unref (store);
	
	return trash;
}

static char *
mail_tool_get_local_movemail_path (const unsigned char *uri)
{
	unsigned char *safe_uri, *c;
	char *path;
	
	safe_uri = g_strdup (uri);
	for (c = safe_uri; *c; c++)
		if (strchr ("/:;=|%&#!*^()\\, ", *c) || !isprint ((int) *c))
			*c = '_';
	
	path = g_strdup_printf ("%s/local/Inbox/movemail.%s",
				mail_component_peek_base_directory (mail_component_peek ()),
				safe_uri);
	g_free (safe_uri);
	
	return path;
}

char *
mail_tool_do_movemail (const char *source_url, CamelException *ex)
{
	char *dest_path;
	struct stat sb;
	CamelURL *uri;

	uri = camel_url_new(source_url, ex);
	if (uri == NULL)
		return NULL;

	if (strcmp(uri->protocol, "mbox") != 0) {
		/* FIXME: use right text here post 1.4 */
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				      _("Could not parse URL `%s'"),
				      source_url);
		camel_url_free(uri);
		return NULL;
	}
	
	/* Set up our destination. */
	dest_path = mail_tool_get_local_movemail_path (source_url);
	
	/* Movemail from source (source_url) to dest_path */
	camel_movemail (uri->path, dest_path, ex);
	camel_url_free(uri);

	if (stat (dest_path, &sb) < 0 || sb.st_size == 0) {
		unlink (dest_path); /* Clean up the movemail.foo file. */
		g_free (dest_path);
		return NULL;
	}
	
	if (camel_exception_is_set (ex)) {
		g_free (dest_path);
		return NULL;
	}
	
	return dest_path;
}

char *
mail_tool_generate_forward_subject (CamelMimeMessage *msg)
{
	const char *subject;
	char *fwd_subj;
	const int max_subject_length = 1024;
	
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
		char *fromstr;
		
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

XEvolution *
mail_tool_remove_xevolution_headers (CamelMimeMessage *message)
{
	XEvolution *xev;
	
	xev = g_new (XEvolution, 1);
	xev->flags = g_strdup (camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution"));
	xev->source = g_strdup (camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Source"));
	xev->transport = g_strdup (camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Transport"));
	xev->account = g_strdup (camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Account"));
	xev->fcc = g_strdup (camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Fcc"));
	xev->format = g_strdup (camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Format"));
	xev->postto = g_strdup (camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-PostTo"));
	
	/* rip off the X-Evolution* headers */
	camel_medium_remove_header (CAMEL_MEDIUM (message), "X-Evolution");
	camel_medium_remove_header (CAMEL_MEDIUM (message), "X-Evolution-Source");
	camel_medium_remove_header (CAMEL_MEDIUM (message), "X-Evolution-Transport");
	camel_medium_remove_header (CAMEL_MEDIUM (message), "X-Evolution-Account");
	camel_medium_remove_header (CAMEL_MEDIUM (message), "X-Evolution-Fcc");
	camel_medium_remove_header (CAMEL_MEDIUM (message), "X-Evolution-Format");
	camel_medium_remove_header (CAMEL_MEDIUM (message), "X-Evolution-PostTo");
	
	return xev;
}

void
mail_tool_restore_xevolution_headers (CamelMimeMessage *message, XEvolution *xev)
{
	if (xev->flags)
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution", xev->flags);
	if (xev->source)
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Source", xev->source);
	if (xev->transport)
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Transport", xev->transport);
	if (xev->account)
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Account", xev->account);
	if (xev->fcc)
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Fcc", xev->fcc);
	if (xev->format)
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Format", xev->format);
	if (xev->postto)
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-PostTo", xev->postto);
}

void
mail_tool_destroy_xevolution (XEvolution *xev)
{
	g_free (xev->flags);
	g_free (xev->source);
	g_free (xev->transport);
	g_free (xev->account);
	g_free (xev->format);
	g_free (xev->fcc);
	g_free (xev->postto);
	g_free (xev);
}

CamelMimePart *
mail_tool_make_message_attachment (CamelMimeMessage *message)
{
	CamelMimePart *part;
	const char *subject;
	XEvolution *xev;
	char *desc;
	
	subject = camel_mime_message_get_subject (message);
	if (subject)
		desc = g_strdup_printf (_("Forwarded message - %s"), subject);
	else
		desc = g_strdup (_("Forwarded message"));
	
	/* rip off the X-Evolution headers */
	xev = mail_tool_remove_xevolution_headers (message);
	mail_tool_destroy_xevolution (xev);
	
	/* remove Bcc headers */
	while (camel_medium_get_header (CAMEL_MEDIUM (message), "Bcc"))
		camel_medium_remove_header (CAMEL_MEDIUM (message), "Bcc");
	
	part = camel_mime_part_new ();
	camel_mime_part_set_disposition (part, "inline");
	camel_mime_part_set_description (part, desc);
	camel_medium_set_content_object (CAMEL_MEDIUM (part),
					 CAMEL_DATA_WRAPPER (message));
	camel_mime_part_set_content_type (part, "message/rfc822");
	g_free (desc);
	
	return part;
}

CamelFolder *
mail_tool_uri_to_folder (const char *uri, guint32 flags, CamelException *ex)
{
	CamelURL *url;
	CamelStore *store = NULL;
	CamelFolder *folder = NULL;
	int offset = 0;
	
	g_return_val_if_fail (uri != NULL, NULL);
	
	/* This hack is still needed for file:/ since it's its own EvolutionStorage type */
	if (!strncmp (uri, "vtrash:", 7))
		offset = 7;
	
	url = camel_url_new (uri + offset, ex);
	if (!url) {
		return NULL;
	}

	store = camel_session_get_store (session, uri + offset, ex);
	if (store) {
		const char *name;

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
		
		if (offset)
			folder = camel_store_get_trash (store, ex);
		else
			folder = camel_store_get_folder (store, name, flags, ex);
		camel_object_unref (store);
	}
	
	if (folder)
		mail_note_folder (folder);
	
	camel_url_free (url);
	
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
mail_tools_x_evolution_message_parse (char *in, unsigned int inlen, GPtrArray **uids)
{
	/* format: "uri\0uid1\0uid2\0uid3\0...\0uidn" */
	char *inptr, *inend;
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
		char *start = inptr;
		
		while (inptr < inend && *inptr)
			inptr++;
		
		g_ptr_array_add (*uids, g_strndup (start, inptr - start));
		inptr++;
	}
	
	return folder;
}


char *
mail_tools_folder_to_url (CamelFolder *folder)
{
	char *service_url, *url;
	const char *full_name;
	CamelService *service;
	
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	
	full_name = folder->full_name;
	while (*full_name == '/')
		full_name++;
	
	service = (CamelService *) folder->parent_store;
	service_url = camel_url_to_string (service->url, CAMEL_URL_HIDE_ALL);
	url = g_strdup_printf ("%s%s%s", service_url, service_url[strlen (service_url)-1] != '/' ? "/" : "",
			       full_name);
	g_free (service_url);
	
	return url;
}

static char *meta_data_key(const char *uri, char **pathp)
{
	const char *base_directory = mail_component_peek_base_directory (mail_component_peek ());
	CamelURL *url;
	GString *path;
	const char *key;
	char *p, c;

	url = camel_url_new(uri, NULL);

	if (url == NULL) {
		g_warning("Trying to retrieve meta-data for unparsable uri: %s", uri);
		*pathp = g_build_path(base_directory, "meta/unknown", NULL);

		return g_strdup("folder");
	}

	path = g_string_new(base_directory);
	g_string_append_printf(path, "/meta/%s/", url->protocol);

	if (url->host && url->host[0]) {
		if (url->user)
			g_string_append_printf(path, "%s@", url->user);
		g_string_append(path, url->host);
		if (url->port)
			g_string_append_printf(path, ":%d", url->port);
		key = url->path;
	} else if (url->path) {
		if (url->fragment) {
			p = url->path;
			while ((c = *p++)) {
				if (c == '/')
					c = '_';
				g_string_append_c(path, c);
			}
			key = url->fragment;
		} else {
			key = url->path;
		}
	}

	if (key == NULL)
		key = uri;

	p = g_strdup(key);
	camel_url_free(url);
	*pathp = path->str;
	g_string_free(path, FALSE);

	return p;
}

EMeta *
mail_tool_get_meta_data(const char *uri)
{
	char *path, *key;
	EMeta *meta;

	key = meta_data_key(uri, &path);
	meta = e_meta_data_find(path, key);
	g_free(key);
	g_free(path);

	return meta;
}

void
mail_tool_delete_meta_data(const char *uri)
{
	char *path, *key;

	key = meta_data_key(uri, &path);
	e_meta_data_delete(path, key);
	g_free(key);
	g_free(path);
}
