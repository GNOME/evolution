/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 *
 *  Copyright (C) 2001 Ximian Inc. (www.ximian.com)
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <ctype.h>

#include "camel-spool-summary.h"
#include "camel-mime-message.h"
#include "camel-file-utils.h"
#include "camel-operation.h"

#define io(x)
#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

#define CAMEL_SPOOL_SUMMARY_VERSION (0x400)

struct _CamelSpoolSummaryPrivate {
};

#define _PRIVATE(o) (((CamelSpoolSummary *)(o))->priv)

static int summary_header_load (CamelFolderSummary *, FILE *);
static int summary_header_save (CamelFolderSummary *, FILE *);

static CamelMessageInfo * message_info_new (CamelFolderSummary *, struct _header_raw *);
static CamelMessageInfo * message_info_new_from_parser(CamelFolderSummary *s, CamelMimeParser *mp);
static CamelMessageInfo * message_info_load (CamelFolderSummary *, FILE *);
static int		  message_info_save (CamelFolderSummary *, FILE *, CamelMessageInfo *);

static int spool_summary_decode_x_evolution(CamelSpoolSummary *cls, const char *xev, CamelMessageInfo *mi);
static char *spool_summary_encode_x_evolution(CamelSpoolSummary *cls, const CamelMessageInfo *mi);

static int spool_summary_load(CamelSpoolSummary *cls, int forceindex, CamelException *ex);
static int spool_summary_check(CamelSpoolSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex);
static int spool_summary_sync(CamelSpoolSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex);
static CamelMessageInfo *spool_summary_add(CamelSpoolSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *, CamelException *ex);

static int spool_summary_sync_full(CamelSpoolSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex);

static void camel_spool_summary_class_init (CamelSpoolSummaryClass *klass);
static void camel_spool_summary_init       (CamelSpoolSummary *obj);
static void camel_spool_summary_finalise   (CamelObject *obj);
static CamelFolderSummaryClass *camel_spool_summary_parent;

CamelType
camel_spool_summary_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_folder_summary_get_type(), "CamelSpoolSummary",
					   sizeof (CamelSpoolSummary),
					   sizeof (CamelSpoolSummaryClass),
					   (CamelObjectClassInitFunc) camel_spool_summary_class_init,
					   NULL,
					   (CamelObjectInitFunc) camel_spool_summary_init,
					   (CamelObjectFinalizeFunc) camel_spool_summary_finalise);
	}
	
	return type;
}

static void
camel_spool_summary_class_init(CamelSpoolSummaryClass *klass)
{
	CamelFolderSummaryClass *sklass = (CamelFolderSummaryClass *) klass;
	
	camel_spool_summary_parent = CAMEL_FOLDER_SUMMARY_CLASS(camel_type_get_global_classfuncs(camel_folder_summary_get_type()));

	sklass->summary_header_load = summary_header_load;
	sklass->summary_header_save = summary_header_save;

	sklass->message_info_new  = message_info_new;
	sklass->message_info_new_from_parser  = message_info_new_from_parser;
	sklass->message_info_load = message_info_load;
	sklass->message_info_save = message_info_save;

	klass->load = spool_summary_load;
	klass->check = spool_summary_check;
	klass->sync = spool_summary_sync;
	klass->add = spool_summary_add;

	klass->encode_x_evolution = spool_summary_encode_x_evolution;
	klass->decode_x_evolution = spool_summary_decode_x_evolution;
}

static void
camel_spool_summary_init(CamelSpoolSummary *obj)
{
	struct _CamelSpoolSummaryPrivate *p;
	struct _CamelFolderSummary *s = (CamelFolderSummary *)obj;

	p = _PRIVATE(obj) = g_malloc0(sizeof(*p));

	/* subclasses need to set the right instance data sizes */
	s->message_info_size = sizeof(CamelSpoolMessageInfo);
	s->content_info_size = sizeof(CamelMessageContentInfo);

	/* and a unique file version */
	s->version += CAMEL_SPOOL_SUMMARY_VERSION;
}

static void
camel_spool_summary_finalise(CamelObject *obj)
{
	CamelSpoolSummary *mbs = CAMEL_SPOOL_SUMMARY(obj);

	g_free(mbs->folder_path);
}

