/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-local.c: Local mailbox support. */

/* 
 * Authors: 
 *  Michael Zucchi <NotZed@helixcode.com>
 *  Peter Williams <peterw@helixcode.com>
 *  Ettore Perazzoli <ettore@helixcode.com>
 *  Dan Winship <danw@helixcode.com>
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

/*
  TODO:

  If we are going to have all this LocalStore stuff, then the LocalStore
  should have a reconfigure_folder method on it, as, in reality, it is
  the maintainer of this information.

*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo.h>
#include <libgnomeui/gnome-dialog.h>
#include <glade/glade.h>
#include <gnome-xml/xmlmemory.h>

#include "Evolution.h"
#include "evolution-storage.h"
#include "evolution-shell-component.h"
#include "evolution-storage-listener.h"

#include "camel/camel.h"

#include "filter/vfolder-context.h"
#include "filter/vfolder-rule.h"
#include "filter/vfolder-editor.h"

#include "mail.h"
#include "mail-local.h"
#include "mail-tools.h"
#include "folder-browser.h"
#include "mail-mt.h"

#define d(x)


/* Local folder metainfo */

struct _local_meta {
	char *path;		/* path of metainfo file */

	char *format;		/* format of mailbox */
	char *name;		/* name of mbox itself */
	int indexed;		/* do we index the body? */
};

static struct _local_meta *
load_metainfo(const char *path)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	struct _local_meta *meta;

	meta = g_malloc0(sizeof(*meta));
	meta->path = g_strdup(path);

	d(printf("Loading folder metainfo from : %s\n", meta->path));

	doc = xmlParseFile(meta->path);
	if (doc == NULL) {
		goto dodefault;
	}
	node = doc->root;
	if (strcmp(node->name, "folderinfo")) {
		goto dodefault;
	}
	node = node->childs;
	while (node) {
		if (!strcmp(node->name, "folder")) {
			char *index, *txt;

			txt = xmlGetProp(node, "type");
			meta->format = g_strdup (txt ? txt : "mbox");
			xmlFree (txt);

			txt = xmlGetProp(node, "name");
			meta->name = g_strdup (txt ? txt : "mbox");
			xmlFree (txt);

			index = xmlGetProp(node, "index");
			if (index) {
				meta->indexed = atoi(index);
				xmlFree(index);
			} else
				meta->indexed = TRUE;
			
		}
		node = node->next;
	}
	xmlFreeDoc(doc);
	return meta;

dodefault:
	meta->format = g_strdup("mbox"); /* defaults */
	meta->name = g_strdup("mbox");
	meta->indexed = TRUE;
	if (doc)
		xmlFreeDoc(doc);
	return meta;
}

static void
free_metainfo(struct _local_meta *meta)
{
	g_free(meta->path);
	g_free(meta->format);
	g_free(meta->name);
	g_free(meta);
}

static int
save_metainfo(struct _local_meta *meta)
{
	xmlDocPtr doc;
	xmlNodePtr root, node;
	int ret;

	d(printf("Saving folder metainfo to : %s\n", meta->path));

	doc = xmlNewDoc("1.0");
	root = xmlNewDocNode(doc, NULL, "folderinfo", NULL);
	xmlDocSetRootElement(doc, root);

	node  = xmlNewChild(root, NULL, "folder", NULL);
	xmlSetProp(node, "type", meta->format);
	xmlSetProp(node, "name", meta->name);
	xmlSetProp(node, "index", meta->indexed?"1":"0");

	ret = xmlSaveFile(meta->path, doc);
	xmlFreeDoc(doc);
	return ret;
}


/* MailLocalStore implementation */
#define MAIL_LOCAL_STORE_TYPE     (mail_local_store_get_type ())
#define MAIL_LOCAL_STORE(obj)     (CAMEL_CHECK_CAST((obj), MAIL_LOCAL_STORE_TYPE, MailLocalStore))
#define MAIL_LOCAL_STORE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), MAIL_LOCAL_STORE_TYPE, MailLocalStoreClass))
#define MAIL_IS_LOCAL_STORE(o)    (CAMEL_CHECK_TYPE((o), MAIL_LOCAL_STORE_TYPE))

typedef struct {
	CamelStore parent_object;	

	GNOME_Evolution_LocalStorage corba_local_storage;
	EvolutionStorageListener *local_storage_listener;

	char *local_path;
	int local_pathlen;
	GHashTable *folders, /* points to MailLocalFolder */
		*unread;
} MailLocalStore;

