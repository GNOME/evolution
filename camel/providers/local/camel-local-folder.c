/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 1999-2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#ifndef _POSIX_PATH_MAX
#include <posix1_lim.h>
#endif

#include "camel-local-folder.h"
#include "camel-local-store.h"
#include "camel-stream-fs.h"
#include "camel-local-summary.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-from.h"
#include "camel-exception.h"
#include "camel-i18n.h"

#include "camel-local-private.h"

#include "camel-text-index.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

#ifndef PATH_MAX
#define PATH_MAX _POSIX_PATH_MAX
#endif

static CamelFolderClass *parent_class;
static GSList *local_folder_properties;

/* Returns the class for a CamelLocalFolder */
#define CLOCALF_CLASS(so) CAMEL_LOCAL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CLOCALS_CLASS(so) CAMEL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static int local_getv(CamelObject *object, CamelException *ex, CamelArgGetV *args);
static int local_setv(CamelObject *object, CamelException *ex, CamelArgV *args);

static int local_lock(CamelLocalFolder *lf, CamelLockType type, CamelException *ex);
static void local_unlock(CamelLocalFolder *lf);

static char *local_get_full_path(const char *toplevel_dir, const char *full_name);
static char *local_get_meta_path(const char *toplevel_dir, const char *full_name, const char *ext);

static void local_refresh_info(CamelFolder *folder, CamelException *ex);

static void local_sync(CamelFolder *folder, gboolean expunge, CamelException *ex);
static void local_expunge(CamelFolder *folder, CamelException *ex);

static GPtrArray *local_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex);
static GPtrArray *local_search_by_uids(CamelFolder *folder, const char *expression, GPtrArray *uids, CamelException *ex);
static void local_search_free(CamelFolder *folder, GPtrArray * result);

static void local_delete(CamelFolder *folder);
static void local_rename(CamelFolder *folder, const char *newname);

static void local_finalize(CamelObject * object);

static void
camel_local_folder_class_init(CamelLocalFolderClass * camel_local_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS(camel_local_folder_class);
	CamelObjectClass *oklass = (CamelObjectClass *)camel_local_folder_class;

	/* virtual method definition */

	/* virtual method overload */
	oklass->getv = local_getv;
	oklass->setv = local_setv;

	camel_folder_class->refresh_info = local_refresh_info;
	camel_folder_class->sync = local_sync;
	camel_folder_class->expunge = local_expunge;

	camel_folder_class->search_by_expression = local_search_by_expression;
	camel_folder_class->search_by_uids = local_search_by_uids;
	camel_folder_class->search_free = local_search_free;

	camel_folder_class->delete = local_delete;
	camel_folder_class->rename = local_rename;

	camel_local_folder_class->get_full_path = local_get_full_path;
	camel_local_folder_class->get_meta_path = local_get_meta_path;
	
	camel_local_folder_class->lock = local_lock;
	camel_local_folder_class->unlock = local_unlock;
}

static void
local_init(gpointer object, gpointer klass)
{
	CamelFolder *folder = object;
	CamelLocalFolder *local_folder = object;

	folder->folder_flags |= (CAMEL_FOLDER_HAS_SUMMARY_CAPABILITY |
				 CAMEL_FOLDER_HAS_SEARCH_CAPABILITY);

	folder->permanent_flags = CAMEL_MESSAGE_ANSWERED |
	    CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_DRAFT |
	    CAMEL_MESSAGE_FLAGGED | CAMEL_MESSAGE_SEEN |
	    CAMEL_MESSAGE_ANSWERED_ALL | CAMEL_MESSAGE_USER;

	folder->summary = NULL;
	local_folder->search = NULL;

	local_folder->priv = g_malloc0(sizeof(*local_folder->priv));
	local_folder->priv->search_lock = g_mutex_new();
}

static void
local_finalize(CamelObject * object)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(object);
	CamelFolder *folder = (CamelFolder *)object;

	if (folder->summary) {
		camel_local_summary_sync((CamelLocalSummary *)folder->summary, FALSE, local_folder->changes, NULL);
		camel_object_unref((CamelObject *)folder->summary);
		folder->summary = NULL;
	}

	if (local_folder->search) {
		camel_object_unref((CamelObject *)local_folder->search);
	}

	if (local_folder->index)
		camel_object_unref((CamelObject *)local_folder->index);

	while (local_folder->locked> 0)
		camel_local_folder_unlock(local_folder);

	g_free(local_folder->base_path);
	g_free(local_folder->folder_path);
	g_free(local_folder->summary_path);
	g_free(local_folder->index_path);

	camel_folder_change_info_free(local_folder->changes);
	
	g_mutex_free(local_folder->priv->search_lock);
	
	g_free(local_folder->priv);
}

