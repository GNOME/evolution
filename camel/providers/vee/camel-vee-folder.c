/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 */

#include "camel-exception.h"
#include "camel-vee-folder.h"
#include "camel-folder-summary.h"
#include "camel-mime-message.h"

#include <string.h>

/* our message info includes the parent folder */
typedef struct _CamelVeeMessageInfo {
	CamelMessageInfo info;
	CamelFolder *folder;
} CamelVeeMessageInfo;

struct _CamelVeeFolderPrivate {
	GList *folders;
};

#define _PRIVATE(o) (((CamelVeeFolder *)(o))->priv)

static void vee_init (CamelFolder *folder, CamelStore *parent_store,
		   CamelFolder *parent_folder, const gchar *name,
		   gchar separator, CamelException *ex);

static void vee_open (CamelFolder *folder, CamelFolderOpenMode mode, CamelException *ex);
static void vee_close (CamelFolder *folder, gboolean expunge, CamelException *ex);
static gboolean vee_exists (CamelFolder *folder, CamelException *ex);

static GList *vee_get_uid_list  (CamelFolder *folder, CamelException *ex);

static gint vee_get_message_count (CamelFolder *folder, CamelException *ex);
static CamelMimeMessage *vee_get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex);

GPtrArray *vee_summary_get_message_info (CamelFolder *folder, int first, int count);
static const CamelMessageInfo *vee_summary_get_by_uid(CamelFolder *f, const char *uid);


static void camel_vee_folder_class_init (CamelVeeFolderClass *klass);
static void camel_vee_folder_init       (CamelVeeFolder *obj);
static void camel_vee_folder_finalise   (GtkObject *obj);

static void vee_folder_build(CamelVeeFolder *vf, CamelException *ex);

static CamelFolderClass *camel_vee_folder_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
camel_vee_folder_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelVeeFolder",
			sizeof (CamelVeeFolder),
			sizeof (CamelVeeFolderClass),
			(GtkClassInitFunc) camel_vee_folder_class_init,
			(GtkObjectInitFunc) camel_vee_folder_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (camel_folder_get_type (), &type_info);
	}
	
	return type;
}

static void
camel_vee_folder_class_init (CamelVeeFolderClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	CamelFolderClass *folder_class = (CamelFolderClass *) klass;

	camel_vee_folder_parent = gtk_type_class (camel_folder_get_type ());

	printf("vfolder class init\n");
	
	folder_class->init = vee_init;
	folder_class->open = vee_open;
	folder_class->close = vee_close;
	folder_class->exists = vee_exists;

	folder_class->get_uid_list = vee_get_uid_list;
	folder_class->get_message_by_uid = vee_get_message_by_uid;

	folder_class->get_message_info = vee_summary_get_message_info;
	folder_class->summary_get_by_uid = vee_summary_get_by_uid;

	folder_class->get_message_count = vee_get_message_count;

	object_class->finalize = camel_vee_folder_finalise;

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
camel_vee_folder_init (CamelVeeFolder *obj)
{
	struct _CamelVeeFolderPrivate *p;

	printf("vfolder init\n");

	p = _PRIVATE(obj) = g_malloc0(sizeof(*p));
}

static void
camel_vee_folder_finalise (GtkObject *obj)
{
	((GtkObjectClass *)(camel_vee_folder_parent))->finalize((GtkObject *)obj);
}

/**
 * camel_vee_folder_new:
 *
 * Create a new CamelVeeFolder object.
 * 
 * Return value: A new CamelVeeFolder widget.
 **/
CamelVeeFolder *
camel_vee_folder_new (void)
{
	CamelVeeFolder *new = CAMEL_VEE_FOLDER ( gtk_type_new (camel_vee_folder_get_type ()));
	return new;
}


static void vee_init (CamelFolder *folder, CamelStore *parent_store,
		   CamelFolder *parent_folder, const gchar *name,
		   gchar separator, CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;

	camel_vee_folder_parent->init (folder, parent_store, parent_folder, name, separator, ex);
	if (camel_exception_get_id (ex))
		return;

	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = FALSE;
	folder->has_summary_capability = TRUE;
	folder->has_uid_capability = TRUE;
	folder->has_search_capability = TRUE;

	/* FIXME: what to do about user flags if the subfolder doesn't support them? */
	folder->permanent_flags = CAMEL_MESSAGE_ANSWERED |
		CAMEL_MESSAGE_DELETED |
		CAMEL_MESSAGE_DRAFT |
		CAMEL_MESSAGE_FLAGGED |
		CAMEL_MESSAGE_SEEN;

	vf->messages = g_ptr_array_new();
	vf->messages_uid = g_hash_table_new(g_str_hash, g_str_equal);

	vf->expression = g_strdup(folder->name);
}

static void vee_open (CamelFolder *folder, CamelFolderOpenMode mode, CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;

	camel_vee_folder_parent->open (folder, mode, ex);
	if (camel_exception_get_id(ex))
		return;

	/* perform search on folders to be searched ... */
	vee_folder_build(vf, ex);
}

static void vee_close (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	camel_vee_folder_parent->close (folder, expunge, ex);

	/* FIXME: close vfolder? */
}

/* vfolders always exist? */
static gboolean vee_exists (CamelFolder *folder, CamelException *ex)
{
	return TRUE;
}

static gint vee_get_message_count (CamelFolder *folder, CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;

	return vf->messages->len;
}

static CamelMimeMessage *vee_get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelVeeMessageInfo *mi;

	mi = (CamelVeeMessageInfo *)vee_summary_get_by_uid(folder, uid);
	if (mi == NULL) {
		camel_exception_setv(ex, 1, "Failed");
		return NULL;
	}
	return camel_folder_get_message_by_uid(mi->folder, strchr(mi->info.uid, ':')+1, ex);
}

