/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "camel-mh-summary.h"
#include <camel/camel-mime-message.h>

#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <dirent.h>

#include <ctype.h>

#define d(x) (printf("%s(%d): ", __FILE__, __LINE__),(x))

#define CAMEL_MH_SUMMARY_VERSION (0x2000)

static CamelMessageInfo *message_info_new(CamelFolderSummary *, struct _header_raw *);

static int mh_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex);
static int mh_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex);
/*static int mh_summary_add(CamelLocalSummary *cls, CamelMimeMessage *msg, CamelMessageInfo *info, CamelFolderChangeInfo *, CamelException *ex);*/

static char *mh_summary_next_uid_string(CamelFolderSummary *s);

static void camel_mh_summary_class_init	(CamelMhSummaryClass *class);
static void camel_mh_summary_init	(CamelMhSummary *gspaper);
static void camel_mh_summary_finalise	(CamelObject *obj);

#define _PRIVATE(x) (((CamelMhSummary *)(x))->priv)

struct _CamelMhSummaryPrivate {
	char *current_uid;
};

static CamelLocalSummaryClass *parent_class;

CamelType
camel_mh_summary_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_local_summary_get_type (), "CamelMhSummary",
					   sizeof(CamelMhSummary),
					   sizeof(CamelMhSummaryClass),
					   (CamelObjectClassInitFunc)camel_mh_summary_class_init,
					   NULL,
					   (CamelObjectInitFunc)camel_mh_summary_init,
					   (CamelObjectFinalizeFunc)camel_mh_summary_finalise);
	}
	
	return type;
}

static void
camel_mh_summary_class_init (CamelMhSummaryClass *class)
{
	CamelFolderSummaryClass *sklass = (CamelFolderSummaryClass *) class;
	CamelLocalSummaryClass *lklass = (CamelLocalSummaryClass *)class;

	parent_class = (CamelLocalSummaryClass *)camel_type_get_global_classfuncs(camel_local_summary_get_type ());

	/* override methods */
	sklass->message_info_new = message_info_new;
	sklass->next_uid_string = mh_summary_next_uid_string;

	lklass->check = mh_summary_check;
	lklass->sync = mh_summary_sync;
	/*lklass->add = mh_summary_add;*/
}

static void
camel_mh_summary_init (CamelMhSummary *o)
{
	struct _CamelFolderSummary *s = (CamelFolderSummary *) o;

	o->priv = g_malloc0(sizeof(*o->priv));
	/* set unique file version */
	s->version += CAMEL_MH_SUMMARY_VERSION;
}

static void
camel_mh_summary_finalise(CamelObject *obj)
{
	CamelMhSummary *o = (CamelMhSummary *)obj;

	g_free(o->priv);
}

/**
 * camel_mh_summary_new:
 *
 * Create a new CamelMhSummary object.
 * 
 * Return value: A new #CamelMhSummary object.
 **/
CamelMhSummary	*camel_mh_summary_new	(const char *filename, const char *mhdir, ibex *index)
{
	CamelMhSummary *o = (CamelMhSummary *)camel_object_new(camel_mh_summary_get_type ());

	camel_local_summary_construct((CamelLocalSummary *)o, filename, mhdir, index);
	return o;
}

static CamelMessageInfo *message_info_new(CamelFolderSummary * s, struct _header_raw *h)
{
	CamelMessageInfo *mi;
	/*CamelMhSummary *mhs = (CamelMhSummary *)s;*/

	mi = ((CamelFolderSummaryClass *) parent_class)->message_info_new(s, h);
	/* hmm, this isn't quite right */
	if (mi) {
		/*mi->uid = mh_summary_next_uid_string(s);*/
	}

	return mi;
}

static char *mh_summary_next_uid_string(CamelFolderSummary *s)
{
	CamelMhSummary *mhs = (CamelMhSummary *)s;
	CamelLocalSummary *cls = (CamelLocalSummary *)s;
	int fd = -1;
	guint32 uid;
	char *name = NULL;

	/* if we are working to add an existing file, then use current_uid */
	if (mhs->priv->current_uid)
		return g_strdup(mhs->priv->current_uid);

	/* else scan for one - and create it too, to make sure */
	do {
		g_free(name);
		close(fd);
		uid = camel_folder_summary_next_uid(s);
		name = g_strdup_printf("%s/%u", cls->folder_path, uid);
		/* O_EXCL isn't guaranteed, sigh.  Oh well, bad luck, mh has problems anyway */
		fd = open(name, O_WRONLY|O_CREAT|O_EXCL, 0600);
	} while (fd == -1 && errno == EEXIST);

	close(fd);

	return g_strdup_printf("%u", uid);
}

