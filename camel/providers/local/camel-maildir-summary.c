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

#include "camel-private.h"
#include "e-util/e-memory.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

#define CAMEL_MAILDIR_SUMMARY_VERSION (0x2000)

static CamelMessageInfo *message_info_load(CamelFolderSummary *s, FILE *in);
static CamelMessageInfo *message_info_new(CamelFolderSummary *, struct _header_raw *);
static void message_info_free(CamelFolderSummary *, CamelMessageInfo *mi);

static int maildir_summary_load(CamelLocalSummary *cls, int forceindex, CamelException *ex);
static int maildir_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex);
static int maildir_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex);
static CamelMessageInfo *maildir_summary_add(CamelLocalSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *, CamelException *ex);

static char *maildir_summary_next_uid_string(CamelFolderSummary *s);
static int maildir_summary_decode_x_evolution(CamelLocalSummary *cls, const char *xev, CamelMessageInfo *mi);
static char *maildir_summary_encode_x_evolution(CamelLocalSummary *cls, const CamelMessageInfo *mi);

static void camel_maildir_summary_class_init	(CamelMaildirSummaryClass *class);
static void camel_maildir_summary_init	(CamelMaildirSummary *gspaper);
static void camel_maildir_summary_finalise	(CamelObject *obj);

#define _PRIVATE(x) (((CamelMaildirSummary *)(x))->priv)

struct _CamelMaildirSummaryPrivate {
	char *current_file;
	char *hostname;

	GHashTable *load_map;
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
	sklass->message_info_load = message_info_load;
	sklass->message_info_new = message_info_new;
	sklass->message_info_free = message_info_free;
	sklass->next_uid_string = maildir_summary_next_uid_string;

	lklass->load = maildir_summary_load;
	lklass->check = maildir_summary_check;
	lklass->sync = maildir_summary_sync;
	lklass->add = maildir_summary_add;
	lklass->encode_x_evolution = maildir_summary_encode_x_evolution;
	lklass->decode_x_evolution = maildir_summary_decode_x_evolution;
}

