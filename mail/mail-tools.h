/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Peter Williams <peterw@ximian.com>
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

#ifndef MAIL_TOOLS_H
#define MAIL_TOOLS_H

#include <camel/camel.h>
#include <camel/camel-filter-driver.h> /*eek*/

typedef struct _xevolution {
	char *flags;
	char *source;
	char *transport;
	char *account;
	char *fcc;
	char *format;
	char *postto;
} XEvolution;

/* Get the CamelFolder for the local inbox */
CamelFolder *mail_tool_get_local_inbox (CamelException *ex);

/* Get the "inbox" for a url (uses global session) */
CamelFolder *mail_tool_get_inbox (const char *url, CamelException *ex);

/* Get the "trash" for a url (uses global session) */
CamelFolder *mail_tool_get_trash (const char *url, int connect, CamelException *ex);

/* Does a camel_movemail into the local movemail folder
 * and returns the path to the new movemail folder that was created. which shoudl be freed later */
char *mail_tool_do_movemail (const char *source_url, CamelException *ex);

XEvolution *mail_tool_remove_xevolution_headers (CamelMimeMessage *message);
void mail_tool_restore_xevolution_headers (CamelMimeMessage *message, XEvolution *xev);
void mail_tool_destroy_xevolution (XEvolution *xev);

/* Generates the subject for a message forwarding @msg */
gchar *mail_tool_generate_forward_subject (CamelMimeMessage *msg);

/* Make a message into an attachment */
CamelMimePart *mail_tool_make_message_attachment (CamelMimeMessage *message);

/* Parse the ui into a real CamelFolder any way we know how. */
CamelFolder *mail_tool_uri_to_folder (const char *uri, guint32 flags, CamelException *ex);

GHashTable *mail_lookup_url_table (CamelMimeMessage *mime_message);

gchar *mail_tool_quote_message (CamelMimeMessage *message, const char *fmt, ...);

gchar *mail_tool_forward_message (CamelMimeMessage *message, gboolean quoted);

CamelFolder *mail_tools_x_evolution_message_parse (char *in, unsigned int inlen, GPtrArray **uids);

char *mail_tools_folder_to_url (CamelFolder *folder);

struct _EMeta *mail_tool_get_meta_data(const char *uri);
void mail_tool_delete_meta_data(const char *uri);

#endif
