/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-local.c: Local mailbox support. */

/* 
 * Authors: 
 *  Michael Zucchi <NotZed@ximian.com>
 *  Peter Williams <peterw@ximian.com>
 *  Ettore Perazzoli <ettore@ximian.com>
 *  Dan Winship <danw@ximian.com>
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

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <libxml/xmlmemory.h>
#include <glade/glade.h>

#include "e-util/e-path.h"
#include "e-util/e-dialog-utils.h"
#include <gal/util/e-xml-utils.h>

#include "Evolution.h"
#include "evolution-storage.h"
#include "evolution-shell-component.h"
#include "evolution-storage-listener.h"

#include "camel/camel.h"
#include "camel/camel-vtrash-folder.h"

#include "mail.h"
#include "mail-local.h"
#include "mail-tools.h"
#include "mail-mt.h"
#include "mail-folder-cache.h"
#include "mail-vfolder.h"
#include "mail-ops.h"

#define d(x) 

/* sigh, required for passing around to some functions */
static GNOME_Evolution_Storage local_corba_storage = CORBA_OBJECT_NIL;

/* ** MailLocalStore ** (protos) ************************************************** */

#define MAIL_LOCAL_STORE_TYPE     (mail_local_store_get_type ())
#define MAIL_LOCAL_STORE(obj)     (CAMEL_CHECK_CAST((obj), MAIL_LOCAL_STORE_TYPE, MailLocalStore))
#define MAIL_LOCAL_STORE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), MAIL_LOCAL_STORE_TYPE, MailLocalStoreClass))
#define MAIL_IS_LOCAL_STORE(o)    (CAMEL_CHECK_TYPE((o), MAIL_LOCAL_STORE_TYPE))

typedef struct {
	CamelStore parent_object;

	/* stores CamelFolderInfo's of the folders we're supposed to know about, by uri */
	GHashTable *folder_infos;
	GMutex *folder_info_lock;

} MailLocalStore;

typedef struct {
	CamelStoreClass parent_class;
} MailLocalStoreClass;

static CamelType mail_local_store_get_type (void);

static MailLocalStore *global_local_store;

/* ** MailLocalFolder ** (protos) ************************************************* */

#define MAIL_LOCAL_FOLDER_TYPE     (mail_local_folder_get_type ())
#define MAIL_LOCAL_FOLDER(obj)     (CAMEL_CHECK_CAST((obj), MAIL_LOCAL_FOLDER_TYPE, MailLocalFolder))
#define MAIL_LOCAL_FOLDER_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), MAIL_LOCAL_FOLDER_TYPE, MailLocalFolderClass))
#define MAIL_IS_LOCAL_FOLDER(o)    (CAMEL_CHECK_TYPE((o), MAIL_LOCAL_FOLDER_TYPE))

#define LOCAL_STORE_LOCK(folder)   (g_mutex_lock   (((MailLocalStore *)folder)->folder_info_lock))
#define LOCAL_STORE_UNLOCK(folder) (g_mutex_unlock (((MailLocalStore *)folder)->folder_info_lock))

struct _local_meta {
	char *path;		/* path of metainfo */

	char *format;		/* format of mailbox */
	char *name;		/* name of actual mbox */
	int indexed;		/* is body indexed? */
};

typedef struct {
	CamelFolder parent_object;

	CamelFolder *real_folder;
	CamelStore *real_store;

	char *description;
	char *real_path;

	struct _local_meta *meta;

	GMutex *real_folder_lock; /* no way to use the CamelFolder's lock, so... */
} MailLocalFolder;

typedef struct {
	CamelFolderClass parent_class;
} MailLocalFolderClass;

static CamelType mail_local_folder_get_type (void);

#ifdef ENABLE_THREADS
#define LOCAL_FOLDER_LOCK(folder)   (g_mutex_lock   (((MailLocalFolder *)folder)->real_folder_lock))
#define LOCAL_FOLDER_UNLOCK(folder) (g_mutex_unlock (((MailLocalFolder *)folder)->real_folder_lock))
#else
#define LOCAL_FOLDER_LOCK(folder)
#define LOCAL_FOLDER_UNLOCK(folder)
#endif

/* ** MailLocalFolder ************************************************************* */

static struct _local_meta *
load_metainfo(const char *path)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr node;
	struct _local_meta *meta;
	struct stat st;
	
	d(printf("Loading folder metainfo from : %s\n", path));

	meta = g_malloc0(sizeof(*meta));
	meta->path = g_strdup(path);
	
	if (stat (path, &st) == -1 || !S_ISREG (st.st_mode))
		goto dodefault;
	
	doc = xmlParseFile(path);
	if (doc == NULL)
		goto dodefault;

	node = doc->children;
	if (strcmp(node->name, "folderinfo"))
		goto dodefault;

	node = node->children;
	while (node) {
		if (!strcmp(node->name, "folder")) {
			char *index, *txt;
			
			txt = xmlGetProp(node, "type");
			meta->format = g_strdup(txt?txt:"mbox");
			xmlFree(txt);
			
			txt = xmlGetProp(node, "name");
			meta->name = g_strdup(txt?txt:"mbox");
			xmlFree(txt);
			
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

static gboolean
save_metainfo (struct _local_meta *meta)
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
	
	ret = e_xml_save_file (meta->path, doc);
	
	xmlFreeDoc (doc);
	
	return ret == -1 ? FALSE : TRUE;
}

static CamelFolderClass *mlf_parent_class = NULL;

/* forward a bunch of functions to the real folder. This pretty
 * much sucks but I haven't found a better way of doing it.
 */

/* We need to do it without having locked our folder, otherwise
   we can get sync hangs with vfolders/trash */
static void
mlf_refresh_info(CamelFolder *folder, CamelException *ex)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER(folder);
	CamelFolder *f;

	LOCAL_FOLDER_LOCK(mlf);
	f = mlf->real_folder;
	camel_object_ref(f);
	LOCAL_FOLDER_UNLOCK(mlf);

	camel_folder_refresh_info(f, ex);
	camel_object_unref(f);
}