static void
camel_maildir_summary_init (CamelMaildirSummary *o)
{
	struct _CamelFolderSummary *s = (CamelFolderSummary *) o;
	char hostname[256];

	o->priv = g_malloc0(sizeof(*o->priv));
	/* set unique file version */
	s->version += CAMEL_MAILDIR_SUMMARY_VERSION;

	s->message_info_size = sizeof(CamelMaildirMessageInfo);
	s->content_info_size = sizeof(CamelMaildirMessageContentInfo);

#ifdef DOESTRV
	s->message_info_strings = CAMEL_MAILDIR_INFO_LAST;
#endif

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

	g_free(o->priv->hostname);
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

/* the 'standard' maildir flags.  should be defined in sorted order. */
static struct {
	char flag;
	guint32 flagbit;
} flagbits[] = {
	{ 'F', CAMEL_MESSAGE_FLAGGED },
	{ 'R', CAMEL_MESSAGE_ANSWERED },
	{ 'S', CAMEL_MESSAGE_SEEN },
	{ 'T', CAMEL_MESSAGE_DELETED },
};

/* convert the uid + flags into a unique:info maildir format */
char *camel_maildir_summary_info_to_name(const CamelMessageInfo *info)
{
	char *p, *buf;
	int i;
	const char *uid;

	uid = camel_message_info_uid(info);
	buf = alloca(strlen(uid) + strlen(":2,") +  (sizeof(flagbits)/sizeof(flagbits[0])) + 1);
	p = buf + sprintf(buf, "%s:2,", uid);
	for (i=0;i<sizeof(flagbits)/sizeof(flagbits[0]);i++) {
		if (info->flags & flagbits[i].flagbit)
			*p++ = flagbits[i].flag;
	}
	*p = 0;

	return g_strdup(buf);
}

/* returns 0 if the info matches (or there was none), otherwise we changed it */
int camel_maildir_summary_name_to_info(CamelMessageInfo *info, const char *name)
{
	char *p, c;
	guint32 set = 0;	/* what we set */
	/*guint32 all = 0;*/	/* all flags */
	int i;

	p = strstr(name, ":2,");
	if (p) {
		p+=3;
		while ((c = *p++)) {
			/* we could assume that the flags are in order, but its just as easy not to require */
			for (i=0;i<sizeof(flagbits)/sizeof(flagbits[0]);i++) {
				if (flagbits[i].flag == c && (info->flags & flagbits[i].flagbit) == 0) {
					set |= flagbits[i].flagbit;
				}
				/*all |= flagbits[i].flagbit;*/
			}
		}

		/* changed? */
		/*if ((info->flags & all) != set) {*/
		if ((info->flags & set) != set) {
			/* ok, they did change, only add the new flags ('merge flags'?) */
			/*info->flags &= all;  if we wanted to set only the new flags, which we probably dont */
			info->flags |= set;
			return 1;
		}
	}

	return 0;
}

/* for maildir, x-evolution isn't used, so dont try and get anything out of it */
static int maildir_summary_decode_x_evolution(CamelLocalSummary *cls, const char *xev, CamelMessageInfo *mi)
{
	return -1;
}

static char *maildir_summary_encode_x_evolution(CamelLocalSummary *cls, const CamelMessageInfo *mi)
{
	return NULL;
}

/* FIXME:
   both 'new' and 'add' will try and set the filename, this is not ideal ...
*/
static CamelMessageInfo *maildir_summary_add(CamelLocalSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *changes, CamelException *ex)
{
	CamelMessageInfo *mi;

	mi = ((CamelLocalSummaryClass *) parent_class)->add(cls, msg, info, changes, ex);
	if (mi) {
		if (info) {
			camel_maildir_info_set_filename(mi, camel_maildir_summary_info_to_name(mi));
			d(printf("Setting filename to %s\n", camel_maildir_info_filename(mi)));
		}
	}

	return mi;
}

static CamelMessageInfo *message_info_new(CamelFolderSummary * s, struct _header_raw *h)
{
	CamelMessageInfo *mi;
	CamelMaildirSummary *mds = (CamelMaildirSummary *)s;
	CamelMaildirMessageInfo *mdi;
	const char *uid;

	mi = ((CamelFolderSummaryClass *) parent_class)->message_info_new(s, h);
	/* assign the uid and new filename */
	if (mi) {
		mdi = (CamelMaildirMessageInfo *)mi;

		uid = camel_message_info_uid(mi);
		if (uid==NULL || uid[0] == 0)
			camel_message_info_set_uid(mi, camel_folder_summary_next_uid_string(s));

		/* with maildir we know the real received date, from the filename */
		mi->date_received = strtoul(camel_message_info_uid(mi), NULL, 10);

		if (mds->priv->current_file) {
#if 0
			char *p1, *p2, *p3;
			unsigned long uid;
#endif
			/* if setting from a file, grab the flags from it */
			camel_maildir_info_set_filename(mi, g_strdup(mds->priv->current_file));
			camel_maildir_summary_name_to_info(mi, mds->priv->current_file);

#if 0
			/* Actually, I dont think all this effort is worth it at all ... */

			/* also, see if we can extract the next-id from tne name, and safe-if-fy ourselves against collisions */
			/* we check for something.something_number.something */
			p1 = strchr(mdi->filename, '.');
			if (p1) {
				p2 = strchr(p1+1, '.');
				p3 = strchr(p1+1, '_');
				if (p2 && p3 && p3<p2) {
					uid = strtoul(p3+1, &p1, 10);
					if (p1 == p2 && uid>0)
						camel_folder_summary_set_uid(s, uid);
				}
			}
#endif
		} else {
			/* if creating a file, set its name from the flags we have */
			camel_maildir_info_set_filename(mdi, camel_maildir_summary_info_to_name(mi));
			d(printf("Setting filename to %s\n", camel_maildir_info_filename(mi)));
		}
	}

	return mi;
}


static void message_info_free(CamelFolderSummary *s, CamelMessageInfo *mi)
{
#ifndef DOESTRV
	CamelMaildirMessageInfo *mdi = (CamelMaildirMessageInfo *)mi;

	g_free(mdi->filename);
#endif
	((CamelFolderSummaryClass *) parent_class)->message_info_free(s, mi);
}


static char *maildir_summary_next_uid_string(CamelFolderSummary *s)
{
	CamelMaildirSummary *mds = (CamelMaildirSummary *)s;

	d(printf("next uid string called?\n"));

	/* if we have a current file, then use that to get the uid */
	if (mds->priv->current_file) {
		char *cln;

		cln = strchr(mds->priv->current_file, ':');
		if (cln)
			return g_strndup(mds->priv->current_file, cln-mds->priv->current_file);
		else
			return g_strdup(mds->priv->current_file);
	} else {
		/* the first would probably work, but just to be safe, check for collisions */
#if 0
		return g_strdup_printf("%ld.%d_%u.%s", time(0), getpid(), camel_folder_summary_next_uid(s), mds->priv->hostname);
#else
		CamelLocalSummary *cls = (CamelLocalSummary *)s;
		char *name = NULL, *uid = NULL;
		struct stat st;
		int retry = 0;
		guint32 nextuid = camel_folder_summary_next_uid(s);

		/* we use time.pid_count.hostname */
		do {
			if (retry > 0) {
				g_free(name);
				g_free(uid);
				sleep(2);
			}
			uid = g_strdup_printf("%ld.%d_%u.%s", time(0), getpid(), nextuid, mds->priv->hostname);
			name = g_strdup_printf("%s/tmp/%s", cls->folder_path, uid);
			retry++;
		} while (stat(name, &st) == 0 && retry<3);

		/* I dont know what we're supposed to do if it fails to find a unique name?? */

		g_free(name);
		return uid;
#endif
	}
}

static CamelMessageInfo *
message_info_load(CamelFolderSummary *s, FILE *in)
{
	CamelMessageInfo *mi;
	CamelMaildirSummary *mds = (CamelMaildirSummary *)s;

	mi = ((CamelFolderSummaryClass *) parent_class)->message_info_load(s, in);
	if (mi) {
		char *name;

		if (mds->priv->load_map
		    && (name = g_hash_table_lookup(mds->priv->load_map, camel_message_info_uid(mi)))) {
			d(printf("Setting filename of %s to %s\n", camel_message_info_uid(mi), name));
			camel_maildir_info_set_filename(mi, g_strdup(name));
			camel_maildir_summary_name_to_info(mi, name);
		}
	}

	return mi;
}

static int maildir_summary_load(CamelLocalSummary *cls, int forceindex, CamelException *ex)
{
	char *cur;
	DIR *dir;
	struct dirent *d;
	CamelMaildirSummary *mds = (CamelMaildirSummary *)cls;
	char *uid;
	EMemPool *pool;
	int ret;

	cur = g_strdup_printf("%s/cur", cls->folder_path);

	d(printf("pre-loading uid <> filename map\n"));

	dir = opendir(cur);
	if (dir == NULL) {
		camel_exception_setv(ex, 1, "Cannot open maildir directory path: %s: %s", cls->folder_path, strerror(errno));
		g_free(cur);
		return -1;
	}

	mds->priv->load_map = g_hash_table_new(g_str_hash, g_str_equal);
	pool = e_mempool_new(1024, 512, E_MEMPOOL_ALIGN_BYTE);

	while ( (d = readdir(dir)) ) {
		if (d->d_name[0] == '.')
			continue;
		
		/* map the filename -> uid */
		uid = strchr(d->d_name, ':');
		if (uid) {
			int len = uid-d->d_name;
			uid = e_mempool_alloc(pool, len+1);
			memcpy(uid, d->d_name, len);
			uid[len] = 0;
			g_hash_table_insert(mds->priv->load_map, uid, e_mempool_strdup(pool, d->d_name));
		} else {
			uid = e_mempool_strdup(pool, d->d_name);
			g_hash_table_insert(mds->priv->load_map, uid, uid);
		}
	}
	closedir(dir);
	g_free(cur);

	ret = ((CamelLocalSummaryClass *) parent_class)->load(cls, forceindex, ex);

	g_hash_table_destroy(mds->priv->load_map);
	mds->priv->load_map = NULL;
	e_mempool_destroy(pool);

	return ret;
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
		ibex_unindex(cls->index, (char *)camel_message_info_uid(info));
	camel_folder_summary_remove((CamelFolderSummary *)cls, info);
	camel_folder_summary_info_free((CamelFolderSummary *)cls, info);
}

static int
sort_receive_cmp(const void *ap, const void *bp)
{
	const CamelMessageInfo
		*a = *((CamelMessageInfo **)ap),
		*b = *((CamelMessageInfo **)bp);

	if (a->date_received < b->date_received)
		return -1;
	else if (a->date_received > b->date_received)
		return 1;

	return 0;
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
			g_hash_table_insert(left, (char *)camel_message_info_uid(info), info);
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
				CamelMessageInfo *old = g_hash_table_lookup(left, camel_message_info_uid(info));
				if (old) {
					g_hash_table_remove(left, uid);
					camel_folder_summary_info_free((CamelFolderSummary *)cls, old);
				}
				camel_folder_summary_remove((CamelFolderSummary *)cls, info);
				camel_folder_summary_info_free((CamelFolderSummary *)cls, info);
			}
			camel_maildir_summary_add(cls, d->d_name, forceindex);
		} else {
			const char *filename;
			CamelMessageInfo *old;

			old = g_hash_table_lookup(left, camel_message_info_uid(info));
			if (old) {
				camel_folder_summary_info_free((CamelFolderSummary *)cls, old);
				g_hash_table_remove(left, camel_message_info_uid(info));
			}

			mdi = (CamelMaildirMessageInfo *)info;
			filename = camel_maildir_info_filename(mdi);
			/* TODO: only store the extension in the mdi->filename struct, not the whole lot */
			if (filename == NULL || strcmp(filename, d->d_name) != 0) {
#ifdef DOESTRV
#warning "cannot modify the estrv after its been setup, for mt-safe code"
				d(printf("filename changed: %s to %s\n", filename, d->d_name));

				/* need to update the summary hash string reference since it might (will) change */
				CAMEL_SUMMARY_LOCK(s, summary_lock);
				g_hash_table_remove(s->messages_uid, uid);
				info->strings = e_strv_set_ref(info->strings, CAMEL_MAILDIR_INFO_FILENAME, d->d_name);
				/* we need to re-pack as well */
				info->strings = e_strv_pack(info->strings);
				g_hash_table_insert(s->messages_uid, (char *)camel_message_info_uid(info), info);
				CAMEL_SUMMARY_UNLOCK(s, summary_lock);
#else	
				g_free(mdi->filename);
				mdi->filename = g_strdup(d->d_name);
#endif	
			}
			camel_folder_summary_info_free((CamelFolderSummary *)cls, info);
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
			if ((info = camel_folder_summary_uid((CamelFolderSummary *)cls, name))) {
				camel_folder_summary_info_free((CamelFolderSummary *)cls, info);
				newname = destname = camel_folder_summary_next_uid_string(s);
			} else {
				newname = NULL;
				destname = name;
			}

			/* copy this to the destination folder, use 'standard' semantics for maildir info field */
			src = g_strdup_printf("%s/%s", new, name);
			destfilename = g_strdup_printf("%s:2,", destname);
			dest = g_strdup_printf("%s/%s", cur, destfilename);

			/* FIXME: This should probably use link/unlink */

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
	closedir(dir);

	g_free(new);
	g_free(cur);

	/* sort the summary based on receive time, since the directory order is not useful */
	CAMEL_SUMMARY_LOCK(s, summary_lock);
	qsort(s->messages->pdata, s->messages->len, sizeof(CamelMessageInfo *), sort_receive_cmp);
	CAMEL_SUMMARY_UNLOCK(s, summary_lock);

	/* FIXME: move this up a class? */

	/* force a save of the index, just to make sure */
	/* note this could be expensive so possibly shouldn't be here
	   as such */
	if (cls->index) {
		ibex_save(cls->index);
	}

	return 0;
}

