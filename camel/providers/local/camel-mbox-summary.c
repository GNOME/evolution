/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 *
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-mbox-summary.h"
#include "camel/camel-mime-message.h"
#include "camel/camel-operation.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "camel-mbox-summary.h"
#include "camel/camel-file-utils.h"
#include "camel/camel-mime-message.h"
#include "camel/camel-operation.h"
#include "camel-i18n.h"

#define io(x)
#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

#define CAMEL_MBOX_SUMMARY_VERSION (1)

static int summary_header_load (CamelFolderSummary *, FILE *);
static int summary_header_save (CamelFolderSummary *, FILE *);

static CamelMessageInfo * message_info_new (CamelFolderSummary *, struct _camel_header_raw *);
static CamelMessageInfo * message_info_new_from_parser (CamelFolderSummary *, CamelMimeParser *);
static CamelMessageInfo * message_info_load (CamelFolderSummary *, FILE *);
static int		  message_info_save (CamelFolderSummary *, FILE *, CamelMessageInfo *);
/*static void		  message_info_free (CamelFolderSummary *, CamelMessageInfo *);*/

static char *mbox_summary_encode_x_evolution (CamelLocalSummary *cls, const CamelMessageInfo *mi);

static int mbox_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex);
static int mbox_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex);
#ifdef STATUS_PINE
static CamelMessageInfo *mbox_summary_add(CamelLocalSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *ci, CamelException *ex);
#endif

static int mbox_summary_sync_quick(CamelMboxSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex);
static int mbox_summary_sync_full(CamelMboxSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex);

static void camel_mbox_summary_class_init (CamelMboxSummaryClass *klass);
static void camel_mbox_summary_init       (CamelMboxSummary *obj);
static void camel_mbox_summary_finalise   (CamelObject *obj);

#ifdef STATUS_PINE
/* Which status flags are stored in each separate header */
#define STATUS_XSTATUS (CAMEL_MESSAGE_FLAGGED|CAMEL_MESSAGE_ANSWERED|CAMEL_MESSAGE_DELETED)
#define STATUS_STATUS (CAMEL_MESSAGE_SEEN)

static void encode_status(guint32 flags, char status[8]);
static guint32 decode_status(const char *status);
#endif

static CamelLocalSummaryClass *camel_mbox_summary_parent;

CamelType
camel_mbox_summary_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_local_summary_get_type(), "CamelMboxSummary",
					   sizeof (CamelMboxSummary),
					   sizeof (CamelMboxSummaryClass),
					   (CamelObjectClassInitFunc) camel_mbox_summary_class_init,
					   NULL,
					   (CamelObjectInitFunc) camel_mbox_summary_init,
					   (CamelObjectFinalizeFunc) camel_mbox_summary_finalise);
	}
	
	return type;
}

static void
camel_mbox_summary_class_init(CamelMboxSummaryClass *klass)
{
	CamelFolderSummaryClass *sklass = (CamelFolderSummaryClass *)klass;
	CamelLocalSummaryClass *lklass = (CamelLocalSummaryClass *)klass;
	
	camel_mbox_summary_parent = (CamelLocalSummaryClass *)camel_type_get_global_classfuncs(camel_local_summary_get_type());

	sklass->summary_header_load = summary_header_load;
	sklass->summary_header_save = summary_header_save;

	sklass->message_info_new  = message_info_new;
	sklass->message_info_new_from_parser = message_info_new_from_parser;
	sklass->message_info_load = message_info_load;
	sklass->message_info_save = message_info_save;
	/*sklass->message_info_free = message_info_free;*/
	
	lklass->encode_x_evolution = mbox_summary_encode_x_evolution;
	lklass->check = mbox_summary_check;
	lklass->sync = mbox_summary_sync;
#ifdef STATUS_PINE
	lklass->add = mbox_summary_add;
#endif

	klass->sync_quick = mbox_summary_sync_quick;
	klass->sync_full = mbox_summary_sync_full;
}

static void
camel_mbox_summary_init(CamelMboxSummary *obj)
{
	struct _CamelFolderSummary *s = (CamelFolderSummary *)obj;
	
	/* subclasses need to set the right instance data sizes */
	s->message_info_size = sizeof(CamelMboxMessageInfo);
	s->content_info_size = sizeof(CamelMboxMessageContentInfo);

	/* and a unique file version */
	s->version += CAMEL_MBOX_SUMMARY_VERSION;
}