static void
mlf_sync(CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER(folder);
	CamelFolder *f;

	LOCAL_FOLDER_LOCK(mlf);
	f = mlf->real_folder;
	camel_object_ref(f);
	LOCAL_FOLDER_UNLOCK(mlf);

	camel_folder_sync(f, expunge, ex);
	camel_object_unref(f);
}

static void
mlf_expunge(CamelFolder *folder, CamelException *ex)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER(folder);
	CamelFolder *f;

	LOCAL_FOLDER_LOCK(mlf);
	f = mlf->real_folder;
	camel_object_ref(f);
	LOCAL_FOLDER_UNLOCK(mlf);

	camel_folder_expunge(f, ex);
	camel_object_unref(f);
}

static void
mlf_append_message(CamelFolder *folder, CamelMimeMessage *message, const CamelMessageInfo *info, char **appended_uid, CamelException *ex)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER(folder);
	CamelFolder *f;

	LOCAL_FOLDER_LOCK(mlf);
	f = mlf->real_folder;
	camel_object_ref(f);
	LOCAL_FOLDER_UNLOCK(mlf);

	camel_folder_append_message(f, message, info, appended_uid, ex);
	camel_object_unref(f);
}

static CamelMimeMessage *
mlf_get_message(CamelFolder *folder, const char *uid, CamelException *ex)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER(folder);
	CamelMimeMessage *ret;
	CamelFolder *f;

	LOCAL_FOLDER_LOCK(mlf);
	f = mlf->real_folder;
	camel_object_ref(f);
	LOCAL_FOLDER_UNLOCK(mlf);

	ret = camel_folder_get_message(f, uid, ex);
	camel_object_unref(f);

	return ret;
}

static GPtrArray *
mlf_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER (folder);
	GPtrArray *ret;
	CamelFolder *f;

	LOCAL_FOLDER_LOCK(mlf);
	f = mlf->real_folder;
	camel_object_ref(f);
	LOCAL_FOLDER_UNLOCK(mlf);

	ret = camel_folder_search_by_expression(f, expression, ex);
	camel_object_unref(f);

	return ret;
}

static GPtrArray *
mlf_search_by_uids(CamelFolder *folder, const char *expression, GPtrArray *uids, CamelException *ex)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER (folder);
	GPtrArray *ret;
	CamelFolder *f;

	LOCAL_FOLDER_LOCK(mlf);
	f = mlf->real_folder;
	camel_object_ref(f);
	LOCAL_FOLDER_UNLOCK(mlf);

	ret = camel_folder_search_by_uids(f, expression, uids, ex);
	camel_object_unref(f);

	return ret;
}

static void
mlf_search_free(CamelFolder *folder, GPtrArray *result)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER(folder);
	CamelFolder *f;

	LOCAL_FOLDER_LOCK(mlf);
	f = mlf->real_folder;
	camel_object_ref(f);
	LOCAL_FOLDER_UNLOCK(mlf);

	camel_folder_search_free(f, result);
	camel_object_unref(f);
}

static void
mlf_set_message_flags(CamelFolder *folder, const char *uid, guint32 flags, guint32 set)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER(folder);
	CamelFolder *f;

	LOCAL_FOLDER_LOCK(mlf);
	f = mlf->real_folder;
	camel_object_ref(f);
	LOCAL_FOLDER_UNLOCK(mlf);

	camel_folder_set_message_flags(mlf->real_folder, uid, flags, set);
	camel_object_unref(f);
}

static void
mlf_set_message_user_flag(CamelFolder *folder, const char *uid, const char *name, gboolean value)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER(folder);
	CamelFolder *f;

	LOCAL_FOLDER_LOCK(mlf);
	f = mlf->real_folder;
	camel_object_ref(f);
	LOCAL_FOLDER_UNLOCK(mlf);

	camel_folder_set_message_user_flag(mlf->real_folder, uid, name, value);
	camel_object_unref(f);
}

static void
mlf_set_message_user_tag(CamelFolder *folder, const char *uid, const char *name, const char *value)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER(folder);
	CamelFolder *f;

	LOCAL_FOLDER_LOCK(mlf);
	f = mlf->real_folder;
	camel_object_ref(f);
	LOCAL_FOLDER_UNLOCK(mlf);

	camel_folder_set_message_user_tag(mlf->real_folder, uid, name, value);
	camel_object_unref(f);
}

/* Internal store-rename call, update our strings */
static void
mlf_rename(CamelFolder *folder, const char *new)
{
	MailLocalFolder *mlf = (MailLocalFolder *)folder;

	/* first, proxy it down */
	if (mlf->real_folder) {
		char *mbox = g_strdup_printf("%s/%s", new, mlf->meta->name);

		d(printf("renaming real folder to %s\n", mbox));

		camel_folder_rename(mlf->real_folder, mbox);
		g_free(mbox);
	}

	/* Then do our stuff */
	g_free(mlf->real_path);
	mlf->real_path = g_strdup(new);

	g_free(mlf->meta->path);
	mlf->meta->path = g_strdup_printf("%s/%s/local-metadata.xml", ((CamelService *)folder->parent_store)->url->path, new);

	/* Then pass it up */
	((CamelFolderClass *)mlf_parent_class)->rename(folder, new);
}

/* and, conversely, forward the real folder's signals. */

static void
mlf_proxy_message_changed(CamelObject *real_folder, gpointer event_data, gpointer user_data)
{
	camel_object_trigger_event(user_data, "message_changed", event_data);
}

static void
mlf_proxy_folder_changed(CamelObject *real_folder, gpointer event_data, gpointer user_data)
{
	camel_object_trigger_event(user_data, "folder_changed", event_data);
}

static void
mlf_unset_folder (MailLocalFolder *mlf)
{
	CamelFolder *folder = (CamelFolder *)mlf;

	g_assert(mlf->real_folder);

	camel_object_unhook_event(mlf->real_folder,
				  "message_changed",
				  mlf_proxy_message_changed,
				  mlf);
	camel_object_unhook_event(mlf->real_folder,
				  "folder_changed",
				  mlf_proxy_folder_changed,
				  mlf);

	camel_object_unref(folder->summary);
	folder->summary = NULL;
	camel_object_unref(mlf->real_folder);
	mlf->real_folder = NULL;
	camel_object_unref(mlf->real_store);
	mlf->real_store = NULL;

	folder->permanent_flags = 0;
	folder->folder_flags = 0;
}