typedef struct {
	CamelStoreClass parent_class;
} MailLocalStoreClass;

typedef struct {
	CamelFolder *folder;
	MailLocalStore *local_store;
	char *path, *name, *uri;
	int last_unread;
} MailLocalFolder;

static MailLocalStore *local_store;

CamelType mail_local_store_get_type (void);

static void local_folder_changed_proxy (CamelObject *folder, gpointer event_data, gpointer user_data);

static char *get_name (CamelService *service, gboolean brief);

static CamelFolder *get_folder (CamelStore *store, const char *folder_name,
				guint32 flags, CamelException *ex);
static CamelFolderInfo *get_folder_info (CamelStore *store, const char *top,
					 gboolean fast, gboolean recursive,
					 gboolean subscribed_only, CamelException *ex);
static void delete_folder (CamelStore *store, const char *folder_name,
			   CamelException *ex);
static void rename_folder (CamelStore *store, const char *old_name,
			   const char *new_name, CamelException *ex);

static CamelStoreClass *local_parent_class;

static void
mail_local_store_class_init (MailLocalStoreClass *mail_local_store_class)
{
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (mail_local_store_class);
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (mail_local_store_class);

	/* virtual method overload */
	camel_service_class->get_name = get_name;

	/* Don't cache folders */
	camel_store_class->hash_folder_name = NULL;
	camel_store_class->compare_folder_name = NULL;

	camel_store_class->get_folder = get_folder;
	camel_store_class->get_folder_info = get_folder_info;
	camel_store_class->free_folder_info = camel_store_free_folder_info_full;
	camel_store_class->delete_folder = delete_folder;
	camel_store_class->rename_folder = rename_folder;

	local_parent_class = (CamelStoreClass *)camel_type_get_global_classfuncs(camel_store_get_type ());
}

static void
mail_local_store_init (gpointer object, gpointer klass)
{
	MailLocalStore *local_store = MAIL_LOCAL_STORE (object);

	local_store->corba_local_storage = CORBA_OBJECT_NIL;
}

static void
free_local_folder(MailLocalFolder *lf)
{
	if (lf->folder) {
		camel_object_unhook_event((CamelObject *)lf->folder,
					  "folder_changed", local_folder_changed_proxy,
					  lf);
		camel_object_unhook_event((CamelObject *)lf->folder,
					  "message_changed", local_folder_changed_proxy,
					  lf);
		camel_object_unref((CamelObject *)lf->folder);
	}
	g_free(lf->path);
	g_free(lf->name);
	g_free(lf->uri);
	camel_object_unref((CamelObject *)lf->local_store);
}

static void
free_folder (gpointer key, gpointer data, gpointer user_data)
{
	MailLocalFolder *lf = data;

	g_free(key);
	free_local_folder(lf);
}

static void
mail_local_store_finalize (gpointer object)
{
	MailLocalStore *local_store = MAIL_LOCAL_STORE (object);
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	if (!CORBA_Object_is_nil (local_store->corba_local_storage, &ev)) {
		Bonobo_Unknown_unref (local_store->corba_local_storage, &ev);
		CORBA_Object_release (local_store->corba_local_storage, &ev);
	}
	CORBA_exception_free (&ev);

	if (local_store->local_storage_listener)
		gtk_object_unref (GTK_OBJECT (local_store->local_storage_listener));

	g_hash_table_foreach (local_store->folders, free_folder, NULL);
	g_hash_table_destroy (local_store->folders);

	g_free (local_store->local_path);
}

CamelType
mail_local_store_get_type (void)
{
	static CamelType mail_local_store_type = CAMEL_INVALID_TYPE;

	if (mail_local_store_type == CAMEL_INVALID_TYPE) {
		mail_local_store_type = camel_type_register (
			CAMEL_STORE_TYPE, "MailLocalStore",
			sizeof (MailLocalStore),
			sizeof (MailLocalStoreClass),
			(CamelObjectClassInitFunc) mail_local_store_class_init,
			NULL,
			(CamelObjectInitFunc) mail_local_store_init,
			(CamelObjectFinalizeFunc) mail_local_store_finalize);
	}

	return mail_local_store_type;
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name,
	    guint32 flags, CamelException *ex)
{
	MailLocalStore *local_store = (MailLocalStore *)store;
	CamelFolder *folder;
	MailLocalFolder *local_folder;

	local_folder = g_hash_table_lookup (local_store->folders, folder_name);
	if (local_folder) {
		folder = local_folder->folder;
		camel_object_ref (CAMEL_OBJECT (folder));
	} else {
		folder = NULL;
		camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER, "No such folder %s", folder_name);
	}
	return folder;
}