GPtrArray *vee_summary_get_message_info (CamelFolder *folder, int first, int count)
{
	GPtrArray *result;
	int i, max;
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;

	result = g_ptr_array_new();
	max = MIN(vf->messages->len, count+first);
	for (i=first;i<max;i++) {
		g_ptr_array_add(result, g_ptr_array_index(vf->messages, i));
	}
	return result;
}

static const CamelMessageInfo *vee_summary_get_by_uid(CamelFolder *f, const char *uid)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)f;

	return g_hash_table_lookup(vf->messages_uid, uid);
}

static GList *vee_get_uid_list  (CamelFolder *folder, CamelException *ex)
{
	GList *result = NULL;
	int i;
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;

	for (i=0;i<vf->messages->len;i++) {
		CamelMessageInfo *mi = g_ptr_array_index(vf->messages, i);
		result = g_list_prepend(result, g_strdup(mi->uid));
	}

	return result;
}

void
camel_vee_folder_add_folder(CamelVeeFolder *vf, CamelFolder *sub)
{
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);

	gtk_object_ref((GtkObject *)sub);
	p->folders = g_list_append(p->folders, sub);
}

/*
  need incremental update, based on folder.
  Need to watch folders for changes and update accordingly.
*/

/* this does most of the vfolder magic */
static void
vee_folder_build(CamelVeeFolder *vf, CamelException *ex)
{
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GList *node;

	GPtrArray *messages;
	GHashTable *messages_uid;

	messages = g_ptr_array_new();
	messages_uid = g_hash_table_new(g_str_hash, g_str_equal);

	node = p->folders;
	while (node) {
		GList *matches, *match;
		CamelFolder *f = node->data;
		CamelVeeMessageInfo *mi;
		const CamelMessageInfo *info;
		CamelFlag *flag;

		matches = camel_folder_search_by_expression(f, vf->expression, ex);
		match = matches;
		while (match) {
			info = camel_folder_summary_get_by_uid(f, match->data);
			if (info) {
				mi = g_malloc0(sizeof(*mi));
				mi->info.subject = g_strdup(info->subject);
				mi->info.to = g_strdup(info->to);
				mi->info.from = g_strdup(info->from);
				mi->info.uid = g_strdup_printf("%p:%s", f, info->uid);
				mi->info.flags = info->flags;
				mi->info.size = info->size;
				mi->info.date_sent = info->date_sent;
				mi->info.date_received = info->date_received;
				flag = info->user_flags;
				while (flag) {
					camel_flag_set(&mi->info.user_flags, flag->name, TRUE);
					flag = flag->next;
				}
				mi->info.content = NULL;
				mi->folder = f;
				g_ptr_array_add(messages, mi);
				g_hash_table_insert(messages_uid, mi->info.uid, mi);
			}
			match = g_list_next(match);
		}
		g_list_free(matches);
		node = g_list_next(node);
	}

#warning "free messages on query update"
	vf->messages = messages;
	vf->messages_uid = messages_uid;
}


/*

  (match-folder "folder1" "folder2")

 */