static void
camel_mbox_summary_finalise(CamelObject *obj)
{
	/*CamelMboxSummary *mbs = CAMEL_MBOX_SUMMARY(obj);*/
}

/**
 * camel_mbox_summary_new:
 *
 * Create a new CamelMboxSummary object.
 * 
 * Return value: A new CamelMboxSummary widget.
 **/
CamelMboxSummary *
camel_mbox_summary_new(const char *filename, const char *mbox_name, CamelIndex *index)
{
	CamelMboxSummary *new = (CamelMboxSummary *)camel_object_new(camel_mbox_summary_get_type());

	camel_local_summary_construct((CamelLocalSummary *)new, filename, mbox_name, index);
	return new;
}

void camel_mbox_summary_xstatus(CamelMboxSummary *mbs, int state)
{
	mbs->xstatus = state;
}

static char *
mbox_summary_encode_x_evolution (CamelLocalSummary *cls, const CamelMessageInfo *mi)
{
	const char *p, *uidstr;
	guint32 uid;
	
	p = uidstr = camel_message_info_uid (mi);
	while (*p && isdigit (*p))
		p++;
	
	if (*p == 0 && sscanf (uidstr, "%u", &uid) == 1) {
		return g_strdup_printf ("%08x-%04x", uid, mi->flags & 0xffff);
	} else {
		return g_strdup_printf ("%s-%04x", uidstr, mi->flags & 0xffff);
	}
}

static int
summary_header_load(CamelFolderSummary *s, FILE *in)
{
	CamelMboxSummary *mbs = CAMEL_MBOX_SUMMARY(s);

	if (((CamelFolderSummaryClass *)camel_mbox_summary_parent)->summary_header_load(s, in) == -1)
		return -1;

	/* legacy version */
	if (s->version == 0x120c)
		return camel_file_util_decode_uint32(in, (guint32 *) &mbs->folder_size);

	/* version 1 */
	if (camel_file_util_decode_fixed_int32(in, &mbs->version) == -1
	    || camel_file_util_decode_size_t(in, &mbs->folder_size) == -1)
		return -1;

	return 0;
}

static int
summary_header_save(CamelFolderSummary *s, FILE *out)
{
	CamelMboxSummary *mbs = CAMEL_MBOX_SUMMARY(s);

	if (((CamelFolderSummaryClass *)camel_mbox_summary_parent)->summary_header_save(s, out) == -1)
		return -1;

	camel_file_util_encode_fixed_int32(out, CAMEL_MBOX_SUMMARY_VERSION);

	return camel_file_util_encode_size_t(out, mbs->folder_size);
}

static CamelMessageInfo *
message_info_new(CamelFolderSummary *s, struct _camel_header_raw *h)
{
	CamelMessageInfo *mi;
	CamelMboxSummary *mbs = (CamelMboxSummary *)s;

	mi = ((CamelFolderSummaryClass *)camel_mbox_summary_parent)->message_info_new(s, h);
	if (mi) {
		CamelMboxMessageInfo *mbi = (CamelMboxMessageInfo *)mi;
		const char *xev, *uid;
		CamelMessageInfo *info = NULL;
		int add = 0;	/* bitmask of things to add, 1 assign uid, 2, just add as new, 4 = recent */
#ifdef STATUS_PINE
		const char *status = NULL, *xstatus = NULL;
		guint32 flags = 0;

		if (mbs->xstatus) {
			/* check for existance of status & x-status headers */
			status = camel_header_raw_find(&h, "Status", NULL);
			if (status)
				flags = decode_status(status);
			xstatus = camel_header_raw_find(&h, "X-Status", NULL);
			if (xstatus)
				flags |= decode_status(xstatus);
		}
#endif
		/* if we have an xev header, use it, else assign a new one */
		xev = camel_header_raw_find(&h, "X-Evolution", NULL);
		if (xev != NULL
		    && camel_local_summary_decode_x_evolution((CamelLocalSummary *)s, xev, mi) == 0) {
			uid = camel_message_info_uid(mi);
			d(printf("found valid x-evolution: %s\n", uid));
			info = camel_folder_summary_uid(s, uid);
			if (info) {
				if ((info->flags & CAMEL_MESSAGE_FOLDER_NOTSEEN)) {
					info->flags &= ~CAMEL_MESSAGE_FOLDER_NOTSEEN;
					camel_folder_summary_info_free(s, mi);
					mbi = (CamelMboxMessageInfo *)(mi = info);
				} else {
					add = 7;
					d(printf("seen '%s' before, adding anew\n", uid));
					camel_folder_summary_info_free(s, info);
				}
			} else {
				add = 2;
				d(printf("but isn't present in summary\n"));
			}
		} else {
			d(printf("didn't find x-evolution\n"));
			add = 7;
		}

		if (add&1) {
			mi->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED | CAMEL_MESSAGE_FOLDER_NOXEV;
			camel_message_info_set_uid(mi, camel_folder_summary_next_uid_string(s));
		} else {
			camel_folder_summary_set_uid(s, strtoul(camel_message_info_uid(mi), NULL, 10));
		}
#ifdef STATUS_PINE
		if (mbs->xstatus && add&2) {
			/* use the status as the flags when we read it the first time */
			if (status)
				mi->flags = (mi->flags & ~(STATUS_STATUS)) | (flags & STATUS_STATUS);
			if (xstatus)
				mi->flags = (mi->flags & ~(STATUS_XSTATUS)) | (flags & STATUS_XSTATUS);
		}
#endif
		if (mbs->changes) {
			if (add&2)
				camel_folder_change_info_add_uid(mbs->changes, camel_message_info_uid(mi));
			if ((add&4) && status == NULL)
				camel_folder_change_info_recent_uid(mbs->changes, camel_message_info_uid(mi));
		}

		mbi->frompos = -1;
	}
	
	return mi;
}