static CamelProperty local_property_list[] = {
	{ CAMEL_LOCAL_FOLDER_INDEX_BODY, "index_body", N_("Index message body data") },
};

CamelType
camel_local_folder_get_type(void)
{
	static CamelType camel_local_folder_type = CAMEL_INVALID_TYPE;

	if (camel_local_folder_type == CAMEL_INVALID_TYPE) {
		int i;

		parent_class = (CamelFolderClass *)camel_folder_get_type();
		camel_local_folder_type = camel_type_register(camel_folder_get_type(), "CamelLocalFolder",
							     sizeof(CamelLocalFolder),
							     sizeof(CamelLocalFolderClass),
							     (CamelObjectClassInitFunc) camel_local_folder_class_init,
							     NULL,
							     (CamelObjectInitFunc) local_init,
							     (CamelObjectFinalizeFunc) local_finalize);

		for (i=0;i<sizeof(local_property_list)/sizeof(local_property_list[0]);i++) {
			local_property_list[i].description = _(local_property_list[i].description);
			local_folder_properties = g_slist_prepend(local_folder_properties, &local_property_list[i]);
		}
	}

	return camel_local_folder_type;
}

CamelLocalFolder *
camel_local_folder_construct(CamelLocalFolder *lf, CamelStore *parent_store, const char *full_name, guint32 flags, CamelException *ex)
{
	CamelFolderInfo *fi;
	CamelFolder *folder;
	const char *root_dir_path, *name;
	char *tmp, *statepath;
	char folder_path[PATH_MAX];
	struct stat st;
	int forceindex, len;
	CamelURL *url;
	
	folder = (CamelFolder *)lf;

	name = strrchr(full_name, '/');
	if (name)
		name++;
	else
		name = full_name;

	camel_folder_construct(folder, parent_store, full_name, name);

	root_dir_path = camel_local_store_get_toplevel_dir(CAMEL_LOCAL_STORE(folder->parent_store));
	/* strip the trailing '/' which is always present */
	len = strlen (root_dir_path);
	tmp = g_alloca (len + 1);
	strcpy (tmp, root_dir_path);
	if (len>1 && tmp[len-1] == '/')
		tmp[len-1] = 0;

	lf->base_path = g_strdup(root_dir_path);

	/* if the base store points to a file, then use that */
	if (stat(tmp, &st) != -1 && S_ISREG(st.st_mode)) {
		lf->folder_path = g_strdup(tmp);
		/* not really sure to do with these for now? */
		lf->summary_path = g_strdup_printf("%s.ev-summary", tmp);
		lf->index_path = g_strdup_printf("%s.ibex", tmp);
		statepath = g_strdup_printf("%s.cmeta", tmp);
	} else {
		lf->folder_path = CLOCALF_CLASS(lf)->get_full_path(root_dir_path, full_name);
		lf->summary_path = CLOCALF_CLASS(lf)->get_meta_path(root_dir_path, full_name, ".ev-summary");
		lf->index_path = CLOCALF_CLASS(lf)->get_meta_path(root_dir_path, full_name, ".ibex");
		statepath = CLOCALF_CLASS(lf)->get_meta_path(root_dir_path, full_name, ".cmeta");
	}
	camel_object_set(lf, NULL, CAMEL_OBJECT_STATE_FILE, statepath, NULL);
	g_free(statepath);

	lf->flags = flags;

	if (camel_object_state_read(lf) == -1) {
		/* No metadata - load defaults and persitify */
		camel_object_set(lf, NULL, CAMEL_LOCAL_FOLDER_INDEX_BODY, TRUE, 0);
		camel_object_state_write(lf);
	}

	/* follow any symlinks to the mailbox */
	if (lstat (lf->folder_path, &st) != -1 && S_ISLNK (st.st_mode) &&
	    realpath (lf->folder_path, folder_path) != NULL) {
		g_free (lf->folder_path);
		lf->folder_path = g_strdup (folder_path);
	}
	
	lf->changes = camel_folder_change_info_new();

	/* TODO: Remove the following line, it is a temporary workaround to remove
	   the old-format 'ibex' files that might be lying around */
	unlink(lf->index_path);

	/* FIXME: Need to run indexing off of the setv method */

	/* if we have no/invalid index file, force it */
	forceindex = camel_text_index_check(lf->index_path) == -1;
	if (lf->flags & CAMEL_STORE_FOLDER_BODY_INDEX) {
		int flag = O_RDWR|O_CREAT;

		if (forceindex)
			flag |= O_TRUNC;

		lf->index = (CamelIndex *)camel_text_index_new(lf->index_path, flag);
		if (lf->index == NULL) {
			/* yes, this isn't fatal at all */
			g_warning("Could not open/create index file: %s: indexing not performed", strerror (errno));
			forceindex = FALSE;
			/* record that we dont have an index afterall */
			lf->flags &= ~CAMEL_STORE_FOLDER_BODY_INDEX;
		}
	} else {
		/* if we do have an index file, remove it (?) */
		if (forceindex == FALSE)
			camel_text_index_remove(lf->index_path);
		forceindex = FALSE;
	}

	folder->summary = (CamelFolderSummary *)CLOCALF_CLASS(lf)->create_summary(lf->summary_path, lf->folder_path, lf->index);
	if (camel_local_summary_load((CamelLocalSummary *)folder->summary, forceindex, ex) == -1) {
		camel_exception_clear(ex);
	}
	
	/*if (camel_local_summary_check((CamelLocalSummary *)folder->summary, lf->changes, ex) == -1) {*/
	/* we sync here so that any hard work setting up the folder isn't lost */
	if (camel_local_summary_sync((CamelLocalSummary *)folder->summary, FALSE, lf->changes, ex) == -1) {
		camel_object_unref (CAMEL_OBJECT (folder));
		return NULL;
	}
	
	url = camel_url_copy (((CamelService *) parent_store)->url);
	camel_url_set_fragment (url, full_name);
	
	fi = g_new0 (CamelFolderInfo, 1);
	fi->full_name = g_strdup (full_name);
	fi->name = g_strdup (name);
	fi->uri = camel_url_to_string (url, 0);
	fi->unread = camel_folder_get_unread_message_count(folder);
	fi->flags = CAMEL_FOLDER_NOCHILDREN;
	
	camel_url_free (url);
	
	camel_object_trigger_event(CAMEL_OBJECT (parent_store), "folder_created", fi);
	camel_folder_info_free(fi);
	
	return lf;
}

