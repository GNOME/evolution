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
#include "camel/camel-vee-store.h"
#include "filter/vfolder-rule.h"
#include "filter/filter-part.h"

#define d(x)

#define VTRASH_LOCK(x) pthread_mutex_lock(&x)
#define VTRASH_UNLOCK(x) pthread_mutex_unlock(&x)

static GHashTable *vtrash_hash = NULL;
static pthread_mutex_t vtrash_hash_lock = PTHREAD_MUTEX_INITIALIZER;

extern char *evolution_dir;
extern CamelSession *session;

/* maps the shell's uri to the real vtrash folder */
CamelFolder *
vtrash_uri_to_folder (const char *uri, CamelException *ex)
{
        CamelFolder *folder = NULL;
        
        g_return_val_if_fail (uri != NULL, NULL);
        
        if (strncmp (uri, "vtrash:", 7))
                        return NULL;
        
        VTRASH_LOCK (vtrash_hash_lock);
        if (vtrash_hash) {
                folder = g_hash_table_lookup (vtrash_hash, uri);
                
                camel_object_ref (CAMEL_OBJECT (folder));
        }
        VTRASH_UNLOCK (vtrash_hash_lock);
        
        return folder;
}

static void
vtrash_add (CamelStore *store, CamelFolder *folder, const char *store_uri, const char *name)
{
	EvolutionStorage *storage;
	char *uri, *path;
	
	g_return_if_fail (CAMEL_IS_STORE (store));
	
	uri = g_strdup_printf ("vtrash:%s", store_uri);
	
	VTRASH_LOCK (vtrash_hash_lock);
	
	if (!vtrash_hash) {
		vtrash_hash = g_hash_table_new (g_str_hash, g_str_equal);
	} else if (g_hash_table_lookup (vtrash_hash, uri) != NULL) {
		VTRASH_UNLOCK (vtrash_hash_lock);
		g_free (uri);
		return;
	}
	
	if (!strcmp (store_uri, "file:/")) {
		storage = mail_vfolder_get_vfolder_storage ();
	} else {
		storage = mail_lookup_storage (store);
	}
	
	if (!storage) {
		VTRASH_UNLOCK (vtrash_hash_lock);
		g_free (uri);
		return;
	}
	
	path = g_strdup_printf ("/%s", name);
	evolution_storage_new_folder (storage, path, g_basename (path),
				      "mail", uri, name, FALSE);
	gtk_object_unref (GTK_OBJECT (storage));
	
	g_free (path);
	
	g_hash_table_insert (vtrash_hash, uri, folder);
	camel_object_ref (CAMEL_OBJECT (folder));
	
	VTRASH_UNLOCK (vtrash_hash_lock);
}

struct _get_trash_msg {
	struct _mail_msg msg;

	CamelStore *store;
	char *store_uri;
	CamelFolder *folder;
	void (*done) (char *store_uri, CamelFolder *folder, void *data);
	void *data;
};

static char *
get_trash_desc (struct _mail_msg *mm, int done)
{
	struct _get_trash_msg *m = (struct _get_trash_msg *)mm;
	
	return g_strdup_printf (_("Opening Trash folder for %s"), m->store_uri);
}

/* maps the shell's uri to the real vfolder uri and open the folder */
static CamelFolder *
create_trash_vfolder (CamelStore *store, const char *name, GPtrArray *urls, CamelException *ex)
{
	void camel_vee_folder_add_folder (CamelFolder *, CamelFolder *);
	
	char *storeuri, *foldername;
	CamelFolder *folder = NULL, *sourcefolder;
	int source = 0;
	
	d(fprintf (stderr, "Creating Trash vfolder\n"));
	
	storeuri = g_strdup_printf ("vfolder:%s/vfolder/%p/%s", evolution_dir, store, name);
	foldername = g_strdup ("mbox?(match-all (system-flag \"Deleted\"))");
	
	/* we dont have indexing on vfolders */
	folder = mail_tool_get_folder_from_urlname (storeuri, foldername,
						    CAMEL_STORE_FOLDER_CREATE | CAMEL_STORE_VEE_FOLDER_AUTO,
						    ex);
	g_free (foldername);
	g_free (storeuri);
	if (camel_exception_is_set (ex))
		return NULL;
	
	while (source < urls->len) {
		const char *sourceuri;
		
		sourceuri = urls->pdata[source];
		d(fprintf (stderr, "adding vfolder source: %s\n", sourceuri));
		
		sourcefolder = mail_tool_uri_to_folder (sourceuri, ex);
		d(fprintf (stderr, "source folder = %p\n", sourcefolder));
		
		if (sourcefolder) {
			camel_vee_folder_add_folder (folder, sourcefolder);
		} else {
			/* we'll just silently ignore now-missing sources */
			camel_exception_clear (ex);
		}
		
		g_free (urls->pdata[source]);
		source++;
	}
	
	g_ptr_array_free (urls, TRUE);
	
	return folder;
}