static CamelMessageInfo *
message_info_new_from_parser(CamelFolderSummary *s, CamelMimeParser *mp)
{
	CamelMessageInfo *mi;

	mi = ((CamelFolderSummaryClass *)camel_mbox_summary_parent)->message_info_new_from_parser(s, mp);
	if (mi) {
		CamelMboxMessageInfo *mbi = (CamelMboxMessageInfo *)mi;

		mbi->frompos = camel_mime_parser_tell_start_from(mp);
	}
	
	return mi;
}

static CamelMessageInfo *
message_info_load(CamelFolderSummary *s, FILE *in)
{
	CamelMessageInfo *mi;

	io(printf("loading mbox message info\n"));

	mi = ((CamelFolderSummaryClass *)camel_mbox_summary_parent)->message_info_load(s, in);
	if (mi) {
		CamelMboxMessageInfo *mbi = (CamelMboxMessageInfo *)mi;
		
		if (camel_file_util_decode_off_t(in, &mbi->frompos) == -1)
			goto error;
	}
	
	return mi;
error:
	camel_folder_summary_info_free(s, mi);
	return NULL;
}

static int
message_info_save(CamelFolderSummary *s, FILE *out, CamelMessageInfo *mi)
{
	CamelMboxMessageInfo *mbi = (CamelMboxMessageInfo *)mi;

	io(printf("saving mbox message info\n"));

	if (((CamelFolderSummaryClass *)camel_mbox_summary_parent)->message_info_save(s, out, mi) == -1
	    || camel_file_util_encode_off_t(out, mbi->frompos) == -1)
		return -1;

	return 0;
}