/* lock the folder, may be called repeatedly (with matching unlock calls),
   with type the same or less than the first call */
int camel_local_folder_lock(CamelLocalFolder *lf, CamelLockType type, CamelException *ex)
{
	if (lf->locked > 0) {
		/* lets be anal here - its important the code knows what its doing */
		g_assert(lf->locktype == type || lf->locktype == CAMEL_LOCK_WRITE);
	} else {
		if (CLOCALF_CLASS(lf)->lock(lf, type, ex) == -1)
			return -1;
		lf->locktype = type;
	}

	lf->locked++;

	return 0;
}

/* unlock folder */
int camel_local_folder_unlock(CamelLocalFolder *lf)
{
	g_assert(lf->locked>0);
	lf->locked--;
	if (lf->locked == 0)
		CLOCALF_CLASS(lf)->unlock(lf);

	return 0;
}

static int
local_getv(CamelObject *object, CamelException *ex, CamelArgGetV *args)
{
	CamelFolder *folder = (CamelFolder *)object;
	int i;
	guint32 tag;

	for (i=0;i<args->argc;i++) {
		CamelArgGet *arg = &args->argv[i];

		tag = arg->tag;

		switch (tag & CAMEL_ARG_TAG) {
		case CAMEL_OBJECT_ARG_DESCRIPTION:
			if (folder->description == NULL) {
				char *tmp, *path;

				/* check some common prefixes to shorten the name */
				tmp = ((CamelService *)folder->parent_store)->url->path;
				if (tmp == NULL)
					goto skip;

				path = g_alloca (strlen (tmp) + strlen (folder->full_name) + 1);
				sprintf (path, "%s/%s", tmp, folder->full_name);

				if ((tmp = getenv("HOME")) && strncmp(tmp, path, strlen(tmp)) == 0)
					/* $HOME relative path + protocol string */
					folder->description = g_strdup_printf(_("~%s (%s)"), path+strlen(tmp),
									      ((CamelService *)folder->parent_store)->url->protocol);
				else if ((tmp = "/var/spool/mail") && strncmp(tmp, path, strlen(tmp)) == 0)
					/* /var/spool/mail relative path + protocol */
					folder->description = g_strdup_printf(_("mailbox:%s (%s)"), path+strlen(tmp),
									      ((CamelService *)folder->parent_store)->url->protocol);
				else if ((tmp = "/var/mail") && strncmp(tmp, path, strlen(tmp)) == 0)
					folder->description = g_strdup_printf(_("mailbox:%s (%s)"), path+strlen(tmp),
									      ((CamelService *)folder->parent_store)->url->protocol);
				else
					/* a full path + protocol */
					folder->description = g_strdup_printf(_("%s (%s)"), path, 
									      ((CamelService *)folder->parent_store)->url->protocol);
			}
			*arg->ca_str = folder->description;
			break;

		case CAMEL_OBJECT_ARG_PERSISTENT_PROPERTIES:
		case CAMEL_FOLDER_ARG_PROPERTIES: {
			CamelArgGetV props;

			props.argc = 1;
			props.argv[0] = *arg;
			((CamelObjectClass *)parent_class)->getv(object, ex, &props);
			*arg->ca_ptr = g_slist_concat(*arg->ca_ptr, g_slist_copy(local_folder_properties));

			break; }

		case CAMEL_LOCAL_FOLDER_ARG_INDEX_BODY:
			/* FIXME: remove this from sotre flags */
			*arg->ca_int = (((CamelLocalFolder *)folder)->flags & CAMEL_STORE_FOLDER_BODY_INDEX) != 0;
			break;

		default: skip:
			continue;
		}

		arg->tag = (tag & CAMEL_ARG_TYPE) | CAMEL_ARG_IGNORE;
	}

	return ((CamelObjectClass *)parent_class)->getv(object, ex, args);
}