static void
populate_folder_urls (CamelFolderInfo *info, GPtrArray *urls)
{
	if (!info)
		return;
	
	g_ptr_array_add (urls, g_strdup (info->url));
	
	if (info->child)
		populate_folder_urls (info->child, urls);
	
	if (info->sibling)
		populate_folder_urls (info->sibling, urls);
}

static void
get_trash_get (struct _mail_msg *mm)
{
	struct _get_trash_msg *m = (struct _get_trash_msg *)mm;
	GPtrArray *urls;

	camel_operation_register(mm->cancel);
	camel_operation_start(mm->cancel, _("Getting matches"));

	urls = g_ptr_array_new ();
	
	/* we don't want to connect */
	m->store = (CamelStore *) camel_session_get_service (session, m->store_uri,
							     CAMEL_PROVIDER_STORE, &mm->ex);
	if (m->store == NULL) {
		g_warning ("Couldn't get service %s: %s\n", m->store_uri,
			   camel_exception_get_description (&mm->ex));
		camel_exception_clear (&mm->ex);
		
		m->folder = NULL;
	} else {
		/* First try to see if we can save time by looking the folder up in the hash table */
		char *uri;
		
		uri = g_strdup_printf ("vtrash:%s", m->store_uri);
		m->folder = vtrash_uri_to_folder (uri, &mm->ex);
		g_free (uri);
		
		if (!m->folder) {
			/* Create and add this new vTrash folder */
			CamelFolderInfo *info;
			
			info = camel_store_get_folder_info (m->store, NULL, TRUE, TRUE, TRUE, &mm->ex);
			populate_folder_urls (info, urls);
			camel_store_free_folder_info (m->store, info);
			
			m->folder = create_trash_vfolder (m->store, _("vTrash"), urls, &mm->ex);
		}
	}

	camel_operation_end(mm->cancel);
	camel_operation_unregister(mm->cancel);
}

static void
get_trash_got (struct _mail_msg *mm)
{
	struct _get_trash_msg *m = (struct _get_trash_msg *)mm;

	if (m->store)
		vtrash_add (m->store, m->folder, m->store_uri, _("vTrash"));
	
	if (m->done)
		m->done (m->store_uri, m->folder, m->data);
}

static void
get_trash_free (struct _mail_msg *mm)
{
	struct _get_trash_msg *m = (struct _get_trash_msg *)mm;

	if (m->store)
		camel_object_unref (CAMEL_OBJECT (m->store));

	g_free (m->store_uri);
	if (m->folder)
		camel_object_unref (CAMEL_OBJECT (m->folder));
}

static struct _mail_msg_op get_trash_op = {
	get_trash_desc,
	get_trash_get,
	get_trash_got,
	get_trash_free,
};

int
vtrash_create (const char *store_uri,
	       void (*done) (char *store_uri, CamelFolder *folder, void *data),
	       void *data)
{
	struct _get_trash_msg *m;
	int id;
	
	m = mail_msg_new (&get_trash_op, NULL, sizeof (*m));
	m->store_uri = g_strdup (store_uri);
	m->data = data;
	m->done = done;
	
	id = m->msg.seq;
	e_thread_put (mail_thread_new, (EMsg *)m);
	
	return id;
}

static void
free_folder (gpointer key, gpointer value, gpointer data)
{
	CamelFolder *folder = CAMEL_FOLDER (value);
	char *uri = key;
	
	g_free (uri);
	camel_object_unref (CAMEL_OBJECT (folder));
}

void
vtrash_cleanup (void)
{
	VTRASH_LOCK (vtrash_hash_lock);
	
	if (vtrash_hash) {
		g_hash_table_foreach (vtrash_hash, free_folder, NULL);
		g_hash_table_destroy (vtrash_hash);
	}
	
	VTRASH_UNLOCK (vtrash_hash_lock);
}
