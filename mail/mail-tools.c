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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <errno.h>
#include "camel/camel.h"
#include "camel/camel-vee-folder.h"
#include "mail-vfolder.h"
#include "filter/vfolder-rule.h"
#include "filter/vfolder-context.h"
#include "filter/filter-option.h"
#include "filter/filter-input.h"
#include "mail.h" /*session*/
#include "mail-tools.h"
#include "mail-local.h"
#include "e-util/e-html-utils.h"

/* **************************************** */

CamelFolder *
mail_tool_get_folder_from_urlname (const gchar *url, const gchar *name,
				   guint32 flags, CamelException *ex)
{
	CamelStore *store;
	CamelFolder *folder;

	store = camel_session_get_store (session, url, ex);
	if (!store)
		return NULL;

	folder = camel_store_get_folder (store, name, flags, ex);
	camel_object_unref (CAMEL_OBJECT (store));

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
	CamelStore *store;
	CamelFolder *folder;

	store = camel_session_get_store (session, url, ex);
	if (!store)
		return NULL;

	folder = camel_store_get_inbox (store, ex);
	camel_object_unref (CAMEL_OBJECT (store));

	return folder;
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
	camel_movemail (source, dest_path, ex);

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

char *
mail_tool_generate_forward_subject (CamelMimeMessage *msg)
{
	const char *subject;
	char *fwd_subj, *fromstr;
	const CamelInternetAddress *from;

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

	return fwd_subj;
}

CamelMimePart *
mail_tool_make_message_attachment (CamelMimeMessage *message)
{
	CamelMimePart *part;
	const char *subject;
	gchar *desc;

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
	g_free(desc);
	/*camel_object_unref (CAMEL_OBJECT (message));*/
	return part;
}

CamelFolder *
mail_tool_uri_to_folder (const char *uri, CamelException *ex)
{
	CamelURL *url;
	CamelStore *store = NULL;
	CamelFolder *folder = NULL;
	int offset = 0;
	
	g_return_val_if_fail (uri != NULL, NULL);
	
	if (!strncmp (uri, "vtrash:", 7))
		offset = 7;
	
	url = camel_url_new (uri + offset, ex);
	if (!url)
		return NULL;
	
	if (!strcmp (url->protocol, "vfolder")) {
		folder = vfolder_uri_to_folder (uri, ex);
	} else {
		store = camel_session_get_store (session, uri + offset, ex);
		if (store) {
			char *name;
			
			if (url->path && *url->path)
				name = url->path + 1;
			else
				name = "";

			if (offset)
				folder = camel_store_get_trash (store, ex);
			else
				folder = camel_store_get_folder (store, name,
								 CAMEL_STORE_FOLDER_CREATE, ex);
		}
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
	
	want_plain = !mail_config_get_send_html ();
	contents = camel_medium_get_content_object (CAMEL_MEDIUM (message));
	text = mail_get_message_body (contents, want_plain, &is_html);
	
	/* Set the quoted reply text. */
	if (text) {
		gchar *ret_text, *credits = NULL;
		
		/* create credits */
		if (fmt) {
			va_list ap;
			
			va_start (ap, fmt);
			credits = g_strdup_vprintf (fmt, ap);
			va_end (ap);
		}
		
		if (is_html) {
			ret_text = g_strdup_printf ("%s<!--+GtkHTML:<DATA class=\"ClueFlow\" key=\"orig\" value=\"1\">-->"
						    "<blockquote><i><font color=\"%06x\">\n%s\n"
						    "</font></i></blockquote>"
						    "<!--+GtkHTML:<DATA class=\"ClueFlow\" clear=\"orig\">-->",
						    credits ? credits : "",
						    mail_config_get_citation_color (), text);
		} else {
			gchar *s, *d, *quoted_text, *orig_text;
			gint lines, len;
			
			/* Count the number of lines in the body. If
			 * the text ends with a \n, this will be one
			 * too high, but that's ok. Allocate enough
			 * space for the text and the "> "s.
			 */
			for (s = text, lines = 0; s; s = strchr (s + 1, '\n'))
				lines++;
			
			/* offset is the size of the credits, strlen (text)
			 * covers the body, lines * 2 does the "> "s, and
			 * the last +2 covers the final "\0", plus an extra
			 * "\n" in case text doesn't end with one.
			 */
			quoted_text = g_malloc (strlen (text) + lines * 2 + 2);
			
			s = text;
			d = quoted_text;
			
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
			orig_text = e_text_to_html_full (quoted_text, E_TEXT_TO_HTML_PRE
							 | (mail_config_get_citation_highlight ()
							    ? E_TEXT_TO_HTML_MARK_CITATION : 0),
							 mail_config_get_citation_color ());
			g_free (quoted_text);
			ret_text = g_strdup_printf ("%s<!--+GtkHTML:<DATA class=\"ClueFlow\" key=\"orig\" value=\"1\">-->"
						    "%s"
						    "<!--+GtkHTML:<DATA class=\"ClueFlow\" clear=\"orig\">-->",
						    credits ? credits : "",
						    orig_text);
			g_free (orig_text);
		}
		
		g_free (text);
		printf ("ret: %s\n", ret_text);
		return ret_text;
	}
	
	return NULL;
}

/**
 * mail_tool_forward_message:
 * @message: mime message to quote
 *
 * Returns an allocated buffer containing the forwarded message.
 */
gchar *
mail_tool_forward_message (CamelMimeMessage *message)
{
	CamelDataWrapper *contents;
	gboolean want_plain, is_html;
	gchar *text;
	
	want_plain = !mail_config_get_send_html ();
	contents = camel_medium_get_content_object (CAMEL_MEDIUM (message));
	text = mail_get_message_body (contents, want_plain, &is_html);
	
	/* Set the quoted reply text. */
	if (text) {
		gchar *ret_text, *credits = NULL;
		const CamelInternetAddress *cia;
		const char *subject;
		char *buf, *from, *to;
		
		/* create credits */
		cia = camel_mime_message_get_from (message);
		buf = camel_address_format (CAMEL_ADDRESS (cia));
		from = e_text_to_html (buf, E_TEXT_TO_HTML_CONVERT_NL | E_TEXT_TO_HTML_CONVERT_URLS);
		g_free (buf);
		
		cia = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
		buf = camel_address_format (CAMEL_ADDRESS (cia));
		to = e_text_to_html (buf, E_TEXT_TO_HTML_CONVERT_NL | E_TEXT_TO_HTML_CONVERT_URLS);
		g_free (buf);
		
		subject = camel_mime_message_get_subject (message);
		
		credits = g_strdup_printf (_("-----Forwarded Message-----<br>"
					     "<b>From:</b> %s<br>"
					     "<b>To:</b> %s<br>"
					     "<b>Subject:</b> %s<br>"),
					   from, to, subject);
		g_free (from);
		g_free (to);
		
		if (!is_html) {
			/* Now convert that to HTML. */
			ret_text = e_text_to_html (text, E_TEXT_TO_HTML_PRE);
			g_free (text);
			text = ret_text;
		}
		
		ret_text = g_strdup_printf ("%s<br>%s\n", credits, text);
		
		g_free (credits);
		g_free (text);
		
		return ret_text;
	}
	
	return NULL;
}
