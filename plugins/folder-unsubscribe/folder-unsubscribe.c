/*
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
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <string.h>

#include <camel/camel-session.h>
#include <camel/camel-store.h>
#include <camel/camel-url.h>

#include "mail/em-popup.h"
#include "mail/mail-mt.h"
#include "mail/mail-ops.h"

void org_gnome_mail_folder_unsubscribe (EPlugin *plug, EMPopupTargetFolder *target);

struct _folder_unsub_t {
	MailMsg base;

	gchar *uri;
};

static gchar *
folder_unsubscribe_desc (struct _folder_unsub_t *msg)
{
	return g_strdup_printf (
		_("Unsubscribing from folder \"%s\""), msg->uri);
}

extern CamelSession *session;

static void
folder_unsubscribe_exec (struct _folder_unsub_t *msg)
{
	const gchar *path = NULL;
	CamelStore *store;
	CamelURL *url;

	if (!(store = camel_session_get_store (session, msg->uri, &msg->base.ex)))
		return;

	url = camel_url_new (msg->uri, NULL);
	if (((CamelService *) store)->provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
		path = url->fragment;
	else if (url->path && url->path[0])
		path = url->path + 1;

	if (path != NULL)
		camel_store_unsubscribe_folder (store, path, &msg->base.ex);

	camel_url_free (url);
}

static void
folder_unsubscribe_free (struct _folder_unsub_t *msg)
{
	g_free (msg->uri);
}

static MailMsgInfo unsubscribe_info = {
	sizeof (struct _folder_unsub_t),
	(MailMsgDescFunc) folder_unsubscribe_desc,
	(MailMsgExecFunc) folder_unsubscribe_exec,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) folder_unsubscribe_free
};

void
org_gnome_mail_folder_unsubscribe (EPlugin *plug, EMPopupTargetFolder *target)
{
	struct _folder_unsub_t *unsub;

	if (target->uri == NULL)
		return;

	unsub = mail_msg_new (&unsubscribe_info);
	unsub->uri = g_strdup (target->uri);

	mail_msg_unordered_push (unsub);
}