CamelSpoolSummary *
camel_spool_summary_new(const char *filename)
{
	CamelSpoolSummary *new = (CamelSpoolSummary *)camel_object_new(camel_spool_summary_get_type());

	camel_folder_summary_set_build_content(CAMEL_FOLDER_SUMMARY(new), FALSE);
	new->folder_path = g_strdup(filename);

	return new;
}

static int
spool_summary_load(CamelSpoolSummary *cls, int forceindex, CamelException *ex)
{
	g_warning("spool_summary_load() should nto b e called\n");

	return camel_folder_summary_load((CamelFolderSummary *)cls);
}

/* load/check the summary */
int
camel_spool_summary_load(CamelSpoolSummary *cls, int forceindex, CamelException *ex)
{
	struct stat st;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;

	g_warning("spool_summary_load() should nto b e called\n");

	d(printf("Loading summary ...\n"));

	if (forceindex
	    || stat(s->summary_path, &st) == -1
	    || ((CamelSpoolSummaryClass *)(CAMEL_OBJECT_GET_CLASS(cls)))->load(cls, forceindex, ex) == -1) {
		camel_folder_summary_clear((CamelFolderSummary *)cls);
	}

	return camel_spool_summary_check(cls, NULL, ex);
}

char *
camel_spool_summary_encode_x_evolution(CamelSpoolSummary *cls, const CamelMessageInfo *info)
{
	return ((CamelSpoolSummaryClass *)(CAMEL_OBJECT_GET_CLASS(cls)))->encode_x_evolution(cls, info);
}

int
camel_spool_summary_decode_x_evolution(CamelSpoolSummary *cls, const char *xev, CamelMessageInfo *info)
{
	return ((CamelSpoolSummaryClass *)(CAMEL_OBJECT_GET_CLASS(cls)))->decode_x_evolution(cls, xev, info);
}

int
camel_spool_summary_check(CamelSpoolSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	int ret;

	ret = ((CamelSpoolSummaryClass *)(CAMEL_OBJECT_GET_CLASS(cls)))->check(cls, changeinfo, ex);

	return ret;
}

int
camel_spool_summary_sync(CamelSpoolSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	return ((CamelSpoolSummaryClass *)(CAMEL_OBJECT_GET_CLASS(cls)))->sync(cls, expunge, changeinfo, ex);
}

CamelMessageInfo *
camel_spool_summary_add(CamelSpoolSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *ci, CamelException *ex)
{
	return ((CamelSpoolSummaryClass *)(CAMEL_OBJECT_GET_CLASS(cls)))->add(cls, msg, info, ci, ex);
}

/**
 * camel_spool_summary_write_headers:
 * @fd: 
 * @header: 
 * @xevline: 
 * 
 * Write a bunch of headers to the file @fd.  IF xevline is non NULL, then
 * an X-Evolution header line is created at the end of all of the headers.
 * The headers written are termianted with a blank line.
 * 
 * Return value: -1 on error, otherwise the number of bytes written.
 **/
int
camel_spool_summary_write_headers(int fd, struct _header_raw *header, char *xevline)
{
	int outlen = 0, len;
	int newfd;
	FILE *out;

	/* dum de dum, maybe the whole sync function should just use stdio for output */
	newfd = dup(fd);
	if (newfd == -1)
		return -1;

	out = fdopen(newfd, "w");
	if (out == NULL) {
		close(newfd);
		errno = EINVAL;
		return -1;
	}

	while (header) {
		if (strcmp(header->name, "X-Evolution")) {
			len = fprintf(out, "%s:%s\n", header->name, header->value);
			if (len == -1) {
				fclose(out);
				return -1;
			}
			outlen += len;
		}
		header = header->next;
	}

	if (xevline) {
		len = fprintf(out, "X-Evolution: %s\n\n", xevline);
		if (len == -1) {
			fclose(out);
			return -1;
		}
		outlen += len;
	}

	if (fclose(out) == -1)
		return -1;

	return outlen;
}

static int
summary_header_load(CamelFolderSummary *s, FILE *in)
{
	CamelSpoolSummary *mbs = CAMEL_SPOOL_SUMMARY(s);

	if (((CamelFolderSummaryClass *)camel_spool_summary_parent)->summary_header_load(s, in) == -1)
		return -1;

	return camel_file_util_decode_uint32(in, &mbs->folder_size);
}

