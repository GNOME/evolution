/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@novell.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
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
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
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
	struct _mail_msg msg;
	
	char *uri;
};

static char *
folder_unsubscribe__desc (struct _mail_msg *mm, int done)
{
	struct _folder_unsub_t *unsub = (struct _folder_unsub_t *) mm;
	
	return g_strdup_printf (_("Unsubscribing from folder \"%s\""), unsub->uri);
}

static void
folder_unsubscribe__unsub (struct _mail_msg *mm)
{
	struct _folder_unsub_t *unsub = (struct _folder_unsub_t *) mm;
	extern CamelSession *session;
	const char *path = NULL;
	CamelStore *store;
	CamelURL *url;
	
	if (!(store = camel_session_get_store (session, unsub->uri, &mm->ex)))
		return;
	
	url = camel_url_new (unsub->uri, NULL);
	if (((CamelService *) store)->provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
		path = url->fragment;
	else if (url->path && url->path[0])
		path = url->path + 1;
	
	if (path != NULL)
		camel_store_unsubscribe_folder (store, path, &mm->ex);
	
	camel_url_free (url);
}

static void
folder_unsubscribe__free (struct _mail_msg *mm)
{
	struct _folder_unsub_t *unsub = (struct _folder_unsub_t *) mm;
	
	g_free (unsub->uri);
}

static struct _mail_msg_op unsubscribe_op = {
	folder_unsubscribe__desc,
	folder_unsubscribe__unsub,
	NULL,
	folder_unsubscribe__free,
};


void
org_gnome_mail_folder_unsubscribe (EPlugin *plug, EMPopupTargetFolder *target)
{
	struct _folder_unsub_t *unsub;
	
	if (target->uri == NULL)
		return;
	
	unsub = mail_msg_new (&unsubscribe_op, NULL, sizeof (struct _folder_unsub_t));
	unsub->uri = g_strdup (target->uri);
	
	e_thread_put (mail_thread_new, (EMsg *) unsub);
}
