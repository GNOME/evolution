/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 * 
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2001 Ximian Inc (http://www.ximian.com/)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "camel-spool-folder.h"
#include "camel-spool-store.h"
#include "string-utils.h"
#include "camel-stream-fs.h"
#include "camel-spool-summary.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-from.h"
#include "camel-exception.h"

#include "camel-lock-client.h"

#include "camel-local-private.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

static CamelFolderClass *parent_class = NULL;

/* Returns the class for a CamelSpoolFolder */
#define CSPOOLF_CLASS(so) CAMEL_SPOOL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CSPOOLS_CLASS(so) CAMEL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static int spool_lock(CamelSpoolFolder *lf, CamelLockType type, CamelException *ex);
static void spool_unlock(CamelSpoolFolder *lf);

static void spool_sync(CamelFolder *folder, gboolean expunge, CamelException *ex);
static void spool_expunge(CamelFolder *folder, CamelException *ex);

static GPtrArray *spool_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex);
static void spool_search_free(CamelFolder *folder, GPtrArray * result);

static void spool_append_message(CamelFolder *folder, CamelMimeMessage * message, const CamelMessageInfo * info, CamelException *ex);
static CamelMimeMessage *spool_get_message(CamelFolder *folder, const gchar * uid, CamelException *ex);
static void spool_set_message_user_flag(CamelFolder *folder, const char *uid, const char *name, gboolean value);
static void spool_set_message_user_tag(CamelFolder *folder, const char *uid, const char *name, const char *value);

static void spool_finalize(CamelObject * object);

static void
camel_spool_folder_class_init(CamelSpoolFolderClass * camel_spool_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS(camel_spool_folder_class);

	parent_class = CAMEL_FOLDER_CLASS(camel_type_get_global_classfuncs(camel_folder_get_type()));

	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->sync = spool_sync;
	camel_folder_class->expunge = spool_expunge;

	camel_folder_class->search_by_expression = spool_search_by_expression;
	camel_folder_class->search_free = spool_search_free;

	/* virtual method overload */
	camel_folder_class->append_message = spool_append_message;
	camel_folder_class->get_message = spool_get_message;

	camel_folder_class->set_message_user_flag = spool_set_message_user_flag;
	camel_folder_class->set_message_user_tag = spool_set_message_user_tag;

	camel_spool_folder_class->lock = spool_lock;
	camel_spool_folder_class->unlock = spool_unlock;
}

static void
spool_init(gpointer object, gpointer klass)
{
	CamelFolder *folder = object;
	CamelSpoolFolder *spool_folder = object;

	folder->has_summary_capability = TRUE;
	folder->has_search_capability = TRUE;

	folder->permanent_flags = CAMEL_MESSAGE_ANSWERED |
	    CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_DRAFT |
	    CAMEL_MESSAGE_FLAGGED | CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_USER;

	folder->summary = NULL;
	spool_folder->search = NULL;

	spool_folder->priv = g_malloc0(sizeof(*spool_folder->priv));
#ifdef ENABLE_THREADS
	spool_folder->priv->search_lock = g_mutex_new();
#endif
}

static void
spool_finalize(CamelObject * object)
{
	CamelSpoolFolder *spool_folder = CAMEL_SPOOL_FOLDER(object);
	CamelFolder *folder = (CamelFolder *)object;

	if (folder->summary) {
		camel_spool_summary_sync((CamelSpoolSummary *)folder->summary, FALSE, spool_folder->changes, NULL);
		camel_object_unref((CamelObject *)folder->summary);
		folder->summary = NULL;
	}

	if (spool_folder->search) {
		camel_object_unref((CamelObject *)spool_folder->search);
	}

	while (spool_folder->locked> 0)
		camel_spool_folder_unlock(spool_folder);

	g_free(spool_folder->base_path);
	g_free(spool_folder->folder_path);

	camel_folder_change_info_free(spool_folder->changes);

#ifdef ENABLE_THREADS
	g_mutex_free(spool_folder->priv->search_lock);
#endif
	g_free(spool_folder->priv);
}

CamelType camel_spool_folder_get_type(void)
{
	static CamelType camel_spool_folder_type = CAMEL_INVALID_TYPE;

	if (camel_spool_folder_type == CAMEL_INVALID_TYPE) {
		camel_spool_folder_type = camel_type_register(CAMEL_FOLDER_TYPE, "CamelSpoolFolder",
							     sizeof(CamelSpoolFolder),
							     sizeof(CamelSpoolFolderClass),
							     (CamelObjectClassInitFunc) camel_spool_folder_class_init,
							     NULL,
							     (CamelObjectInitFunc) spool_init,
							     (CamelObjectFinalizeFunc) spool_finalize);
	}

	return camel_spool_folder_type;
}