static int
summary_header_save(CamelFolderSummary *s, FILE *out)
{
	CamelSpoolSummary *mbs = CAMEL_SPOOL_SUMMARY(s);

	if (((CamelFolderSummaryClass *)camel_spool_summary_parent)->summary_header_save(s, out) == -1)
		return -1;

	return camel_file_util_encode_uint32(out, mbs->folder_size);
}

static CamelMessageInfo *
message_info_new(CamelFolderSummary *s, struct _header_raw *h)
{
	CamelMessageInfo *mi;
	CamelSpoolSummary *cls = (CamelSpoolSummary *)s;

	mi = ((CamelFolderSummaryClass *)camel_spool_summary_parent)->message_info_new(s, h);
	if (mi) {
		CamelSpoolMessageInfo *mbi = (CamelSpoolMessageInfo *)mi;
		const char *xev;

		xev = header_raw_find(&h, "X-Evolution", NULL);
		if (xev==NULL || camel_spool_summary_decode_x_evolution(cls, xev, mi) == -1) {
			/* to indicate it has no xev header */
			mi->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED | CAMEL_MESSAGE_FOLDER_NOXEV;
			camel_message_info_set_uid(mi, camel_folder_summary_next_uid_string(s));
		}

		mbi->frompos = -1;
	}
	
	return mi;
}

static CamelMessageInfo *
message_info_new_from_parser(CamelFolderSummary *s, CamelMimeParser *mp)
{
	CamelMessageInfo *mi;

	mi = ((CamelFolderSummaryClass *)camel_spool_summary_parent)->message_info_new_from_parser(s, mp);
	if (mi) {
		CamelSpoolMessageInfo *mbi = (CamelSpoolMessageInfo *)mi;

		mbi->frompos = camel_mime_parser_tell_start_from(mp);
	}
	
	return mi;
}

static CamelMessageInfo *
message_info_load(CamelFolderSummary *s, FILE *in)
{
	CamelMessageInfo *mi;

	io(printf("loading spool message info\n"));

	mi = ((CamelFolderSummaryClass *)camel_spool_summary_parent)->message_info_load(s, in);
	if (mi) {
		CamelSpoolMessageInfo *mbi = (CamelSpoolMessageInfo *)mi;
		
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
	CamelSpoolMessageInfo *mbi = (CamelSpoolMessageInfo *)mi;

	io(printf("saving spool message info\n"));

	if (((CamelFolderSummaryClass *)camel_spool_summary_parent)->message_info_save(s, out, mi) == -1
	    || camel_file_util_encode_off_t(out, mbi->frompos) == -1)
		return -1;

	return 0;
}

static int
summary_rebuild(CamelSpoolSummary *cls, off_t offset, CamelException *ex)
{
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
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
				     cls->folder_path, offset, strerror(errno));
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
			cls->folder_size = st.st_size;
			s->time = st.st_mtime;
		}
	}

	camel_operation_end(NULL);

	return ok;
}