static void
populate_folders (gpointer key, gpointer data, gpointer user_data)
{
	GPtrArray *folders = user_data;
	MailLocalFolder *folder;
	CamelFolderInfo *fi;
	
	folder = data;
	
	fi = g_new0 (CamelFolderInfo, 1);
	fi->full_name = g_strdup (folder->path);
	fi->name = g_strdup (folder->name);
	fi->url = g_strdup (folder->uri);
	fi->unread_message_count = -1;
	
	g_ptr_array_add (folders, fi);
}

static CamelFolderInfo *
get_folder_info (CamelStore *store, const char *top,
		 gboolean fast, gboolean recursive,
		 gboolean subscribed_only, CamelException *ex)
{
	MailLocalStore *local_store = MAIL_LOCAL_STORE (store);
	CamelFolderInfo *fi = NULL;
	GPtrArray *folders;
	
	folders = g_ptr_array_new ();
	g_hash_table_foreach (local_store->folders, populate_folders, folders);
	
	fi = camel_folder_info_build (folders, top, '/', TRUE);
	g_ptr_array_free (folders, TRUE);
	
	return fi;
}

static void
delete_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	/* No-op. The shell local storage deals with this. */
}

static void
rename_folder (CamelStore *store, const char *old, const char *new,
	       CamelException *ex)
{
	/* Probable no-op... */
}

static char *
get_name (CamelService *service, gboolean brief)
{
	return g_strdup ("Local mail folders");
}


/* Callbacks for the EvolutionStorageListner signals.  */

static void
local_storage_destroyed_cb (EvolutionStorageListener *storage_listener,
			    void *data)
{
	/* FIXME: Dunno how to handle this yet.  */
	g_warning ("%s -- The LocalStorage has gone?!", __FILE__);
}


static void
local_folder_changed (CamelObject *object, gpointer event_data,
		      gpointer user_data)
{
	MailLocalFolder *local_folder = user_data;
	int unread = GPOINTER_TO_INT (event_data);
	char *display;

	if (unread != local_folder->last_unread) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		if (unread > 0) {
			display = g_strdup_printf ("%s (%d)", local_folder->name, unread);
			GNOME_Evolution_LocalStorage_updateFolder (
				local_folder->local_store->corba_local_storage,
				local_folder->path, display, TRUE, &ev);
			g_free (display);
		} else {
			GNOME_Evolution_LocalStorage_updateFolder (
				local_folder->local_store->corba_local_storage,
				local_folder->path, local_folder->name,
				FALSE, &ev);
		}
		CORBA_exception_free (&ev);

		local_folder->last_unread = unread;
	}
}

static void
local_folder_changed_proxy (CamelObject *folder, gpointer event_data, gpointer user_data)
{
	int unread;

	unread = camel_folder_get_unread_message_count (CAMEL_FOLDER (folder));
	mail_proxy_event (local_folder_changed, folder,
			  GINT_TO_POINTER (unread), user_data);
}

/* ********************************************************************** */
/* Register folder */

struct _register_msg {
	struct _mail_msg msg;

	MailLocalFolder *local_folder;
};

static char *register_folder_desc(struct _mail_msg *mm, int done)
{
	struct _register_msg *m = (struct _register_msg *)mm;

	printf("returning description for %s\n", m->local_folder->uri);

	return g_strdup_printf(_("Opening '%s'"), m->local_folder->uri);
}

