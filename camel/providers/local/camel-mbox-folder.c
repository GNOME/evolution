/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *          Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999, 2003 Ximian Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#include "camel-mbox-folder.h"
#include "camel-mbox-store.h"
#include "camel-stream-fs.h"
#include "camel-mbox-summary.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-from.h"
#include "camel-exception.h"
#include "camel-i18n.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

static CamelLocalFolderClass *parent_class = NULL;

/* Returns the class for a CamelMboxFolder */
#define CMBOXF_CLASS(so) CAMEL_MBOX_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CMBOXS_CLASS(so) CAMEL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))

char *camel_mbox_folder_get_full_path (const char *toplevel_dir, const char *full_name);
char *camel_mbox_folder_get_meta_path (const char *toplevel_dir, const char *full_name, const char *ext);

static int mbox_lock(CamelLocalFolder *lf, CamelLockType type, CamelException *ex);
static void mbox_unlock(CamelLocalFolder *lf);

#ifdef STATUS_PINE
static gboolean mbox_set_message_flags(CamelFolder *folder, const char *uid, guint32 flags, guint32 set);
#endif

static void mbox_set_message_user_flag(CamelFolder *folder, const char *uid, const char *name, gboolean value);
static void mbox_set_message_user_tag(CamelFolder *folder, const char *uid, const char *name, const char *value);

static void mbox_append_message(CamelFolder *folder, CamelMimeMessage * message, const CamelMessageInfo * info,	char **appended_uid, CamelException *ex);
static CamelMimeMessage *mbox_get_message(CamelFolder *folder, const gchar * uid, CamelException *ex);
static CamelLocalSummary *mbox_create_summary(const char *path, const char *folder, CamelIndex *index);

static void mbox_finalise(CamelObject * object);

static void
camel_mbox_folder_class_init(CamelMboxFolderClass * camel_mbox_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS(camel_mbox_folder_class);
	CamelLocalFolderClass *lclass = (CamelLocalFolderClass *)camel_mbox_folder_class;

	parent_class = (CamelLocalFolderClass *)camel_type_get_global_classfuncs(camel_local_folder_get_type());

	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->append_message = mbox_append_message;
	camel_folder_class->get_message = mbox_get_message;

#ifdef STATUS_PINE
	camel_folder_class->set_message_flags = mbox_set_message_flags;
#endif
	camel_folder_class->set_message_user_flag = mbox_set_message_user_flag;
	camel_folder_class->set_message_user_tag = mbox_set_message_user_tag;
	
	lclass->get_full_path = camel_mbox_folder_get_full_path;
	lclass->get_meta_path = camel_mbox_folder_get_meta_path;
	lclass->create_summary = mbox_create_summary;
	lclass->lock = mbox_lock;
	lclass->unlock = mbox_unlock;
}

static void
mbox_init(gpointer object, gpointer klass)
{
	/*CamelFolder *folder = object;*/
	CamelMboxFolder *mbox_folder = object;

	mbox_folder->lockfd = -1;
}

static void
mbox_finalise(CamelObject * object)
{
	CamelMboxFolder *mbox_folder = (CamelMboxFolder *)object;

	g_assert(mbox_folder->lockfd == -1);
}

CamelType camel_mbox_folder_get_type(void)
{
	static CamelType camel_mbox_folder_type = CAMEL_INVALID_TYPE;

	if (camel_mbox_folder_type == CAMEL_INVALID_TYPE) {
		camel_mbox_folder_type = camel_type_register(CAMEL_LOCAL_FOLDER_TYPE, "CamelMboxFolder",
							     sizeof(CamelMboxFolder),
							     sizeof(CamelMboxFolderClass),
							     (CamelObjectClassInitFunc) camel_mbox_folder_class_init,
							     NULL,
							     (CamelObjectInitFunc) mbox_init,
							     (CamelObjectFinalizeFunc) mbox_finalise);
	}

	return camel_mbox_folder_type;
}

CamelFolder *
camel_mbox_folder_new(CamelStore *parent_store, const char *full_name, guint32 flags, CamelException *ex)
{
	CamelFolder *folder;

	d(printf("Creating mbox folder: %s in %s\n", full_name, camel_local_store_get_toplevel_dir((CamelLocalStore *)parent_store)));

	folder = (CamelFolder *)camel_object_new(CAMEL_MBOX_FOLDER_TYPE);
	folder = (CamelFolder *)camel_local_folder_construct((CamelLocalFolder *)folder,
							     parent_store, full_name, flags, ex);

	return folder;
}