/* like summary_rebuild, but also do changeinfo stuff (if supplied) */
static int
summary_update(CamelSpoolSummary *cls, off_t offset, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	int ret, i, count;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;

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
	ret = summary_rebuild(cls, offset, ex);

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
spool_summary_check(CamelSpoolSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
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
		d(printf("Empty spool, clearing summary\n"));
		camel_folder_summary_clear(s);
		ret = 0;
	} else if (s->messages->len == 0) {
		/* if we are empty, then we rebuilt from scratch */
		d(printf("Empty summary, rebuilding from start\n"));
		ret = summary_update(cls, 0, changeinfo, ex);
	} else {
		/* is the summary uptodate? */
		if (st.st_size != cls->folder_size || st.st_mtime != s->time) {
			if (cls->folder_size < st.st_size) {
				/* this will automatically rescan from 0 if there is a problem */
				d(printf("folder grew, attempting to rebuild from %d\n", cls->folder_size));
				ret = summary_update(cls, cls->folder_size, changeinfo, ex);
			} else {
				d(printf("folder shrank!  rebuilding from start\n"));
				camel_folder_summary_clear(s);
				ret = summary_update(cls, 0, changeinfo, ex);
			}
		}
	}

	if (ret != -1) {
		int i, work, count;

		/* check to see if we need to copy/update the file; missing xev headers prompt this */
		work = FALSE;
		count = camel_folder_summary_count(s);
		for (i=0;!work && i<count; i++) {
			CamelMessageInfo *info = camel_folder_summary_index(s, i);
			g_assert(info);
			work = (info->flags & (CAMEL_MESSAGE_FOLDER_NOXEV)) != 0;
			camel_folder_summary_info_free(s, info);
		}

		/* if we do, then write out the headers using sync_full, etc */
		if (work) {
			d(printf("Have to add new headers, re-syncing from the start to accomplish this\n"));
			ret = spool_summary_sync_full(cls, FALSE, changeinfo, ex);

			if (stat(cls->folder_path, &st) == -1) {
				camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, _("Unknown error: %s"), strerror(errno));
				return -1;
			}
		}
		cls->folder_size = st.st_size;
		s->time = st.st_mtime;
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
camel_spool_summary_build_from(struct _header_raw *header)
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
spool_summary_sync_full(CamelSpoolSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
	CamelMimeParser *mp = NULL;
	int i, count;
	CamelSpoolMessageInfo *info = NULL;
	int fd = -1, fdout = -1;
	char *tmpname = NULL;
	char *buffer, *xevnew = NULL, *p;
	int len;
	const char *fromline;
	int lastdel = FALSE;
	off_t spoollen, outlen;
	int size, sizeout;
	struct stat st;

	d(printf("performing full summary/sync\n"));

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

#ifdef HAVE_MKSTEMP
	tmpname = alloca(64);
	sprintf(tmpname, "/tmp/spool.camel.XXXXXX");
	fdout = mkstemp(tmpname);
#else
#warning "Your system has no mkstemp(3), spool updating may be insecure"
	tmpname = alloca(L_tmpnam);
	tmpnam(tmpname);
	fdout = open(tmpname, O_RDWR|O_CREAT|O_EXCL, 0600);
#endif
	d(printf("Writing tmp file to %s\n", tmpname));
	if (fdout == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot open temporary mailbox: %s"), strerror(errno));
		goto error;
	}

	count = camel_folder_summary_count(s);
	for (i = 0; i < count; i++) {
		int pc = (i + 1) * 100 / count;

		camel_operation_progress(NULL, pc);

		info = (CamelSpoolMessageInfo *)camel_folder_summary_index(s, i);

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

#if 0
			if (cls->index)
				ibex_unindex(cls->index, (char *)uid);
#endif
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

			xevnew = camel_spool_summary_encode_x_evolution(cls, (CamelMessageInfo *)info);
			if (camel_spool_summary_write_headers(fdout, camel_mime_parser_headers_raw(mp), xevnew) == -1) {
				d(printf("Error writing to tmp mailbox\n"));
				camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
						     _("Error writing to temp mailbox: %s"),
						     strerror(errno));
				goto error;
			}

			/* mark this message as recent */
			if (info->info.flags & CAMEL_MESSAGE_FOLDER_NOXEV)
				camel_folder_change_info_recent_uid(changeinfo, camel_message_info_uid(info));

			info->info.flags &= 0xffff;
			g_free(xevnew);
			xevnew = NULL;
			camel_mime_parser_drop_step(mp);
		}

		camel_mime_parser_drop_step(mp);
		if (info) {
			d(printf("looking for message content to copy across from %d\n", (int)camel_mime_parser_tell(mp)));
			while (camel_mime_parser_step(mp, &buffer, &len) == HSCAN_PRE_FROM) {
				d(printf("copying spool contents to tmp: '%.*s'\n", len, buffer));
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

	/* sync out content */
	if (fsync(fdout) == -1) {
		g_warning("Cannot sync temporary folder: %s", strerror(errno));
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not sync temporary folder %s: %s"),
				     cls->folder_path, strerror(errno));
		goto error;
	}

	/* see if we can write this much to the spool file */
	if (fstat(fd, &st) == -1) {
		g_warning("Cannot sync temporary folder: %s", strerror(errno));
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not sync temporary folder %s: %s"),
				     cls->folder_path, strerror(errno));
		goto error;
	}
	spoollen = st.st_size;

	if (fstat(fdout, &st) == -1) {
		g_warning("Cannot sync temporary folder: %s", strerror(errno));
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not sync temporary folder %s: %s"),
				     cls->folder_path, strerror(errno));
		goto error;
	}
	outlen = st.st_size;

	/* I think this is the right way to do this */
	if (outlen>0
	    && (lseek(fd, outlen-1, SEEK_SET) == -1
		|| write(fd, "", 1) != 1
		|| fsync(fd) == -1
		|| lseek(fd, 0, SEEK_SET) == -1
		|| lseek(fdout, 0, SEEK_SET) == -1)) {
		g_warning("Cannot sync spool folder: %s", strerror(errno));
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not sync spool folder %s: %s"),
				     cls->folder_path, strerror(errno));
		/* incase we ran out of room, remove any trailing space first */
		ftruncate(fd, spoollen);
		goto error;
	}


	/* now copy content back */
	buffer = g_malloc(8192);
	size = 1;
	while (size>0) {
		do {
			size = read(fdout, buffer, 8192);
		} while (size == -1 && errno == EINTR);

		if (size > 0) {
			p = buffer;
			do {
				sizeout = write(fd, p, size);
				if (sizeout > 0) {
					p+= sizeout;
					size -= sizeout;
				}
			} while ((sizeout == -1 && errno == EINTR) && size > 0);
			size = sizeout;
		}

		if (size == -1) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Could not sync spool folder %s: %s\n"
					       "Folder may be corrupt, copy saved in `%s'"),
					     cls->folder_path, strerror(errno), tmpnam);
			/* so we dont delete it */
			close(fdout);
			tmpname = NULL;
			fdout = -1;
			g_free(buffer);
			goto error;
		}
	}

	g_free(buffer);

	d(printf("Closing folders\n"));

	if (ftruncate(fd, outlen) == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not sync spool folder %s: %s\n"
				       "Folder may be corrupt, copy saved in `%s'"),
				     cls->folder_path, strerror(errno), tmpnam);
		close(fdout);
		tmpname = NULL;
		fdout = -1;
		goto error;
	}

	if (close(fd) == -1) {
		g_warning("Cannot close source folder: %s", strerror(errno));
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not sync spool folder %s: %s\n"
				       "Folder may be corrupt, copy saved in `%s'"),
				     cls->folder_path, strerror(errno), tmpnam);
		close(fdout);
		tmpname = NULL;
		fdout = -1;
		fd = -1;
		goto error;
	}

	close(fdout);
	unlink(tmpname);

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
spool_summary_sync_quick(CamelSpoolSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
	CamelMimeParser *mp = NULL;
	int i, count;
	CamelSpoolMessageInfo *info = NULL;
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
				     _("Could not file: %s: %s"),
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

		info = (CamelSpoolMessageInfo *)camel_folder_summary_index(s, i);

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
		if (xev == NULL || camel_spool_summary_decode_x_evolution(cls, xev, NULL) == -1) {
			g_warning("We're supposed to have a valid x-ev header, but we dont");
			goto error;
		}
		xevnew = camel_spool_summary_encode_x_evolution(cls, (CamelMessageInfo *)info);
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
spool_summary_sync(CamelSpoolSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	struct stat st;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;
	int i, count;
	int quick = TRUE, work=FALSE;
	int ret;

	/* first, sync ourselves up, just to make sure */
	summary_update(cls, cls->folder_size, changeinfo, ex);
	if (camel_exception_is_set(ex))
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
			ret = spool_summary_sync_quick(cls, expunge, changeinfo, ex);
			if (ret == -1) {
				g_warning("failed a quick-sync, trying a full sync");
				camel_exception_clear(ex);
			}
		} else {
			ret = 0;
		}
	}

	if (ret == -1)
		ret = spool_summary_sync_full(cls, expunge, changeinfo, ex);
	if (ret == -1)
		return -1;

	if (stat(cls->folder_path, &st) == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, _("Unknown error: %s"), strerror(errno));
		return -1;
	}

	camel_folder_summary_touch(s);
	s->time = st.st_mtime;
	cls->folder_size = st.st_size;
	/*camel_folder_summary_save(s);*/

	return 0;
}


