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

#include "camel-maildir-summary.h"
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

#define CAMEL_MAILDIR_SUMMARY_VERSION (0x2000)

static CamelMessageInfo *message_info_new(CamelFolderSummary *, struct _header_raw *);

static int maildir_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex);
static int maildir_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex);
/*static int maildir_summary_add(CamelLocalSummary *cls, CamelMimeMessage *msg, CamelMessageInfo *info, CamelFolderChangeInfo *, CamelException *ex);*/

static char *maildir_summary_next_uid_string(CamelFolderSummary *s);

static void camel_maildir_summary_class_init	(CamelMaildirSummaryClass *class);
static void camel_maildir_summary_init	(CamelMaildirSummary *gspaper);
static void camel_maildir_summary_finalise	(CamelObject *obj);

#define _PRIVATE(x) (((CamelMaildirSummary *)(x))->priv)

struct _CamelMaildirSummaryPrivate {
	char *current_file;
	char *hostname;
};

static CamelLocalSummaryClass *parent_class;

CamelType
camel_maildir_summary_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_local_summary_get_type (), "CamelMaildirSummary",
					   sizeof(CamelMaildirSummary),
					   sizeof(CamelMaildirSummaryClass),
					   (CamelObjectClassInitFunc)camel_maildir_summary_class_init,
					   NULL,
					   (CamelObjectInitFunc)camel_maildir_summary_init,
					   (CamelObjectFinalizeFunc)camel_maildir_summary_finalise);
	}
	
	return type;
}

static void
camel_maildir_summary_class_init (CamelMaildirSummaryClass *class)
{
	CamelFolderSummaryClass *sklass = (CamelFolderSummaryClass *) class;
	CamelLocalSummaryClass *lklass = (CamelLocalSummaryClass *)class;

	parent_class = (CamelLocalSummaryClass *)camel_type_get_global_classfuncs(camel_local_summary_get_type ());

	/* override methods */
	sklass->message_info_new = message_info_new;
	sklass->next_uid_string = maildir_summary_next_uid_string;

	lklass->check = maildir_summary_check;
	lklass->sync = maildir_summary_sync;
	/*lklass->add = maildir_summary_add;*/
}

static void
camel_maildir_summary_init (CamelMaildirSummary *o)
{
	struct _CamelFolderSummary *s = (CamelFolderSummary *) o;
	char hostname[256];

	o->priv = g_malloc0(sizeof(*o->priv));
	/* set unique file version */
	s->version += CAMEL_MAILDIR_SUMMARY_VERSION;

	if (gethostname(hostname, 256) == 0) {
		o->priv->hostname = g_strdup(hostname);
	} else {
		o->priv->hostname = g_strdup("localhost");
	}
}

static void
camel_maildir_summary_finalise(CamelObject *obj)
{
	CamelMaildirSummary *o = (CamelMaildirSummary *)obj;

	g_free(o->priv);
}

/**
 * camel_maildir_summary_new:
 *
 * Create a new CamelMaildirSummary object.
 * 
 * Return value: A new #CamelMaildirSummary object.
 **/
CamelMaildirSummary	*camel_maildir_summary_new	(const char *filename, const char *maildirdir, ibex *index)
{
	CamelMaildirSummary *o = (CamelMaildirSummary *)camel_object_new(camel_maildir_summary_get_type ());

	camel_local_summary_construct((CamelLocalSummary *)o, filename, maildirdir, index);
	return o;
}

static CamelMessageInfo *message_info_new(CamelFolderSummary * s, struct _header_raw *h)
{
	CamelMessageInfo *mi;
	CamelMaildirSummary *mds = (CamelMaildirSummary *)s;
	CamelMaildirMessageInfo *mdi;

	mi = ((CamelFolderSummaryClass *) parent_class)->message_info_new(s, h);
	/* assign the uid and new filename */
	if (mi) {
		mdi = (CamelMaildirMessageInfo *)mi;

		mi->uid = camel_folder_summary_next_uid_string(s);

		/* should store some status info in the filename, but we wont (yet) (fixme) */
		if (mds->priv->current_file) {
			mdi->filename = g_strdup(mds->priv->current_file);
		} else {
			mdi->filename = g_strdup_printf("%s:2,", mi->uid);
		}
	}

	return mi;
}

static char *maildir_summary_next_uid_string(CamelFolderSummary *s)
{
	CamelMaildirSummary *mds = (CamelMaildirSummary *)s;
	/*CamelLocalSummary *cls = (CamelLocalSummary *)s;*/

	/* current_file is more a current_filename, so map the filename to a uid */
	if (mds->priv->current_file) {
		char *cln;

		cln = strchr(mds->priv->current_file, ':');
		if (cln)
			return g_strndup(mds->priv->current_file, cln-mds->priv->current_file);
		else
			return g_strdup(mds->priv->current_file);
	} else {
		/* we use time.pid_count.hostname */
		return g_strdup_printf("%ld.%d_%u.%s", time(0), getpid(), camel_folder_summary_next_uid(s), mds->priv->hostname);
	}
}