static gboolean
mlf_set_folder(MailLocalFolder *mlf, guint32 flags, CamelException *ex)
{
	CamelFolder *folder = (CamelFolder *)mlf;
	char *uri, *mbox;

	g_assert(mlf->real_folder == NULL);

	uri = g_strdup_printf("%s:%s", mlf->meta->format, ((CamelService *)folder->parent_store)->url->path);

	d(printf("opening real store: %s\n", uri));
	mlf->real_store = camel_session_get_store(session, uri, ex);
	g_free(uri);
	if (mlf->real_store == NULL)
		return FALSE;

	if (mlf->meta->indexed)
		flags |= CAMEL_STORE_FOLDER_BODY_INDEX;

	/* mlf->real_folder = camel_store_get_folder(mlf->real_store, mlf->meta->name, flags, ex); */
	mbox = g_strdup_printf("%s/%s", mlf->real_path, mlf->meta->name);
	d(printf("Opening mbox on real path: %s\n", mbox));
	mlf->real_folder = camel_store_get_folder(mlf->real_store, mbox, flags, ex);
	g_free(mbox);
	if (mlf->real_folder == NULL)
		return FALSE;

	if (mlf->real_folder->folder_flags & CAMEL_FOLDER_HAS_SUMMARY_CAPABILITY) {
		folder->summary = mlf->real_folder->summary;
		camel_object_ref(mlf->real_folder->summary);
	}

	folder->permanent_flags = mlf->real_folder->permanent_flags;
	folder->folder_flags = mlf->real_folder->folder_flags;

	camel_object_hook_event(mlf->real_folder, "message_changed", mlf_proxy_message_changed, mlf);
	camel_object_hook_event(mlf->real_folder, "folder_changed", mlf_proxy_folder_changed, mlf);

	return TRUE;
}

static int
mlf_getv(CamelObject *object, CamelException *ex, CamelArgGetV *args)
{
	CamelFolder *folder = (CamelFolder *)object;
	MailLocalFolder *mlf = (MailLocalFolder *)object;
	int i, count=args->argc;
	guint32 tag;

	for (i=0;i<args->argc;i++) {
		CamelArgGet *arg = &args->argv[i];
		
		tag = arg->tag;
		
		switch (tag & CAMEL_ARG_TAG) {
			/* CamelObject args */
		case CAMEL_OBJECT_ARG_DESCRIPTION:
			if (mlf->description == NULL) {
				int pathlen;

				/* string to describe a local folder as the location of a message */
				pathlen = strlen(evolution_dir) + strlen("local") + 1;
				if (strlen(folder->full_name) > pathlen)
					mlf->description = g_strdup_printf(_("Local folders/%s"), folder->full_name+pathlen);
				else
					mlf->description = g_strdup_printf(_("Local folders/%s"), folder->name);
			}
			*arg->ca_str = mlf->description;
			break;
		default:
			count--;
			continue;
		}

		arg->tag = (tag & CAMEL_ARG_TYPE) | CAMEL_ARG_IGNORE;
	}

	if (count)
		return ((CamelObjectClass *)mlf_parent_class)->getv(object, ex, args);

	return 0;
}

static gboolean
mlf_meta_set(CamelObject *obj, const char *name, const char *value)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER(obj);
	CamelFolder *f;
	gboolean res;

	LOCAL_FOLDER_LOCK(mlf);
	f = mlf->real_folder;
	camel_object_ref(f);
	LOCAL_FOLDER_UNLOCK(mlf);

	/* We must write this ourselves, since MailLocalFolder is not persistent itself */
	if ((res = camel_object_meta_set(f, name, value)))
		camel_object_state_write(f);
	camel_object_unref(f);

	return res;
}

static char *
mlf_meta_get(CamelObject *obj, const char *name)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER(obj);
	CamelFolder *f;
	char * res;

	LOCAL_FOLDER_LOCK(mlf);
	f = mlf->real_folder;
	camel_object_ref(f);
	LOCAL_FOLDER_UNLOCK(mlf);

	res = camel_object_meta_get(f, name);
	camel_object_unref(f);

	return res;
}

static void 
mlf_class_init (CamelObjectClass *camel_object_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_object_class);

	/* override all the functions subclassed in providers/local/ */
	camel_folder_class->refresh_info = mlf_refresh_info;
	camel_folder_class->sync = mlf_sync;
	camel_folder_class->expunge = mlf_expunge;
	camel_folder_class->append_message = mlf_append_message;
	camel_folder_class->get_message = mlf_get_message;
	camel_folder_class->search_free = mlf_search_free;

	camel_folder_class->search_by_expression = mlf_search_by_expression;
	camel_folder_class->search_by_uids = mlf_search_by_uids;
	camel_folder_class->set_message_flags = mlf_set_message_flags;
	camel_folder_class->set_message_user_flag = mlf_set_message_user_flag;
	camel_folder_class->set_message_user_tag = mlf_set_message_user_tag;

	camel_folder_class->rename = mlf_rename;

	camel_object_class->getv = mlf_getv;

	camel_object_class->meta_get = mlf_meta_get;
	camel_object_class->meta_set = mlf_meta_set;
}

static void
mlf_init (CamelObject *obj, CamelObjectClass *klass)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER (obj);

#ifdef ENABLE_THREADS
	mlf->real_folder_lock = g_mutex_new();
#endif
}

static void
mlf_finalize (CamelObject *obj)
{
	MailLocalFolder *mlf = MAIL_LOCAL_FOLDER (obj);

	if (mlf->real_folder)
		mlf_unset_folder(mlf);

	free_metainfo(mlf->meta);
	
	g_free (mlf->real_path);
	
#ifdef ENABLE_THREADS
	g_mutex_free (mlf->real_folder_lock);
#endif
}

