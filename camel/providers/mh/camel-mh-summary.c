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

#include <gtk/gtk.h>

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

#define d(x)

#define CAMEL_MH_SUMMARY_VERSION (0x2000)

static CamelMessageInfo *message_info_new(CamelFolderSummary *, struct _header_raw *);

static void camel_mh_summary_class_init	(CamelMhSummaryClass *class);
static void camel_mh_summary_init	(CamelMhSummary *gspaper);
static void camel_mh_summary_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((CamelMhSummary *)(x))->priv)

struct _CamelMhSummaryPrivate {
	char *current_uid;
};

static CamelFolderSummaryClass *parent_class;

guint
camel_mh_summary_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelMhSummary",
			sizeof(CamelMhSummary),
			sizeof(CamelMhSummaryClass),
			(GtkClassInitFunc)camel_mh_summary_class_init,
			(GtkObjectInitFunc)camel_mh_summary_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(camel_folder_summary_get_type (), &type_info);
	}
	
	return type;
}

static void
camel_mh_summary_class_init (CamelMhSummaryClass *class)
{
	GtkObjectClass *object_class;
	CamelFolderSummaryClass *sklass = (CamelFolderSummaryClass *) class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(camel_folder_summary_get_type ());

	object_class->finalize = camel_mh_summary_finalise;

	/* override methods */
	sklass->message_info_new = message_info_new;

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
camel_mh_summary_finalise(GtkObject *obj)
{
	CamelMhSummary *o = (CamelMhSummary *)obj;

	g_free(o->mh_path);

        ((GtkObjectClass *)(parent_class))->finalize(obj);
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
	CamelMhSummary *o = (CamelMhSummary *)gtk_type_new(camel_mh_summary_get_type ());

	camel_folder_summary_set_build_content((CamelFolderSummary *)o, TRUE);
	camel_folder_summary_set_filename((CamelFolderSummary *)o, filename);
	o->mh_path = g_strdup(mhdir);
	o->index = index;
	return o;
}

static CamelMessageInfo *message_info_new(CamelFolderSummary * s, struct _header_raw *h)
{
	CamelMessageInfo *mi;
	CamelMhSummary *mhs = (CamelMhSummary *)s;

	mi = ((CamelFolderSummaryClass *) parent_class)->message_info_new(s, h);
	if (mi) {
		/* it only ever indexes 1 message at a time */
		mi->uid = g_strdup(mhs->priv->current_uid);
	}

	return mi;
}

int		camel_mh_summary_load(CamelMhSummary * mhs, int forceindex)
{
	CamelFolderSummary *s = CAMEL_FOLDER_SUMMARY(mhs);

	d(printf("loading summary ...\n"));

	if (forceindex || camel_folder_summary_load(s) == -1) {
		camel_folder_summary_clear(s);
	}
	return camel_mh_summary_check(mhs, forceindex);
}

int		camel_mh_summary_add(CamelMhSummary * mhs, const char *name, int forceindex)
{
	char *filename = g_strdup_printf("%s/%s", mhs->mh_path, name);
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
	if (forceindex || !ibex_contains_name(mhs->index, (char *)name)) {
		d(printf("forcing indexing of message content\n"));
		camel_folder_summary_set_index((CamelFolderSummary *)mhs, mhs->index);
	} else {
		camel_folder_summary_set_index((CamelFolderSummary *)mhs, NULL);
	}
	mhs->priv->current_uid = (char *)name;
	camel_folder_summary_add_from_parser((CamelFolderSummary *)mhs, mp);
	gtk_object_unref((GtkObject *)mp);
	mhs->priv->current_uid = NULL;
	camel_folder_summary_set_index((CamelFolderSummary *)mhs, NULL);
	g_free(filename);
	return 0;
}

static void
remove_summary(char *key, CamelMessageInfo *info, CamelMhSummary *mhs)
{
	d(printf("removing message %s from summary\n", key));
	ibex_unindex(mhs->index, info->uid);
	camel_folder_summary_remove((CamelFolderSummary *)mhs, info);
}

int		camel_mh_summary_check(CamelMhSummary * mhs, int forceindex)
{
	DIR *dir;
	struct dirent *d;
	char *p, c;
	CamelMessageInfo *info;
	GHashTable *left;
	int i, count;

	d(printf("checking summary ...\n"));

	/* scan the directory, check for mail files not in the index, or index entries that
	   no longer exist */
	dir = opendir(mhs->mh_path);
	if (dir == NULL)
		return -1;

	/* keeps track of all uid's that have not been processed */
	left = g_hash_table_new(g_str_hash, g_str_equal);
	count = camel_folder_summary_count((CamelFolderSummary *)mhs);
	for (i=0;i<count;i++) {
		info = camel_folder_summary_index((CamelFolderSummary *)mhs, i);
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
			info = camel_folder_summary_uid((CamelFolderSummary *)mhs, d->d_name);
			if (info == NULL || (!ibex_contains_name(mhs->index, d->d_name))) {
				/* need to add this file to the summary */
				if (info != NULL) {
					g_hash_table_remove(left, info->uid);
					camel_folder_summary_remove((CamelFolderSummary *)mhs, info);
				}
				camel_mh_summary_add(mhs, d->d_name, forceindex);
			} else {
				g_hash_table_remove(left, info->uid);
			}
		}
	}
	closedir(dir);
	g_hash_table_foreach(left, (GHFunc)remove_summary, mhs);
	g_hash_table_destroy(left);

	return 0;
}

/* sync the summary with the ondisk files.
   It doesnt store the state in the file, the summary only, == MUCH faster */
int		camel_mh_summary_sync(CamelMhSummary * mhs, int expunge, CamelException *ex)
{
	int count, i;
	CamelMessageInfo *info;
	char *name;

	printf("summary_sync(expunge=%s)\n", expunge?"true":"false");

	if (!expunge)
		return 0;

	count = camel_folder_summary_count((CamelFolderSummary *)mhs);
	for (i=count-1;i>=0;i--) {
		info = camel_folder_summary_index((CamelFolderSummary *)mhs, i);
		if (info && info->flags & CAMEL_MESSAGE_DELETED) {
			name = g_strdup_printf("%s/%s", mhs->mh_path, info->uid);
			(printf("deleting %s\n", name));
			if (unlink(name) == 0 || errno==ENOENT) {
				camel_folder_summary_remove((CamelFolderSummary *)mhs, info);
			}
		}
	}
	return 0;
}

