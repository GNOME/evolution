/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 *
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

#include "camel-mbox-summary.h"
#include <camel/camel-mime-message.h>

#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define io(x)
#define d(x) (printf("%s(%d): ", __FILE__, __LINE__),(x))

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
camel_mbox_summary_new(const char *filename, const char *mbox_name, ibex *index)
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

	return camel_folder_summary_decode_uint32(in, &mbs->folder_size);
}

static int
summary_header_save(CamelFolderSummary *s, FILE *out)
{
	CamelMboxSummary *mbs = CAMEL_MBOX_SUMMARY(s);

	if (((CamelFolderSummaryClass *)camel_mbox_summary_parent)->summary_header_save(s, out) == -1)
		return -1;

	return camel_folder_summary_encode_uint32(out, mbs->folder_size);
}

static int
header_evolution_decode(const char *in, guint32 *uid, guint32 *flags)
{
        char *header;
	
        if (in && (header = header_token_decode(in))) {
                if (strlen (header) == strlen ("00000000-0000")
                    && sscanf (header, "%08x-%04x", uid, flags) == 2) {
                        g_free(header);
                        return *uid;
                }
                g_free(header);
        }

        return -1;
}

/* we still use our own version here, as we dont grok the flag stuff yet, during an expunge
   anyway */
static char *
header_evolution_encode(guint32 uid, guint32 flags)
{
	return g_strdup_printf("%08x-%04x", uid, flags & 0xffff);
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

		camel_folder_summary_decode_off_t(in, &mbi->frompos);
	}
	
	return mi;
}

static int
message_info_save(CamelFolderSummary *s, FILE *out, CamelMessageInfo *mi)
{
	CamelMboxMessageInfo *mbi = (CamelMboxMessageInfo *)mi;

	io(printf("saving mbox message info\n"));

	((CamelFolderSummaryClass *)camel_mbox_summary_parent)->message_info_save(s, out, mi);

	return camel_folder_summary_encode_off_t(out, mbi->frompos);
}

static int
summary_rebuild(CamelMboxSummary *mbs, off_t offset, CamelException *ex)
{
	CamelLocalSummary *cls = (CamelLocalSummary *)mbs;
	CamelFolderSummary *s = (CamelFolderSummary *)mbs;
	CamelMimeParser *mp;
	int fd;
	int ok = 0;

	fd = open(cls->folder_path, O_RDONLY);
	if (fd == -1) {
		printf("%s failed to open: %s", cls->folder_path, strerror(errno));
		camel_exception_setv(ex, 1, _("Could not open folder: %s: summarising from position %ld: %s"),
				     cls->folder_path, offset, strerror(errno));
		return -1;
	}
	
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
			camel_object_unref(CAMEL_OBJECT(mp));
			/* end of file - no content? */
			return -1;
		}
	}

	while (camel_mime_parser_step(mp, NULL, NULL) == HSCAN_FROM) {
		CamelMessageInfo *info;

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
		struct stat st;

		if (stat(cls->folder_path, &st) == 0) {
			mbs->folder_size = st.st_size;
			s->time = st.st_mtime;
		}
	}

	return ok;
}