static CamelType
mail_local_folder_get_type (void)
{
	static CamelType mail_local_folder_type = CAMEL_INVALID_TYPE;

	if (mail_local_folder_type == CAMEL_INVALID_TYPE) {
		mail_local_folder_type = camel_type_register(CAMEL_FOLDER_TYPE,
							     "MailLocalFolder",
							     sizeof (MailLocalFolder),
							     sizeof (MailLocalFolderClass),
							     mlf_class_init,
							     NULL,
							     mlf_init,
							     mlf_finalize);
		mlf_parent_class = (CamelFolderClass *)CAMEL_FOLDER_TYPE;
	}

	return mail_local_folder_type;
}

static MailLocalFolder *
mail_local_folder_construct(MailLocalFolder *mlf, MailLocalStore *parent_store, const char *full_name, CamelException *ex)
{
	char *metapath, *name;
	
	name = g_path_get_basename (full_name);
	d(printf ("constructing local folder: full = %s, name = %s\n", full_name, name));
	camel_folder_construct (CAMEL_FOLDER (mlf), CAMEL_STORE (parent_store), full_name, name);
	g_free (name);
	
	mlf->real_path = g_strdup (((CamelFolder *) mlf)->full_name);
	
	metapath = g_strdup_printf ("%s/%s/local-metadata.xml", ((CamelService *) parent_store)->url->path, full_name);
	mlf->meta = load_metainfo (metapath);
	g_free (metapath);
	
	return mlf;
}

static gboolean
mail_local_folder_reconfigure (MailLocalFolder *mlf, const char *new_format, int index_body, CamelException *ex)
{
	CamelStore *fromstore = NULL;
	CamelFolder *fromfolder = NULL;
	char *oldformat = NULL;
	char *store_uri;
	GPtrArray *uids;
	int real_folder_frozen = FALSE;
	int format_change, index_changed;
	char *tmpname = NULL;
	char *mbox = NULL;

	format_change = strcmp(mlf->meta->format, new_format) != 0;
	index_changed = mlf->meta->indexed != index_body;

	if (format_change == FALSE && index_changed == FALSE)
		return TRUE;

	camel_operation_start(NULL, _("Reconfiguring folder"));

	/* first things first */
	g_assert (ex);
	LOCAL_FOLDER_LOCK (mlf);

	/* first, 'close' the old folder */
	if (mlf->real_folder) {
		camel_folder_sync(mlf->real_folder, FALSE, ex);
		if (camel_exception_is_set (ex))
			goto cleanup;
		mlf_unset_folder(mlf);
	}

	/* only indexed change, just re-open with new flags */
	if (!format_change) {
		mlf->meta->indexed = index_body;
		mlf_set_folder(mlf, CAMEL_STORE_FOLDER_CREATE, ex);
		save_metainfo(mlf->meta);
		goto cleanup;
	}

	store_uri = g_strdup_printf("%s:%s", mlf->meta->format, ((CamelService *)((CamelFolder *)mlf)->parent_store)->url->path);
	fromstore = camel_session_get_store(session, store_uri, ex);
	g_free(store_uri);
	if (fromstore == NULL)
		goto cleanup;

	oldformat = mlf->meta->format;
	mlf->meta->format = g_strdup(new_format);

	/* rename the old mbox and open it again, without indexing */
	tmpname = g_strdup_printf ("%s/%s_reconfig", mlf->real_path, mlf->meta->name);
	mbox = g_strdup_printf("%s/%s", mlf->real_path, mlf->meta->name);
	d(printf("renaming %s to %s, and opening it\n", mbox, tmpname));
	
	camel_store_rename_folder(fromstore, mbox, tmpname, ex);
	if (camel_exception_is_set(ex))
		goto cleanup;
	
	/* we dont need to set the create flag ... or need an index if it has one */
	fromfolder = camel_store_get_folder(fromstore, tmpname, 0, ex);
	if (fromfolder == NULL || camel_exception_is_set(ex)) {
		/* try and recover ... */
		camel_exception_clear(ex);
		camel_store_rename_folder(fromstore, tmpname, mbox, ex);
		goto cleanup;
	}
	
	/* create a new mbox */
	d(printf("Creating the destination mbox\n"));

	if (!mlf_set_folder(mlf, CAMEL_STORE_FOLDER_CREATE, ex)) {
		d(printf("cannot open destination folder\n"));
		/* try and recover ... */
		camel_exception_clear(ex);
		camel_store_rename_folder(fromstore, tmpname, mbox, ex);
		goto cleanup;
	}

	real_folder_frozen = TRUE;
	camel_folder_freeze(mlf->real_folder);

	uids = camel_folder_get_uids(fromfolder);
	camel_folder_transfer_messages_to(fromfolder, uids, mlf->real_folder, NULL, TRUE, ex);
	camel_folder_free_uids(fromfolder, uids);
	if (camel_exception_is_set(ex))
		goto cleanup;
	
	camel_folder_expunge(fromfolder, ex);
	
	d(printf("delete old mbox ...\n"));
	camel_object_unref(fromfolder);
	fromfolder = NULL;
	camel_store_delete_folder(fromstore, tmpname, ex);
	
	/* switch format */
	g_free(oldformat);
	oldformat = NULL;
	if (save_metainfo(mlf->meta) == FALSE) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot save folder metainfo; "
					"you may find you can't\n"
					"open this folder anymore: %s: %s"),
				      mlf->meta->path, strerror(errno));
	}
	
 cleanup:
	if (oldformat) {
		g_free(mlf->meta->format);
		mlf->meta->format = oldformat;
	}
	if (mlf->real_folder == NULL)
		mlf_set_folder (mlf, CAMEL_STORE_FOLDER_CREATE, ex);
	if (fromfolder)
		camel_object_unref(fromfolder);
	if (fromstore)
		camel_object_unref(fromstore);

	g_free(tmpname);
	g_free(mbox);

	LOCAL_FOLDER_UNLOCK (mlf);

	if (real_folder_frozen)
		camel_folder_thaw(mlf->real_folder);

	camel_operation_end(NULL);

	return !camel_exception_is_set(ex);
}
		