/* like summary_rebuild, but also do changeinfo stuff (if supplied) */
static int
summary_update(CamelLocalSummary *cls, off_t offset, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	int i, count;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
	CamelMboxSummary *mbs = (CamelMboxSummary *)cls;
	CamelMimeParser *mp;
	CamelMessageInfo *mi;
	int fd;
	int ok = 0;
	struct stat st;
	off_t size = 0;

	d(printf("Calling summary update, from pos %d\n", (int)offset));

	cls->index_force = FALSE;

	camel_operation_start(NULL, _("Storing folder"));

	fd = open(cls->folder_path, O_RDONLY);
	if (fd == -1) {
		d(printf("%s failed to open: %s\n", cls->folder_path, strerror (errno)));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not open folder: %s: %s"),
				      cls->folder_path, g_strerror (errno));
		camel_operation_end(NULL);
		return -1;
	}
	
	if (fstat(fd, &st) == 0)
		size = st.st_size;

	mp = camel_mime_parser_new();
	camel_mime_parser_init_with_fd(mp, fd);
	camel_mime_parser_scan_from(mp, TRUE);
	camel_mime_parser_seek(mp, offset, SEEK_SET);

	if (offset > 0) {
		if (camel_mime_parser_step(mp, NULL, NULL) == CAMEL_MIME_PARSER_STATE_FROM
		    && camel_mime_parser_tell_start_from(mp) == offset) {
			camel_mime_parser_unstep(mp);
		} else {
			g_warning("The next message didn't start where I expected, building summary from start");
			camel_mime_parser_drop_step(mp);
			offset = 0;
			camel_mime_parser_seek(mp, offset, SEEK_SET);
		}
	}

	/* we mark messages as to whether we've seen them or not.
	   If we're not starting from the start, we must be starting
	   from the old end, so everything must be treated as new */
	count = camel_folder_summary_count(s);
	for (i=0;i<count;i++) {
		mi = camel_folder_summary_index(s, i);
		if (offset == 0)
			mi->flags |= CAMEL_MESSAGE_FOLDER_NOTSEEN;
		else
			mi->flags &= ~CAMEL_MESSAGE_FOLDER_NOTSEEN;
		camel_folder_summary_info_free(s, mi);
	}
	mbs->changes = changeinfo;

	while (camel_mime_parser_step(mp, NULL, NULL) == CAMEL_MIME_PARSER_STATE_FROM) {
		CamelMessageInfo *info;
		off_t pc = camel_mime_parser_tell_start_from (mp) + 1;
		
		camel_operation_progress (NULL, (int) (((float) pc / size) * 100));

		info = camel_folder_summary_add_from_parser(s, mp);
		if (info == NULL) {
			camel_exception_setv(ex, 1, _("Fatal mail parser error near position %ld in folder %s"),
					     camel_mime_parser_tell(mp), cls->folder_path);
			ok = -1;
			break;
		}

		g_assert(camel_mime_parser_step(mp, NULL, NULL) == CAMEL_MIME_PARSER_STATE_FROM_END);
	}

	camel_object_unref(CAMEL_OBJECT (mp));

	count = camel_folder_summary_count(s);
	for (i=0;i<count;i++) {
		mi = camel_folder_summary_index(s, i);
		/* must've dissapeared from the file? */
		if (mi->flags & CAMEL_MESSAGE_FOLDER_NOTSEEN) {
			d(printf("uid '%s' vanished, removing", camel_message_info_uid(mi)));
			if (changeinfo)
				camel_folder_change_info_remove_uid(changeinfo, camel_message_info_uid(mi));
			camel_folder_summary_remove(s, mi);
			count--;
			i--;
		}
		camel_folder_summary_info_free(s, mi);
	}
	mbs->changes = NULL;
	
	/* update the file size/mtime in the summary */
	if (ok != -1) {
		if (stat(cls->folder_path, &st) == 0) {
			camel_folder_summary_touch(s);
			mbs->folder_size = st.st_size;
			s->time = st.st_mtime;
		}
	}

	camel_operation_end(NULL);

	return ok;
}

static int
mbox_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changes, CamelException *ex)
{
	CamelMboxSummary *mbs = (CamelMboxSummary *)cls;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
	struct stat st;
	int ret = 0;
	int i, count;

	d(printf("Checking summary\n"));

	/* check if the summary is up-to-date */
	if (stat(cls->folder_path, &st) == -1) {
		camel_folder_summary_clear(s);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot check folder: %s: %s"),
				      cls->folder_path, g_strerror (errno));
		return -1;
	}

	if (cls->check_force)
		mbs->folder_size = 0;
	cls->check_force = 0;

	if (st.st_size == 0) {
		/* empty?  No need to scan at all */
		d(printf("Empty mbox, clearing summary\n"));
		count= camel_folder_summary_count(s);
		for (i=0;i<count;i++) {
			CamelMessageInfo *info = camel_folder_summary_index(s, i);

			if (info) {
				camel_folder_change_info_remove_uid(changes, camel_message_info_uid(info));
				camel_folder_summary_info_free(s, info);
			}
		}
		camel_folder_summary_clear(s);
		ret = 0;
	} else {
		/* is the summary uptodate? */
		if (st.st_size != mbs->folder_size || st.st_mtime != s->time) {
			if (mbs->folder_size < st.st_size) {
				/* this will automatically rescan from 0 if there is a problem */
				d(printf("folder grew, attempting to rebuild from %d\n", mbs->folder_size));
				ret = summary_update(cls, mbs->folder_size, changes, ex);
			} else {
				d(printf("folder shrank!  rebuilding from start\n"));
				ret = summary_update(cls, 0, changes, ex);
			}
		} else {
			d(printf("Folder unchanged, do nothing\n"));
		}
	}

	/* FIXME: move upstream? */

	if (ret != -1) {
		if (mbs->folder_size != st.st_size || s->time != st.st_mtime) {
			mbs->folder_size = st.st_size;
			s->time = st.st_mtime;
			camel_folder_summary_touch(s);
		}
	}

	return ret;
}