static int camel_maildir_summary_add(CamelLocalSummary *cls, const char *name, int forceindex)
{
	CamelMaildirSummary *maildirs = (CamelMaildirSummary *)cls;
	char *filename = g_strdup_printf("%s/cur/%s", cls->folder_path, name);
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
		camel_folder_summary_set_index((CamelFolderSummary *)maildirs, cls->index);
	} else {
		camel_folder_summary_set_index((CamelFolderSummary *)maildirs, NULL);
	}
	maildirs->priv->current_file = (char *)name;
	camel_folder_summary_add_from_parser((CamelFolderSummary *)maildirs, mp);
	camel_object_unref((CamelObject *)mp);
	maildirs->priv->current_file = NULL;
	camel_folder_summary_set_index((CamelFolderSummary *)maildirs, NULL);
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
maildir_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changes, CamelException *ex)
{
	DIR *dir;
	struct dirent *d;
	char *p;
	CamelMessageInfo *info;
	CamelMaildirMessageInfo *mdi;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
	GHashTable *left;
	int i, count;
	int forceindex;
	char *new, *cur;
	char *uid;

	new = g_strdup_printf("%s/new", cls->folder_path);
	cur = g_strdup_printf("%s/cur", cls->folder_path);

	/* FIXME: Handle changeinfo */

	d(printf("checking summary ...\n"));

	/* scan the directory, check for mail files not in the index, or index entries that
	   no longer exist */
	dir = opendir(cur);
	if (dir == NULL) {
		camel_exception_setv(ex, 1, "Cannot open maildir directory path: %s: %s", cls->folder_path, strerror(errno));
		g_free(cur);
		g_free(new);
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
		if (p[0] == '.')
			continue;

		/* map the filename -> uid */
		uid = strchr(d->d_name, ':');
		if (uid)
			uid = g_strndup(d->d_name, uid-d->d_name);
		else
			uid = g_strdup(d->d_name);

		info = camel_folder_summary_uid((CamelFolderSummary *)cls, uid);
		if (info == NULL || (cls->index && (!ibex_contains_name(cls->index, uid)))) {
			/* need to add this file to the summary */
			if (info != NULL) {
				g_hash_table_remove(left, info->uid);
				camel_folder_summary_remove((CamelFolderSummary *)cls, info);
			}
			camel_maildir_summary_add(cls, d->d_name, forceindex);
		} else {
			if (info) {
				mdi = (CamelMaildirMessageInfo *)info;
				/* TODO: only store the extension in the mdi->filename struct, not the whole lot */
				if (mdi->filename == NULL || strcmp(mdi->filename, d->d_name) != 0) {
					g_free(mdi->filename);
					mdi->filename = g_strdup(d->d_name);
				}
			}
			g_hash_table_remove(left, info->uid);
		}
		g_free(uid);
	}
	closedir(dir);
	g_hash_table_foreach(left, (GHFunc)remove_summary, cls);
	g_hash_table_destroy(left);

	/* now, scan new for new messages, and copy them to cur, and so forth */
	dir = opendir(new);
	if (dir != NULL) {
		while ( (d = readdir(dir)) ) {
			char *name, *newname, *destname, *destfilename;
			char *src, *dest;

			name = d->d_name;
			if (name[0] == '.')
				continue;

			/* already in summary?  shouldn't happen, but just incase ... */
			if (camel_folder_summary_uid((CamelFolderSummary *)cls, name))
				newname = destname = camel_folder_summary_next_uid_string(s);
			else {
				newname = NULL;
				destname = name;
			}

			/* copy this to the destination folder, use 'standard' semantics for maildir info field */
			src = g_strdup_printf("%s/%s", new, name);
			destfilename = g_strdup_printf("%s:2,", destname);
			dest = g_strdup_printf("%s/%s", cur, destfilename);
			if (rename(src, dest) == 0) {
				camel_maildir_summary_add(cls, destfilename, forceindex);
				if (changes)
					camel_folder_change_info_add_uid(changes, destname);
			} else {
				/* else?  we should probably care about failures, but wont */
				g_warning("Failed to move new maildir message %s to cur %s", src, dest);
			}

			/* c strings are painful to work with ... */
			g_free(destfilename);
			g_free(newname);
			g_free(src);
			g_free(dest);
		}
	}

	g_free(new);
	g_free(cur);

	/* FIXME: move this up a class? */

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
maildir_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changes, CamelException *ex)
{
	int count, i;
	CamelMessageInfo *info;
	CamelMaildirMessageInfo *mdi;
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
			mdi = (CamelMaildirMessageInfo *)info;
			name = g_strdup_printf("%s/cur/%s", cls->folder_path, mdi->filename);
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