/* ******************************************************************************** */

static CamelObjectClass *local_store_parent_class = NULL;

static CamelFolder *
mls_get_folder(CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	MailLocalStore *local_store = MAIL_LOCAL_STORE (store);
	MailLocalFolder *folder;

	d(printf("--LOCAL-- get_folder: %s\n", folder_name));

	folder = (MailLocalFolder *)camel_object_new(MAIL_LOCAL_FOLDER_TYPE);
	folder = mail_local_folder_construct(folder, local_store, folder_name, ex);
	if (folder == NULL)
		return NULL;

	if (!mlf_set_folder(folder, flags, ex)) {
		camel_object_unref(folder);
		return NULL;
	}

	if (flags & CAMEL_STORE_FOLDER_CREATE) {
		if (save_metainfo(folder->meta) == FALSE) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Cannot save folder metainfo to %s: %s"),
					      folder->meta->path, g_strerror(errno));
			camel_object_unref(folder);
			return NULL;
		}
	}

	return (CamelFolder *)folder;
}

static void
mls_delete_folder(CamelStore *store, const char *folder_name, CamelException *ex)
{
	CamelStore *real_store;
	char *metapath, *uri, *mbox;
	CamelException local_ex;
	struct _local_meta *meta;

	d(printf("Deleting folder: %s %s\n", ((CamelService *)store)->url->path, folder_name));

	camel_exception_init(&local_ex);

	/* find the real store for this folder, and proxy the call */
	metapath = g_strdup_printf("%s%s/local-metadata.xml", ((CamelService *)store)->url->path, folder_name);
	meta = load_metainfo(metapath);
	uri = g_strdup_printf("%s:%s", meta->format, ((CamelService *)store)->url->path);
	real_store = (CamelStore *)camel_session_get_service(session, uri, CAMEL_PROVIDER_STORE, ex);
	g_free(uri);
	if (real_store == NULL) {
		g_free(metapath);
		free_metainfo(meta);
		camel_object_unref(real_store);
		return;
	}

	mbox = g_strdup_printf("%s/%s", folder_name, meta->name);
	camel_store_delete_folder(real_store, mbox, &local_ex);
	g_free(mbox);
	if (camel_exception_is_set(&local_ex)) {
		camel_exception_xfer(ex, &local_ex);
		g_free(metapath);
		free_metainfo(meta);
		camel_object_unref(real_store);
		return;
	}

	camel_object_unref((CamelObject *)real_store);

	free_metainfo(meta);

	if (unlink(metapath) == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot delete folder metadata %s: %s"),
				     metapath, g_strerror(errno));
	}

	g_free(metapath);
}

static void
mls_rename_folder(CamelStore *store, const char *old_name, const char *new_name, CamelException *ex)
{
	CamelStore *real_store;
	/*MailLocalStore *mls = (MailLocalStore *)store;*/
	char *uri;
	/*CamelException local_ex;*/
	struct _local_meta *meta;
	char *oldname, *newname;
	char *oldmeta, *newmeta;
	struct stat st;

	/* folder:rename() updates all our in-memory data to match */

	/* FIXME: Need to lock the subfolder that matches this if its open
	   Then rename it and unlock it when done */

	d(printf("Renaming folder from '%s' to '%s'\n", old_name, new_name));

	oldmeta = g_strdup_printf("%s%s/local-metadata.xml", ((CamelService *)store)->url->path, old_name);
	newmeta = g_strdup_printf("%s%s/local-metadata.xml", ((CamelService *)store)->url->path, new_name);

	meta = load_metainfo(oldmeta);
	uri = g_strdup_printf("%s:%s", meta->format, ((CamelService *)store)->url->path);
	real_store = (CamelStore *)camel_session_get_service(session, uri, CAMEL_PROVIDER_STORE, ex);
	g_free(uri);
	if (real_store == NULL) {
		g_free(newmeta);
		g_free(oldmeta);
		free_metainfo(meta);
		return;
	}

	oldname = g_strdup_printf("%s/%s", old_name, meta->name);
	newname = g_strdup_printf("%s/%s", new_name, meta->name);

	camel_store_rename_folder(real_store, oldname, newname, ex);
	if (!camel_exception_is_set(ex)) {
		/* If this fails?  Well, doesn't really matter but 'fail' anyway */
		if (stat(oldmeta, &st) == 0
		    && rename(oldmeta, newmeta) == -1) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Could not rename folder %s to %s: %s"),
					     old_name, new_name, strerror(errno));
		} else {
			/* So .. .the shell does a remove/add now the rename worked, so we dont
			   have to do this.  However totally broken that idea might be */
#if 0
			CamelFolderInfo *info;
			const char *tmp;
			char *olduri, *newuri;

			olduri = g_strdup_printf("%s:%s%s", ((CamelService *)store)->url->protocol, ((CamelService *)store)->url->path, old_name);
			newuri = g_strdup_printf("%s:%s%s", ((CamelService *)store)->url->protocol, ((CamelService *)store)->url->path, new_name);
			info = g_hash_table_lookup(mls->folder_infos, olduri);
			if (info) {
				CamelRenameInfo reninfo;

				g_free(info->url);
				g_free(info->full_name);
				g_free(info->name);
				g_free(info->path);
				info->url = newuri;
				info->full_name = g_strdup(new_name);
				info->path = g_strdup_printf("/%s", new_name);
				tmp = strchr(new_name, '/');
				if (tmp == NULL)
					tmp = new_name;
				info->name = g_strdup(tmp);
				g_hash_table_insert(mls->folder_infos, info->url, info);

				reninfo.new = info;
				reninfo.old_base = (char *)old_name;
				
				camel_object_trigger_event(store, "folder_renamed", &reninfo);
			} else {
				g_free(newuri);
				g_warning("Cannot find existing folder '%s' in table?\n", olduri);
			}

			g_free(olduri);
#endif
		}
	}

	g_free(newname);
	g_free(oldname);

	camel_object_unref(real_store);

	free_metainfo(meta);

	g_free(newmeta);
	g_free(oldmeta);
}