/* perform a full sync */
static int
mbox_summary_sync_full(CamelMboxSummary *mbs, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	CamelLocalSummary *cls = (CamelLocalSummary *)mbs;
	int fd = -1, fdout = -1;
	char *tmpname = NULL;
	guint32 flags = (expunge?1:0);

	d(printf("performing full summary/sync\n"));

	camel_operation_start(NULL, _("Storing folder"));

	fd = open(cls->folder_path, O_RDONLY);
	if (fd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not open file: %s: %s"),
				      cls->folder_path, g_strerror (errno));
		camel_operation_end(NULL);
		return -1;
	}

	tmpname = g_alloca (strlen (cls->folder_path) + 5);
	sprintf (tmpname, "%s.tmp", cls->folder_path);
	d(printf("Writing tmp file to %s\n", tmpname));
	fdout = open(tmpname, O_WRONLY|O_CREAT|O_TRUNC, 0600);
	if (fdout == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot open temporary mailbox: %s"),
				      g_strerror (errno));
		goto error;
	}

	if (camel_mbox_summary_sync_mbox((CamelMboxSummary *)cls, flags, changeinfo, fd, fdout, ex) == -1)
		goto error;

	d(printf("Closing folders\n"));

	if (close(fd) == -1) {
		g_warning("Cannot close source folder: %s", strerror (errno));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not close source folder %s: %s"),
				      cls->folder_path, g_strerror (errno));
		fd = -1;
		goto error;
	}

	if (close(fdout) == -1) {
		g_warning("Cannot close tmp folder: %s", strerror (errno));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not close temp folder: %s"),
				      g_strerror (errno));
		fdout = -1;
		goto error;
	}

	/* this should probably either use unlink/link/unlink, or recopy over
	   the original mailbox, for various locking reasons/etc */
	if (rename(tmpname, cls->folder_path) == -1) {
		g_warning("Cannot rename folder: %s", strerror (errno));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not rename folder: %s"),
				      g_strerror (errno));
		goto error;
	}
	tmpname = NULL;

	camel_operation_end(NULL);
		
	return 0;
 error:
	if (fd != -1)
		close(fd);
	
	if (fdout != -1)
		close(fdout);
	
	if (tmpname)
		unlink(tmpname);

	camel_operation_end(NULL);

	return -1;
}

