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

#include "camel-mbox-summary.h"
#include "camel/camel-file-utils.h"
#include "camel/camel-mime-message.h"
#include "camel/camel-operation.h"

#define io(x)
#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

#define CAMEL_MBOX_SUMMARY_VERSION (0x1000)

struct _CamelMboxSummaryPrivate {
};

#define _PRIVATE(o) (((CamelMboxSummary *)(o))->priv)

static int summary_header_load (CamelFolderSummary *, FILE *);
static int summary_header_save (CamelFolderSummary *, FILE *);

static CamelMessageInfo * message_info_new (CamelFolderSummary *, struct _header_raw *);
static CamelMessageInfo * message_info_new_from_parser (CamelFolderSummary *, CamelMimeParser *);
static CamelMessageInfo * message_info_load (CamelFolderSummary *, FILE *);
static int		  message_info_save (CamelFolderSummary *, FILE *, CamelMessageInfo *);
/*static void		  message_info_free (CamelFolderSummary *, CamelMessageInfo *);*/

static int mbox_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex);
static int mbox_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex);

static void camel_mbox_summary_class_init (CamelMboxSummaryClass *klass);
static void camel_mbox_summary_init       (CamelMboxSummary *obj);
static void camel_mbox_summary_finalise   (CamelObject *obj);

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

	lklass->check = mbox_summary_check;
	lklass->sync = mbox_summary_sync;
}