static char *
mls_get_name (CamelService *service, gboolean brief)
{
	if (brief)
		return g_strdup("local");

	return g_strdup("Local mail folders");
}

static void
mls_init (MailLocalStore *mls, MailLocalStoreClass *mlsclass)
{
	mls->folder_infos = g_hash_table_new(g_str_hash, g_str_equal);
	mls->folder_info_lock = g_mutex_new();
}

static void
free_info(void *key, void *value, void *data)
{
	CamelFolderInfo *info = value;

	camel_folder_info_free (info);
}

static void
mls_finalise(MailLocalStore *mls)
{
	g_hash_table_foreach(mls->folder_infos, (GHFunc)free_info, NULL);
	g_hash_table_destroy(mls->folder_infos);
	g_mutex_free(mls->folder_info_lock);
}

static void
mls_class_init (CamelObjectClass *camel_object_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS(camel_object_class);
	CamelServiceClass *camel_service_class = CAMEL_SERVICE_CLASS(camel_object_class);
	
	/* virtual method overload -- the bare minimum */
	camel_service_class->get_name    = mls_get_name;
	camel_store_class->get_folder    = mls_get_folder;
	camel_store_class->delete_folder = mls_delete_folder;
	camel_store_class->rename_folder = mls_rename_folder;

	local_store_parent_class = camel_type_get_global_classfuncs (CAMEL_STORE_TYPE);
}

static CamelType
mail_local_store_get_type (void)
{
	static CamelType mail_local_store_type = CAMEL_INVALID_TYPE;

	if (mail_local_store_type == CAMEL_INVALID_TYPE) {
		mail_local_store_type = camel_type_register (
			CAMEL_STORE_TYPE, "MailLocalStore",
			sizeof (MailLocalStore),
			sizeof (MailLocalStoreClass),
			(CamelObjectClassInitFunc) mls_class_init,
			NULL,
			(CamelObjectInitFunc) mls_init,
			(CamelObjectFinalizeFunc) mls_finalise);
	}

	return mail_local_store_type;
}

static void mail_local_store_add_folder(MailLocalStore *mls, const char *uri, const char *path, const char *name)
{
	CamelFolderInfo *info = NULL;
	CamelURL *url;

	d(printf("Shell adding folder: '%s' path = '%s'\n", uri, path));

	url = camel_url_new(uri, NULL);
	if (url == NULL) {
		g_warning("Shell trying to add invalid folder url: %s", uri);
		return;
	}
	if (url->path == NULL || url->path[0] == 0) {
		g_warning("Shell trying to add invalid folder url: %s", uri);
		camel_url_free(url);
		return;
	}

	LOCAL_STORE_LOCK(mls);

	if (g_hash_table_lookup(mls->folder_infos, uri)) {
		g_warning("Shell trying to add a folder I already have!");
	} else {
		info = g_malloc0(sizeof(*info));
		info->url = g_strdup(uri);
		info->full_name = g_strdup(url->path+1);
		info->name = g_strdup(name);
		info->unread_message_count = -1;
		info->path = g_strdup (path);
		g_hash_table_insert(mls->folder_infos, info->url, info);
	}

	LOCAL_STORE_UNLOCK(mls);

	camel_url_free(url);

	if (info) {
		/* FIXME: should copy info, so we dont get a removed while we're using it? */
		camel_object_trigger_event(mls, "folder_created", info);

		/* this is just so the folder is opened at least once to setup the folder
		   counts etc in the display.  Joy eh?   The result is discarded. */
		mail_get_folder (uri, CAMEL_STORE_FOLDER_CREATE, NULL, NULL, mail_thread_queued_slow);
	}
}

struct _search_info {
	const char *path;
	CamelFolderInfo *info;
};

static void
remove_find_path(char *uri, CamelFolderInfo *info, struct _search_info *data)
{
	if (!strcmp(info->path, data->path))
		data->info = info;
}

static void mail_local_store_remove_folder(MailLocalStore *mls, const char *path)
{
	struct _search_info data = { path, NULL };

	d(printf("shell removing folder? '%s'\n", path));

	/* we're keyed on uri, not path, so have to search for it manually */

	LOCAL_STORE_LOCK(mls);
	g_hash_table_foreach(mls->folder_infos, (GHFunc)remove_find_path, &data);
	if (data.info)
		g_hash_table_remove(mls->folder_infos, data.info->url);
	LOCAL_STORE_UNLOCK(mls);

	if (data.info) {
		camel_object_trigger_event(mls, "folder_deleted", data.info);

		g_free(data.info->url);
		g_free(data.info->full_name);
		g_free(data.info->name);
		g_free(data.info);
	}
}

/* ** Local Provider ************************************************************** */

static CamelProvider local_provider = {
	"file", "Local mail", "Local mailbox file", "mail",
	CAMEL_PROVIDER_IS_STORAGE | CAMEL_PROVIDER_IS_EXTERNAL,
	CAMEL_URL_NEED_PATH,
	/* ... */
};

/* There's only one "file:" store. */
static guint
non_hash (gconstpointer key)
{
	return 0;
}

static gint
non_equal (gconstpointer ap, gconstpointer bp)
{
	const CamelURL *a = ap, *b = bp;

	return strcmp(a->protocol, "file") == 0
		&& strcmp(a->protocol, b->protocol) == 0;
}

static void
mail_local_provider_init (void)
{
	/* Register with Camel to handle file: URLs */
	local_provider.object_types[CAMEL_PROVIDER_STORE] = MAIL_LOCAL_STORE_TYPE;

	local_provider.url_hash = non_hash;
	local_provider.url_equal = non_equal;
	camel_session_register_provider (session, &local_provider);
}

/* ** Local Storage Listener ****************************************************** */