CamelSpoolFolder *
camel_spool_folder_construct(CamelSpoolFolder *lf, CamelStore *parent_store, const char *full_name, guint32 flags, CamelException *ex)
{
	CamelFolderInfo *fi;
	CamelFolder *folder;
	const char *root_dir_path, *name;

	folder = (CamelFolder *)lf;

	name = strrchr(full_name, '/');
	if (name)
		name++;
	else
		name = full_name;

	camel_folder_construct(folder, parent_store, full_name, name);

	root_dir_path = camel_spool_store_get_toplevel_dir(CAMEL_SPOOL_STORE(folder->parent_store));

	lf->base_path = g_strdup(root_dir_path);
	lf->folder_path = g_strdup(root_dir_path);

	lf->changes = camel_folder_change_info_new();
	lf->flags = flags;

	folder->summary = (CamelFolderSummary *)camel_spool_summary_new(lf->folder_path);
	if (camel_spool_folder_lock(lf, CAMEL_LOCK_WRITE, ex) == -1) {
		camel_object_unref((CamelObject *)lf);
		return NULL;
	}

	camel_spool_summary_check((CamelSpoolSummary *)folder->summary, NULL, ex);
	camel_spool_folder_unlock(lf);

	fi = g_malloc0(sizeof(*fi));
	fi->full_name = g_strdup(full_name);
	fi->name = g_strdup(name);
	fi->url = g_strdup(lf->folder_path);
	fi->unread_message_count = camel_folder_get_unread_message_count(folder);
	camel_object_trigger_event(CAMEL_OBJECT(parent_store), "folder_created", fi);
	
	camel_folder_info_free (fi);
	
	return lf;
}

CamelFolder *
camel_spool_folder_new(CamelStore *parent_store, const char *full_name, guint32 flags, CamelException *ex)
{
	CamelFolder *folder;

	d(printf("Creating spool folder: %s in %s\n", full_name, camel_local_store_get_toplevel_dir((CamelLocalStore *)parent_store)));

	folder = (CamelFolder *)camel_object_new(CAMEL_SPOOL_FOLDER_TYPE);
	folder = (CamelFolder *)camel_spool_folder_construct((CamelSpoolFolder *)folder,
							     parent_store, full_name, flags, ex);

	return folder;
}

/* lock the folder, may be called repeatedly (with matching unlock calls),
   with type the same or less than the first call */
int camel_spool_folder_lock(CamelSpoolFolder *lf, CamelLockType type, CamelException *ex)
{
	if (lf->locked > 0) {
		/* lets be anal here - its important the code knows what its doing */
		g_assert(lf->locktype == type || lf->locktype == CAMEL_LOCK_WRITE);
	} else {
		if (CSPOOLF_CLASS(lf)->lock(lf, type, ex) == -1)
			return -1;
		lf->locktype = type;
	}

	lf->locked++;

	return 0;
}

/* unlock folder */
int camel_spool_folder_unlock(CamelSpoolFolder *lf)
{
	g_assert(lf->locked>0);
	lf->locked--;
	if (lf->locked == 0)
		CSPOOLF_CLASS(lf)->unlock(lf);

	return 0;
}

static int
spool_lock(CamelSpoolFolder *lf, CamelLockType type, CamelException *ex)
{
	int retry = 0;

	lf->lockfd = open(lf->folder_path, O_RDWR, 0);
	if (lf->lockfd == -1) {
		camel_exception_setv(ex, 1, _("Cannot create folder lock on %s: %s"), lf->folder_path, strerror(errno));
		return -1;
	}

	while (retry < CAMEL_LOCK_RETRY) {
		if (retry > 0)
			sleep(CAMEL_LOCK_DELAY);

		camel_exception_clear(ex);

		if (camel_lock_fcntl(lf->lockfd, type, ex) == 0) {
			if (camel_lock_flock(lf->lockfd, type, ex) == 0) {
				if ((lf->lockid = camel_lock_helper_lock(lf->folder_path, ex)) != -1)
					return 0;
				camel_unlock_flock(lf->lockfd);
			}
			camel_unlock_fcntl(lf->lockfd);
		}
		retry++;
	}

	return -1;
}

