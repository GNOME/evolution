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
		      gchar *separator, gboolean path_begins_with_sep,
		      CamelException *ex);

static GPtrArray *vee_get_uids  (CamelFolder *folder, CamelException *ex);
GPtrArray *vee_get_summary (CamelFolder *folder, CamelException *ex);
void vee_free_summary (CamelFolder *folder, GPtrArray *array);

static gint vee_get_message_count (CamelFolder *folder, CamelException *ex);
static CamelMimeMessage *vee_get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex);

static const CamelMessageInfo *vee_summary_get_by_uid(CamelFolder *f, const char *uid);
static GList *vee_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex);

static guint32 vee_get_message_flags (CamelFolder *folder, const char *uid, CamelException *ex);
static void vee_set_message_flags (CamelFolder *folder, const char *uid, guint32 flags, guint32 set, CamelException *ex);
static gboolean vee_get_message_user_flag (CamelFolder *folder, const char *uid, const char *name, CamelException *ex);
static void vee_set_message_user_flag (CamelFolder *folder, const char *uid, const char *name, gboolean value, CamelException *ex);


static void camel_vee_folder_class_init (CamelVeeFolderClass *klass);
static void camel_vee_folder_init       (CamelVeeFolder *obj);
static void camel_vee_folder_finalise   (GtkObject *obj);

static void vee_folder_build(CamelVeeFolder *vf, CamelException *ex);
static void vee_folder_build_folder(CamelVeeFolder *vf, CamelFolder *source, CamelException *ex);

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

	folder_class->get_uids = vee_get_uids;
	folder_class->get_summary = vee_get_summary;
	folder_class->free_summary = vee_free_summary;
	folder_class->get_message_by_uid = vee_get_message_by_uid;

	folder_class->summary_get_by_uid = vee_summary_get_by_uid;

	folder_class->get_message_count = vee_get_message_count;
	folder_class->search_by_expression = vee_search_by_expression;

	folder_class->get_message_flags = vee_get_message_flags;
	folder_class->set_message_flags = vee_set_message_flags;
	folder_class->get_message_user_flag = vee_get_message_user_flag;
	folder_class->set_message_user_flag = vee_set_message_user_flag;

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
	CamelVeeFolder *vf = (CamelVeeFolder *)obj;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	GList *node;

	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;
		gtk_object_unref((GtkObject *)f);
		node = g_list_next(node);
	}
	
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

static void
folder_changed(CamelFolder *sub, int type, CamelVeeFolder *vf)
{
	CamelException *ex;

	printf("subfolder changed!!, re-searching\n");

	ex = camel_exception_new();
	vee_folder_build_folder(vf, sub, ex);
	camel_exception_free(ex);
	/* FIXME: should only raise follow-on event if the result changed */
	gtk_signal_emit_by_name((GtkObject *)vf, "folder_changed", 0);
}

/* track flag changes in the summary */
static void
message_changed(CamelFolder *f, const char *uid, CamelVeeFolder *mf)
{
	const CamelMessageInfo *info;
	CamelMessageInfo *vinfo;
	CamelFlag *flag;
	char *vuid;

	printf("VMessage changed: %s\n", uid);
	info = camel_folder_summary_get_by_uid(f, uid);

	vuid = g_strdup_printf("%p:%s", f, uid);
	vinfo = (CamelMessageInfo *)vee_summary_get_by_uid((CamelFolder *)mf, vuid);
	if (info && vinfo) {
		vinfo->flags = info->flags;
		camel_flag_list_free(&vinfo->user_flags);
		flag = info->user_flags;
		while (flag) {
			camel_flag_set(&vinfo->user_flags, flag->name, TRUE);
			flag = flag->next;
		}
		gtk_signal_emit_by_name((GtkObject *)mf, "message_changed", vinfo->uid);
	}
	g_free(vuid);
}

