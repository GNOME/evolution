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
#include "camel-i18n.h"

#define io(x)
#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

#define CAMEL_SPOOL_SUMMARY_VERSION (0x400)

static int spool_summary_load(CamelLocalSummary *cls, int forceindex, CamelException *ex);
static int spool_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex);

static int spool_summary_sync_full(CamelMboxSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex);

static void camel_spool_summary_class_init (CamelSpoolSummaryClass *klass);
static void camel_spool_summary_init       (CamelSpoolSummary *obj);
static void camel_spool_summary_finalise   (CamelObject *obj);

static CamelFolderSummaryClass *camel_spool_summary_parent;

CamelType
camel_spool_summary_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_mbox_summary_get_type(), "CamelSpoolSummary",
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
	CamelLocalSummaryClass *lklass = (CamelLocalSummaryClass *)klass;
	CamelMboxSummaryClass *mklass = (CamelMboxSummaryClass *)klass;

	camel_spool_summary_parent = CAMEL_FOLDER_SUMMARY_CLASS(camel_mbox_summary_get_type());

	lklass->load = spool_summary_load;
	lklass->check = spool_summary_check;

	mklass->sync_full = spool_summary_sync_full;
}

static void
camel_spool_summary_init(CamelSpoolSummary *obj)
{
	struct _CamelFolderSummary *s = (CamelFolderSummary *)obj;
	
	/* message info size is from mbox parent */

	/* and a unique file version */
	s->version += CAMEL_SPOOL_SUMMARY_VERSION;
}

static void
camel_spool_summary_finalise(CamelObject *obj)
{
	/*CamelSpoolSummary *mbs = CAMEL_SPOOL_SUMMARY(obj);*/
}

CamelSpoolSummary *
camel_spool_summary_new(struct _CamelFolder *folder, const char *mbox_name)
{
	CamelSpoolSummary *new = (CamelSpoolSummary *)camel_object_new(camel_spool_summary_get_type());

	((CamelFolderSummary *)new)->folder = folder;

	camel_local_summary_construct((CamelLocalSummary *)new, NULL, mbox_name, NULL);
	return new;
}

static int
spool_summary_load(CamelLocalSummary *cls, int forceindex, CamelException *ex)
{
	g_warning("spool summary - not loading anything\n");
	return 0;
}

/* perform a full sync */
static int
spool_summary_sync_full(CamelMboxSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	int fd = -1, fdout = -1;
	char *tmpname = NULL;
	char *buffer, *p;
	off_t spoollen, outlen;
	int size, sizeout;
	struct stat st;
	guint32 flags = (expunge?1:0);

	d(printf("performing full summary/sync\n"));

	camel_operation_start(NULL, _("Storing folder"));

	fd = open(((CamelLocalSummary *)cls)->folder_path, O_RDWR);
	if (fd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not open file: %s: %s"),
				      ((CamelLocalSummary *)cls)->folder_path,
				      g_strerror (errno));
		camel_operation_end(NULL);
		return -1;
	}

#ifdef HAVE_MKSTEMP
	tmpname = alloca (64);
	sprintf (tmpname, "/tmp/spool.camel.XXXXXX");
	fdout = mkstemp (tmpname);
#else
#warning "Your system has no mkstemp(3), spool updating may be insecure"
	tmpname = alloca (L_tmpnam);
	tmpnam (tmpname);
	fdout = open (tmpname, O_RDWR|O_CREAT|O_EXCL, 0600);
#endif
	d(printf("Writing tmp file to %s\n", tmpname));
	if (fdout == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot open temporary mailbox: %s"),
				      g_strerror (errno));
		goto error;
	}

	if (camel_mbox_summary_sync_mbox((CamelMboxSummary *)cls, flags, changeinfo, fd, fdout, ex) == -1)
		goto error;


	/* sync out content */
	if (fsync(fdout) == -1) {
		g_warning("Cannot sync temporary folder: %s", strerror (errno));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not sync temporary folder %s: %s"),
				      ((CamelLocalSummary *)cls)->folder_path,
				      g_strerror (errno));
		goto error;
	}

	/* see if we can write this much to the spool file */
	if (fstat(fd, &st) == -1) {
		g_warning("Cannot sync temporary folder: %s", strerror (errno));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not sync temporary folder %s: %s"),
				      ((CamelLocalSummary *)cls)->folder_path,
				      g_strerror (errno));
		goto error;
	}
	spoollen = st.st_size;

	if (fstat(fdout, &st) == -1) {
		g_warning("Cannot sync temporary folder: %s", strerror (errno));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not sync temporary folder %s: %s"),
				      ((CamelLocalSummary *)cls)->folder_path,
				      g_strerror (errno));
		goto error;
	}
	outlen = st.st_size;

	/* I think this is the right way to do this - checking that the file will fit the new data */
	if (outlen>0
	    && (lseek(fd, outlen-1, SEEK_SET) == -1
		|| write(fd, "", 1) != 1
		|| fsync(fd) == -1
		|| lseek(fd, 0, SEEK_SET) == -1
		|| lseek(fdout, 0, SEEK_SET) == -1)) {
		g_warning("Cannot sync spool folder: %s", strerror (errno));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not sync spool folder %s: %s"),
				      ((CamelLocalSummary *)cls)->folder_path,
				      g_strerror (errno));
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
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Could not sync spool folder %s: %s\n"
						"Folder may be corrupt, copy saved in `%s'"),
					      ((CamelLocalSummary *)cls)->folder_path,
					      g_strerror (errno), tmpnam);
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
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not sync spool folder %s: %s\n"
					"Folder may be corrupt, copy saved in `%s'"),
				      ((CamelLocalSummary *)cls)->folder_path,
				      g_strerror (errno), tmpnam);
		close(fdout);
		tmpname = NULL;
		fdout = -1;
		goto error;
	}

	if (close(fd) == -1) {
		g_warning("Cannot close source folder: %s", strerror (errno));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not sync spool folder %s: %s\n"
					"Folder may be corrupt, copy saved in `%s'"),
				      ((CamelLocalSummary *)cls)->folder_path,
				      g_strerror (errno), tmpnam);
		close(fdout);
		tmpname = NULL;
		fdout = -1;
		fd = -1;
		goto error;
	}

	close(fdout);
	unlink(tmpname);

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

static int
spool_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	int i, work, count;
	struct stat st;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;

	if (((CamelLocalSummaryClass *)camel_spool_summary_parent)->check(cls, changeinfo, ex) == -1)
		return -1;

	/* check to see if we need to copy/update the file; missing xev headers prompt this */
	work = FALSE;
	count = camel_folder_summary_count(s);
	for (i=0;!work && i<count; i++) {
		CamelMboxMessageInfo *info = (CamelMboxMessageInfo *)camel_folder_summary_index(s, i);
		g_assert(info);
		work = (info->info.info.flags & (CAMEL_MESSAGE_FOLDER_NOXEV)) != 0;
		camel_message_info_free((CamelMessageInfo *)info);
	}

	/* if we do, then write out the headers using sync_full, etc */
	if (work) {
		d(printf("Have to add new headers, re-syncing from the start to accomplish this\n"));
		if (((CamelMboxSummaryClass *)((CamelObject *)cls)->klass)->sync_full((CamelMboxSummary *)cls, FALSE, changeinfo, ex) == -1)
			return -1;
		
		if (stat(cls->folder_path, &st) == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Unknown error: %s"),
					      g_strerror (errno));
			return -1;
		}

		((CamelMboxSummary *)cls)->folder_size = st.st_size;
		((CamelFolderSummary *)cls)->time = st.st_mtime;
	}

	return 0;
}