static CamelMessageInfo *
spool_summary_add(CamelSpoolSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *ci, CamelException *ex)
{
	CamelMessageInfo *mi;
	char *xev;

	d(printf("Adding message to summary\n"));
	
	mi = camel_folder_summary_add_from_message((CamelFolderSummary *)cls, msg);
	if (mi) {
		d(printf("Added, uid = %s\n", camel_message_info_uid(mi)));
		if (info) {
			CamelTag *tag = info->user_tags;
			CamelFlag *flag = info->user_flags;

			while (flag) {
				camel_flag_set(&mi->user_flags, flag->name, TRUE);
				flag = flag->next;
			}
			
			while (tag) {
				camel_tag_set(&mi->user_tags, tag->name, tag->value);
				tag = tag->next;
			}

			mi->flags = mi->flags | (info->flags & 0xffff);
		}
		mi->flags &= ~(CAMEL_MESSAGE_FOLDER_NOXEV|CAMEL_MESSAGE_FOLDER_FLAGGED);
		xev = camel_spool_summary_encode_x_evolution(cls, mi);
		camel_medium_set_header((CamelMedium *)msg, "X-Evolution", xev);
		g_free(xev);
		camel_folder_change_info_add_uid(ci, camel_message_info_uid(mi));
	} else {
		d(printf("Failed!\n"));
		camel_exception_set(ex, 1, _("Unable to add message to summary: unknown reason"));
	}
	return mi;
}