static void
local_storage_new_folder_cb_trash_or_junk (const GNOME_Evolution_Folder *folder, const gchar *path, gchar *full_name, gchar *url_base)
{
		CamelFolderInfo info;
		CamelURL *url;

		url = camel_url_new(folder->physicalUri, NULL);
		if (url == NULL) {
			g_warning("Shell trying to add invalid folder url: %s", folder->physicalUri);
			return;
		}
		if (url->path == NULL || url->path[0] == 0) {
			g_warning("Shell trying to add invalid folder url: %s", folder->physicalUri);
			camel_url_free(url);
			return;
		}

		memset(&info, 0, sizeof(info));
		info.full_name = full_name;
		info.name = folder->displayName;
		info.url = g_strdup_printf("%s:%s", url_base, folder->physicalUri);
		info.unread_message_count = 0;
		info.path = (char *)path;

		camel_object_trigger_event(global_local_store, "folder_created", &info);
		g_free(info.url);
		camel_url_free(url);
}

static void
local_storage_new_folder_cb (EvolutionStorageListener *storage_listener,
			     const char *path,
			     const GNOME_Evolution_Folder *folder,
			     void *data)
{
	d(printf("Local folder new:\n"));
	d(printf(" path = '%s'\n uri = '%s'\n display = '%s'\n",
		 path, folder->physicalUri, folder->displayName));

	/* We dont actually add the trash/junk to our local folders list, get_trash is handled
	   outside our internal folder list */

	if (strcmp(folder->type, "mail") == 0) {
		mail_local_store_add_folder(global_local_store, folder->physicalUri, path, folder->displayName);
	} else if (strcmp(folder->type, "vtrash") == 0)
		local_storage_new_folder_cb_trash_or_junk (folder, path, CAMEL_VTRASH_NAME, "vtrash");
	else if (strcmp(folder->type, "vjunk") == 0)
		local_storage_new_folder_cb_trash_or_junk (folder, path, CAMEL_VJUNK_NAME, "vjunk");

}


static void
local_storage_removed_folder_cb (EvolutionStorageListener *storage_listener,
				 const char *path,
				 void *data)
{
	d(printf("Local folder remove:\n"));
	d(printf(" path = '%s'\n", path));

	mail_local_store_remove_folder(global_local_store, path);
}

static void
storage_listener_startup (EvolutionShellClient *shellclient)
{
	EvolutionStorageListener *local_storage_listener;
	GNOME_Evolution_StorageListener corba_local_storage_listener;
	GNOME_Evolution_Storage corba_storage;
	CORBA_Environment ev;

	d(printf("---- CALLING STORAGE LISTENER STARTUP ---\n"));

	local_corba_storage = corba_storage = evolution_shell_client_get_local_storage (shellclient);
	if (corba_storage == CORBA_OBJECT_NIL) {
		g_warning ("No local storage available from shell client!");
		return;
	}

	/* setup to record this store's changes */
	mail_note_store((CamelStore *)global_local_store, NULL, NULL, local_corba_storage, NULL, NULL);

	local_storage_listener = evolution_storage_listener_new ();
	corba_local_storage_listener = evolution_storage_listener_corba_objref (
		local_storage_listener);

	g_signal_connect(local_storage_listener,
			 "new_folder",
			 G_CALLBACK (local_storage_new_folder_cb),
			 corba_storage);
	g_signal_connect(local_storage_listener,
			 "removed_folder",
			 G_CALLBACK (local_storage_removed_folder_cb),
			 corba_storage);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_addListener (corba_storage,
					     corba_local_storage_listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot add a listener to the Local Storage.");
		CORBA_exception_free (&ev);
		return;
	}
	CORBA_exception_free (&ev);
}

/* ** The rest ******************************************************************** */

void
mail_local_storage_startup (EvolutionShellClient *shellclient, const char *evolution_path)
{
	mail_local_provider_init ();

	global_local_store = MAIL_LOCAL_STORE(camel_session_get_service (session, "file:/", CAMEL_PROVIDER_STORE, NULL));

	if (!global_local_store) {
		g_warning ("No local store!");
		return;
	}

	storage_listener_startup (shellclient);
}

void
mail_local_storage_shutdown (void)
{
	bonobo_object_release_unref (local_corba_storage, NULL);
	local_corba_storage = CORBA_OBJECT_NIL;
}


/*----------------------------------------------------------------------
 * Local folder reconfiguration stuff
 *----------------------------------------------------------------------*/

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

/* we should have our own progress bar for this */

struct _reconfigure_msg {
	struct _mail_msg msg;

	char *uri;
	CamelFolder *folder;

	char *newtype;
	unsigned int index_body:1;
	GtkWidget *frame;
	GtkWidget *apply;
	GtkWidget *cancel;
	GtkWidget *check_index_body;
	GtkOptionMenu *optionlist;

	void (*done)(const char *uri, CamelFolder *folder, void*data);
	void *done_data;
};

/* hash table of folders that the user has a reconfig-folder dialog for */
static GHashTable *reconfigure_folder_hash = NULL;

static char *
reconfigure_folder_describe (struct _mail_msg *mm, int done)
{
	struct _reconfigure_msg *m = (struct _reconfigure_msg *)mm;
	
	return g_strdup_printf (_("Changing folder \"%s\" to \"%s\" format"),
				camel_folder_get_full_name (m->folder),
				m->newtype);
}

static void
reconfigure_folder_reconfigure (struct _mail_msg *mm)
{
	struct _reconfigure_msg *m = (struct _reconfigure_msg *)mm;

	d(printf("reconfiguring folder: %s to type %s\n", m->uri, m->newtype));
	
	mail_local_folder_reconfigure (MAIL_LOCAL_FOLDER (m->folder), m->newtype, m->index_body, &mm->ex);
}

static void
reconfigure_folder_reconfigured (struct _mail_msg *mm)
{
	struct _reconfigure_msg *m = (struct _reconfigure_msg *)mm;
	GtkWidget *dialog;
	/*char *uri;*/
	
	if (camel_exception_is_set (&mm->ex)) {
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK, "%s",
						 _("If you can no longer open this mailbox, then\n"
						   "you may need to repair it manually."));
		gtk_dialog_run (GTK_DIALOG (dialog));
	}

	if (m->done)
		m->done(m->uri, m->folder, m->done_data);
}

