/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#include "Evolution.h"
#include "evolution-storage.h"
#include "evolution-shell-component.h"

#include "mail.h"
#include "mail-vfolder.h"
#include "mail-vtrash.h"
#include "mail-tools.h"
#include "mail-mt.h"

#include "camel/camel.h"

#define d(x)

extern char *evolution_dir;
extern CamelSession *session;


/**
 * mail_vtrash_add: add a "vTrash" folder on the EvolutionStorage
 * @store: the CamelStore that the vTrash exists on
 * @store_uri: the URL of the store
 * @name: the name to give the vTrash folder
 *
 * Creates the vTrash folder for the provided store in the folder view
 * (EvolutionStorage) and creates the URL for that vTrash folder.
 **/
void
mail_vtrash_add (CamelStore *store, const char *store_uri, const char *name)
{
	EvolutionStorage *storage;
	char *uri, *path;
	
	g_return_if_fail (CAMEL_IS_STORE (store));
	
	uri = g_strdup_printf ("vtrash:%s", store_uri);
	
	if (!strcmp (store_uri, "file:/")) {
		storage = mail_vfolder_get_vfolder_storage ();
	} else {
		storage = mail_lookup_storage (store);
	}
	
	if (!storage) {
		g_free (uri);
		return;
	}
	
	path = g_strdup_printf ("/%s", name);
	evolution_storage_new_folder (storage, path, g_basename (path),
				      "mail", uri, name, FALSE);
	gtk_object_unref (GTK_OBJECT (storage));
	
	g_free (path);
}

struct _get_trash_msg {
	struct _mail_msg msg;
	
	CamelStore *store;
	char *store_uri;
	char *name;
};

static char *
get_trash_desc (struct _mail_msg *mm, int done)
{
	struct _get_trash_msg *m = (struct _get_trash_msg *)mm;
	
	return g_strdup_printf (_("Opening Trash folder for %s"), m->store_uri);
}

static void
get_trash_get (struct _mail_msg *mm)
{
	struct _get_trash_msg *m = (struct _get_trash_msg *)mm;
	
	camel_operation_register (mm->cancel);
	camel_operation_start (mm->cancel, _("Getting matches"));
	
	/* we don't want to connect */
	m->store = (CamelStore *) camel_session_get_service (session, m->store_uri,
							     CAMEL_PROVIDER_STORE, &mm->ex);
	if (m->store == NULL) {
		g_warning ("Couldn't get service %s: %s\n", m->store_uri,
			   camel_exception_get_description (&mm->ex));
		camel_exception_clear (&mm->ex);
	}
	
	camel_operation_end (mm->cancel);
	camel_operation_unregister (mm->cancel);
}

static void
get_trash_got (struct _mail_msg *mm)
{
	struct _get_trash_msg *m = (struct _get_trash_msg *)mm;
	
	if (m->store)
		mail_vtrash_add (m->store, m->store_uri, m->name);
}

static void
get_trash_free (struct _mail_msg *mm)
{
	struct _get_trash_msg *m = (struct _get_trash_msg *)mm;
	
	if (m->store)
		camel_object_unref (CAMEL_OBJECT (m->store));
	
	g_free (m->store_uri);
	g_free (m->name);
}

static struct _mail_msg_op get_trash_op = {
	get_trash_desc,
	get_trash_get,
	get_trash_got,
	get_trash_free,
};


/**
 * mail_vtrash_create: Create a vTrash folder
 * @store_uri: URL of the CamelStore
 * @name: name to give the vTrash folder
 *
 * Async function to lookup the CamelStore corresponding to @store_uri
 * and then calls mail_vtrash_add() to create the vTrash folder/URL on
 * the EvolutionStorage.
 **/
int
mail_vtrash_create (const char *store_uri, const char *name)
{
	struct _get_trash_msg *m;
	int id;
	
	m = mail_msg_new (&get_trash_op, NULL, sizeof (*m));
	m->store_uri = g_strdup (store_uri);
	m->name = g_strdup (name);
	
	id = m->msg.seq;
	e_thread_put (mail_thread_new, (EMsg *)m);
	
	return id;
}