static void
spool_unlock(CamelSpoolFolder *lf)
{
	camel_lock_helper_unlock(lf->lockid);
	lf->lockid = -1;
	camel_unlock_flock(lf->lockid);
	camel_unlock_fcntl(lf->lockid);

	close(lf->lockfd);
	lf->lockfd = -1;
}

static void
spool_sync(CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelSpoolFolder *lf = CAMEL_SPOOL_FOLDER(folder);

	d(printf("spool sync, expunge=%s\n", expunge?"true":"false"));

	if (camel_spool_folder_lock(lf, CAMEL_LOCK_WRITE, ex) == -1)
		return;

	/* if sync fails, we'll pass it up on exit through ex */
	camel_spool_summary_sync((CamelSpoolSummary *)folder->summary, expunge, lf->changes, ex);
	camel_spool_folder_unlock(lf);

	if (camel_folder_change_info_changed(lf->changes)) {
		camel_object_trigger_event(CAMEL_OBJECT(folder), "folder_changed", lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}
}

static void
spool_expunge(CamelFolder *folder, CamelException *ex)
{
	d(printf("expunge\n"));

	/* Just do a sync with expunge, serves the same purpose */
	/* call the callback directly, to avoid locking problems */
	CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(folder))->sync(folder, TRUE, ex);
}

static GPtrArray *
spool_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex)
{
	CamelSpoolFolder *spool_folder = CAMEL_SPOOL_FOLDER(folder);
	GPtrArray *summary, *matches;

	/* NOTE: could get away without the search lock by creating a new
	   search object each time */

	CAMEL_SPOOL_FOLDER_LOCK(folder, search_lock);

	if (spool_folder->search == NULL)
		spool_folder->search = camel_folder_search_new();

	camel_folder_search_set_folder(spool_folder->search, folder);
	summary = camel_folder_get_summary(folder);
	camel_folder_search_set_summary(spool_folder->search, summary);

	matches = camel_folder_search_execute_expression(spool_folder->search, expression, ex);

	CAMEL_SPOOL_FOLDER_UNLOCK(folder, search_lock);

	camel_folder_free_summary(folder, summary);

	return matches;
}

static void
spool_search_free(CamelFolder *folder, GPtrArray * result)
{
	CamelSpoolFolder *spool_folder = CAMEL_SPOOL_FOLDER(folder);

	/* we need to lock this free because of the way search_free_result works */
	/* FIXME: put the lock inside search_free_result */
	CAMEL_SPOOL_FOLDER_LOCK(folder, search_lock);

	camel_folder_search_free_result(spool_folder->search, result);

	CAMEL_SPOOL_FOLDER_UNLOCK(folder, search_lock);
}