static int camel_mh_summary_add(CamelLocalSummary *cls, const char *name, int forceindex)
{
	CamelMhSummary *mhs = (CamelMhSummary *)cls;
	char *filename = g_strdup_printf("%s/%s", cls->folder_path, name);
	int fd;
	CamelMimeParser *mp;

	d(printf("summarising: %s\n", name));

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		g_warning("Cannot summarise/index: %s: %s", filename, strerror(errno));
		g_free(filename);
		return -1;
	}
	mp = camel_mime_parser_new();
	camel_mime_parser_scan_from(mp, FALSE);
	camel_mime_parser_init_with_fd(mp, fd);
	if (cls->index && (forceindex || !ibex_contains_name(cls->index, (char *)name))) {
		d(printf("forcing indexing of message content\n"));
		camel_folder_summary_set_index((CamelFolderSummary *)mhs, cls->index);
	} else {
		camel_folder_summary_set_index((CamelFolderSummary *)mhs, NULL);
	}
	mhs->priv->current_uid = (char *)name;
	camel_folder_summary_add_from_parser((CamelFolderSummary *)mhs, mp);
	camel_object_unref((CamelObject *)mp);
	mhs->priv->current_uid = NULL;
	camel_folder_summary_set_index((CamelFolderSummary *)mhs, NULL);
	g_free(filename);
	return 0;
}

static void
remove_summary(char *key, CamelMessageInfo *info, CamelLocalSummary *cls)
{
	d(printf("removing message %s from summary\n", key));
	if (cls->index)
		ibex_unindex(cls->index, info->uid);
	camel_folder_summary_remove((CamelFolderSummary *)cls, info);
}

static int
mh_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	DIR *dir;
	struct dirent *d;
	char *p, c;
	CamelMessageInfo *info;
	GHashTable *left;
	int i, count;
	int forceindex;

	/* FIXME: Handle changeinfo */

	d(printf("checking summary ...\n"));

	/* scan the directory, check for mail files not in the index, or index entries that
	   no longer exist */
	dir = opendir(cls->folder_path);
	if (dir == NULL) {
		camel_exception_setv(ex, 1, "Cannot open MH directory path: %s: %s", cls->folder_path, strerror(errno));
		return -1;
	}

	/* keeps track of all uid's that have not been processed */
	left = g_hash_table_new(g_str_hash, g_str_equal);
	count = camel_folder_summary_count((CamelFolderSummary *)cls);
	forceindex = count == 0;
	for (i=0;i<count;i++) {
		info = camel_folder_summary_index((CamelFolderSummary *)cls, i);
		if (info) {
			g_hash_table_insert(left, info->uid, info);
		}
	}

	while ( (d = readdir(dir)) ) {
		/* FIXME: also run stat to check for regular file */
		p = d->d_name;
		while ( (c = *p++) ) {
			if (!isdigit(c))
				break;
		}
		if (c==0) {
			info = camel_folder_summary_uid((CamelFolderSummary *)cls, d->d_name);
			if (info == NULL || (cls->index && (!ibex_contains_name(cls->index, d->d_name)))) {
				/* need to add this file to the summary */
				if (info != NULL) {
					g_hash_table_remove(left, info->uid);
					camel_folder_summary_remove((CamelFolderSummary *)cls, info);
				}
				camel_mh_summary_add(cls, d->d_name, forceindex);
			} else {
				g_hash_table_remove(left, info->uid);
			}
		}
	}
	closedir(dir);
	g_hash_table_foreach(left, (GHFunc)remove_summary, cls);
	g_hash_table_destroy(left);

	/* FIXME: move this up a class */

	/* force a save of the index, just to make sure */
	/* note this could be expensive so possibly shouldn't be here
	   as such */
	if (cls->index) {
		ibex_save(cls->index);
	}

	return 0;
}



/* sync the summary with the ondisk files.
   It doesnt store the state in the file, the summary only, == MUCH faster */
static int
mh_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changes, CamelException *ex)
{
	int count, i;
	CamelMessageInfo *info;
	char *name;

	d(printf("summary_sync(expunge=%s)\n", expunge?"true":"false"));

	if (cls->index) {
		ibex_save(cls->index);
	}
	if (!expunge)
		return 0;

	count = camel_folder_summary_count((CamelFolderSummary *)cls);
	for (i=count-1;i>=0;i--) {
		info = camel_folder_summary_index((CamelFolderSummary *)cls, i);
		if (info && info->flags & CAMEL_MESSAGE_DELETED) {
			name = g_strdup_printf("%s/%s", cls->folder_path, info->uid);
			d(printf("deleting %s\n", name));
			if (unlink(name) == 0 || errno==ENOENT) {

				/* FIXME: put this in folder_summary::remove()? */
				if (cls->index)
					ibex_unindex(cls->index, info->uid);

				camel_folder_change_info_remove_uid(changes, info->uid);
				camel_folder_summary_remove((CamelFolderSummary *)cls, info);
			}
		}
	}
	return 0;
}