char *
camel_mbox_folder_get_full_path (const char *toplevel_dir, const char *full_name)
{
	const char *inptr = full_name;
	int subdirs = 0;
	char *path, *p;
	
	while (*inptr != '\0') {
		if (*inptr == '/')
			subdirs++;
		inptr++;
	}
	
	path = g_malloc (strlen (toplevel_dir) + (inptr - full_name) + (4 * subdirs) + 1);
	p = g_stpcpy (path, toplevel_dir);
	
	inptr = full_name;
	while (*inptr != '\0') {
		while (*inptr != '/' && *inptr != '\0')
			*p++ = *inptr++;
		
		if (*inptr == '/') {
			p = g_stpcpy (p, ".sbd/");
			inptr++;
			
			/* strip extranaeous '/'s */
			while (*inptr == '/')
				inptr++;
		}
	}
	
	*p = '\0';
	
	return path;
}

char *
camel_mbox_folder_get_meta_path (const char *toplevel_dir, const char *full_name, const char *ext)
{
/*#define USE_HIDDEN_META_FILES*/
#ifdef USE_HIDDEN_META_FILES
	char *name, *slash;
	
	name = g_alloca (strlen (full_name) + strlen (ext) + 2);
	if ((slash = strrchr (full_name, '/')))
		sprintf (name, "%.*s.%s%s", slash - full_name + 1, full_name, slash + 1, ext);
	else
		sprintf (name, ".%s%s", full_name, ext);
	
	return camel_mbox_folder_get_full_path (toplevel_dir, name);
#else
	char *full_path, *path;
	
	full_path = camel_mbox_folder_get_full_path (toplevel_dir, full_name);
	path = g_strdup_printf ("%s%s", full_path, ext);
	g_free (full_path);
	
	return path;
#endif
}

static CamelLocalSummary *mbox_create_summary(const char *path, const char *folder, CamelIndex *index)
{
	return (CamelLocalSummary *)camel_mbox_summary_new(path, folder, index);
}

static int mbox_lock(CamelLocalFolder *lf, CamelLockType type, CamelException *ex)
{
	CamelMboxFolder *mf = (CamelMboxFolder *)lf;

	/* make sure we have matching unlocks for locks, camel-local-folder class should enforce this */
	g_assert(mf->lockfd == -1);

	mf->lockfd = open(lf->folder_path, O_RDWR, 0);
	if (mf->lockfd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot create folder lock on %s: %s"),
				      lf->folder_path, g_strerror (errno));
		return -1;
	}

	if (camel_lock_folder(lf->folder_path, mf->lockfd, type, ex) == -1) {
		close(mf->lockfd);
		mf->lockfd = -1;
		return -1;
	}

	return 0;
}

static void mbox_unlock(CamelLocalFolder *lf)
{
	CamelMboxFolder *mf = (CamelMboxFolder *)lf;

	g_assert(mf->lockfd != -1);
	camel_unlock_folder(lf->folder_path, mf->lockfd);
	close(mf->lockfd);
	mf->lockfd = -1;
}

static void
mbox_append_message(CamelFolder *folder, CamelMimeMessage * message, const CamelMessageInfo * info, char **appended_uid, CamelException *ex)
{
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;
	CamelStream *output_stream = NULL, *filter_stream = NULL;
	CamelMimeFilter *filter_from = NULL;
	CamelMboxSummary *mbs = (CamelMboxSummary *)folder->summary;
	CamelMessageInfo *mi;
	char *fromline = NULL;
	int fd, retval;
	struct stat st;
#if 0
	char *xev;
#endif
	/* If we can't lock, dont do anything */
	if (camel_local_folder_lock(lf, CAMEL_LOCK_WRITE, ex) == -1)
		return;

	d(printf("Appending message\n"));

	/* first, check the summary is correct (updates folder_size too) */
	retval = camel_local_summary_check ((CamelLocalSummary *)folder->summary, lf->changes, ex);
	if (retval == -1)
		goto fail;

	/* add it to the summary/assign the uid, etc */
	mi = camel_local_summary_add((CamelLocalSummary *)folder->summary, message, info, lf->changes, ex);
	if (mi == NULL)
		goto fail;

	d(printf("Appending message: uid is %s\n", camel_message_info_uid(mi)));

	output_stream = camel_stream_fs_new_with_name(lf->folder_path, O_WRONLY|O_APPEND, 0600);
	if (output_stream == NULL) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot open mailbox: %s: %s\n"),
				      lf->folder_path, g_strerror (errno));
		goto fail;
	}

	/* and we need to set the frompos/XEV explicitly */
	((CamelMboxMessageInfo *)mi)->frompos = mbs->folder_size;