static void
spool_append_message(CamelFolder *folder, CamelMimeMessage * message, const CamelMessageInfo * info, CamelException *ex)
{
	CamelSpoolFolder *lf = (CamelSpoolFolder *)folder;
	CamelStream *output_stream = NULL, *filter_stream = NULL;
	CamelMimeFilter *filter_from = NULL;
	CamelSpoolSummary *mbs = (CamelSpoolSummary *)folder->summary;
	CamelMessageInfo *mi;
	char *fromline = NULL;
	int fd;
	struct stat st;
#if 0
	char *xev;
#endif
	/* If we can't lock, dont do anything */
	if (camel_spool_folder_lock(lf, CAMEL_LOCK_WRITE, ex) == -1)
		return;

	d(printf("Appending message\n"));

	/* first, check the summary is correct (updates folder_size too) */
	camel_spool_summary_check((CamelSpoolSummary *)folder->summary, lf->changes, ex);
	if (camel_exception_is_set(ex))
		goto fail;

	/* add it to the summary/assign the uid, etc */
	mi = camel_spool_summary_add((CamelSpoolSummary *)folder->summary, message, info, lf->changes, ex);
	if (camel_exception_is_set(ex))
		goto fail;

	d(printf("Appending message: uid is %s\n", camel_message_info_uid(mi)));

	output_stream = camel_stream_fs_new_with_name(lf->folder_path, O_WRONLY|O_APPEND, 0600);
	if (output_stream == NULL) {
		camel_exception_setv(ex, 1, _("Cannot open mailbox: %s: %s\n"), lf->folder_path, strerror(errno));
		goto fail;
	}

	/* and we need to set the frompos/XEV explicitly */
	((CamelSpoolMessageInfo *)mi)->frompos = mbs->folder_size?mbs->folder_size+1:0;
#if 0
	xev = camel_spool_summary_encode_x_evolution((CamelLocalSummary *)folder->summary, mi);
	if (xev) {
		/* the x-ev header should match the 'current' flags, no problem, so store as much */
		camel_medium_set_header((CamelMedium *)message, "X-Evolution", xev);
		mi->flags &= ~ CAMEL_MESSAGE_FOLDER_NOXEV|CAMEL_MESSAGE_FOLDER_FLAGGED;
		g_free(xev);
	}
#endif

	/* we must write this to the non-filtered stream ... prepend a \n if not at the start of the file */
	fromline = camel_spool_summary_build_from(((CamelMimePart *)message)->headers);
	if (camel_stream_printf(output_stream, mbs->folder_size==0?"%s":"\n%s", fromline) == -1)
		goto fail_write;

	/* and write the content to the filtering stream, that translated '\nFrom' into '\n>From' */
	filter_stream = (CamelStream *) camel_stream_filter_new_with_stream(output_stream);
	filter_from = (CamelMimeFilter *) camel_mime_filter_from_new();
	camel_stream_filter_add((CamelStreamFilter *) filter_stream, filter_from);
	if (camel_data_wrapper_write_to_stream((CamelDataWrapper *)message, filter_stream) == -1)
		goto fail_write;

	if (camel_stream_close(filter_stream) == -1)
		goto fail_write;

	/* unlock as soon as we can */
	camel_spool_folder_unlock(lf);

	/* filter stream ref's the output stream itself, so we need to unref it too */
	camel_object_unref((CamelObject *)filter_from);
	camel_object_unref((CamelObject *)filter_stream);
	camel_object_unref((CamelObject *)output_stream);
	g_free(fromline);

	/* now we 'fudge' the summary  to tell it its uptodate, because its idea of uptodate has just changed */
	/* the stat really shouldn't fail, we just wrote to it */
	if (stat(lf->folder_path, &st) == 0) {
		mbs->folder_size = st.st_size;
		((CamelFolderSummary *)mbs)->time = st.st_mtime;
	}

	if (camel_folder_change_info_changed(lf->changes)) {
		camel_object_trigger_event((CamelObject *)folder, "folder_changed", lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}

	return;

fail_write:
	camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
			      _("Cannot append message to spool file: %s: %s"),
			      lf->folder_path, g_strerror (errno));

	if (filter_stream)
		camel_object_unref(CAMEL_OBJECT(filter_stream));

	if (output_stream)
		camel_object_unref(CAMEL_OBJECT(output_stream));

	if (filter_from)
		camel_object_unref(CAMEL_OBJECT(filter_from));

	g_free(fromline);

	/* reset the file to original size */
	fd = open(lf->folder_path, O_WRONLY, 0600);
	if (fd != -1) {
		ftruncate(fd, mbs->folder_size);
		close(fd);
	}
	
	/* remove the summary info so we are not out-of-sync with the spool */
	camel_folder_summary_remove_uid (CAMEL_FOLDER_SUMMARY (mbs), camel_message_info_uid (mi));
	
	/* and tell the summary its uptodate */
	if (stat(lf->folder_path, &st) == 0) {
		mbs->folder_size = st.st_size;
		((CamelFolderSummary *)mbs)->time = st.st_mtime;
	}
	
fail:
	/* make sure we unlock the folder - before we start triggering events into appland */
	camel_spool_folder_unlock(lf);

	/* cascade the changes through, anyway, if there are any outstanding */
	if (camel_folder_change_info_changed(lf->changes)) {
		camel_object_trigger_event((CamelObject *)folder, "folder_changed", lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}
}

static CamelMimeMessage *
spool_get_message(CamelFolder *folder, const gchar * uid, CamelException *ex)
{
	CamelSpoolFolder *lf = (CamelSpoolFolder *)folder;
	CamelMimeMessage *message;
	CamelSpoolMessageInfo *info;
	CamelMimeParser *parser;
	int fd;
	int retried = FALSE;
	
	d(printf("Getting message %s\n", uid));

	/* lock the folder first, burn if we can't, need write lock for summary check */
	if (camel_spool_folder_lock(lf, CAMEL_LOCK_WRITE, ex) == -1)
		return NULL;

	/* check for new messages, this may renumber uid's though */
	if (camel_spool_summary_check((CamelSpoolSummary *)folder->summary, lf->changes, ex) == -1)
		return NULL;

retry:
	/* get the message summary info */
	info = (CamelSpoolMessageInfo *) camel_folder_summary_uid(folder->summary, uid);

	if (info == NULL) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				     _("Cannot get message: %s\n  %s"), uid, _("No such message"));
		camel_spool_folder_unlock(lf);
		return NULL;
	}

	/* no frompos, its an error in the library (and we can't do anything with it */
	g_assert(info->frompos != -1);
	
	/* we use an fd instead of a normal stream here - the reason is subtle, camel_mime_part will cache
	   the whole message in memory if the stream is non-seekable (which it is when built from a parser
	   with no stream).  This means we dont have to lock the spool for the life of the message, but only
	   while it is being created. */

	fd = open(lf->folder_path, O_RDONLY);
	if (fd == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				     _("Cannot get message: %s from folder %s\n  %s"), uid, lf->folder_path,
				     strerror(errno));
		camel_spool_folder_unlock(lf);
		camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)info);
		return NULL;
	}

	/* we use a parser to verify the message is correct, and in the correct position */
	parser = camel_mime_parser_new();
	camel_mime_parser_init_with_fd(parser, fd);
	camel_mime_parser_scan_from(parser, TRUE);

	camel_mime_parser_seek(parser, info->frompos, SEEK_SET);
	if (camel_mime_parser_step(parser, NULL, NULL) != HSCAN_FROM
	    || camel_mime_parser_tell_start_from(parser) != info->frompos) {

		g_warning("Summary doesn't match the folder contents!  eek!\n"
			  "  expecting offset %ld got %ld, state = %d", (long int)info->frompos,
			  (long int)camel_mime_parser_tell_start_from(parser),
			  camel_mime_parser_state(parser));

		camel_object_unref((CamelObject *)parser);
		camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)info);

		if (!retried) {
			retried = TRUE;
			camel_spool_summary_check((CamelSpoolSummary *)folder->summary, lf->changes, ex);
			if (!camel_exception_is_set(ex))
				goto retry;
		}

		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				     _("Cannot get message: %s from folder %s\n  %s"), uid, lf->folder_path,
				     _("The folder appears to be irrecoverably corrupted."));

		camel_spool_folder_unlock(lf);
		return NULL;
	}

	camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)info);
	
	message = camel_mime_message_new();
	if (camel_mime_part_construct_from_parser((CamelMimePart *)message, parser) == -1) {
		g_warning("Construction failed");
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				     _("Cannot get message: %s from folder %s\n  %s"), uid, lf->folder_path,
				     _("Message construction failed: Corrupt mailbox?"));
		camel_object_unref((CamelObject *)parser);
		camel_object_unref((CamelObject *)message);
		camel_spool_folder_unlock(lf);
		return NULL;
	}

	/* and unlock now we're finished with it */
	camel_spool_folder_unlock(lf);

	camel_object_unref((CamelObject *)parser);
	
	/* use the opportunity to notify of changes (particularly if we had a rebuild) */
	if (camel_folder_change_info_changed(lf->changes)) {
		camel_object_trigger_event((CamelObject *)folder, "folder_changed", lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}
	
	return message;
}