/* like summary_rebuild, but also do changeinfo stuff (if supplied) */
static int
summary_update(CamelMboxSummary *mbs, off_t offset, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	int ret, i, count;
	CamelFolderSummary *s = (CamelFolderSummary *)mbs;
	CamelLocalSummary *cls = (CamelLocalSummary *)mbs;

	if (changeinfo) {
		/* we use the diff function of the change_info to build the update list. */
		for (i = 0; i < camel_folder_summary_count(s); i++) {
			CamelMessageInfo *mi = camel_folder_summary_index(s, i);

			camel_folder_change_info_add_source(changeinfo, mi->uid);
		}
	}

	/* do the actual work */
	cls->index_force = FALSE;
	ret = summary_rebuild(mbs, offset, ex);

	if (changeinfo) {
		count = camel_folder_summary_count(s);
		for (i = 0; i < count; i++) {
			CamelMessageInfo *mi = camel_folder_summary_index(s, i);
			camel_folder_change_info_add_update(changeinfo, mi->uid);
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
		camel_exception_setv(ex, 1, _("Cannot summarise folder: %s: %s"), cls->folder_path, strerror(errno));
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
		ret = summary_update(mbs, 0, changes, ex);
	} else {
		/* is the summary uptodate? */
		if (st.st_size != mbs->folder_size || st.st_mtime != s->time) {
			if (mbs->folder_size < st.st_size) {
				/* this will automatically rescan from 0 if there is a problem */
				d(printf("folder grew, attempting to rebuild from %d\n", mbs->folder_size));
				ret = summary_update(mbs, mbs->folder_size, changes, ex);
			} else {
				d(printf("folder shrank!  rebuilding from start\n"));
				camel_folder_summary_clear(s);
				ret = summary_update(mbs, 0, changes, ex);
			}
		}
	}

	/* FIXME: move upstream? */

	if (ret != -1) {
		mbs->folder_size = st.st_size;
		s->time = st.st_mtime;
#if 0
		/* this failing is not a fatal event */
		if (camel_folder_summary_save(s) == -1)
			g_warning("Could not save summary: %s", strerror(errno));
		if (cls->index)
			ibex_save(cls->index);
#endif
	}

	return ret;
}

static int
header_write(int fd, struct _header_raw *header, char *xevline)
{
	struct iovec iv[4];
	int outlen = 0, len;

	iv[1].iov_base = ":";
	iv[1].iov_len = 1;
	iv[3].iov_base = "\n";
	iv[3].iov_len = 1;

	while (header) {
		if (strcasecmp(header->name, "X-Evolution")) {
			iv[0].iov_base = header->name;
			iv[0].iov_len = strlen(header->name);
			iv[2].iov_base = header->value;
			iv[2].iov_len = strlen(header->value);

			do {
				len = writev(fd, iv, 4);
			} while (len == -1 && errno == EINTR);

			if (len == -1)
				return -1;
			outlen += len;
		}
		header = header->next;
	}

	iv[0].iov_base = "X-Evolution: ";
	iv[0].iov_len = strlen(iv[0].iov_base);
	iv[1].iov_base = xevline;
	iv[1].iov_len = strlen(xevline);
	iv[2].iov_base = "\n\n";
	iv[2].iov_len = 2;

	do {
		len = writev(fd, iv, 3);
	} while (len == -1 && errno == EINTR);

	if (len == -1)
		return -1;

	outlen += 1;

	d(printf("Wrote %d bytes of headers\n", outlen));

	return outlen;
}

static int
copy_block(int fromfd, int tofd, off_t start, size_t bytes)
{
	char buffer[4096];
	int written = 0;

	d(printf("writing %d bytes ... \n", bytes));

	if (lseek(fromfd, start, SEEK_SET) != start)
		return -1;

	while (bytes > 0) {
		int toread, towrite;

		toread = bytes;
		if (bytes > 4096)
			toread = 4096;
		else
			toread = bytes;
		do {
			towrite = read(fromfd, buffer, toread);
		} while (towrite == -1 && errno == EINTR);

		if (towrite == -1)
			return -1;

		/* check for 'end of file' */
		if (towrite == 0) {
			d(printf("end of file?\n"));
			break;
		}

		do {
			toread = write(tofd, buffer, towrite);
		} while (toread == -1 && errno == EINTR);

		if (toread == -1)
			return -1;

		written += toread;
		bytes -= toread;
	}

	d(printf("written %d bytes\n", written));

	return written;
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

	g_string_sprintfa(out, " %s %s %d %02d:%02d:%02d %4d\n",
			  tz_days[tm.tm_wday],
			  tz_months[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year + 1900);

	ret = out->str;
	g_string_free(out, FALSE);
	return ret;
}

static int
mbox_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	CamelMboxSummary *mbs = (CamelMboxSummary *)cls;
	CamelFolderSummary *s = (CamelFolderSummary *)mbs;
	CamelMimeParser *mp = NULL;
	int i, count;
	CamelMboxMessageInfo *info;
	int fd = -1, fdout = -1;
	off_t offset = 0;
	char *tmpname = NULL;
	char *buffer, *xevnew = NULL;
	const char *xev;
	int len;
	guint32 uid, flags;
	int quick = TRUE, work = FALSE;
	struct stat st;
	char *fromline;

	/* make sure we're in sync, after this point we at least have a complete list of id's */
	count = camel_folder_summary_count (s);
	if (count > 0) {
		CamelMessageInfo *mi = camel_folder_summary_index(s, count - 1);
		summary_update(mbs, mi->content->endpos, changeinfo, ex);
	} else {
		summary_update(mbs, 0, changeinfo, ex);
	}

	if (camel_exception_is_set(ex))
		return -1;

	/* FIXME: This needs to take the user flags and tags fields into account */

	/* check if we have any work to do */
	d(printf("Performing sync, %d messages in inbox\n", count));
	for (i = 0; quick && i < count; i++) {
		info = (CamelMboxMessageInfo *)camel_folder_summary_index(s, i);
		if ((expunge && (info->info.flags & CAMEL_MESSAGE_DELETED)) ||
		    (info->info.flags & CAMEL_MESSAGE_FOLDER_NOXEV))
			quick = FALSE;
		else
			work |= (info->info.flags & CAMEL_MESSAGE_FOLDER_FLAGGED) != 0;
	}

	d(printf("Options: %s %s %s\n", expunge ? "expunge" : "", quick ? "quick" : "", work ? "Work" : ""));

	if (quick && !work)
		return 0;

	fd = open(cls->folder_path, O_RDWR);
	if (fd == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not open folder to summarise: %s: %s"),
				     cls->folder_path, strerror(errno));
		return -1;
	}

	mp = camel_mime_parser_new();
	camel_mime_parser_scan_from(mp, TRUE);
	camel_mime_parser_init_with_fd(mp, fd);

	if (!quick) {
		tmpname = alloca(strlen (cls->folder_path) + 5);
		sprintf(tmpname, "%s.tmp", cls->folder_path);
		d(printf("Writing tmp file to %s\n", tmpname));
	retry_out:
		fdout = open(tmpname, O_WRONLY|O_CREAT|O_EXCL, 0600);
		if (fdout == -1) {
			if (errno == EEXIST)
				if (unlink(tmpname) != -1)
					goto retry_out;
			
			tmpname = NULL;
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Cannot open temporary mailbox: %s"), strerror(errno));
			goto error;
		}
	}

	for (i = 0; i < count; i++) {
		off_t frompos, bodypos, lastpos;
		/* This has to be an int, not an off_t, because that's
		 * what camel_mime_parser_header returns... FIXME.
		 */
		int xevoffset;

		info = (CamelMboxMessageInfo *)camel_folder_summary_index(s, i);

		g_assert(info);

		d(printf("Looking at message %s\n", info->info.uid));

		if (expunge && info->info.flags & CAMEL_MESSAGE_DELETED) {
			d(printf("Deleting %s\n", info->info.uid));

			g_assert(!quick);
			offset -= (info->info.content->endpos - info->frompos);

			/* FIXME: put this in folder_summary::remove()? */
			if (cls->index)
				ibex_unindex(cls->index, info->info.uid);

			/* remove it from the change list */
			camel_folder_change_info_remove_uid(changeinfo, info->info.uid);
			camel_folder_summary_remove(s, (CamelMessageInfo *)info);
			count--;
			i--;
			info = NULL;
		} else if (info->info.flags & (CAMEL_MESSAGE_FOLDER_NOXEV | CAMEL_MESSAGE_FOLDER_FLAGGED)) {
			int xevok = FALSE;

			d(printf("Updating header for %s flags = %08x\n", info->info.uid, info->info.flags));

			/* find the next message, header parts */
			camel_mime_parser_seek(mp, info->frompos, SEEK_SET);
			if (camel_mime_parser_step(mp, &buffer, &len) != HSCAN_FROM) {
				g_warning("camel_mime_parser_step failed (1)");
				goto error;
			}

			if (camel_mime_parser_tell_start_from (mp) != info->frompos) {
				g_warning("Summary/mbox mismatch, aborting sync");
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      _("Summary mismatch, aborting sync"));
				goto error;
			}
			
			if (camel_mime_parser_step (mp, &buffer, &len) == HSCAN_FROM_END) {
				g_warning("camel_mime_parser_step failed (2)");
				goto error;
			}

			/* Check if the X-Evolution header is valid.  */

			/* FIXME: Use camel_local_summary versions here */

			xev = camel_mime_parser_header(mp, "X-Evolution", &xevoffset);
			if (xev && header_evolution_decode (xev, &uid, &flags) != -1)
				xevok = TRUE;

			xevnew = header_evolution_encode(strtoul (info->info.uid, NULL, 10), info->info.flags & 0xffff);
			if (quick) {
				if (!xevok) {
					g_warning("The summary told me I had an X-Evolution header, but i dont!");
					camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
							     _("Summary mismatch, X-Evolution header missing"));
					goto error;
				}
				buffer = g_strdup_printf("X-Evolution: %s", xevnew);
				lastpos = lseek(fd, 0, SEEK_CUR);
				lseek(fd, xevoffset, SEEK_SET);
				do {
					len = write(fd, buffer, strlen (buffer));
				} while (len == -1 && errno == EINTR);
				lseek(fd, lastpos, SEEK_SET);
				g_free(buffer);
				if (len == -1) {
					goto error;
				}
			} else {
				frompos = lseek(fdout, 0, SEEK_CUR);
				fromline = camel_mbox_summary_build_from(camel_mime_parser_headers_raw (mp));
				write(fdout, fromline, strlen(fromline));
				g_free(fromline);
				if (header_write(fdout, camel_mime_parser_headers_raw(mp), xevnew) == -1) {
					d(printf("Error writing to tmp mailbox\n"));
					camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
							     _("Error writing to temp mailbox: %s"),
							     strerror(errno));
					goto error;
				}
				bodypos = lseek(fdout, 0, SEEK_CUR);
				d(printf("pos = %d, endpos = %d, bodypos = %d\n",
					 (int) info->info.content->pos,
					 (int) info->info.content->endpos,
					 (int) info->info.content->bodypos));
				if (copy_block(fd, fdout, info->info.content->bodypos,
					       info->info.content->endpos - info->info.content->bodypos) == -1) {
					g_warning("Cannot copy data to output fd");
					camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
							     _("Cannot copy data to output file: %s"),
							     strerror (errno));
					goto error;
				}
				info->frompos = frompos;
				offset = bodypos - info->info.content->bodypos;
			}
			info->info.flags &= 0xffff;
			g_free(xevnew);
			xevnew = NULL;
			camel_mime_parser_drop_step(mp);
			camel_mime_parser_drop_step(mp);
		} else {
			if (!quick) {
				if (copy_block(fd, fdout, info->frompos,
					       info->info.content->endpos - info->frompos) == -1) {
					g_warning("Cannot copy data to output fd");
					camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
							     _("Cannot copy data to output file: %s"),
							     strerror(errno));
					goto error;
				}
				/* update from pos here? */
				info->frompos += offset;
			} else {
				d(printf("Nothing to do for this message\n"));
			}
		}
		if (!quick && info != NULL && offset != 0) {
			d(printf("offsetting content: %d\n", (int)offset));
			camel_folder_summary_offset_content(info->info.content, offset);
			d(printf("pos = %d, endpos = %d, bodypos = %d\n",
				 (int) info->info.content->pos,
				 (int) info->info.content->endpos,
				 (int) info->info.content->bodypos));
		}
	}

	d(printf("Closing folders\n"));

	if (close(fd) == -1) {
		g_warning("Cannot close source folder: %s", strerror(errno));
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not close source folder %s: %s"),
				     cls->folder_path, strerror(errno));
		goto error;
	}

	if (!quick) {
		if (close(fdout) == -1) {
			g_warning("Cannot close tmp folder: %s", strerror(errno));
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Could not close temp folder: %s"),
					     strerror(errno));
			goto error;
		}

		if (rename(tmpname, cls->folder_path) == -1) {
			g_warning("Cannot rename folder: %s", strerror(errno));
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Could not rename folder: %s"),
					     strerror(errno));
			goto error;
		}
		tmpname = NULL;

		/* TODO: move up? */
		if (cls->index)
			ibex_save(cls->index);
	}

	if (stat(cls->folder_path, &st) == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Unknown error: %s"),
				     strerror(errno));
		goto error;
	}

	camel_folder_summary_touch(s);
	s->time = st.st_mtime;
	mbs->folder_size = st.st_size;
	camel_folder_summary_save(s);

	camel_object_unref(CAMEL_OBJECT(mp));
	
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
		camel_object_unref(CAMEL_OBJECT(mp));

	return -1;
}

	
	
	
	