/* perform a quick sync - only system flags have changed */
static int
mbox_summary_sync_quick(CamelMboxSummary *mbs, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	CamelLocalSummary *cls = (CamelLocalSummary *)mbs;
	CamelFolderSummary *s = (CamelFolderSummary *)mbs;
	CamelMimeParser *mp = NULL;
	int i, count;
	CamelMboxMessageInfo *info = NULL;
	int fd = -1;
	char *xevnew, *xevtmp;
	const char *xev;
	int len;
	off_t lastpos;

	d(printf("Performing quick summary sync\n"));

	camel_operation_start(NULL, _("Storing folder"));

	fd = open(cls->folder_path, O_RDWR);
	if (fd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not open file: %s: %s"),
				      cls->folder_path, g_strerror (errno));

		camel_operation_end(NULL);
		return -1;
	}

	mp = camel_mime_parser_new();
	camel_mime_parser_scan_from(mp, TRUE);
	camel_mime_parser_scan_pre_from(mp, TRUE);
	camel_mime_parser_init_with_fd(mp, fd);

	count = camel_folder_summary_count(s);
	for (i = 0; i < count; i++) {
		int xevoffset;
		int pc = (i+1)*100/count;

		camel_operation_progress(NULL, pc);

		info = (CamelMboxMessageInfo *)camel_folder_summary_index(s, i);

		g_assert(info);

		d(printf("Checking message %s %08x\n", camel_message_info_uid(info), info->info.flags));

		if ((info->info.flags & CAMEL_MESSAGE_FOLDER_FLAGGED) == 0) {
			camel_folder_summary_info_free(s, (CamelMessageInfo *)info);
			info = NULL;
			continue;
		}

		d(printf("Updating message %s\n", camel_message_info_uid(info)));

		camel_mime_parser_seek(mp, info->frompos, SEEK_SET);

		if (camel_mime_parser_step(mp, 0, 0) != CAMEL_MIME_PARSER_STATE_FROM) {
			g_warning("Expected a From line here, didn't get it");
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Summary and folder mismatch, even after a sync"));
			goto error;
		}

		if (camel_mime_parser_tell_start_from(mp) != info->frompos) {
			g_warning("Didn't get the next message where I expected (%d) got %d instead",
				  (int)info->frompos, (int)camel_mime_parser_tell_start_from(mp));
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Summary and folder mismatch, even after a sync"));
			goto error;
		}

		if (camel_mime_parser_step(mp, 0, 0) == CAMEL_MIME_PARSER_STATE_FROM_END) {
			g_warning("camel_mime_parser_step failed (2)");
			goto error;
		}

		xev = camel_mime_parser_header(mp, "X-Evolution", &xevoffset);
		if (xev == NULL || camel_local_summary_decode_x_evolution(cls, xev, NULL) == -1) {
			g_warning("We're supposed to have a valid x-ev header, but we dont");
			goto error;
		}
		xevnew = camel_local_summary_encode_x_evolution(cls, (CamelMessageInfo *)info);
		/* SIGH: encode_param_list is about the only function which folds headers by itself.
		   This should be fixed somehow differently (either parser doesn't fold headers,
		   or param_list doesn't, or something */
		xevtmp = camel_header_unfold(xevnew);
		/* the raw header contains a leading ' ', so (dis)count that too */
		if (strlen(xev)-1 != strlen(xevtmp)) {
			printf ("strlen(xev)-1 = %d; strlen(xevtmp) = %d\n", strlen(xev)-1, strlen(xevtmp));
			printf ("xev='%s'; xevtmp='%s'\n", xev, xevtmp);
			g_free(xevnew);
			g_free(xevtmp);
			g_warning("Hmm, the xev headers shouldn't have changed size, but they did");
			goto error;
		}
		g_free(xevtmp);

		/* we write out the xevnew string, assuming its been folded identically to the original too! */

		lastpos = lseek(fd, 0, SEEK_CUR);
		lseek(fd, xevoffset+strlen("X-Evolution: "), SEEK_SET);
		do {
			len = write(fd, xevnew, strlen(xevnew));
		} while (len == -1 && errno == EINTR);
		lseek(fd, lastpos, SEEK_SET);
		g_free(xevnew);

		camel_mime_parser_drop_step(mp);
		camel_mime_parser_drop_step(mp);

		info->info.flags &= 0xffff;
		camel_folder_summary_info_free(s, (CamelMessageInfo *)info);
	}

	d(printf("Closing folders\n"));

	if (close(fd) == -1) {
		g_warning ("Cannot close source folder: %s", strerror (errno));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not close source folder %s: %s"),
				      cls->folder_path, g_strerror (errno));
		fd = -1;
		goto error;
	}

	camel_object_unref((CamelObject *)mp);

	camel_operation_end(NULL);
	
	return 0;
 error:
	if (fd != -1)
		close(fd);
	if (mp)
		camel_object_unref((CamelObject *)mp);
	if (info)
		camel_folder_summary_info_free(s, (CamelMessageInfo *)info);

	camel_operation_end(NULL);

	return -1;
}

static int
mbox_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	struct stat st;
	CamelMboxSummary *mbs = (CamelMboxSummary *)cls;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
	int i, count;
	int quick = TRUE, work=FALSE;
	int ret;

	/* first, sync ourselves up, just to make sure */
	if (camel_local_summary_check(cls, changeinfo, ex) == -1)
		return -1;

	count = camel_folder_summary_count(s);
	if (count == 0)
		return 0;

	/* check what work we have to do, if any */
	for (i=0;quick && i<count; i++) {
		CamelMessageInfo *info = camel_folder_summary_index(s, i);
		g_assert(info);
		if ((expunge && (info->flags & CAMEL_MESSAGE_DELETED)) ||
		    (info->flags & (CAMEL_MESSAGE_FOLDER_NOXEV|CAMEL_MESSAGE_FOLDER_XEVCHANGE)))
			quick = FALSE;
		else
			work |= (info->flags & CAMEL_MESSAGE_FOLDER_FLAGGED) != 0;
		camel_folder_summary_info_free(s, info);
	}

	/* yuck i hate this logic, but its to simplify the 'all ok, update summary' and failover cases */
	ret = -1;
	if (quick) {
		if (work) {
			ret = ((CamelMboxSummaryClass *)((CamelObject *)cls)->klass)->sync_quick(mbs, expunge, changeinfo, ex);
			if (ret == -1) {
				g_warning("failed a quick-sync, trying a full sync");
				camel_exception_clear(ex);
			}
		} else {
			ret = 0;
		}
	}

	if (ret == -1)
		ret = ((CamelMboxSummaryClass *)((CamelObject *)cls)->klass)->sync_full(mbs, expunge, changeinfo, ex);
	if (ret == -1)
		return -1;

	if (stat(cls->folder_path, &st) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Unknown error: %s"), g_strerror (errno));
		return -1;
	}

	if (mbs->folder_size != st.st_size || s->time != st.st_mtime) {
		s->time = st.st_mtime;
		mbs->folder_size = st.st_size;
		camel_folder_summary_touch(s);
	}

	return ((CamelLocalSummaryClass *)camel_mbox_summary_parent)->sync(cls, expunge, changeinfo, ex);
}

