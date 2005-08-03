/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libxml/tree.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-i18n.h>

#include <camel/camel-session.h>
#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-vee-store.h>
#include <camel/camel-vtrash-folder.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-file-utils.h>
#include <camel/camel-stream-fs.h>

#include "e-util/e-mktemp.h"
#include "e-util/e-request.h"
#include "e-util/e-icon-factory.h"

#include "e-util/e-error.h"

#include "em-vfolder-rule.h"

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-config.h"
#include "mail-component.h"
#include "mail-vfolder.h"

#include "em-utils.h"
#include "em-popup.h"
#include "em-marshal.h"
#include "em-folder-tree.h"
#include "em-folder-tree-model.h"
#include "em-folder-utils.h"
#include "em-folder-selector.h"
#include "em-folder-selection.h"
#include "em-folder-properties.h"

#define d(x)

extern CamelSession *session;

static gboolean
emfu_is_special_local_folder (const char *name)
{
	return (!strcmp (name, "Drafts") || !strcmp (name, "Inbox") || !strcmp (name, "Outbox") || !strcmp (name, "Sent"));
}

struct _EMCopyFolders {
	struct _mail_msg msg;
	
	/* input data */
	CamelStore *fromstore;
	CamelStore *tostore;
	
	char *frombase;
	char *tobase;
	
	int delete;
};

static char *
emft_copy_folders__desc (struct _mail_msg *mm, int complete)
{
	struct _EMCopyFolders *m = (struct _EMCopyFolders *) mm;
	
	return g_strdup_printf (_("Copying `%s' to `%s'"), m->frombase, m->tobase);
}

static void
emft_copy_folders__copy (struct _mail_msg *mm)
{
	struct _EMCopyFolders *m = (struct _EMCopyFolders *) mm;
	guint32 flags = CAMEL_STORE_FOLDER_INFO_FAST | CAMEL_STORE_FOLDER_INFO_RECURSIVE | CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
	GList *pending = NULL, *deleting = NULL, *l;
	GString *fromname, *toname;
	CamelFolderInfo *fi;
	const char *tmp;
	int fromlen;
	
	if (!(fi = camel_store_get_folder_info (m->fromstore, m->frombase, flags, &mm->ex)))
		return;
	
	pending = g_list_append (pending, fi);
	
	toname = g_string_new ("");
	fromname = g_string_new ("");
	
	tmp = strrchr (m->frombase, '/');
	if (tmp == NULL)
		fromlen = 0;
	else
		fromlen = tmp - m->frombase + 1;
	
	d(printf ("top name is '%s'\n", fi->full_name));
	
	while (pending) {
		CamelFolderInfo *info = pending->data;
		
		pending = g_list_remove_link (pending, pending);
		while (info) {
			CamelFolder *fromfolder, *tofolder;
			GPtrArray *uids;
			int deleted = 0;
			
			if (info->child)
				pending = g_list_append (pending, info->child);
			
			if (m->tobase[0])
				g_string_printf (toname, "%s/%s", m->tobase, info->full_name + fromlen);
			else
				g_string_printf (toname, "%s", info->full_name + fromlen);
			
			d(printf ("Copying from '%s' to '%s'\n", info->full_name, toname->str));
			
			/* This makes sure we create the same tree, e.g. from a nonselectable source */
			/* Not sure if this is really the 'right thing', e.g. for spool stores, but it makes the ui work */
			if ((info->flags & CAMEL_FOLDER_NOSELECT) == 0) {
				d(printf ("this folder is selectable\n"));
				if (m->tostore == m->fromstore && m->delete) {
					camel_store_rename_folder (m->fromstore, info->full_name, toname->str, &mm->ex);
					if (camel_exception_is_set (&mm->ex))
						goto exception;
					
					/* this folder no longer exists, unsubscribe it */
					if (camel_store_supports_subscriptions (m->fromstore))
						camel_store_unsubscribe_folder (m->fromstore, info->full_name, NULL);
					
					deleted = 1;
				} else {
					if (!(fromfolder = camel_store_get_folder (m->fromstore, info->full_name, 0, &mm->ex)))
						goto exception;
					
					if (!(tofolder = camel_store_get_folder (m->tostore, toname->str, CAMEL_STORE_FOLDER_CREATE, &mm->ex))) {
						camel_object_unref (fromfolder);
						goto exception;
					}
					
					uids = camel_folder_get_uids (fromfolder);
					camel_folder_transfer_messages_to (fromfolder, uids, tofolder, NULL, m->delete, &mm->ex);
					camel_folder_free_uids (fromfolder, uids);
					
					if (m->delete)
						camel_folder_sync(fromfolder, TRUE, NULL);
					
					camel_object_unref (fromfolder);
					camel_object_unref (tofolder);
				}
			}
			
			if (camel_exception_is_set (&mm->ex))
				goto exception;
			else if (m->delete && !deleted)
				deleting = g_list_prepend (deleting, info);
			
			/* subscribe to the new folder if appropriate */
			if (camel_store_supports_subscriptions (m->tostore)
			    && !camel_store_folder_subscribed (m->tostore, toname->str))
				camel_store_subscribe_folder (m->tostore, toname->str, NULL);
			
			info = info->next;
		}
	}
	
	/* delete the folders in reverse order from how we copyied them, if we are deleting any */
	l = deleting;
	while (l) {
		CamelFolderInfo *info = l->data;
		
		d(printf ("deleting folder '%s'\n", info->full_name));
		
		/* FIXME: we need to do something with the exception
		   since otherwise the users sees a failed operation
		   with no error message or even any warnings */
		if (camel_store_supports_subscriptions (m->fromstore))
			camel_store_unsubscribe_folder (m->fromstore, info->full_name, NULL);
		
		camel_store_delete_folder (m->fromstore, info->full_name, NULL);
		l = l->next;
	}
	
 exception:
	
	camel_store_free_folder_info (m->fromstore, fi);
	g_list_free (deleting);
	
	g_string_free (toname, TRUE);
	g_string_free (fromname, TRUE);
}