#if 0
	xev = camel_local_summary_encode_x_evolution((CamelLocalSummary *)folder->summary, mi);
	if (xev) {
		/* the x-ev header should match the 'current' flags, no problem, so store as much */
		camel_medium_set_header((CamelMedium *)message, "X-Evolution", xev);
		mi->flags &= ~ CAMEL_MESSAGE_FOLDER_NOXEV|CAMEL_MESSAGE_FOLDER_FLAGGED;
		g_free(xev);
	}
#endif

	/* we must write this to the non-filtered stream ... */
	fromline = camel_mime_message_build_mbox_from(message);
	if (camel_stream_write(output_stream, fromline, strlen(fromline)) == -1)
		goto fail_write;

	/* and write the content to the filtering stream, that translates '\nFrom' into '\n>From' */
	filter_stream = (CamelStream *) camel_stream_filter_new_with_stream(output_stream);
	filter_from = (CamelMimeFilter *) camel_mime_filter_from_new();
	camel_stream_filter_add((CamelStreamFilter *) filter_stream, filter_from);
	if (camel_data_wrapper_write_to_stream((CamelDataWrapper *)message, filter_stream) == -1
	    || camel_stream_write(filter_stream, "\n", 1) == -1
	    || camel_stream_close(filter_stream) == -1)
		goto fail_write;

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

	/* unlock as soon as we can */
	camel_local_folder_unlock(lf);

	if (camel_folder_change_info_changed(lf->changes)) {
		camel_object_trigger_event((CamelObject *)folder, "folder_changed", lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}

	if (appended_uid)
		*appended_uid = g_strdup(camel_message_info_uid(mi));

	return;

fail_write:
	if (errno == EINTR)
		camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
				     _("Mail append cancelled"));
	else
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot append message to mbox file: %s: %s"),
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
	
	/* remove the summary info so we are not out-of-sync with the mbox */
	camel_folder_summary_remove_uid (CAMEL_FOLDER_SUMMARY (mbs), camel_message_info_uid (mi));
	
	/* and tell the summary its uptodate */
	if (stat(lf->folder_path, &st) == 0) {
		mbs->folder_size = st.st_size;
		((CamelFolderSummary *)mbs)->time = st.st_mtime;
	}
	
fail:
	/* make sure we unlock the folder - before we start triggering events into appland */
	camel_local_folder_unlock(lf);

	/* cascade the changes through, anyway, if there are any outstanding */
	if (camel_folder_change_info_changed(lf->changes)) {
		camel_object_trigger_event((CamelObject *)folder, "folder_changed", lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}
}

static CamelMimeMessage *
mbox_get_message(CamelFolder *folder, const gchar * uid, CamelException *ex)
{
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;
	CamelMimeMessage *message = NULL;
	CamelMboxMessageInfo *info;
	CamelMimeParser *parser = NULL;
	int fd, retval;
	int retried = FALSE;
	off_t frompos;

	d(printf("Getting message %s\n", uid));

	/* lock the folder first, burn if we can't, need write lock for summary check */
	if (camel_local_folder_lock(lf, CAMEL_LOCK_WRITE, ex) == -1)
		return NULL;

	/* check for new messages always */
	if (camel_local_summary_check((CamelLocalSummary *)folder->summary, lf->changes, ex) == -1) {
		camel_local_folder_unlock(lf);
		return NULL;
	}
	
retry:
	/* get the message summary info */
	info = (CamelMboxMessageInfo *) camel_folder_summary_uid(folder->summary, uid);

	if (info == NULL) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				     _("Cannot get message: %s from folder %s\n  %s"),
				     uid, lf->folder_path, _("No such message"));
		goto fail;
	}

	/* no frompos, its an error in the library (and we can't do anything with it) */
	g_assert(info->frompos != -1);

	frompos = info->frompos;
	camel_folder_summary_info_free(folder->summary, (CamelMessageInfo *)info);
	
	/* we use an fd instead of a normal stream here - the reason is subtle, camel_mime_part will cache
	   the whole message in memory if the stream is non-seekable (which it is when built from a parser
	   with no stream).  This means we dont have to lock the mbox for the life of the message, but only
	   while it is being created. */

	fd = open(lf->folder_path, O_RDONLY);
	if (fd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot get message: %s from folder %s\n  %s"),
				      uid, lf->folder_path, g_strerror (errno));
		goto fail;
	}

	/* we use a parser to verify the message is correct, and in the correct position */
	parser = camel_mime_parser_new();
	camel_mime_parser_init_with_fd(parser, fd);
	camel_mime_parser_scan_from(parser, TRUE);

	camel_mime_parser_seek(parser, frompos, SEEK_SET);
	if (camel_mime_parser_step(parser, NULL, NULL) != CAMEL_MIME_PARSER_STATE_FROM
	    || camel_mime_parser_tell_start_from(parser) != frompos) {

		g_warning("Summary doesn't match the folder contents!  eek!\n"
			  "  expecting offset %ld got %ld, state = %d", (long int)frompos,
			  (long int)camel_mime_parser_tell_start_from(parser),
			  camel_mime_parser_state(parser));

		camel_object_unref((CamelObject *)parser);
		parser = NULL;

		if (!retried) {
			retried = TRUE;
			camel_local_summary_check_force((CamelLocalSummary *)folder->summary);
			retval = camel_local_summary_check((CamelLocalSummary *)folder->summary, lf->changes, ex);
			if (retval != -1)
				goto retry;
		}

		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_INVALID,
				     _("Cannot get message: %s from folder %s\n  %s"), uid, lf->folder_path,
				     _("The folder appears to be irrecoverably corrupted."));
		goto fail;
	}
	
	message = camel_mime_message_new();
	if (camel_mime_part_construct_from_parser((CamelMimePart *)message, parser) == -1) {
		camel_exception_setv(ex, errno==EINTR?CAMEL_EXCEPTION_USER_CANCEL:CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot get message: %s from folder %s\n  %s"), uid, lf->folder_path,
				     _("Message construction failed."));
		camel_object_unref((CamelObject *)message);
		message = NULL;
		goto fail;
	}

	camel_medium_remove_header((CamelMedium *)message, "X-Evolution");