int
camel_mbox_summary_sync_mbox(CamelMboxSummary *cls, guint32 flags, CamelFolderChangeInfo *changeinfo, int fd, int fdout, CamelException *ex)
{
	CamelMboxSummary *mbs = (CamelMboxSummary *)cls;
	CamelFolderSummary *s = (CamelFolderSummary *)mbs;
	CamelMimeParser *mp = NULL;
	int i, count;
	CamelMboxMessageInfo *info = NULL;
	char *buffer, *xevnew = NULL;
	size_t len;
	const char *fromline;
	int lastdel = FALSE;
#ifdef STATUS_PINE
	char statnew[8], xstatnew[8];
#endif

	d(printf("performing full summary/sync\n"));

	/* need to dup this because the mime-parser owns the fd after we give it to it */
	fd = dup(fd);
	if (fd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not store folder: %s"),
				      g_strerror (errno));
		return -1;
	}

	mp = camel_mime_parser_new();
	camel_mime_parser_scan_from(mp, TRUE);
	camel_mime_parser_scan_pre_from(mp, TRUE);
	camel_mime_parser_init_with_fd(mp, fd);

	count = camel_folder_summary_count(s);
	for (i = 0; i < count; i++) {
		int pc = (i + 1) * 100 / count;

		camel_operation_progress(NULL, pc);

		info = (CamelMboxMessageInfo *)camel_folder_summary_index(s, i);

		g_assert(info);

		d(printf("Looking at message %s\n", camel_message_info_uid(info)));

		/* only need to seek past deleted messages, otherwise we should be at the right spot/state already */
		if (lastdel) {
			d(printf("seeking to %d\n", (int)info->frompos));
			camel_mime_parser_seek(mp, info->frompos, SEEK_SET);
		}

		if (camel_mime_parser_step(mp, &buffer, &len) != CAMEL_MIME_PARSER_STATE_FROM) {
			g_warning("Expected a From line here, didn't get it");
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Summary and folder mismatch, even after a sync"));
			goto error;
		}

		if (camel_mime_parser_tell_start_from(mp) != info->frompos) {
			g_warning("Didn't get the next message where I expected (%d) got %d instead",
				  (int)info->frompos, (int)camel_mime_parser_tell_start_from(mp));
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Summary and folder mismatch, even after a sync"));
			goto error;
		}

		lastdel = FALSE;
		if ((flags&1) && info->info.flags & CAMEL_MESSAGE_DELETED) {
			const char *uid = camel_message_info_uid(info);

			d(printf("Deleting %s\n", uid));

			if (((CamelLocalSummary *)cls)->index)
				camel_index_delete_name(((CamelLocalSummary *)cls)->index, uid);

			/* remove it from the change list */
			camel_folder_change_info_remove_uid(changeinfo, uid);
			camel_folder_summary_remove(s, (CamelMessageInfo *)info);
			camel_folder_summary_info_free(s, (CamelMessageInfo *)info);
			count--;
			i--;
			info = NULL;
			lastdel = TRUE;
		} else {
			/* otherwise, the message is staying, copy its From_ line across */
#if 0
			if (i>0)
				write(fdout, "\n", 1);
#endif
			info->frompos = lseek(fdout, 0, SEEK_CUR);
			fromline = camel_mime_parser_from_line(mp);
			write(fdout, fromline, strlen(fromline));
		}

		if (info && info->info.flags & (CAMEL_MESSAGE_FOLDER_NOXEV | CAMEL_MESSAGE_FOLDER_FLAGGED)) {
			d(printf("Updating header for %s flags = %08x\n", camel_message_info_uid(info), info->info.flags));

			if (camel_mime_parser_step(mp, &buffer, &len) == CAMEL_MIME_PARSER_STATE_FROM_END) {
				g_warning("camel_mime_parser_step failed (2)");
				goto error;
			}

			xevnew = camel_local_summary_encode_x_evolution((CamelLocalSummary *)cls, (CamelMessageInfo *)info);
#ifdef STATUS_PINE
			if (mbs->xstatus) {
				encode_status(((CamelMessageInfo *)info)->flags & STATUS_STATUS, statnew);
				encode_status(((CamelMessageInfo *)info)->flags & STATUS_XSTATUS, xstatnew);
				len = camel_local_summary_write_headers(fdout, camel_mime_parser_headers_raw(mp), xevnew, statnew, xstatnew);
			} else {
#endif
				len = camel_local_summary_write_headers(fdout, camel_mime_parser_headers_raw(mp), xevnew, NULL, NULL);
#ifdef STATUS_PINE
			}
#endif
			if (len == -1) {
				d(printf("Error writing to tmp mailbox\n"));
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      _("Error writing to temp mailbox: %s"),
						      g_strerror (errno));
				goto error;
			}
			info->info.flags &= 0xffff;
			g_free(xevnew);
			xevnew = NULL;
			camel_mime_parser_drop_step(mp);
		}

		camel_mime_parser_drop_step(mp);
		if (info) {
			d(printf("looking for message content to copy across from %d\n", (int)camel_mime_parser_tell(mp)));
			while (camel_mime_parser_step(mp, &buffer, &len) == CAMEL_MIME_PARSER_STATE_PRE_FROM) {
				/*d(printf("copying mbox contents to tmp: '%.*s'\n", len, buffer));*/
				if (write(fdout, buffer, len) != len) {
					camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
							      _("Writing to tmp mailbox failed: %s: %s"),
							      ((CamelLocalSummary *)cls)->folder_path,
							      g_strerror (errno));
					goto error;
				}
			}

			if (write(fdout, "\n", 1) != 1) {
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      _("Error writing to temp mailbox: %s"),
						      g_strerror (errno));
				goto error;
			}

			d(printf("we are now at %d, from = %d\n", (int)camel_mime_parser_tell(mp),
				 (int)camel_mime_parser_tell_start_from(mp)));
			camel_mime_parser_unstep(mp);
			camel_folder_summary_info_free(s, (CamelMessageInfo *)info);
			info = NULL;
		}
	}