static int
local_setv(CamelObject *object, CamelException *ex, CamelArgV *args)
{
	int i;
	guint32 tag;

	for (i=0;i<args->argc;i++) {
		CamelArg *arg = &args->argv[i];

		tag = arg->tag;

		switch (tag & CAMEL_ARG_TAG) {
		case CAMEL_LOCAL_FOLDER_ARG_INDEX_BODY:
			/* FIXME: implement */
			/* TODO: When turning on (off?) the index, we want to launch a task for it,
			   and make sure we dont have multiple tasks doing the same job */
			if (arg->ca_int)
				((CamelLocalFolder *)object)->flags |= CAMEL_STORE_FOLDER_BODY_INDEX;
			else
				((CamelLocalFolder *)object)->flags &= ~CAMEL_STORE_FOLDER_BODY_INDEX;
			break;
		default:
			continue;
		}

		arg->tag = (tag & CAMEL_ARG_TYPE) | CAMEL_ARG_IGNORE;
	}

	return ((CamelObjectClass *)parent_class)->setv(object, ex, args);
}

static char *
local_get_full_path (const char *toplevel_dir, const char *full_name)
{
	return g_strdup_printf ("%s/%s", toplevel_dir, full_name);
}

static char *
local_get_meta_path (const char *toplevel_dir, const char *full_name, const char *ext)
{
	return g_strdup_printf ("%s/%s%s", toplevel_dir, full_name, ext);
}

static int
local_lock(CamelLocalFolder *lf, CamelLockType type, CamelException *ex)
{
	return 0;
}

static void
local_unlock(CamelLocalFolder *lf)
{
	/* nothing */
}

