/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
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

#include "camel-mbox-summary.h"
#include <camel/camel-mime-message.h>

#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define io(x)
#define d(x) (x)

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

static void camel_mbox_summary_class_init (CamelMboxSummaryClass *klass);
static void camel_mbox_summary_init       (CamelMboxSummary *obj);
static void camel_mbox_summary_finalise   (GtkObject *obj);

static CamelFolderSummaryClass *camel_mbox_summary_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
camel_mbox_summary_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelMboxSummary",
			sizeof (CamelMboxSummary),
			sizeof (CamelMboxSummaryClass),
			(GtkClassInitFunc) camel_mbox_summary_class_init,
			(GtkObjectInitFunc) camel_mbox_summary_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (camel_folder_summary_get_type (), &type_info);
	}
	
	return type;
}

static void
camel_mbox_summary_class_init (CamelMboxSummaryClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	CamelFolderSummaryClass *sklass = (CamelFolderSummaryClass *) klass;
	
	camel_mbox_summary_parent = gtk_type_class (camel_folder_summary_get_type ());

	object_class->finalize = camel_mbox_summary_finalise;

	sklass->summary_header_load = summary_header_load;
	sklass->summary_header_save = summary_header_save;

	sklass->message_info_new  = message_info_new;
	sklass->message_info_new_from_parser = message_info_new_from_parser;
	sklass->message_info_load = message_info_load;
	sklass->message_info_save = message_info_save;
	/*sklass->message_info_free = message_info_free;*/

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
camel_mbox_summary_init (CamelMboxSummary *obj)
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
camel_mbox_summary_finalise (GtkObject *obj)
{
	CamelMboxSummary *mbs = CAMEL_MBOX_SUMMARY (obj);

	g_free (mbs->folder_path);

	((GtkObjectClass *)(camel_mbox_summary_parent))->finalize(GTK_OBJECT (obj));
}

/**
 * camel_mbox_summary_new:
 *
 * Create a new CamelMboxSummary object.
 * 
 * Return value: A new CamelMboxSummary widget.
 **/
CamelMboxSummary *
camel_mbox_summary_new (const char *filename, const char *mbox_name, ibex *index)
{
	CamelMboxSummary *new = CAMEL_MBOX_SUMMARY (gtk_type_new (camel_mbox_summary_get_type ()));
	
	if (new) {
		/* ?? */
		camel_folder_summary_set_build_content (CAMEL_FOLDER_SUMMARY (new), TRUE);
		camel_folder_summary_set_filename (CAMEL_FOLDER_SUMMARY (new), filename);
		new->folder_path = g_strdup (mbox_name);
		new->index = index;
	}
	return new;
}

static int
summary_header_load (CamelFolderSummary *s, FILE *in)
{
	CamelMboxSummary *mbs = CAMEL_MBOX_SUMMARY (s);

	if (((CamelFolderSummaryClass *)camel_mbox_summary_parent)->summary_header_load (s, in) == -1)
		return -1;

	return camel_folder_summary_decode_uint32 (in, &mbs->folder_size);
}

static int
summary_header_save (CamelFolderSummary *s, FILE *out)
{
	CamelMboxSummary *mbs = CAMEL_MBOX_SUMMARY (s);

	if (((CamelFolderSummaryClass *)camel_mbox_summary_parent)->summary_header_save (s, out) == -1)
		return -1;

	return camel_folder_summary_encode_uint32 (out, mbs->folder_size);
}

static int
header_evolution_decode (const char *in, guint32 *uid, guint32 *flags)
{
        char *header;
	
        if (in && (header = header_token_decode(in))) {
                if (strlen (header) == strlen ("00000000-0000")
                    && sscanf (header, "%08x-%04x", uid, flags) == 2) {
                        g_free (header);
                        return *uid;
                }
                g_free (header);
        }

        return -1;
}

static char *
header_evolution_encode (guint32 uid, guint32 flags)
{
	return g_strdup_printf ("%08x-%04x", uid, flags & 0xffff);
}

static CamelMessageInfo *
message_info_new (CamelFolderSummary *s, struct _header_raw *h)
{
	CamelMessageInfo *mi;

	mi = ((CamelFolderSummaryClass *)camel_mbox_summary_parent)->message_info_new (s, h);
	if (mi) {
		const char *xev;
		guint32 uid, flags;
		CamelMboxMessageInfo *mbi = (CamelMboxMessageInfo *)mi;

		xev = header_raw_find (&h, "X-Evolution", NULL);
		if (xev && header_evolution_decode(xev, &uid, &flags) != -1) {
			g_free (mi->uid);
			mi->uid = g_strdup_printf ("%u", uid);
			mi->flags = flags;
		} else {
			/* to indicate it has no xev header? */
			mi->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED | CAMEL_MESSAGE_FOLDER_NOXEV;
			mi->uid = g_strdup_printf ("%u", camel_folder_summary_next_uid (s));
		}
		mbi->frompos = -1;
	}
	
	return mi;
}

static CamelMessageInfo *
message_info_new_from_parser (CamelFolderSummary *s, CamelMimeParser *mp)
{
	CamelMessageInfo *mi;
	CamelMboxSummary *mbs = CAMEL_MBOX_SUMMARY (s);

	mi = ((CamelFolderSummaryClass *)camel_mbox_summary_parent)->message_info_new_from_parser (s, mp);
	if (mi) {
		CamelMboxMessageInfo *mbi = (CamelMboxMessageInfo *)mi;

		mbi->frompos = camel_mime_parser_tell_start_from (mp);

		/* do we want to index this message as we add it, as well? */
		if (mbs->index_force
		    || (mi->flags & CAMEL_MESSAGE_FOLDER_FLAGGED) != 0
		    || !ibex_contains_name(mbs->index, mi->uid)) {
			
			camel_folder_summary_set_index (s, mbs->index);
		} else {
			camel_folder_summary_set_index (s, NULL);
		}
	}
	
	return mi;
}

static CamelMessageInfo *
message_info_load (CamelFolderSummary *s, FILE *in)
{
	CamelMessageInfo *mi;

	io (printf ("loading mbox message info\n"));

	mi = ((CamelFolderSummaryClass *)camel_mbox_summary_parent)->message_info_load (s, in);
	if (mi) {
		CamelMboxMessageInfo *mbi = (CamelMboxMessageInfo *)mi;

		camel_folder_summary_decode_uint32 (in, &mbi->frompos);
	}
	
	return mi;
}

static int
message_info_save (CamelFolderSummary *s, FILE *out, CamelMessageInfo *mi)
{
	CamelMboxMessageInfo *mbi = (CamelMboxMessageInfo *)mi;

	io (printf ("saving mbox message info\n"));

	((CamelFolderSummaryClass *)camel_mbox_summary_parent)->message_info_save (s, out, mi);

	return camel_folder_summary_encode_uint32 (out, mbi->frompos);
}

static int
summary_rebuild (CamelMboxSummary *mbs, off_t offset)
{
	CamelFolderSummary *s = CAMEL_FOLDER_SUMMARY (mbs);
	CamelMimeParser *mp;
	int fd;
	int ok = 0;

	printf ("(re)Building summary from %d (%s)\n", (int)offset, mbs->folder_path);

	fd = open (mbs->folder_path, O_RDONLY);
	if (fd == -1) {
		printf ("%s failed to open: %s", mbs->folder_path, strerror (errno));
		return -1;
	}
	
	mp = camel_mime_parser_new ();
	camel_mime_parser_init_with_fd (mp, fd);
	camel_mime_parser_scan_from (mp, TRUE);
	camel_mime_parser_seek (mp, offset, SEEK_SET);

	if (offset > 0) {
		if (camel_mime_parser_step (mp, NULL, NULL) == HSCAN_FROM) {
			if (camel_mime_parser_tell_start_from (mp) != offset) {
				g_warning ("The next message didn't start where I expected\nbuilding summary from start");
				camel_mime_parser_drop_step (mp);
				offset = 0;
				camel_mime_parser_seek (mp, offset, SEEK_SET);
				camel_folder_summary_clear (CAMEL_FOLDER_SUMMARY (mbs));
			} else {
				camel_mime_parser_unstep (mp);
			}
		} else {
			gtk_object_unref (GTK_OBJECT (mp));
			/* end of file - no content? */
			printf("We ran out of file?\n");
			return -1;
		}
	}

	while (camel_mime_parser_step (mp, NULL, NULL) == HSCAN_FROM) {
		CamelMessageInfo *info;

		info = camel_folder_summary_add_from_parser (CAMEL_FOLDER_SUMMARY (mbs), mp);
		if (info == NULL) {
			printf ("Could not build info from file?\n");
			ok = -1;
			break;
		}

		g_assert (camel_mime_parser_step (mp, NULL, NULL) == HSCAN_FROM_END);
	}

	gtk_object_unref (GTK_OBJECT (mp));
	
	/* update the file size/mtime in the summary */
	if (ok != -1) {
		struct stat st;

		if (stat (mbs->folder_path, &st) == 0) {
			mbs->folder_size = st.st_size;
			s->time = st.st_mtime;
		}
	}

	return ok;
}

int
camel_mbox_summary_update (CamelMboxSummary *mbs, off_t offset)
{
	int ret;

	mbs->index_force = FALSE;
	ret = summary_rebuild (mbs, offset);

#if 0
#warning "Saving full summary and index after every summarisation is slow ..."
	if (ret != -1) {
		if (camel_folder_summary_save((CamelFolderSummary *)mbs) == -1)
			g_warning("Could not save summary: %s", strerror(errno));
		printf("summary saved\n");
		if (mbs->index)
			ibex_save(mbs->index);
		printf("ibex saved\n");
	}
#endif
	return ret;
}

int
camel_mbox_summary_load (CamelMboxSummary *mbs, int forceindex)
{
	CamelFolderSummary *s = CAMEL_FOLDER_SUMMARY (mbs);
	struct stat st;
	int ret = 0;
	off_t minstart;

	mbs->index_force = forceindex;

	/* is the summary out of date? */
	if (stat (mbs->folder_path, &st) == -1) {
		camel_folder_summary_clear (s);
		printf ("Cannot summarise folder: '%s': %s\n", mbs->folder_path, strerror(errno));
		return -1;
	}

	if (forceindex || camel_folder_summary_load (s) == -1) {
		printf ("REBUILDING SUMMARY: %s\n",
			forceindex ? "Summary non-existent." : "Summary load failed.");
		camel_folder_summary_clear (s);
		ret = summary_rebuild (mbs, 0);
	} else {
		minstart = st.st_size;
#if 0
		/* find out the first unindexed message ... */
		/* TODO: For this to work, it has to check that the message is
		   indexable, and contains content ... maybe it cannot be done 
		   properly? */
		for (i=0;i<camel_folder_summary_count(s);i++) {
			CamelMessageInfo *mi = camel_folder_summary_index(s, i);
			if (!ibex_contains_name(mbs->index, mi->uid)) {
				minstart = ((CamelMboxMessageInfo *)mi)->frompos;
				printf("Found unindexed message: %s\n", mi->uid);
				break;
			}
		}
#endif
		/* is the summary uptodate? */
		if (st.st_size == mbs->folder_size && st.st_mtime == s->time) {
			printf ("Summary time and date match mbox\n");
			if (minstart < st.st_size) {
				/* FIXME: Only clear the messages and reindex from this point forward */
				printf ("REBUILDING SUMMARY: Index file is incomplete.\n");
				camel_folder_summary_clear (s);
				ret = summary_rebuild (mbs, 0);
			}
		} else {
			if (mbs->folder_size < st.st_size) {
				printf ("REBUILDING SUMMARY: Summary is for a smaller mbox\n");
				if (minstart < mbs->folder_size) {
					/* FIXME: only make it rebuild as necessary */
					camel_folder_summary_clear (s);
					ret = summary_rebuild (mbs, 0);
				} else {
					ret = summary_rebuild (mbs, mbs->folder_size);
				}
			} else {
				if (mbs->folder_size > st.st_size)
					printf ("REBUILDING_SUMMARY: Summary is for a bigger mbox\n");
				else
					printf ("REBUILDING SUMMARY: Summary is for an older mbox\n");
				camel_folder_summary_clear (s);
				ret = summary_rebuild (mbs, 0);
			}
		}
	}

	if (ret != -1) {
		mbs->folder_size = st.st_size;
		s->time = st.st_mtime;
		printf ("saving summary\n");
		if (camel_folder_summary_save (s) == -1)
			g_warning("Could not save summary: %s", strerror (errno));
		printf ("summary saved\n");
		if (mbs->index)
			ibex_save (mbs->index);
		printf ("ibex saved\n");
	}

	return ret;
}

static int
header_write (int fd, struct _header_raw *header, char *xevline)
{
        struct iovec iv[4];
        int outlen = 0, len;

        iv[1].iov_base = ":";
        iv[1].iov_len = 1;
        iv[3].iov_base = "\n";
        iv[3].iov_len = 1;

        while (header) {
		if (strcasecmp (header->name, "X-Evolution")) {
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
        iv[0].iov_len = strlen (iv[0].iov_base);
        iv[1].iov_base = xevline;
        iv[1].iov_len = strlen (xevline);
        iv[2].iov_base = "\n\n";
        iv[2].iov_len = 2;

	do {
		len = writev (fd, iv, 3);
	} while (len == -1 && errno == EINTR);

	if (len == -1)
		return -1;

	outlen += 1;

	d(printf ("Wrote %d bytes of headers\n", outlen));

        return outlen;
}

static int
copy_block(int fromfd, int tofd, off_t start, size_t bytes)
{
        char buffer[4096];
        int written = 0;

	d(printf ("writing %d bytes ... ", bytes));

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
			towrite = read (fromfd, buffer, toread);
		} while (towrite == -1 && errno == EINTR);

		if (towrite == -1)
			return -1;

                /* check for 'end of file' */
                if (towrite == 0) {
			d(printf ("end of file?\n"));
                        break;
		}

		do {
			toread = write (tofd, buffer, towrite);
		} while (toread == -1 && errno == EINTR);

		if (toread == -1)
			return -1;

                written += toread;
                bytes -= toread;
        }

        d(printf ("written %d bytes\n", written));

        return written;
}

int
camel_mbox_summary_sync (CamelMboxSummary *mbs, gboolean expunge, CamelException *ex)
{
	CamelMimeParser *mp = NULL;
	int i, count;
	CamelMboxMessageInfo *info;
	CamelFolderSummary *s = CAMEL_FOLDER_SUMMARY (mbs);
	int fd = -1, fdout = -1;
	off_t offset = 0;
	char *tmpname = NULL;
	char *buffer, *xevnew = NULL;
	const char *xev;
	int len;
	guint32 uid, flags;
	int quick = TRUE, work = FALSE;
	struct stat st;

	/* make sure we're in sync */
	count = camel_folder_summary_count (s);
	if (count > 0) {
		CamelMessageInfo *mi = camel_folder_summary_index (s, count - 1);
		camel_mbox_summary_update (mbs, mi->content->endpos);
	} else {
		camel_mbox_summary_update (mbs, 0);
	}

	/* check if we have any work to do */
	d(printf ("Performing sync, %d messages in inbox\n", count));
	for (i = 0; quick && i < count; i++) {
		info = (CamelMboxMessageInfo *)camel_folder_summary_index (s, i);
		if ((expunge && (info->info.flags & CAMEL_MESSAGE_DELETED)) ||
		    (info->info.flags & CAMEL_MESSAGE_FOLDER_NOXEV))
			quick = FALSE;
		else
			work |= (info->info.flags & CAMEL_MESSAGE_FOLDER_FLAGGED) != 0;
	}

	d(printf ("Options: %s %s %s\n", expunge ? "expunge" : "", quick ? "quick" : "", work ? "Work" : ""));

	if (quick && !work)
		return 0;

	fd = open (mbs->folder_path, O_RDWR);
	if (fd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not open summary %s", mbs->folder_path);
		return -1;
	}

	mp = camel_mime_parser_new ();
	camel_mime_parser_scan_from (mp, TRUE);
	camel_mime_parser_init_with_fd (mp, fd);

	if (!quick) {
		tmpname = alloca (strlen (mbs->folder_path) + 5);
		sprintf (tmpname, "%s.tmp", mbs->folder_path);
		d(printf ("Writing tmp file to %s\n", tmpname));
	retry_out:
		fdout = open (tmpname, O_WRONLY | O_CREAT | O_EXCL, 0600);
		if (fdout == -1) {
			if (errno == EEXIST)
				if (unlink(tmpname) != -1)
					goto retry_out;
			
			free (tmpname);
			tmpname = NULL;
			g_warning ("Something failed (yo!)");
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Cannot open temporary mailbox: %s", strerror (errno));
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

		g_assert (info);

		d(printf ("Looking at message %s\n", info->info.uid));

		if (expunge && info->info.flags & CAMEL_MESSAGE_DELETED) {
			d(printf ("Deleting %s\n", info->info.uid));

			g_assert (!quick);
			offset -= (info->info.content->endpos - info->frompos);
			if (mbs->index)
				ibex_unindex (mbs->index, info->info.uid);
			camel_folder_summary_remove (s, (CamelMessageInfo *)info);
			count--;
			i--;
			info = NULL;
		} else if (info->info.flags & (CAMEL_MESSAGE_FOLDER_NOXEV | CAMEL_MESSAGE_FOLDER_FLAGGED)) {
			int xevok = FALSE;

			d(printf ("Updating header for %s flags = %08x\n", info->info.uid, info->info.flags));

			/* find the next message, header parts */
			camel_mime_parser_seek (mp, info->frompos, SEEK_SET);
			if (camel_mime_parser_step (mp, &buffer, &len) != HSCAN_FROM) {
				g_warning ("camel_mime_parser_step failed (1)");
				goto error;
			}

			if (camel_mime_parser_tell_start_from (mp) != info->frompos) {
				g_warning ("Summary/mbox mismatch, aborting sync");
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      "Summary mismatch, aborting sync");
				goto error;
			}
			
			if (camel_mime_parser_step (mp, &buffer, &len) == HSCAN_FROM_END) {
				g_warning ("camel_mime_parser_step failed (2)");
				goto error;
			}

			xev = camel_mime_parser_header (mp, "X-Evolution", &xevoffset);
			if (xev && header_evolution_decode (xev, &uid, &flags) != -1) {
				char name[64];

				sprintf (name, "%u", uid);
				if (strcmp (name, info->info.uid)) {
					d(printf ("Summary mismatch, aborting leaving mailbox intact\n"));
					camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
							      "Summary mismatch, aborting leaving mailbox intact");
					goto error;
				}
				xevok = TRUE;
			}
			xevnew = header_evolution_encode (strtoul (info->info.uid, NULL, 10), info->info.flags & 0xffff);
			if (quick) {
				if (!xevok) {
					g_warning ("The summary told me I had an X-Evolution header, but i dont!");
					camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
							      "Summary mismatch, X-Evolution header missing");
					goto error;
				}
				buffer = g_strdup_printf ("X-Evolution: %s", xevnew);
				lastpos = lseek (fd, 0, SEEK_CUR);
				lseek (fd, xevoffset, SEEK_SET);
				do {
					len = write (fd, buffer, strlen (buffer));
				} while (len == -1 && errno == EINTR);
				lseek (fd, lastpos, SEEK_SET);
				g_free (buffer);
				if (len == -1) {
					g_warning ("Yahoo!  len == -1");
					goto error;
				}
			} else {
				frompos = lseek (fdout, 0, SEEK_CUR);
				write (fdout, "From -\n", strlen("From -\n"));
				if (header_write (fdout, camel_mime_parser_headers_raw (mp), xevnew) == -1) {
					d(printf ("Error writing to tmp mailbox\n"));
					camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
							      "Error writing to temp mailbox: %s",
							      strerror (errno));
					goto error;
				}
				bodypos = lseek (fdout, 0, SEEK_CUR);
				d(printf ("pos = %d, endpos = %d, bodypos = %d\n",
					  (int) info->info.content->pos,
					  (int) info->info.content->endpos,
					  (int) info->info.content->bodypos));
				if (copy_block (fd, fdout, info->info.content->bodypos,
						info->info.content->endpos - info->info.content->bodypos) == -1) {
					g_warning ("Cannot copy data to output fd");
					camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
							      "Cannot copy data to output fd: %s",
							      strerror (errno));
					goto error;
				}
				info->frompos = frompos;
				offset = bodypos - info->info.content->bodypos;
			}
			info->info.flags &= 0xffff;
			g_free (xevnew);
			xevnew = NULL;
			camel_mime_parser_drop_step (mp);
			camel_mime_parser_drop_step (mp);
		} else {
			if (!quick) {
				if (copy_block (fd, fdout, info->frompos,
						info->info.content->endpos - info->frompos) == -1) {
					g_warning ("Cannot copy data to output fd");
					camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
							      "Cannot copy data to output fd: %s",
							      strerror (errno));
					goto error;
				}
				/* update from pos here? */
				info->frompos += offset;
			} else {
				d(printf ("Nothing to do for this message\n"));
			}
		}
		if (!quick && info != NULL && offset != 0) {
			d(printf ("offsetting content: %d\n", (int) offset));
			camel_folder_summary_offset_content (info->info.content, offset);
			d(printf ("pos = %d, endpos = %d, bodypos = %d\n",
				  (int) info->info.content->pos,
				  (int) info->info.content->endpos,
				  (int) info->info.content->bodypos));
		}
	}

	d(printf ("Closing folders\n"));

	if (close (fd) == -1) {
		g_warning ("Cannot close source folder: %s", strerror (errno));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not close source folder %s: %s",
				      mbs->folder_path, strerror (errno));
		goto error;
	}

	if (!quick) {
		if (close (fdout) == -1) {
			g_warning ("Cannot close tmp folder: %s", strerror (errno));
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Could not close temp folder: %s",
					      strerror (errno));
			goto error;
		}

		if (rename (tmpname, mbs->folder_path) == -1) {
			g_warning ("Cannot rename folder: %s", strerror (errno));
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Could not rename folder: %s",
					      strerror (errno));
			goto error;
		}
		tmpname = NULL;

		if (mbs->index)
			ibex_save (mbs->index);
	}

	if (stat (mbs->folder_path, &st) == -1) {
		g_warning ("Hmm...  stat(mbs->folder_path, &st) == -1");
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Unknown error: %s",
				      strerror (errno));
		goto error;
	}

	camel_folder_summary_touch (s);
	s->time = st.st_mtime;
	mbs->folder_size = st.st_size;
	camel_folder_summary_save (s);

	gtk_object_unref (GTK_OBJECT (mp));
	
	return 0;
 error:
	if (fd != -1)
		close (fd);
	
	if (fdout != -1)
		close (fdout);
	
	g_free (xevnew);
	
	if (tmpname)
		unlink (tmpname);
	if (mp)
		gtk_object_unref (GTK_OBJECT (mp));

	return -1;
}

	
	
	
	
