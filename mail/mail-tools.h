/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Peter Williams <peterw@helixcode.com>
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

#ifndef MAIL_TOOLS_H
#define MAIL_TOOLS_H

#include <camel/camel.h>
#include <filter/filter-driver.h> /*eek*/

/* A global recursive lock on Camel */
void mail_tool_camel_lock_up (void);
void mail_tool_camel_lock_down (void);

/* Get a CamelFolder from a root url and a foldername (uses the global session)*/
CamelFolder *
mail_tool_get_folder_from_urlname (const gchar *url, const gchar *name,
				   gboolean create, CamelException *ex);

/* Get a useful name for a given CamelFolder (ie, not "mbox") */
const gchar *mail_tool_get_folder_name (CamelFolder *folder);

/* Get the url for the local inbox */
gchar *mail_tool_get_local_inbox_url (void);

/* Get the filename for our movemail folder or storage */
gchar *mail_tool_get_local_movemail_path (void);
gchar *mail_tool_get_local_movemail_url (void);

/* Get the CamelFolder for the local inbox */
CamelFolder *mail_tool_get_local_inbox (CamelException *ex);

/* Get the "inbox" for a url (uses global session) */
CamelFolder *mail_tool_get_inbox (const gchar *url, CamelException *ex);

/* Does a camel_movemail into the local movemail folder
 * and returns the movemail folder that was created. */
CamelFolder *
mail_tool_do_movemail (const gchar *source_url, CamelException *ex);

/* Transfers all the messages from source into dest;
 * source is emptied and synced. */
void
mail_tool_move_folder_contents (CamelFolder *source, CamelFolder *dest, gboolean use_cache, CamelException *ex);

/* Sets the flags on a message represented by a UID in a folder. */
void
mail_tool_set_uid_flags (CamelFolder *folder, const char *uid, guint32 mask, guint32 set);

/* Generates the subject for a message forwarding @msg */
gchar *
mail_tool_generate_forward_subject (CamelMimeMessage *msg);

/* Sends the medium over transport */
void
mail_tool_send_via_transport (CamelTransport *transport, CamelMedium *medium, CamelException *ex);

/* Make a message into an attachment */
CamelMimePart *
mail_tool_make_message_attachment (CamelMimeMessage *message);

/* Fetch mail from the source URL's inbox into a searchable folder.
 * (not guaranteed to be local). Returns the searchable folder. */
CamelFolder *
mail_tool_fetch_mail_into_searchable (const char *source_url, gboolean keep_on_server, CamelException *ex);

/* Filter source into dest using the default filters. */
void
mail_tool_filter_contents_into (CamelFolder *source, CamelFolder *dest, 
				gboolean delete_source,
				gpointer hook_func, gpointer hook_data,
				CamelException *ex);

/* Get the root folder of the store specified by @source_uri */
CamelFolder *
mail_tool_get_root_of_store (const char *source_uri, CamelException *ex);

/* Parse the ui into a real CamelFolder any way we know how. */
CamelFolder *
mail_tool_uri_to_folder (const char *uri, CamelException *ex);

/* Same as above taking no exceptions, popping up a GnomeErrorDialog
 * if any problems occur. */
CamelFolder *
mail_tool_uri_to_folder_noex (const char *uri);

GHashTable *
mail_lookup_url_table (CamelMimeMessage *mime_message);

/* Appropriate for filter_driver_run */
CamelFolder *
mail_tool_filter_get_folder_func (FilterDriver *d, const char *uri, void *data);


#endif