fail:
	/* and unlock now we're finished with it */
	camel_local_folder_unlock(lf);

	if (parser)
		camel_object_unref((CamelObject *)parser);
	
	/* use the opportunity to notify of changes (particularly if we had a rebuild) */
	if (camel_folder_change_info_changed(lf->changes)) {
		camel_object_trigger_event((CamelObject *)folder, "folder_changed", lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}
	
	return message;
}

#ifdef STATUS_PINE
static gboolean
mbox_set_message_flags(CamelFolder *folder, const char *uid, guint32 flags, guint32 set)
{
	/* Basically, if anything could change the Status line, presume it does */
	if (((CamelMboxSummary *)folder->summary)->xstatus
	    && (flags & (CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_FLAGGED|CAMEL_MESSAGE_ANSWERED|CAMEL_MESSAGE_DELETED))) {
		flags |= CAMEL_MESSAGE_FOLDER_XEVCHANGE|CAMEL_MESSAGE_FOLDER_FLAGGED;
		set |= CAMEL_MESSAGE_FOLDER_XEVCHANGE|CAMEL_MESSAGE_FOLDER_FLAGGED;
	}

	return ((CamelFolderClass *)parent_class)->set_message_flags(folder, uid, flags, set);
}
#endif

static void
mbox_set_message_user_flag(CamelFolder *folder, const char *uid, const char *name, gboolean value)
{
	CamelMessageInfo *info;

	g_return_if_fail(folder->summary != NULL);

	info = camel_folder_summary_uid(folder->summary, uid);
	if (info == NULL)
		return;

	if (camel_flag_set(&info->user_flags, name, value)) {
		CamelFolderChangeInfo *changes = camel_folder_change_info_new();

		info->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED|CAMEL_MESSAGE_FOLDER_XEVCHANGE;
		camel_folder_summary_touch(folder->summary);

		camel_folder_change_info_change_uid(changes, uid);
		camel_object_trigger_event(folder, "folder_changed", changes);
		camel_folder_change_info_free(changes);
	}
	camel_folder_summary_info_free(folder->summary, info);
}

static void
mbox_set_message_user_tag(CamelFolder *folder, const char *uid, const char *name, const char *value)
{
	CamelMessageInfo *info;

	g_return_if_fail(folder->summary != NULL);

	info = camel_folder_summary_uid(folder->summary, uid);
	if (info == NULL)
		return;

	if (camel_tag_set(&info->user_tags, name, value)) {
		CamelFolderChangeInfo *changes = camel_folder_change_info_new();

		info->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED|CAMEL_MESSAGE_FOLDER_XEVCHANGE;
		camel_folder_summary_touch(folder->summary);

		camel_folder_change_info_change_uid(changes, uid);
		camel_object_trigger_event (folder, "folder_changed", changes);
		camel_folder_change_info_free(changes);
	}
	camel_folder_summary_info_free(folder->summary, info);
}