void
camel_vee_folder_add_folder(CamelVeeFolder *vf, CamelFolder *sub)
{
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	CamelException *ex;

	gtk_object_ref((GtkObject *)sub);
	p->folders = g_list_append(p->folders, sub);

	gtk_signal_connect((GtkObject *)sub, "folder_changed", folder_changed, vf);
	gtk_signal_connect((GtkObject *)sub, "message_changed", message_changed, vf);

	ex = camel_exception_new();
	vee_folder_build_folder(vf, sub, ex);
	camel_exception_free(ex);
	/* FIXME: should only raise follow-on event if the result changed */
	gtk_signal_emit_by_name((GtkObject *)vf, "folder_changed", 0);
}


static void vee_init (CamelFolder *folder, CamelStore *parent_store,
		      CamelFolder *parent_folder, const gchar *name,
		      gchar *separator, gboolean path_begins_with_sep,
		      CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;
	char *namepart, *searchpart;

	namepart = g_strdup(name);
	searchpart = strchr(namepart, '?');
	if (searchpart == NULL) {
		/* no search, no result! */
		searchpart = "(body-contains \"=some-invalid_string-sequence=xx\")";
	} else {
		*searchpart++ = 0;
	}

	camel_vee_folder_parent->init (folder, parent_store, parent_folder, name, separator, TRUE, ex);
	if (camel_exception_get_id (ex))
		return;

	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = FALSE;
	folder->has_summary_capability = TRUE;
	folder->has_search_capability = TRUE;

	/* FIXME: what to do about user flags if the subfolder doesn't support them? */
	folder->permanent_flags = CAMEL_MESSAGE_ANSWERED |
		CAMEL_MESSAGE_DELETED |
		CAMEL_MESSAGE_DRAFT |
		CAMEL_MESSAGE_FLAGGED |
		CAMEL_MESSAGE_SEEN;

	vf->messages = g_ptr_array_new();
	vf->messages_uid = g_hash_table_new(g_str_hash, g_str_equal);

	vf->expression = g_strdup_printf("(or\n (match-all (user-flag \"%s\"))\n %s\n)", namepart, searchpart);
	vf->vname = g_strdup(namepart);

	printf("VFolder expression is %s\n", vf->expression);
	printf("VFolder full name = %s\n", camel_folder_get_full_name(folder));

	g_free(namepart);

	vee_folder_build(vf, ex);
}

static gint vee_get_message_count (CamelFolder *folder, CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;

	return vf->messages->len;
}

static gboolean
get_real_message (CamelFolder *folder, const char *uid,
		  CamelFolder **out_folder, const char **out_uid,
		  CamelException *ex)
{
	CamelVeeMessageInfo *mi;

	mi = (CamelVeeMessageInfo *)vee_summary_get_by_uid(folder, uid);
	if (mi == NULL) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				     "No such message %s in %s", uid,
				     folder->name);
		return FALSE;
	}

	*out_folder = mi->folder;
	*out_uid = strchr(mi->info.uid, ':')+1;
	return TRUE;
}

static CamelMimeMessage *vee_get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	const char *real_uid;
	CamelFolder *real_folder;

	if (!get_real_message (folder, uid, &real_folder, &real_uid, ex))
		return NULL;

	return camel_folder_get_message_by_uid(real_folder, real_uid, ex);
}

GPtrArray *vee_get_summary (CamelFolder *folder, CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;

	return vf->messages;
}

void vee_free_summary (CamelFolder *folder, GPtrArray *array)
{
	/* no op */
}

static const CamelMessageInfo *vee_summary_get_by_uid(CamelFolder *f, const char *uid)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)f;

	return g_hash_table_lookup(vf->messages_uid, uid);
}

static GPtrArray *vee_get_uids (CamelFolder *folder, CamelException *ex)
{
	GPtrArray *result;
	int i;
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;

	result = g_ptr_array_new ();
	g_ptr_array_set_size (result, vf->messages->len);
	for (i=0;i<vf->messages->len;i++) {
		CamelMessageInfo *mi = g_ptr_array_index(vf->messages, i);
		result->pdata[i] = g_strdup(mi->uid);
	}

	return result;
}