static void
reconfigure_folder_free (struct _mail_msg *mm)
{
	struct _reconfigure_msg *m = (struct _reconfigure_msg *)mm;

	/* remove this folder from our hash since we are done with it */
	g_hash_table_remove (reconfigure_folder_hash, m->folder);
	if (g_hash_table_size (reconfigure_folder_hash) == 0) {
		/* additional cleanup */
		g_hash_table_destroy (reconfigure_folder_hash);
		reconfigure_folder_hash = NULL;
	}

	if (m->folder)
		camel_object_unref (m->folder);
	g_free(m->uri);
	g_free (m->newtype);
}

static struct _mail_msg_op reconfigure_folder_op = {
	reconfigure_folder_describe,
	reconfigure_folder_reconfigure,
	reconfigure_folder_reconfigured,
	reconfigure_folder_free,
};

static void
reconfigure_response(GtkDialog *dialog, int button, struct _reconfigure_msg *m)
{
	switch(button) {
	case GTK_RESPONSE_OK: {
		GtkWidget *menu, *item;

		menu = gtk_option_menu_get_menu(m->optionlist);
		item = gtk_menu_get_active(GTK_MENU(menu));
		m->newtype = g_strdup(g_object_get_data ((GObject *)item, "type"));
		m->index_body = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m->check_index_body));

		gtk_widget_set_sensitive (m->frame, FALSE);
		gtk_widget_set_sensitive (m->apply, FALSE);
		gtk_widget_set_sensitive (m->cancel, FALSE);
		
		e_thread_put (mail_thread_new, (EMsg *)m);
		break; }
	case GTK_RESPONSE_CANCEL:
	default:
		if (m->done)
			m->done(m->uri, NULL, m->done_data);
		mail_msg_free ((struct _mail_msg *)m);
		break;
	}

	gtk_widget_destroy((GtkWidget *)dialog);
}

static void
reconfigure_got_folder(char *uri, CamelFolder *folder, void *data)
{
	GladeXML *gui;
	GtkDialog *gd;
	struct _reconfigure_msg *m = data;
	char *title;
	GList *p;
	GtkWidget *menu;
	char *currentformat;
	int index=0, history=0;

	if (folder == NULL
	    || !MAIL_IS_LOCAL_FOLDER (folder)) {
		g_warning ("Trying to reconfigure nonexistant folder");
		/* error display ? */
		if (m->done)
			m->done(uri, NULL, m->done_data);
		mail_msg_free((struct _mail_msg *)m);
		return;
	}
	
	if (!reconfigure_folder_hash)
		reconfigure_folder_hash = g_hash_table_new (NULL, NULL);
	
	if ((gd = g_hash_table_lookup (reconfigure_folder_hash, folder))) {
		gdk_window_raise (GTK_WIDGET (gd)->window);
		if (m->done)
			m->done(uri, NULL, m->done_data);
		mail_msg_free((struct _mail_msg *)m);
		return;
	}
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/local-config.glade", "dialog_format", NULL);
	gd = (GtkDialog *)glade_xml_get_widget (gui, "dialog_format");
	
	title = g_strdup_printf (_("Reconfigure /%s"),
				 camel_folder_get_full_name (folder));
	gtk_window_set_title (GTK_WINDOW (gd), title);
	g_free (title);

	m->uri = g_strdup(uri);
	m->frame = glade_xml_get_widget (gui, "frame_format");
	m->apply = glade_xml_get_widget (gui, "apply_format");
	m->cancel = glade_xml_get_widget (gui, "cancel_format");
	m->optionlist = (GtkOptionMenu *)glade_xml_get_widget (gui, "option_format");
	m->check_index_body = glade_xml_get_widget (gui, "check_index_body");
	m->newtype = NULL;
	m->folder = folder;
	camel_object_ref(folder);
	
	/* dynamically create the folder type list from camel */
	/* we assume the list is static and never freed */
	currentformat = MAIL_LOCAL_FOLDER (folder)->meta->format;
	p = camel_session_list_providers (session, TRUE);
	menu = gtk_menu_new ();
	while (p) {
		CamelProvider *cp = p->data;
		
		/* we only want local providers */
		if (cp->flags & CAMEL_PROVIDER_IS_LOCAL) {
			GtkWidget *item;
			char *label;
			
			if (!strcmp (cp->protocol, currentformat))
				history = index;
			
			label = g_strdup_printf("%s (%s)", cp->protocol, _(cp->name));
			item = gtk_menu_item_new_with_label (label);
			g_free (label);
			g_object_set_data ((GObject *) item, "type", cp->protocol);
			gtk_widget_show (item);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
			index++;
		}
		p = p->next;
	}
	gtk_option_menu_remove_menu (GTK_OPTION_MENU (m->optionlist));
	gtk_option_menu_set_menu (GTK_OPTION_MENU(m->optionlist), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(m->optionlist), history);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m->check_index_body), MAIL_LOCAL_FOLDER (folder)->meta->indexed);

	gtk_label_set_text ((GtkLabel *)glade_xml_get_widget (gui, "label_format"),
			    MAIL_LOCAL_FOLDER (folder)->meta->format);
	
	g_signal_connect(gd, "response", G_CALLBACK(reconfigure_response), m);
	g_object_unref(gui);
	
	g_hash_table_insert (reconfigure_folder_hash, (gpointer) folder, (gpointer) gd);
	
	gtk_widget_show((GtkWidget *)gd);
}

void
mail_local_reconfigure_folder(const char *uri, void (*done)(const char *uri, CamelFolder *folder, void *data), void *done_data)
{
	struct _reconfigure_msg *m;

	if (strncmp(uri, "file:", 5) != 0) {
		e_notice (NULL, GTK_MESSAGE_WARNING,
			  _("You cannot change the format of a non-local folder."));
		if (done) 
			done(uri, NULL, done_data);
		return;
	}

	m = mail_msg_new (&reconfigure_folder_op, NULL, sizeof (*m));
	m->done = done;
	m->done_data = done_data;

	mail_get_folder(uri, 0, reconfigure_got_folder, m, mail_thread_new);
}