/* for auto-check to work */
static void
local_refresh_info(CamelFolder *folder, CamelException *ex)
{
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;

	if (camel_local_summary_check((CamelLocalSummary *)folder->summary, lf->changes, ex) == -1)
		return;

	if (camel_folder_change_info_changed(lf->changes)) {
		camel_object_trigger_event((CamelObject *)folder, "folder_changed", lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}
}

static void
local_sync(CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelLocalFolder *lf = CAMEL_LOCAL_FOLDER(folder);

	d(printf("local sync '%s' , expunge=%s\n", folder->full_name, expunge?"true":"false"));

	if (camel_local_folder_lock(lf, CAMEL_LOCK_WRITE, ex) == -1)
		return;

	camel_object_state_write(lf);

	/* if sync fails, we'll pass it up on exit through ex */
	camel_local_summary_sync((CamelLocalSummary *)folder->summary, expunge, lf->changes, ex);
	camel_local_folder_unlock(lf);

	if (camel_folder_change_info_changed(lf->changes)) {
		camel_object_trigger_event(CAMEL_OBJECT(folder), "folder_changed", lf->changes);
		camel_folder_change_info_clear(lf->changes);
	}
}

static void
local_expunge(CamelFolder *folder, CamelException *ex)
{
	d(printf("expunge\n"));

	/* Just do a sync with expunge, serves the same purpose */
	/* call the callback directly, to avoid locking problems */
	CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(folder))->sync(folder, TRUE, ex);
}

static void
local_delete(CamelFolder *folder)
{
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;
	
	if (lf->index)
		camel_index_delete(lf->index);

	parent_class->delete(folder);
}

static void
local_rename(CamelFolder *folder, const char *newname)
{
	CamelLocalFolder *lf = (CamelLocalFolder *)folder;
	char *statepath;

	d(printf("renaming local folder paths to '%s'\n", newname));

	/* Sync? */

	g_free(lf->folder_path);
	g_free(lf->summary_path);
	g_free(lf->index_path);

	lf->folder_path = CLOCALF_CLASS(lf)->get_full_path(lf->base_path, newname);
	lf->summary_path = CLOCALF_CLASS(lf)->get_meta_path(lf->base_path, newname, ".ev-summary");
	lf->index_path = CLOCALF_CLASS(lf)->get_meta_path(lf->base_path, newname, ".ibex");
	statepath = CLOCALF_CLASS(lf)->get_meta_path(lf->base_path, newname, ".cmeta");
	camel_object_set(lf, NULL, CAMEL_OBJECT_STATE_FILE, statepath, NULL);
	g_free(statepath);

	/* FIXME: Poke some internals, sigh */
	camel_folder_summary_set_filename(folder->summary, lf->summary_path);
	g_free(((CamelLocalSummary *)folder->summary)->folder_path);
	((CamelLocalSummary *)folder->summary)->folder_path = g_strdup(lf->folder_path);

	parent_class->rename(folder, newname);
}

static GPtrArray *
local_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);
	GPtrArray *matches;

	CAMEL_LOCAL_FOLDER_LOCK(folder, search_lock);

	if (local_folder->search == NULL)
		local_folder->search = camel_folder_search_new();

	camel_folder_search_set_folder(local_folder->search, folder);
	camel_folder_search_set_body_index(local_folder->search, local_folder->index);
	matches = camel_folder_search_search(local_folder->search, expression, NULL, ex);

	CAMEL_LOCAL_FOLDER_UNLOCK(folder, search_lock);

	return matches;
}

static GPtrArray *
local_search_by_uids(CamelFolder *folder, const char *expression, GPtrArray *uids, CamelException *ex)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);
	GPtrArray *matches;

	if (uids->len == 0)
		return g_ptr_array_new();

	CAMEL_LOCAL_FOLDER_LOCK(folder, search_lock);

	if (local_folder->search == NULL)
		local_folder->search = camel_folder_search_new();

	camel_folder_search_set_folder(local_folder->search, folder);
	camel_folder_search_set_body_index(local_folder->search, local_folder->index);
	matches = camel_folder_search_search(local_folder->search, expression, uids, ex);

	CAMEL_LOCAL_FOLDER_UNLOCK(folder, search_lock);

	return matches;
}

static void
local_search_free(CamelFolder *folder, GPtrArray * result)
{
	CamelLocalFolder *local_folder = CAMEL_LOCAL_FOLDER(folder);

	/* we need to lock this free because of the way search_free_result works */
	/* FIXME: put the lock inside search_free_result */
	CAMEL_LOCAL_FOLDER_LOCK(folder, search_lock);

	camel_folder_search_free_result(local_folder->search, result);

	CAMEL_LOCAL_FOLDER_UNLOCK(folder, search_lock);
}