static void
register_folder_register(struct _mail_msg *mm)
{
	struct _register_msg *m = (struct _register_msg *)mm;
	MailLocalFolder *local_folder = m->local_folder;
	char *name, *path = local_folder->uri + 7;
	struct _local_meta *meta;
	CamelStore *store;
	guint32 flags;

	name = g_strdup_printf ("%s/local-metadata.xml", path);
	meta = load_metainfo (name);
	g_free (name);

	camel_operation_register(mm->cancel);

	name = g_strdup_printf ("%s:%s", meta->format, path);
	store = camel_session_get_store (session, name, &mm->ex);
	g_free (name);
	if (!store) {
		free_metainfo (meta);
		camel_operation_unregister(mm->cancel);
		return;
	}

	flags = CAMEL_STORE_FOLDER_CREATE;
	if (meta->indexed)
		flags |= CAMEL_STORE_FOLDER_BODY_INDEX;
	local_folder->folder = camel_store_get_folder (store, meta->name, flags, &mm->ex);
	if (local_folder->folder) {
		camel_object_hook_event (CAMEL_OBJECT (local_folder->folder),
					 "folder_changed", local_folder_changed_proxy,
					 local_folder);
		camel_object_hook_event (CAMEL_OBJECT (local_folder->folder),
					 "message_changed", local_folder_changed_proxy,
					 local_folder);
		local_folder->last_unread = camel_folder_get_unread_message_count(local_folder->folder);
	}

	camel_object_unref (CAMEL_OBJECT (store));
	free_metainfo (meta);

	camel_operation_unregister(mm->cancel);
}

static void
register_folder_registered(struct _mail_msg *mm)
{
	struct _register_msg *m = (struct _register_msg *)mm;
	MailLocalFolder *local_folder = m->local_folder;
	int unread;

	if (local_folder->folder) {
		g_hash_table_insert (local_folder->local_store->folders, local_folder->uri + 8, local_folder);

		unread = local_folder->last_unread;
		local_folder->last_unread = 0;
		local_folder_changed (CAMEL_OBJECT (local_folder->folder), GINT_TO_POINTER (unread), local_folder);
		m->local_folder = NULL;
	}
}

static void 
register_folder_free(struct _mail_msg *mm)
{
	struct _register_msg *m = (struct _register_msg *)mm;

	if (m->local_folder)
		free_local_folder(m->local_folder);
}

static struct _mail_msg_op register_folder_op = {
	register_folder_desc,
	register_folder_register,
	register_folder_registered,
	register_folder_free,
};

static void
local_storage_new_folder_cb (EvolutionStorageListener *storage_listener,
			     const char *path,
			     const GNOME_Evolution_Folder *folder,
			     void *data)
{
	MailLocalStore *local_store = data;
	MailLocalFolder *local_folder;
	struct _register_msg *m;
	int id;

	if (strcmp (folder->type, "mail") != 0 ||
	    strncmp (folder->physical_uri, "file://", 7) != 0 ||
	    strncmp (folder->physical_uri + 7, local_store->local_path,
		     local_store->local_pathlen) != 0)
		return;

	local_folder = g_new0 (MailLocalFolder, 1);
	local_folder->name = g_strdup (strrchr (path, '/') + 1);
	local_folder->path = g_strdup (path);
	local_folder->uri = g_strdup (folder->physical_uri);
	local_folder->local_store = local_store;
	camel_object_ref((CamelObject *)local_store);

	m = mail_msg_new(&register_folder_op, NULL, sizeof(*m));

	m->local_folder = local_folder;

	/* run synchronous, the shell expects it (I think) */
	id = m->msg.seq;
	e_thread_put(mail_thread_queued, (EMsg *)m);
	mail_msg_wait(id);
}

static void
local_storage_removed_folder_cb (EvolutionStorageListener *storage_listener,
				 const char *path,
				 void *data)
{
	MailLocalStore *local_store = data;
	MailLocalFolder *local_folder;

	if (strncmp (path, "file://", 7) != 0 ||
	    strncmp (path + 7, local_store->local_path,
		     local_store->local_pathlen) != 0)
		return;

	local_folder = g_hash_table_lookup (local_store->folders, path + 8);
	if (local_folder) {
		g_hash_table_remove (local_store->folders, path);
		free_local_folder(local_folder);
	}
}

static CamelProvider local_provider = {
	"file", "Local mail", NULL, "mail",
	CAMEL_PROVIDER_IS_STORAGE, CAMEL_URL_NEED_PATH,
	{ 0, 0 }, NULL
};

/* There's only one "file:" store. */
static guint
non_hash (gconstpointer key)
{
	return 0;
}

static gint
non_equal (gconstpointer a, gconstpointer b)
{
	return TRUE;
}