/* sync the summary with the ondisk files. */
static int
maildir_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changes, CamelException *ex)
{
	int count, i;
	CamelMessageInfo *info;
	CamelMaildirMessageInfo *mdi;
#ifdef DOESTRV
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
#endif
	char *name;
	struct stat st;

	d(printf("summary_sync(expunge=%s)\n", expunge?"true":"false"));

	if (camel_local_summary_check(cls, changes, ex) == -1)
		return -1;

	if (cls->index) {
		ibex_save(cls->index);
	}

	count = camel_folder_summary_count((CamelFolderSummary *)cls);
	for (i=count-1;i>=0;i--) {
		info = camel_folder_summary_index((CamelFolderSummary *)cls, i);
		mdi = (CamelMaildirMessageInfo *)info;
		if (info && (info->flags & CAMEL_MESSAGE_DELETED) && expunge) {
			name = g_strdup_printf("%s/cur/%s", cls->folder_path, camel_maildir_info_filename(mdi));
			d(printf("deleting %s\n", name));
			if (unlink(name) == 0 || errno==ENOENT) {

				/* FIXME: put this in folder_summary::remove()? */
				if (cls->index)
					ibex_unindex(cls->index, (char *)camel_message_info_uid(info));

				camel_folder_change_info_remove_uid(changes, camel_message_info_uid(info));
				camel_folder_summary_remove((CamelFolderSummary *)cls, info);
			}
			g_free(name);
		} else if (info && (info->flags & CAMEL_MESSAGE_FOLDER_FLAGGED)) {
			char *newname = camel_maildir_summary_info_to_name(info);
			char *dest;

			/* do we care about additional metainfo stored inside the message? */
			/* probably should all go in the filename? */

			/* have our flags/ i.e. name changed? */
			if (strcmp(newname, camel_maildir_info_filename(mdi))) {
				name = g_strdup_printf("%s/cur/%s", cls->folder_path, camel_maildir_info_filename(mdi));
				dest = g_strdup_printf("%s/cur/%s", cls->folder_path, newname);
				rename(name, dest);
				if (stat(dest, &st) == -1) {
					/* we'll assume it didn't work, but dont change anything else */
					g_free(newname);
				} else {
					/* TODO: If this is made mt-safe, then this code could be a problem, since
					   the estrv is being modified.
					   Sigh, this may mean the maildir name has to be cached another way */
#ifdef DOESTRV
#warning "cannot modify the estrv after its been setup, for mt-safe code"
					CAMEL_SUMMARY_LOCK(s, summary_lock);
					/* need to update the summary hash ref */
					g_hash_table_remove(s->messages_uid, camel_message_info_uid(info));
					info->strings = e_strv_set_ref_free(info->strings, CAMEL_MAILDIR_INFO_FILENAME, newname);
					info->strings = e_strv_pack(info->strings);
					g_hash_table_insert(s->messages_uid, (char *)camel_message_info_uid(info), info);
					CAMEL_SUMMARY_UNLOCK(s, summary_lock);
#else
					g_free(mdi->filename);
					mdi->filename = newname;
#endif
				}
				g_free(name);
				g_free(dest);
			} else {
				g_free(newname);
			}

			/* strip FOLDER_MESSAGE_FLAGED, etc */
			info->flags &= 0xffff;
		}
		camel_folder_summary_info_free((CamelFolderSummary *)cls, info);
	}
	return 0;
}