static void
spool_set_message_user_flag(CamelFolder *folder, const char *uid, const char *name, gboolean value)
{
	CamelMessageInfo *info;

	g_return_if_fail(folder->summary != NULL);

	info = camel_folder_summary_uid(folder->summary, uid);
	g_return_if_fail(info != NULL);

	if (camel_flag_set(&info->user_flags, name, value)) {
		info->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED|CAMEL_MESSAGE_FOLDER_XEVCHANGE;
		camel_folder_summary_touch(folder->summary);
		camel_object_trigger_event(CAMEL_OBJECT(folder), "message_changed", (char *) uid);
	}
	camel_folder_summary_info_free(folder->summary, info);
}

static void
spool_set_message_user_tag(CamelFolder *folder, const char *uid, const char *name, const char *value)
{
	CamelMessageInfo *info;

	g_return_if_fail(folder->summary != NULL);

	info = camel_folder_summary_uid(folder->summary, uid);
	g_return_if_fail(info != NULL);

	if (camel_tag_set(&info->user_tags, name, value)) {
		info->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED|CAMEL_MESSAGE_FOLDER_XEVCHANGE;
		camel_folder_summary_touch(folder->summary);
		camel_object_trigger_event(CAMEL_OBJECT(folder), "message_changed", (char *) uid);
	}
	camel_folder_summary_info_free(folder->summary, info);
}