static GList *
vee_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex)
{
	GList *result = NULL, *node;
	char *expr;
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);

	expr = g_strdup_printf("(and %s %s)", vf->expression, expression);
	node = p->folders;
	while (node) {
		CamelFolder *f = node->data;
		GList *matches, *match;
		matches = camel_folder_search_by_expression(f, vf->expression, ex);
		match = matches;
		while (match) {
			char *uid = match->data;
			result = g_list_prepend(result, g_strdup_printf("%p:%s", f, uid));
			match = g_list_next(match);
		}
		g_list_free(matches);
		node = g_list_next(node);
	}
	return result;
}

static guint32
vee_get_message_flags(CamelFolder *folder, const char *uid, CamelException *ex)
{
	const char *real_uid;
	CamelFolder *real_folder;

	if (!get_real_message (folder, uid, &real_folder, &real_uid, ex))
		return 0;

	return camel_folder_get_message_flags(real_folder, real_uid, ex);
}

static void
vee_set_message_flags(CamelFolder *folder, const char *uid, guint32 flags,
		      guint32 set, CamelException *ex)
{
	const char *real_uid;
	CamelFolder *real_folder;

	if (!get_real_message (folder, uid, &real_folder, &real_uid, ex))
		return;

	camel_folder_set_message_flags(real_folder, real_uid, flags, set, ex);
}

static gboolean
vee_get_message_user_flag(CamelFolder *folder, const char *uid,
			  const char *name, CamelException *ex)
{
	const char *real_uid;
	CamelFolder *real_folder;

	if (!get_real_message (folder, uid, &real_folder, &real_uid, ex))
		return FALSE;

	return camel_folder_get_message_user_flag(real_folder, real_uid, name, ex);
}

static void
vee_set_message_user_flag(CamelFolder *folder, const char *uid,
			  const char *name, gboolean value,
			  CamelException *ex)
{
	const char *real_uid;
	CamelFolder *real_folder;

	if (!get_real_message (folder, uid, &real_folder, &real_uid, ex))
		return;

	return camel_folder_set_message_user_flag(real_folder, real_uid, name, value, ex);
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

	{
		int i;

		for (i=0;i<vf->messages->len;i++) {
			CamelVeeMessageInfo *mi = g_ptr_array_index(vf->messages, i);
			g_free(mi->info.subject);
			g_free(mi->info.to);
			g_free(mi->info.from);
			g_free(mi->info.uid);
			camel_flag_list_free(&mi->info.user_flags);
			g_free(mi);
		}
	}

	printf("building folder expression: %s\n", vf->expression);

	messages = g_ptr_array_new();
	messages_uid = g_hash_table_new(g_str_hash, g_str_equal);

	node = p->folders;
	while (node) {
		GList *matches, *match;
		CamelFolder *f = node->data;
		CamelVeeMessageInfo *mi;
		const CamelMessageInfo *info;
		CamelFlag *flag;

		printf("searching folder: (%s)%s\n",
		       gtk_type_name(((GtkObject *)f)->klass->type),
		       camel_folder_get_full_name(f));

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

	printf("search complete\n");

	g_ptr_array_free(vf->messages, TRUE);
	vf->messages = messages;
	g_hash_table_destroy(vf->messages_uid);
	vf->messages_uid = messages_uid;
}


/* build query contents for a single folder */
static void
vee_folder_build_folder(CamelVeeFolder *vf, CamelFolder *source, CamelException *ex)
{
	GList *matches, *match;
	CamelFolder *f = source;
	CamelVeeMessageInfo *mi;
	const CamelMessageInfo *info;
	CamelFlag *flag;

	GPtrArray *messages;
	GHashTable *messages_uid;

	{
		int i;

		for (i=0;i<vf->messages->len;i++) {
			CamelVeeMessageInfo *mi = g_ptr_array_index(vf->messages, i);
			if (mi->folder == source) {
				g_hash_table_remove(vf->messages_uid, mi->info.uid);
				g_ptr_array_remove_index_fast(vf->messages, i);

				g_free(mi->info.subject);
				g_free(mi->info.to);
				g_free(mi->info.from);
				g_free(mi->info.uid);
				camel_flag_list_free(&mi->info.user_flags);
				g_free(mi);
				i--;
			}
		}
	}

	messages = vf->messages;
	messages_uid = vf->messages_uid;
	
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
}


/*

  (match-folder "folder1" "folder2")

 */