static void
camel_mbox_summary_init(CamelMboxSummary *obj)
{
	struct _CamelMboxSummaryPrivate *p;
	struct _CamelFolderSummary *s = (CamelFolderSummary *)obj;

	p = _PRIVATE(obj) = g_malloc0(sizeof(*p));

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

static int
summary_header_load(CamelFolderSummary *s, FILE *in)
{
	CamelMboxSummary *mbs = CAMEL_MBOX_SUMMARY(s);

	if (((CamelFolderSummaryClass *)camel_mbox_summary_parent)->summary_header_load(s, in) == -1)
		return -1;

	return camel_file_util_decode_uint32(in, &mbs->folder_size);
}

static int
summary_header_save(CamelFolderSummary *s, FILE *out)
{
	CamelMboxSummary *mbs = CAMEL_MBOX_SUMMARY(s);

	if (((CamelFolderSummaryClass *)camel_mbox_summary_parent)->summary_header_save(s, out) == -1)
		return -1;

	return camel_file_util_encode_uint32(out, mbs->folder_size);
}

static CamelMessageInfo *
message_info_new(CamelFolderSummary *s, struct _header_raw *h)
{
	CamelMessageInfo *mi;

	mi = ((CamelFolderSummaryClass *)camel_mbox_summary_parent)->message_info_new(s, h);
	if (mi) {
		CamelMboxMessageInfo *mbi = (CamelMboxMessageInfo *)mi;

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

static int
summary_rebuild(CamelMboxSummary *mbs, off_t offset, CamelException *ex)
{
	CamelLocalSummary *cls = (CamelLocalSummary *)mbs;
	CamelFolderSummary *s = (CamelFolderSummary *)mbs;
	CamelMimeParser *mp;
	int fd;
	int ok = 0;
	struct stat st;
	off_t size = 0;

	/* FIXME: If there is a failure, it shouldn't clear the summary and restart,
	   it should try and merge the summary info's.  This is a bit tricky. */

	camel_operation_start(NULL, _("Storing folder"));

	fd = open(cls->folder_path, O_RDONLY);
	if (fd == -1) {
		d(printf("%s failed to open: %s\n", cls->folder_path, strerror(errno)));
		camel_exception_setv(ex, 1, _("Could not open folder: %s: %s"),
				     cls->folder_path, strerror(errno));
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
		if (camel_mime_parser_step(mp, NULL, NULL) == HSCAN_FROM) {
			if (camel_mime_parser_tell_start_from(mp) != offset) {
				g_warning("The next message didn't start where I expected, building summary from start");
				camel_mime_parser_drop_step(mp);
				offset = 0;
				camel_mime_parser_seek(mp, offset, SEEK_SET);
				camel_folder_summary_clear(s);
			} else {
				camel_mime_parser_unstep(mp);
			}
		} else {
			d(printf("mime parser state ran out? state is %d\n", camel_mime_parser_state(mp)));
			camel_object_unref(CAMEL_OBJECT(mp));
			/* end of file - no content? no error either */
			camel_operation_end(NULL);
			return 0;
		}
	}

	while (camel_mime_parser_step(mp, NULL, NULL) == HSCAN_FROM) {
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

		g_assert(camel_mime_parser_step(mp, NULL, NULL) == HSCAN_FROM_END);
	}

	camel_object_unref(CAMEL_OBJECT (mp));
	
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

/* like summary_rebuild, but also do changeinfo stuff (if supplied) */
static int
summary_update(CamelLocalSummary *cls, off_t offset, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	int ret, i, count;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
	CamelMboxSummary *mbs = (CamelMboxSummary *)cls;

	d(printf("Calling summary update, from pos %d\n", (int)offset));

	if (changeinfo) {
		/* we use the diff function of the change_info to build the update list. */
		for (i = 0; i < camel_folder_summary_count(s); i++) {
			CamelMessageInfo *mi = camel_folder_summary_index(s, i);

			camel_folder_change_info_add_source(changeinfo, camel_message_info_uid(mi));
			camel_folder_summary_info_free(s, mi);
		}
	}

	/* do the actual work */
	cls->index_force = FALSE;
	ret = summary_rebuild(mbs, offset, ex);

	if (changeinfo) {
		count = camel_folder_summary_count(s);
		for (i = 0; i < count; i++) {
			CamelMessageInfo *mi = camel_folder_summary_index(s, i);
			camel_folder_change_info_add_update(changeinfo, camel_message_info_uid(mi));
			camel_folder_summary_info_free(s, mi);
		}
		camel_folder_change_info_build_diff(changeinfo);
	}

	return ret;
}

static int
mbox_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changes, CamelException *ex)
{
	CamelMboxSummary *mbs = (CamelMboxSummary *)cls;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
	struct stat st;
	int ret = 0;

	d(printf("Checking summary\n"));

	/* check if the summary is up-to-date */
	if (stat(cls->folder_path, &st) == -1) {
		camel_folder_summary_clear(s);
		camel_exception_setv(ex, 1, _("Cannot check folder: %s: %s"), cls->folder_path, strerror(errno));
		return -1;
	}

	if (st.st_size == 0) {
		/* empty?  No need to scan at all */
		d(printf("Empty mbox, clearing summary\n"));
		camel_folder_summary_clear(s);
		ret = 0;
	} else if (s->messages->len == 0) {
		/* if we are empty, then we rebuilt from scratch */
		d(printf("Empty summary, rebuilding from start\n"));
		ret = summary_update(cls, 0, changes, ex);
	} else {
		/* is the summary uptodate? */
		if (st.st_size != mbs->folder_size || st.st_mtime != s->time) {
			if (mbs->folder_size < st.st_size) {
				/* this will automatically rescan from 0 if there is a problem */
				d(printf("folder grew, attempting to rebuild from %d\n", mbs->folder_size));
				ret = summary_update(cls, mbs->folder_size, changes, ex);
			} else {
				d(printf("folder shrank!  rebuilding from start\n"));
				camel_folder_summary_clear(s);
				ret = summary_update(cls, 0, changes, ex);
			}
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

static char *tz_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static char *tz_days[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

/* tries to build a From line, based on message headers */
char *
camel_mbox_summary_build_from(struct _header_raw *header)
{
	GString *out = g_string_new("From ");
	char *ret;
	const char *tmp;
	time_t thetime;
	int offset;
	struct tm tm;

	tmp = header_raw_find(&header, "Sender", NULL);
	if (tmp == NULL)
		tmp = header_raw_find(&header, "From", NULL);
	if (tmp != NULL) {
		struct _header_address *addr = header_address_decode(tmp);

		tmp = NULL;
		if (addr) {
			if (addr->type == HEADER_ADDRESS_NAME) {
				g_string_append(out, addr->v.addr);
				tmp = "";
			}
			header_address_unref(addr);
		}
	}
	if (tmp == NULL) {
		g_string_append(out, "unknown@nodomain.now.au");
	}

	/* try use the received header to get the date */
	tmp = header_raw_find(&header, "Received", NULL);
	if (tmp) {
		tmp = strrchr(tmp, ';');
		if (tmp)
			tmp++;
	}

	/* if there isn't one, try the Date field */
	if (tmp == NULL)
		tmp = header_raw_find(&header, "Date", NULL);

	thetime = header_decode_date(tmp, &offset);

	thetime += ((offset / 100) * (60 * 60)) + (offset % 100) * 60;

	/* a pseudo, but still bogus attempt at thread safing the function */
	/*memcpy(&tm, gmtime(&thetime), sizeof(tm));*/
	gmtime_r(&thetime, &tm);

	g_string_sprintfa(out, " %s %s %2d %02d:%02d:%02d %4d\n",
			  tz_days[tm.tm_wday],
			  tz_months[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year + 1900);

	ret = out->str;
	g_string_free(out, FALSE);
	return ret;
}

/* perform a full sync */
static int
mbox_summary_sync_full(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	CamelMboxSummary *mbs = (CamelMboxSummary *)cls;
	CamelFolderSummary *s = (CamelFolderSummary *)mbs;
	CamelMimeParser *mp = NULL;
	int i, count;
	CamelMboxMessageInfo *info = NULL;
	int fd = -1, fdout = -1;
	char *tmpname = NULL;
	char *buffer, *xevnew = NULL;
	int len;
	const char *fromline;
	int lastdel = FALSE;

	d(printf("performing full summary/sync\n"));

	camel_operation_start(NULL, _("Storing folder"));

	fd = open(cls->folder_path, O_RDONLY);
	if (fd == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not open file: %s: %s"),
				     cls->folder_path, strerror(errno));
		camel_operation_end(NULL);
		return -1;
	}

	mp = camel_mime_parser_new();
	camel_mime_parser_scan_from(mp, TRUE);
	camel_mime_parser_scan_pre_from(mp, TRUE);
	camel_mime_parser_init_with_fd(mp, fd);

	tmpname = alloca(strlen (cls->folder_path) + 5);
	sprintf(tmpname, "%s.tmp", cls->folder_path);
	d(printf("Writing tmp file to %s\n", tmpname));
	fdout = open(tmpname, O_WRONLY|O_CREAT|O_TRUNC, 0600);
	if (fdout == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot open temporary mailbox: %s"), strerror(errno));
		goto error;
	}

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

		if (camel_mime_parser_step(mp, &buffer, &len) != HSCAN_FROM) {
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
		if (expunge && info->info.flags & CAMEL_MESSAGE_DELETED) {
			const char *uid = camel_message_info_uid(info);

			d(printf("Deleting %s\n", uid));

			if (cls->index)
				camel_index_delete_name(cls->index, uid);

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
			if (i>0) {
				write(fdout, "\n", 1);
			}
			info->frompos = lseek(fdout, 0, SEEK_CUR);
			fromline = camel_mime_parser_from_line(mp);
			write(fdout, fromline, strlen(fromline));
		}

		if (info && info->info.flags & (CAMEL_MESSAGE_FOLDER_NOXEV | CAMEL_MESSAGE_FOLDER_FLAGGED)) {
			d(printf("Updating header for %s flags = %08x\n", camel_message_info_uid(info), info->info.flags));

			if (camel_mime_parser_step(mp, &buffer, &len) == HSCAN_FROM_END) {
				g_warning("camel_mime_parser_step failed (2)");
				goto error;
			}

			xevnew = camel_local_summary_encode_x_evolution(cls, (CamelMessageInfo *)info);
			if (camel_local_summary_write_headers(fdout, camel_mime_parser_headers_raw(mp), xevnew) == -1) {
				d(printf("Error writing to tmp mailbox\n"));
				camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
						     _("Error writing to temp mailbox: %s"),
						     strerror(errno));
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
			while (camel_mime_parser_step(mp, &buffer, &len) == HSCAN_PRE_FROM) {
				/*d(printf("copying mbox contents to tmp: '%.*s'\n", len, buffer));*/
				if (write(fdout, buffer, len) != len) {
					camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
							     _("Writing to tmp mailbox failed: %s: %s"),
							     cls->folder_path, strerror(errno));
					goto error;
				}
			}
			d(printf("we are now at %d, from = %d\n", (int)camel_mime_parser_tell(mp),
				 (int)camel_mime_parser_tell_start_from(mp)));
			camel_mime_parser_unstep(mp);
			camel_folder_summary_info_free(s, (CamelMessageInfo *)info);
			info = NULL;
		}
	}

	d(printf("Closing folders\n"));

	if (close(fd) == -1) {
		g_warning("Cannot close source folder: %s", strerror(errno));
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not close source folder %s: %s"),
				     cls->folder_path, strerror(errno));
		fd = -1;
		goto error;
	}

	if (close(fdout) == -1) {
		g_warning("Cannot close tmp folder: %s", strerror(errno));
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not close temp folder: %s"),
				     strerror(errno));
		fdout = -1;
		goto error;
	}

	/* this should probably either use unlink/link/unlink, or recopy over
	   the original mailbox, for various locking reasons/etc */
	if (rename(tmpname, cls->folder_path) == -1) {
		g_warning("Cannot rename folder: %s", strerror(errno));
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not rename folder: %s"),
				     strerror(errno));
		goto error;
	}
	tmpname = NULL;

	camel_object_unref((CamelObject *)mp);
	camel_operation_end(NULL);
		
	return 0;
 error:
	if (fd != -1)
		close(fd);
	
	if (fdout != -1)
		close(fdout);
	
	g_free(xevnew);
	
	if (tmpname)
		unlink(tmpname);
	if (mp)
		camel_object_unref((CamelObject *)mp);
	if (info)
		camel_folder_summary_info_free(s, (CamelMessageInfo *)info);

	camel_operation_end(NULL);

	return -1;
}

/* perform a quick sync - only system flags have changed */
static int
mbox_summary_sync_quick(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	CamelMboxSummary *mbs = (CamelMboxSummary *)cls;
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
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not open file: %s: %s"),
				     cls->folder_path, strerror(errno));

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

		if (camel_mime_parser_step(mp, 0, 0) != HSCAN_FROM) {
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

		if (camel_mime_parser_step(mp, 0, 0) == HSCAN_FROM_END) {
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
		xevtmp = header_unfold(xevnew);
		/* the raw header contains a leading ' ', so (dis)count that too */
		if (strlen(xev)-1 != strlen(xevtmp)) {
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
		g_warning("Cannot close source folder: %s", strerror(errno));
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not close source folder %s: %s"),
				     cls->folder_path, strerror(errno));
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
			ret = mbox_summary_sync_quick(cls, expunge, changeinfo, ex);
			if (ret == -1) {
				g_warning("failed a quick-sync, trying a full sync");
				camel_exception_clear(ex);
			}
		} else {
			ret = 0;
		}
	}

	if (ret == -1)
		ret = mbox_summary_sync_full(cls, expunge, changeinfo, ex);
	if (ret == -1)
		return -1;

	if (stat(cls->folder_path, &st) == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, _("Unknown error: %s"), strerror(errno));
		return -1;
	}

	if (mbs->folder_size != st.st_size || s->time != st.st_mtime) {
		s->time = st.st_mtime;
		mbs->folder_size = st.st_size;
		camel_folder_summary_touch(s);
	}

	return ((CamelLocalSummaryClass *)camel_mbox_summary_parent)->sync(cls, expunge, changeinfo, ex);
}
