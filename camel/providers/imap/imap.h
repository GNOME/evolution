/*  Spruce
 *  Copyright (C) 1999-2000 Jeffrey Stedfast
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef __IMAP_H__
#define __IMAP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtk.h>
#undef MIN
#undef MAX
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "parse.h"
#include "server.h"
#include "mime.h"
#include "cram-md5.h"

struct imap_account
{
	Server server;
	gchar *username;
	gchar *password;
	gint socket;
};

gint imap_ok (gint tag, gchar *line);

gint imap_login_cram_md5(gint socket, gchar *username, gchar *password);

gint imap_login (gint socket, gchar *username, gchar *password);

GList *imap_list (gint socket, gchar *namespace);

gint imap_select_mailbox (gint socket, gchar *mailbox, gchar *namespace);

gint imap_logout (gint socket);

gint imap_mailbox_create (gint socket, gchar *mailbox);

gint imap_mailbox_delete (gint socket, gchar *mailbox);

gchar *imap_fetch (gint socket, gint mesgnum, gchar *part, gint *seen);

gboolean imap_delete (const ImapAccount_t *imap, GList *sorted);

gint imap_connect(Server *server);

gint imap_add_part(gchar *c);

gint imap_parts (gint socket, gint mesg_num);

gint imap_get_string (gchar *index, gchar *dest, gint destlen, gchar *def);

gint imap_get_number (gchar *index, gint *dest);

gint imap_skip_section(gchar *index);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