void
mail_local_storage_startup (EvolutionShellClient *shellclient,
			    const char *evolution_path)
{
	GNOME_Evolution_StorageListener corba_local_storage_listener;
	CORBA_Environment ev;

	/* Register with Camel to handle file: URLs */
	local_provider.object_types[CAMEL_PROVIDER_STORE] =
		mail_local_store_get_type();

	local_provider.service_cache = g_hash_table_new (non_hash, non_equal);
	camel_session_register_provider (session, &local_provider);


	/* Now build the storage. */
	local_store = (MailLocalStore *)camel_session_get_service (
		session, "file:/", CAMEL_PROVIDER_STORE, NULL);
	if (!local_store) {
		g_warning ("No local store!");
		return;
	}
	local_store->corba_local_storage =
		evolution_shell_client_get_local_storage (shellclient);
	if (local_store->corba_local_storage == CORBA_OBJECT_NIL) {
		g_warning ("No local storage!");
		camel_object_unref (CAMEL_OBJECT (local_store));
		return;
	}
	
	local_store->local_storage_listener =
		evolution_storage_listener_new ();
	corba_local_storage_listener =
		evolution_storage_listener_corba_objref (
			local_store->local_storage_listener);

	gtk_signal_connect (GTK_OBJECT (local_store->local_storage_listener),
			    "destroyed",
			    GTK_SIGNAL_FUNC (local_storage_destroyed_cb),
			    local_store);
	gtk_signal_connect (GTK_OBJECT (local_store->local_storage_listener),
			    "new_folder",
			    GTK_SIGNAL_FUNC (local_storage_new_folder_cb),
			    local_store);
	gtk_signal_connect (GTK_OBJECT (local_store->local_storage_listener),
			    "removed_folder",
			    GTK_SIGNAL_FUNC (local_storage_removed_folder_cb),
			    local_store);

	local_store->local_path = g_strdup_printf ("%s/local",
						   evolution_path);
	local_store->local_pathlen = strlen (local_store->local_path);

	local_store->folders = g_hash_table_new (g_str_hash, g_str_equal);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_addListener (local_store->corba_local_storage,
					corba_local_storage_listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot add a listener to the Local Storage.");
		camel_object_unref (CAMEL_OBJECT (local_store));
		CORBA_exception_free (&ev);
		return;
	}
	CORBA_exception_free (&ev);
}


/* Local folder reconfiguration stuff */

/*
   open new
   copy old->new
   close old
   rename old oldsave
   rename new old
   open oldsave
   delete oldsave

   close old
   rename oldtmp
   open new
   open oldtmp
   copy oldtmp new
   close oldtmp
   close oldnew

*/

static void
update_progress(char *fmt, float percent)
{
	if (fmt)
		mail_status(fmt);
	/*mail_op_set_percentage (percent);*/
}

/* ******************** */

/* we should have our own progress bar for this */

struct _reconfigure_msg {
	struct _mail_msg msg;

	FolderBrowser *fb;
	gchar *newtype;
	GtkWidget *frame;
	GtkWidget *apply;
	GtkWidget *cancel;
	GtkOptionMenu *optionlist;
};

#if 0
static gchar *
describe_reconfigure_folder (gpointer in_data, gboolean gerund)
{
	reconfigure_folder_input_t *input = (reconfigure_folder_input_t *) in_data;

	if (gerund)
		return g_strdup_printf (_("Changing folder \"%s\" to \"%s\" format"),
					input->fb->uri,
					input->newtype);
	else
		return g_strdup_printf (_("Change folder \"%s\" to \"%s\" format"),
					input->fb->uri,
					input->newtype);
}
#endif