#if 0
	/* if last was deleted, append the \n we removed */
	if (lastdel && count > 0)
		write(fdout, "\n", 1);
#endif

	camel_object_unref((CamelObject *)mp);
		
	return 0;
 error:
	g_free(xevnew);
	
	if (mp)
		camel_object_unref((CamelObject *)mp);
	if (info)
		camel_folder_summary_info_free(s, (CamelMessageInfo *)info);

	return -1;
}

#ifdef STATUS_PINE
static CamelMessageInfo *
mbox_summary_add(CamelLocalSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *ci, CamelException *ex)
{
	CamelMessageInfo *mi;

	mi = ((CamelLocalSummaryClass *)camel_mbox_summary_parent)->add(cls, msg, info, ci, ex);
	if (mi && ((CamelMboxSummary *)cls)->xstatus) {
		char status[8];

		/* we snoop and add status/x-status headers to suit */
		encode_status(mi->flags & STATUS_STATUS, status);
		camel_medium_set_header((CamelMedium *)msg, "Status", status);
		encode_status(mi->flags & STATUS_XSTATUS, status);
		camel_medium_set_header((CamelMedium *)msg, "X-Status", status);
	}

	return mi;
}

static struct {
	char tag;
	guint32 flag;
} status_flags[] = {
	{ 'F', CAMEL_MESSAGE_FLAGGED },
	{ 'A', CAMEL_MESSAGE_ANSWERED },
	{ 'D', CAMEL_MESSAGE_DELETED },
	{ 'R', CAMEL_MESSAGE_SEEN },
};

static void
encode_status(guint32 flags, char status[8])
{
	char *p;
	int i;

	p = status;
	for (i=0;i<sizeof(status_flags)/sizeof(status_flags[0]);i++)
		if (status_flags[i].flag & flags)
			*p++ = status_flags[i].tag;
	*p++ = 'O';
	*p=0;
}

static guint32
decode_status(const char *status)
{
	const char *p;
	char c;
	guint32 flags = 0;
	int i;

	p = status;
	while ((c = *p++)) {
		for (i=0;i<sizeof(status_flags)/sizeof(status_flags[0]);i++)
			if (status_flags[i].tag == *p)
				flags |= status_flags[i].flag;
	}

	return flags;
}

#endif /* STATUS_PINE */
