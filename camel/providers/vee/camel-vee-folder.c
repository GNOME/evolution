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

static void vee_append_message (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex);

static const CamelMessageInfo *vee_summary_get_by_uid(CamelFolder *f, const char *uid);
static GList *vee_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex);


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
	folder_class->append_message = vee_append_message;

	folder_class->summary_get_by_uid = vee_summary_get_by_uid;

	folder_class->get_message_count = vee_get_message_count;
	folder_class->search_by_expression = vee_search_by_expression;

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

void
camel_vee_folder_add_folder(CamelVeeFolder *vf, CamelFolder *sub)
{
	struct _CamelVeeFolderPrivate *p = _PRIVATE(vf);
	CamelException *ex;

	gtk_object_ref((GtkObject *)sub);
	p->folders = g_list_append(p->folders, sub);

	gtk_signal_connect((GtkObject *)sub, "folder_changed", folder_changed, vf);

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

static void vee_append_message (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;

	if (message->folder && message->folder->permanent_flags & CAMEL_MESSAGE_USER) {
		/* set the flag on the message ... */
		camel_mime_message_set_user_flag(message, vf->vname, TRUE);
	} else {
		/* FIXME: error code */
		camel_exception_setv(ex, 1, "Cannot append this message to virtual folder");
	}
}

static gint vee_get_message_count (CamelFolder *folder, CamelException *ex)
{
	CamelVeeFolder *vf = (CamelVeeFolder *)folder;

	return vf->messages->len;
}

/* track flag changes in the summary */
static void
message_changed(CamelMimeMessage *m, int type, CamelVeeFolder *mf)
{
	CamelMessageInfo *info;
	CamelFlag *flag;
	char *uid;

	printf("VMessage changed: %s: %d\n", m->message_uid, type);
	switch (type) {
	case MESSAGE_FLAGS_CHANGED:
		uid = g_strdup_printf("%p:%s", m->folder, m->message_uid);
		info = (CamelMessageInfo *)vee_summary_get_by_uid((CamelFolder *)mf, uid);
		if (info) {
			info->flags = m->flags;
			camel_flag_list_free(&info->user_flags);
			flag = m->user_flags;
			while (flag) {
				camel_flag_set(&info->user_flags, flag->name, TRUE);
				flag = flag->next;
			}
		} else {
			g_warning("Message changed event on message not in summary: %s", uid);
		}
		g_free(uid);
		break;
	default:
		printf("Unhandled message change event: %d\n", type);
		break;
	}
}

static CamelMimeMessage *vee_get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelVeeMessageInfo *mi;
	CamelMimeMessage *mm;

	mi = (CamelVeeMessageInfo *)vee_summary_get_by_uid(folder, uid);
	if (mi == NULL) {
		camel_exception_setv(ex, 1, "Failed");
		return NULL;
	}

	mm = camel_folder_get_message_by_uid(mi->folder, strchr(mi->info.uid, ':')+1, ex);
	if (mm) {
		gtk_signal_connect((GtkObject *)mm, "message_changed", message_changed, folder);
	}
	return mm;
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