static void
reconfigure_folder_reconfigure(struct _mail_msg *mm)
{
	struct _reconfigure_msg *m = (struct _reconfigure_msg *)mm;
	MailLocalFolder *local_folder = NULL;
	CamelStore *fromstore = NULL, *tostore = NULL;
	char *fromurl = NULL, *tourl = NULL;
	CamelFolder *fromfolder = NULL, *tofolder = NULL;
	GPtrArray *uids;
	int i;
	char *metapath;
	char *tmpname;
	CamelURL *url = NULL;
	struct _local_meta *meta = NULL;
	guint32 flags;

	d(printf("reconfiguring folder: %s to type %s\n", m->fb->uri, m->newtype));

	mail_status_start(_("Reconfiguring folder"));

	/* NOTE: This var is cleared by the folder_browser via the set_uri method */
	m->fb->reconfigure = TRUE;

	/* get the actual location of the mailbox */
	url = camel_url_new(m->fb->uri, &mm->ex);
	if (camel_exception_is_set(&mm->ex)) {
		g_warning("%s is not a workable url!", m->fb->uri);
		goto cleanup;
	}

	tmpname = strchr (m->fb->uri, '/');
	if (tmpname) {
		while (*tmpname == '/')
			tmpname++;
		local_folder = g_hash_table_lookup (local_store->folders, tmpname);
	} else
		local_folder = NULL;
	if (!local_folder) {
		g_warning("%s is not a registered local folder!", m->fb->uri);
		goto cleanup;
	}

	metapath = g_strdup_printf("%s/local-metadata.xml", url->path);
	meta = load_metainfo(metapath);
	g_free(metapath);

	/* first, 'close' the old folder */
	update_progress(_("Closing current folder"), 0.0);
	camel_folder_sync(local_folder->folder, FALSE, &mm->ex);
	camel_object_unhook_event(CAMEL_OBJECT (local_folder->folder),
				  "folder_changed", local_folder_changed_proxy,
				  local_folder);
	camel_object_unhook_event(CAMEL_OBJECT (local_folder->folder),
				  "message_changed", local_folder_changed_proxy,
				  local_folder);
	/* Once for the FolderBrowser, once for the local store */
	camel_object_unref(CAMEL_OBJECT(local_folder->folder));
	camel_object_unref(CAMEL_OBJECT(local_folder->folder));
	local_folder->folder = m->fb->folder = NULL;

	camel_url_set_protocol (url, meta->format);
	fromurl = camel_url_to_string (url, FALSE);
	camel_url_set_protocol (url, m->newtype);
	tourl = camel_url_to_string (url, FALSE);

	d(printf("opening stores %s and %s\n", fromurl, tourl));

	fromstore = camel_session_get_store(session, fromurl, &mm->ex);

	if (camel_exception_is_set(&mm->ex))
		goto cleanup;

	tostore = camel_session_get_store(session, tourl, &mm->ex);
	if (camel_exception_is_set(&mm->ex))
		goto cleanup;

	/* rename the old mbox and open it again, without indexing */
	tmpname = g_strdup_printf("%s_reconfig", meta->name);
	d(printf("renaming %s to %s, and opening it\n", meta->name, tmpname));
	update_progress(_("Renaming old folder and opening"), 0.0);

	camel_store_rename_folder(fromstore, meta->name, tmpname, &mm->ex);
	if (camel_exception_is_set(&mm->ex)) {
		goto cleanup;
	}
	
	/* we dont need to set the create flag ... or need an index if it has one */
	fromfolder = camel_store_get_folder(fromstore, tmpname, 0, &mm->ex);
	if (fromfolder == NULL || camel_exception_is_set(&mm->ex)) {
		/* try and recover ... */
		camel_exception_clear (&mm->ex);
		camel_store_rename_folder(fromstore, tmpname, meta->name, &mm->ex);
		goto cleanup;
	}

	/* create a new mbox */
	d(printf("Creating the destination mbox\n"));
	update_progress(_("Creating new folder"), 0.0);

	flags = CAMEL_STORE_FOLDER_CREATE;
	if (meta->indexed)
		flags |= CAMEL_STORE_FOLDER_BODY_INDEX;
	tofolder = camel_store_get_folder(tostore, meta->name, flags, &mm->ex);
	if (tofolder == NULL || camel_exception_is_set(&mm->ex)) {
		d(printf("cannot open destination folder\n"));
		/* try and recover ... */
		camel_exception_clear (&mm->ex);
		camel_store_rename_folder(fromstore, tmpname, meta->name, &mm->ex);
		goto cleanup;
	}

	update_progress(_("Copying messages"), 0.0);
	uids = camel_folder_get_uids(fromfolder);
	for (i=0;i<uids->len;i++) {
		mail_statusf("Copying message %d of %d", i, uids->len);
		camel_folder_move_message_to(fromfolder, uids->pdata[i], tofolder, &mm->ex);
		if (camel_exception_is_set(&mm->ex)) {
			camel_folder_free_uids(fromfolder, uids);
			goto cleanup;
		}
	}
	camel_folder_free_uids(fromfolder, uids);
	camel_folder_expunge(fromfolder, &mm->ex);

	d(printf("delete old mbox ...\n"));
	camel_store_delete_folder(fromstore, tmpname, &mm->ex);

	/* switch format */
	g_free(meta->format);
	meta->format = g_strdup(m->newtype);
	if (save_metainfo(meta) == -1) {
		camel_exception_setv (&mm->ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot save folder metainfo; "
					"you'll probably find you can't\n"
					"open this folder anymore: %s"),
				      tourl);
	}

 cleanup:
	if (local_folder && !local_folder->folder) {
		struct _register_msg *rm = mail_msg_new(&register_folder_op, NULL, sizeof(*m));

		/* fake the internal part of this operation, nasty hackish thing */
		rm->local_folder = local_folder;
		register_folder_register((struct _mail_msg *)rm);
		rm->local_folder = NULL;
		mail_msg_free((struct _mail_msg *)rm);
	}
	if (tofolder)
		camel_object_unref (CAMEL_OBJECT (tofolder));
	if (fromfolder)
		camel_object_unref (CAMEL_OBJECT (fromfolder));
	if (fromstore)
		camel_object_unref (CAMEL_OBJECT (fromstore));
	if (tostore)
		camel_object_unref (CAMEL_OBJECT (tostore));
	if (meta)
		free_metainfo(meta);
	g_free(fromurl);
	g_free(tourl);
	if (url)
		camel_url_free (url);
}