static char *
spool_summary_encode_x_evolution(CamelSpoolSummary *cls, const CamelMessageInfo *mi)
{
	GString *out = g_string_new("");
	struct _header_param *params = NULL;
	GString *val = g_string_new("");
	CamelFlag *flag = mi->user_flags;
	CamelTag *tag = mi->user_tags;
	char *ret;
	const char *p, *uidstr;
	guint32 uid;

	/* FIXME: work out what to do with uid's that aren't stored here? */
	/* FIXME: perhaps make that a mbox folder only issue?? */
	p = uidstr = camel_message_info_uid(mi);
	while (*p && isdigit(*p))
		p++;
	if (*p == 0 && sscanf(uidstr, "%u", &uid) == 1) {
		g_string_sprintf(out, "%08x-%04x", uid, mi->flags & 0xffff);
	} else {
		g_string_sprintf(out, "%s-%04x", uidstr, mi->flags & 0xffff);
	}

	if (flag || tag) {
		val = g_string_new("");

		if (flag) {
			while (flag) {
				g_string_append(val, flag->name);
				if (flag->next)
					g_string_append_c(val, ',');
				flag = flag->next;
			}
			header_set_param(&params, "flags", val->str);
			g_string_truncate(val, 0);
		}
		if (tag) {
			while (tag) {
				g_string_append(val, tag->name);
				g_string_append_c(val, '=');
				g_string_append(val, tag->value);
				if (tag->next)
					g_string_append_c(val, ',');
				tag = tag->next;
			}
			header_set_param(&params, "tags", val->str);
		}
		g_string_free(val, TRUE);
		header_param_list_format_append(out, params);
		header_param_list_free(params);
	}
	ret = out->str;
	g_string_free(out, FALSE);
	return ret;
}

static int
spool_summary_decode_x_evolution(CamelSpoolSummary *cls, const char *xev, CamelMessageInfo *mi)
{
	struct _header_param *params, *scan;
	guint32 uid, flags;
	char *header;
	int i;

	/* check for uid/flags */
	header = header_token_decode(xev);
	if (header && strlen(header) == strlen("00000000-0000")
	    && sscanf(header, "%08x-%04x", &uid, &flags) == 2) {
		char uidstr[20];
		if (mi) {
			sprintf(uidstr, "%u", uid);
			camel_message_info_set_uid(mi, g_strdup(uidstr));
			mi->flags = flags;
		}
	} else {
		g_free(header);
		return -1;
	}
	g_free(header);

	if (mi == NULL)
		return 0;

	/* check for additional data */	
	header = strchr(xev, ';');
	if (header) {
		params = header_param_list_decode(header+1);
		scan = params;
		while (scan) {
			if (!strcasecmp(scan->name, "flags")) {
				char **flagv = g_strsplit(scan->value, ",", 1000);

				for (i=0;flagv[i];i++) {
					camel_flag_set(&mi->user_flags, flagv[i], TRUE);
				}
				g_strfreev(flagv);
			} else if (!strcasecmp(scan->name, "tags")) {
				char **tagv = g_strsplit(scan->value, ",", 10000);
				char *val;

				for (i=0;tagv[i];i++) {
					val = strchr(tagv[i], '=');
					if (val) {
						*val++ = 0;
						camel_tag_set(&mi->user_tags, tagv[i], val);
						val[-1]='=';
					}
				}
				g_strfreev(tagv);
			}
			scan = scan->next;
		}
		header_param_list_free(params);
	}
	return 0;
}