static void
emft_copy_folders__free (struct _mail_msg *mm)
{
	struct _EMCopyFolders *m = (struct _EMCopyFolders *) mm;
	
	camel_object_unref (m->fromstore);
	camel_object_unref (m->tostore);
	
	g_free (m->frombase);
	g_free (m->tobase);
}

static struct _mail_msg_op copy_folders_op = {
	emft_copy_folders__desc,
	emft_copy_folders__copy,
	NULL,
	emft_copy_folders__free,
};

static void
emfu_copy_folders (CamelStore *tostore, const char *tobase, CamelStore *fromstore, const char *frombase, int delete)
{
	struct _EMCopyFolders *m;
	
	m = mail_msg_new (&copy_folders_op, NULL, sizeof (struct _EMCopyFolders));
	camel_object_ref (fromstore);
	m->fromstore = fromstore;
	camel_object_ref (tostore);
	m->tostore = tostore;
	m->frombase = g_strdup (frombase);
	m->tobase = g_strdup (tobase);
	m->delete = delete;
	
	e_thread_put (mail_thread_new, (EMsg *) m);
}

struct _copy_folder_data {
	CamelFolderInfo *fi;
	gboolean delete;
};

static void
emfu_copy_folder_selected (const char *uri, void *data)
{
	struct _copy_folder_data *cfd = data;
	CamelStore *fromstore = NULL, *tostore = NULL;
	char *tobase = NULL;
	CamelException ex;
	CamelURL *url;

	if (uri == NULL) {
		g_free (cfd);
		return;
	}
	
	camel_exception_init (&ex);

	if (!(fromstore = camel_session_get_store (session, cfd->fi->uri, &ex))) {
		e_error_run(NULL,
			    cfd->delete?"mail:no-move-folder-notexist":"mail:no-copy-folder-notexist", cfd->fi->full_name, uri, ex.desc, NULL);
		goto fail;
	}
	
	if (cfd->delete && fromstore == mail_component_peek_local_store (NULL) && emfu_is_special_local_folder (cfd->fi->full_name)) {
		e_error_run(NULL,
			    "mail:no-rename-special-folder", cfd->fi->full_name, NULL);
		goto fail;
	}
	
	if (!(tostore = camel_session_get_store (session, uri, &ex))) {
		e_error_run(NULL,
			    cfd->delete?"mail:no-move-folder-to-notexist":"mail:no-copy-folder-to-notexist", cfd->fi->full_name, uri, ex.desc, NULL);
		goto fail;
	}

	url = camel_url_new (uri, NULL);
	if (((CamelService *)tostore)->provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
		tobase = url->fragment;
	else if (url->path && url->path[0])
		tobase = url->path+1;
	if (tobase == NULL)
		tobase = "";

	emfu_copy_folders (tostore, tobase, fromstore, cfd->fi->full_name, cfd->delete);
	
	camel_url_free (url);
fail:
	if (fromstore)
		camel_object_unref(fromstore);
	if (tostore)
		camel_object_unref(tostore);
	camel_exception_clear (&ex);
	g_free (cfd);
}

/* tree here is the 'destination' selector, not 'self' */
static gboolean
emfu_copy_folder_exclude(EMFolderTree *tree, GtkTreeModel *model, GtkTreeIter *iter, void *data)
{
	struct _copy_folder_data *cfd = data;
	int fromvfolder, tovfolder;
	char *touri;
	guint flags;
	gboolean is_store;

	/* handles moving to/from vfolders */

	fromvfolder = strncmp(cfd->fi->uri, "vfolder:", 8) == 0;
	gtk_tree_model_get(model, iter, COL_STRING_URI, &touri, COL_UINT_FLAGS, &flags, COL_BOOL_IS_STORE, &is_store, -1);
	tovfolder = strncmp(touri, "vfolder:", 8) == 0;
	g_free(touri);

	/* moving from vfolder to normal- not allowed */
	if (fromvfolder && !tovfolder && cfd->delete)
		return FALSE;
	/* copy/move from normal folder to vfolder - not allowed */
	if (!fromvfolder && tovfolder)
		return FALSE;
	/* copying to vfolder - not allowed */
	if (tovfolder && !cfd->delete)
		return FALSE;

	return (flags & EMFT_EXCLUDE_NOINFERIORS) == 0;
}

/* FIXME: this interface references the folderinfo without copying it  */
/* FIXME: these functions must be documented */
void
em_folder_utils_copy_folder (CamelFolderInfo *folderinfo)
{
	struct _copy_folder_data *cfd;
	
	cfd = g_malloc (sizeof (*cfd));
	cfd->fi = folderinfo;
	cfd->delete = FALSE;

	em_select_folder (NULL, _("Select folder"), _("C_opy"),
			  NULL, emfu_copy_folder_exclude,
			  emfu_copy_folder_selected, cfd);}


/* FIXME: this interface references the folderinfo without copying it  */
/* FIXME: these functions must be documented */
void
em_folder_utils_move_folder (CamelFolderInfo *folderinfo)
{
	struct _copy_folder_data *cfd;

	cfd = g_malloc (sizeof (*cfd));
	cfd->fi = folderinfo;
	cfd->delete = TRUE;

	em_select_folder (NULL, _("Select folder"), _("_Move"),
			  NULL, emfu_copy_folder_exclude,
			  emfu_copy_folder_selected, cfd);
}

static void
emfu_delete_rec (CamelStore *store, CamelFolderInfo *fi, CamelException *ex)
{
	while (fi) {
		CamelFolder *folder;
		
		if (fi->child) {
			emfu_delete_rec (store, fi->child, ex);
			if (camel_exception_is_set (ex))
				return;
		}
		
		d(printf ("deleting folder '%s'\n", fi->full_name));
		
		/* shouldn't camel do this itself? */
		if (camel_store_supports_subscriptions (store))
			camel_store_unsubscribe_folder (store, fi->full_name, NULL);
		
		if (!(folder = camel_store_get_folder (store, fi->full_name, 0, ex)))
			return;
		
		if (!CAMEL_IS_VEE_FOLDER (folder)) {
			GPtrArray *uids = camel_folder_get_uids (folder);
			int i;
			
			camel_folder_freeze (folder);
			for (i = 0; i < uids->len; i++)
				camel_folder_delete_message (folder, uids->pdata[i]);
			
			camel_folder_free_uids (folder, uids);
			
			camel_folder_sync (folder, TRUE, NULL);
			camel_folder_thaw (folder);
		}
		
		camel_store_delete_folder (store, fi->full_name, ex);
		if (camel_exception_is_set (ex))
			return;
		
		fi = fi->next;
	}
}

static void
emfu_delete_folders (CamelStore *store, const char *full_name, CamelException *ex)
{
	guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE | CAMEL_STORE_FOLDER_INFO_FAST | CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
	CamelFolderInfo *fi;
	
	fi = camel_store_get_folder_info (store, full_name, flags, ex);
	if (camel_exception_is_set (ex))
		return;
	
	emfu_delete_rec (store, fi, ex);
	camel_store_free_folder_info (store, fi);
}

static void
emfu_delete_response (GtkWidget *dialog, int response, gpointer data)
{
	CamelStore *store;
	CamelException ex;
	char *full_name;
	
	full_name = g_object_get_data ((GObject *) dialog, "full_name");
	store = g_object_get_data ((GObject *) dialog, "store");
	
	if (response == GTK_RESPONSE_OK) {
		camel_exception_init (&ex);
		emfu_delete_folders (store, full_name, &ex);
		if (camel_exception_is_set (&ex)) {
			e_error_run(NULL,
				    "mail:no-delete-folder", full_name, ex.desc, NULL);
			camel_exception_clear (&ex);
		}
	}
	
	gtk_widget_destroy (dialog);
}

/* FIXME: these functions must be documented */
void
em_folder_utils_delete_folder (CamelFolder *folder)
{
	CamelStore *local;
	GtkWidget *dialog;

	local = mail_component_peek_local_store (NULL);
	
	if (folder->parent_store == local && emfu_is_special_local_folder (folder->full_name)) {
		e_error_run(NULL, "mail:no-delete-special-folder", folder->full_name, NULL);
		return;
	}

	camel_object_ref (folder->parent_store);

	dialog = e_error_new(NULL,
			     (folder->parent_store && CAMEL_IS_VEE_STORE(folder->parent_store))?"mail:ask-delete-vfolder":"mail:ask-delete-folder",
			     folder->full_name, NULL);
	g_object_set_data_full ((GObject *) dialog, "full_name", g_strdup (folder->full_name), g_free);
	g_object_set_data_full ((GObject *) dialog, "store", folder->parent_store, camel_object_unref);
	g_signal_connect (dialog, "response", G_CALLBACK (emfu_delete_response), NULL);
	gtk_widget_show (dialog);
}

/* FIXME: this must become threaded */
/* FIXME: these functions must be documented */
void
em_folder_utils_rename_folder (CamelFolder *folder)
{
	char *prompt, *new_name;
	const char *p;
	CamelStore *local;
	gboolean done = FALSE;
	size_t base_len;

	local = mail_component_peek_local_store (NULL);

	/* don't allow user to rename one of the special local folders */
	if (folder->parent_store == local && emfu_is_special_local_folder (folder->full_name)) {
		e_error_run(NULL,
			    "mail:no-rename-special-folder", folder->full_name, NULL);
		return;
	}
	
	if ((p = strrchr (folder->full_name, '/')))
		base_len = (size_t) (p - folder->full_name);
	else
		base_len = 0;
	
	prompt = g_strdup_printf (_("Rename the \"%s\" folder to:"), folder->name);
	while (!done) {
		new_name = e_request_string (NULL, _("Rename Folder"), prompt, folder->name);
		if (new_name == NULL || !strcmp (folder->name, new_name)) {
			/* old name == new name */
			done = TRUE;
		} else if (strchr(new_name, '/') != NULL) {
			e_error_run(NULL,
				    "mail:no-rename-folder", folder->name, new_name, _("Folder names cannot contain '/'"), NULL);
			done = TRUE;
		} else {
			CamelFolderInfo *fi;
			CamelException ex;
			char *path, *p;
			
			if (base_len > 0) {
				path = g_malloc (base_len + strlen (new_name) + 2);
				memcpy (path, folder->full_name, base_len);
				p = path + base_len;
				*p++ = '/';
				strcpy (p, new_name);
			} else {
				path = g_strdup (new_name);
			}
			
			camel_exception_init (&ex);
			if ((fi = camel_store_get_folder_info (folder->parent_store, path, CAMEL_STORE_FOLDER_INFO_FAST, &ex)) != NULL) {
				camel_store_free_folder_info (folder->parent_store, fi);
				e_error_run(NULL,
					    "mail:no-rename-folder-exists", folder->name, new_name, NULL);
			} else {
				const char *oldpath, *newpath;
				
				oldpath = folder->full_name;
				newpath = path;
				
				d(printf ("renaming %s to %s\n", oldpath, newpath));
				
				camel_exception_clear (&ex);
				camel_store_rename_folder (folder->parent_store, oldpath, newpath, &ex);
				if (camel_exception_is_set (&ex)) {
					e_error_run(NULL,
						    "mail:no-rename-folder", oldpath, newpath, ex.desc, NULL);
					camel_exception_clear (&ex);
				}
				
				done = TRUE;
			}
			
			g_free (path);
		}
		
		g_free (new_name);
	}
}

struct _EMCreateFolder {
	struct _mail_msg msg;
	
	/* input data */
	CamelStore *store;
	char *full_name;
	char *parent;
	char *name;
	
	/* output data */
	CamelFolderInfo *fi;
	
	/* callback data */
	void (* done) (CamelFolderInfo *fi, void *user_data);
	void *user_data;
};

static char *
emfu_create_folder__desc (struct _mail_msg *mm, int done)
{
	struct _EMCreateFolder *m = (struct _EMCreateFolder *) mm;
	
	return g_strdup_printf (_("Creating folder `%s'"), m->full_name);
}

static void
emfu_create_folder__create (struct _mail_msg *mm)
{
	struct _EMCreateFolder *m = (struct _EMCreateFolder *) mm;
	
	d(printf ("creating folder parent='%s' name='%s' full_name='%s'\n", m->parent, m->name, m->full_name));
	
	if ((m->fi = camel_store_create_folder (m->store, m->parent, m->name, &mm->ex))) {
		if (camel_store_supports_subscriptions (m->store))
			camel_store_subscribe_folder (m->store, m->full_name, &mm->ex);
	}
}

static void
emfu_create_folder__created (struct _mail_msg *mm)
{
	struct _EMCreateFolder *m = (struct _EMCreateFolder *) mm;
	
	if (m->done)
		m->done (m->fi, m->user_data);
}

static void
emfu_create_folder__free (struct _mail_msg *mm)
{
	struct _EMCreateFolder *m = (struct _EMCreateFolder *) mm;
	
	camel_store_free_folder_info (m->store, m->fi);
	camel_object_unref (m->store);
	g_free (m->full_name);
	g_free (m->parent);
	g_free (m->name);
}

static struct _mail_msg_op create_folder_op = {
	emfu_create_folder__desc,
	emfu_create_folder__create,
	emfu_create_folder__created,
	emfu_create_folder__free,
};


static int
emfu_create_folder_real (CamelStore *store, const char *full_name, void (* done) (CamelFolderInfo *fi, void *user_data), void *user_data)
{
	char *name, *namebuf = NULL;
	struct _EMCreateFolder *m;
	const char *parent;
	int id;
	
	namebuf = g_strdup (full_name);
	if (!(name = strrchr (namebuf, '/'))) {
		name = namebuf;
		parent = "";
	} else {
		*name++ = '\0';
		parent = namebuf;
	}
	
	m = mail_msg_new (&create_folder_op, NULL, sizeof (struct _EMCreateFolder));
	camel_object_ref (store);
	m->store = store;
	m->full_name = g_strdup (full_name);
	m->parent = g_strdup (parent);
	m->name = g_strdup (name);
	m->user_data = user_data;
	m->done = done;
	
	g_free (namebuf);
	
	id = m->msg.seq;
	e_thread_put (mail_thread_new, (EMsg *) m);
	
	return id;
}

static void
new_folder_created_cb (CamelFolderInfo *fi, void *user_data)
{
	EMFolderSelector *emfs = user_data;
	
	if (fi)
		gtk_widget_destroy ((GtkWidget *) emfs);
	
	g_object_unref (emfs);
}

static void
emfu_popup_new_folder_response (EMFolderSelector *emfs, int response, gpointer data)
{
	EMFolderTreeModelStoreInfo *si;
	const char *uri, *path;
	CamelException ex;
	CamelStore *store;
	
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy ((GtkWidget *) emfs);
		return;
	}

	uri = em_folder_selector_get_selected_uri (emfs);
	path = em_folder_selector_get_selected_path (emfs);

	d(printf ("Creating new folder: %s (%s)\n", path, uri));

	g_print ("DEBUG: %s (%s)\n", path, uri);

	camel_exception_init (&ex);
	if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
		camel_exception_clear (&ex);
		return;
	}

	if (!(si = em_folder_tree_get_model_storeinfo (emfs->emft, store))) {
		g_assert_not_reached ();
		camel_object_unref (store);
		return;
	}

	/* HACK: we need to create vfolders using the vfolder editor */
	if (CAMEL_IS_VEE_STORE(store)) {
		EMVFolderRule *rule;

		rule = em_vfolder_rule_new();
		filter_rule_set_name((FilterRule *)rule, path);
		vfolder_gui_add_rule(rule);
		gtk_widget_destroy((GtkWidget *)emfs);
	} else {
		g_object_ref (emfs);
		emfu_create_folder_real (si->store, path, new_folder_created_cb, emfs);
	}

	camel_object_unref (store);
	camel_exception_clear (&ex);
}

/* FIXME: these functions must be documented */
void
em_folder_utils_create_folder (CamelFolderInfo *folderinfo) {
	EMFolderTree *folder_tree;
	EMFolderTreeModel *model;
	GtkWidget *dialog;

	model = mail_component_peek_tree_model (mail_component_peek ());
	folder_tree = (EMFolderTree *) em_folder_tree_new_with_model (model);
	
	dialog = em_folder_selector_create_new (folder_tree, 0, _("Create folder"), _("Specify where to create the folder:"));
	if (folderinfo != NULL)
		em_folder_selector_set_selected ((EMFolderSelector *) dialog, folderinfo->uri);
	g_signal_connect (dialog, "response", G_CALLBACK (emfu_popup_new_folder_response), folder_tree);
	gtk_widget_show (dialog);
}