static void
reconfigure_folder_reconfigured(struct _mail_msg *mm)
{
	struct _reconfigure_msg *m = (struct _reconfigure_msg *)mm;
	char *uri;

	if (camel_exception_is_set(&mm->ex)) {
		gnome_error_dialog (_("If you can no longer open this mailbox, then\n"
				      "you may need to repair it manually."));
	}

	/* force a reload of the newly formatted folder */
	d(printf("opening new source\n"));
	uri = g_strdup(m->fb->uri);
	folder_browser_set_uri(m->fb, uri);
	g_free(uri);
}

static void
reconfigure_folder_free(struct _mail_msg *mm)
{
	struct _reconfigure_msg *m = (struct _reconfigure_msg *)mm;

	gtk_object_unref (GTK_OBJECT (m->fb));
	g_free (m->newtype);
}

static struct _mail_msg_op reconfigure_folder_op = {
	NULL,
	reconfigure_folder_reconfigure,
	reconfigure_folder_reconfigured,
	reconfigure_folder_free,
};

static void
reconfigure_clicked(GnomeDialog *d, int button, struct _reconfigure_msg *m)
{
	if (button == 0) {
		GtkMenu *menu;
		int type;
		char *types[] = { "mbox", "maildir", "mh" };

		menu = (GtkMenu *)gtk_option_menu_get_menu(m->optionlist);
		type = g_list_index(GTK_MENU_SHELL(menu)->children, gtk_menu_get_active(menu));
		if (type < 0 || type > 2)
			type = 0;

		gtk_widget_set_sensitive(m->frame, FALSE);
		gtk_widget_set_sensitive(m->apply, FALSE);
		gtk_widget_set_sensitive(m->cancel, FALSE);

		m->newtype = g_strdup (types[type]);
		e_thread_put(mail_thread_queued, (EMsg *)m);
	} else
		mail_msg_free((struct _mail_msg *)m);

	if (button != -1)
		gnome_dialog_close(d);
}

void
mail_local_reconfigure_folder(FolderBrowser *fb)
{
	CamelStore *store;
	GladeXML *gui;
	GnomeDialog *gd;
	struct _reconfigure_msg *m;

	if (fb->folder == NULL) {
		g_warning("Trying to reconfigure nonexistant folder");
		return;
	}

	m = mail_msg_new(&reconfigure_folder_op, NULL, sizeof(*m));
	store = camel_folder_get_parent_store(fb->folder);

	gui = glade_xml_new(EVOLUTION_GLADEDIR "/local-config.glade", "dialog_format");
	gd = (GnomeDialog *)glade_xml_get_widget (gui, "dialog_format");

	m->frame = glade_xml_get_widget (gui, "frame_format");
	m->apply = glade_xml_get_widget (gui, "apply_format");
	m->cancel = glade_xml_get_widget (gui, "cancel_format");
	m->optionlist = (GtkOptionMenu *)glade_xml_get_widget (gui, "option_format");
	m->newtype = NULL;
	m->fb = fb;
	gtk_object_ref((GtkObject *)fb);

	gtk_label_set_text((GtkLabel *)glade_xml_get_widget (gui, "label_format"),
			   ((CamelService *)store)->url->protocol);

	gtk_signal_connect((GtkObject *)gd, "clicked", reconfigure_clicked, m);
	gtk_object_unref((GtkObject *)gui);

	gnome_dialog_run_and_close (GNOME_DIALOG (gd));
}
