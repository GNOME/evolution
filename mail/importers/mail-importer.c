/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-importer.c
 *
 * Authors: Iain Holmes <iain@ximian.com>
 *          Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2001-2003 Ximian, Inc.
 * Copyright (C) 2004 Novell Inc.
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

#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <gmodule.h>
#include <libgnome/gnome-util.h>
#include <camel/camel-folder.h>
#include <camel/camel-store.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-mime-parser.h>
#include <camel/camel-exception.h>
#include <camel/camel-stream-mem.h>
#include <e-util/e-path.h>

#include "mail/mail-mt.h"
#include "mail/mail-component.h"
#include "mail/mail-tools.h"

#include "mail-importer.h"

/**
 * mail_importer_make_local_folder:
 * @folderpath: 
 * 
 * Check a local folder exists at path @folderpath, and if not, create it.
 * 
 * Return value: The physical uri of the folder, or NULL if the folder did
 * not exist and could not be created.
 **/
char *
mail_importer_make_local_folder(const char *folderpath)
{
	return g_strdup_printf("mbox:/home/notzed/.evolution/mail/local/%s", folderpath);
}

/**
 * mail_importer_add_line:
 * importer: A MailImporter structure.
 * str: Next line of the mbox.
 * finished: TRUE if @str is the last line of the message.
 *
 * Adds lines to the message until it is finished, and then adds
 * the complete message to the folder.
 */
void
mail_importer_add_line (MailImporter *importer,
			const char *str,
			gboolean finished)
{
	CamelMimeMessage *msg;
	CamelMessageInfo *info;
	CamelException *ex;
	
	if (importer->mstream == NULL)
		importer->mstream = CAMEL_STREAM_MEM (camel_stream_mem_new ());

	camel_stream_write (CAMEL_STREAM (importer->mstream), str,  strlen (str));
	
	if (finished == FALSE)
		return;

	camel_stream_reset (CAMEL_STREAM (importer->mstream));
	info = g_new0 (CamelMessageInfo, 1);
	info->flags = CAMEL_MESSAGE_SEEN;

	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg),
						  CAMEL_STREAM (importer->mstream));
	
	camel_object_unref (importer->mstream);
	importer->mstream = NULL;

	ex = camel_exception_new ();
	camel_folder_append_message (importer->folder, msg, info, NULL, ex);
	camel_object_unref (msg);

	camel_exception_free (ex);
	g_free (info);
}

struct _BonoboObject *mail_importer_factory_cb(struct _BonoboGenericFactory *factory, const char *iid, void *data)
{
	if (strcmp(iid, ELM_INTELLIGENT_IMPORTER_IID) == 0)
		return elm_intelligent_importer_new();
	else if (strcmp(iid, PINE_INTELLIGENT_IMPORTER_IID) == 0)
		return pine_intelligent_importer_new();
	else if (strcmp(iid, NETSCAPE_INTELLIGENT_IMPORTER_IID) == 0)
		return netscape_intelligent_importer_new();
	else if (strcmp(iid, MBOX_IMPORTER_IID) == 0)
		return mbox_importer_new();
	else if (strcmp(iid, OUTLOOK_IMPORTER_IID) == 0)
		return outlook_importer_new();

	return NULL;
}

struct _import_mbox_msg {
	struct _mail_msg msg;
	
	char *path;
	char *uri;
	CamelOperation *cancel;
};

static char *
import_mbox_describe(struct _mail_msg *mm, int complete)
{
	return g_strdup (_("Importing mailbox"));
}

static struct {
	char tag;
	guint32 mozflag;
	guint32 flag;
} status_flags[] = {
	{ 'F', MSG_FLAG_MARKED, CAMEL_MESSAGE_FLAGGED },
	{ 'A', MSG_FLAG_REPLIED, CAMEL_MESSAGE_ANSWERED },
	{ 'D', MSG_FLAG_EXPUNGED, CAMEL_MESSAGE_DELETED },
	{ 'R', MSG_FLAG_READ, CAMEL_MESSAGE_SEEN },
};

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

static guint32
decode_mozilla_status(const char *tmp)
{
	unsigned long status = strtoul(tmp, NULL, 16);
	guint32 flags = 0;
	int i;

	for (i=0;i<sizeof(status_flags)/sizeof(status_flags[0]);i++)
		if (status_flags[i].mozflag & status)
			flags |= status_flags[i].flag;
	return flags;
}

static void
import_mbox_import(struct _mail_msg *mm)
{
	struct _import_mbox_msg *m = (struct _import_mbox_msg *) mm;
	CamelFolder *folder;
	CamelMimeParser *mp = NULL;
	struct stat st;
	int fd;
	CamelMessageInfo *info;

	if (stat(m->path, &st) == -1) {
		g_warning("cannot find source file to import '%s': %s", m->path, g_strerror(errno));
		return;
	}

	if (m->uri == NULL || m->uri[0] == 0)
		folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_INBOX);
	else
		folder = mail_tool_uri_to_folder(m->uri, CAMEL_STORE_FOLDER_CREATE, &mm->ex);

	if (folder == NULL)
		return;

	if (S_ISREG(st.st_mode)) {
		CamelOperation *oldcancel = NULL;

		fd = open(m->path, O_RDONLY);
		if (fd == -1) {
			g_warning("cannot find source file to import '%s': %s", m->path, g_strerror(errno));
			goto fail1;
		}

		mp = camel_mime_parser_new();
		camel_mime_parser_scan_from(mp, TRUE);
		if (camel_mime_parser_init_with_fd(mp, fd) == -1) {
			goto fail2;
		}

		if (m->cancel)
			oldcancel = camel_operation_register(m->cancel);

		camel_operation_start(NULL, _("Importing `%s'"), folder->full_name);
		camel_folder_freeze(folder);
		while (camel_mime_parser_step(mp, 0, 0) == CAMEL_MIME_PARSER_STATE_FROM) {
			CamelMimeMessage *msg;
			const char *tmp;
			int pc;

			if (st.st_size > 0)
				pc = (int)(100.0 * ((double)camel_mime_parser_tell(mp) / (double)st.st_size));
			camel_operation_progress(NULL, pc);

			msg = camel_mime_message_new();
			if (camel_mime_part_construct_from_parser((CamelMimePart *)msg, mp) == -1) {
				/* set exception? */
				camel_object_unref(msg);
				break;
			}

			info = camel_message_info_new();

			tmp = camel_medium_get_header((CamelMedium *)msg, "X-Mozilla-Status");
			if (tmp)
				info->flags |= decode_mozilla_status(tmp);
			tmp = camel_medium_get_header((CamelMedium *)msg, "Status");
			if (tmp)
				info->flags |= decode_status(tmp);
			tmp = camel_medium_get_header((CamelMedium *)msg, "X-Status");
			if (tmp)
				info->flags |= decode_status(tmp);

			camel_folder_append_message(folder, msg, info, NULL, &mm->ex);
			camel_message_info_free(info);
			camel_object_unref(msg);

			if (camel_exception_is_set(&mm->ex))
				break;

			camel_mime_parser_step(mp, 0, 0);
		}
		camel_folder_sync(folder, FALSE, NULL);
		camel_folder_thaw(folder);
		camel_operation_end(NULL);
		/* TODO: these api's are a bit weird, registering the old is the same as deregistering */
		if (m->cancel)
			camel_operation_register(oldcancel);
	fail2:
		camel_object_unref(mp);
	}
fail1:
	camel_folder_sync(folder, FALSE, NULL);
	camel_object_unref(folder);
}

static void
import_mbox_done(struct _mail_msg *mm)
{
}

static void
import_mbox_free (struct _mail_msg *mm)
{
	struct _import_mbox_msg *m = (struct _import_mbox_msg *)mm;
	
	if (m->cancel)
		camel_operation_unref(m->cancel);
	g_free(m->uri);
	g_free(m->path);
}

static struct _mail_msg_op import_mbox_op = {
	import_mbox_describe,
	import_mbox_import,
	import_mbox_done,
	import_mbox_free,
};

int
mail_importer_import_mbox(const char *path, const char *folderuri, CamelOperation *cancel)
{
	struct _import_mbox_msg *m;
	int id;

	m = mail_msg_new(&import_mbox_op, NULL, sizeof (*m));
	m->path = g_strdup(path);
	m->uri = g_strdup(folderuri);
	if (cancel) {
		m->cancel = cancel;
		camel_operation_ref(cancel);
	}

	id = m->msg.seq;
	e_thread_put(mail_thread_queued, (EMsg *)m);

	return id;
}

void
mail_importer_import_mbox_sync(const char *path, const char *folderuri, CamelOperation *cancel)
{
	struct _import_mbox_msg *m;

	m = mail_msg_new(&import_mbox_op, NULL, sizeof (*m));
	m->path = g_strdup(path);
	m->uri = g_strdup(folderuri);
	if (cancel) {
		m->cancel = cancel;
		camel_operation_ref(cancel);
	}

	import_mbox_import(&m->msg);
	import_mbox_done(&m->msg);
	mail_msg_free(&m->msg);
}

struct _import_folders_data {
	MailImporterSpecial *special_folders;
	CamelOperation *cancel;

	int elmfmt:1;
};

static void
import_folders_rec(struct _import_folders_data *m, const char *filepath, const char *folderparent)
{
	DIR *dir;
	struct dirent *d;
	struct stat st;
	char *filefull, *foldersub, *uri;
	const char *folder;

	dir = opendir(filepath);
	if (dir == NULL)
		return;

	camel_operation_start(NULL, _("Scanning %s"), filepath);

	while ( (d=readdir(dir)) ) {
		if (d->d_name[0] == '.')
			continue;

		filefull = g_build_filename(filepath, d->d_name, NULL);

		/* skip non files and directories, and skip directories in mozilla mode */
		if (stat(filefull, &st) == -1
		    || !(S_ISREG(st.st_mode)
			 || (m->elmfmt && S_ISDIR(st.st_mode)))) {
			g_free(filefull);
			continue;
		}

		folder = d->d_name;
		if (folderparent == NULL) {
			int i;

			for (i=0;m->special_folders[i].orig;i++)
				if (strcmp(m->special_folders[i].orig, folder) == 0) {
					folder = m->special_folders[i].new;
					break;
				}
			/* FIXME: need a better way to get default store location */
			uri = g_strdup_printf("mbox:%s/mail/local#%s", mail_component_peek_base_directory(NULL), folder);
		} else {
			uri = g_strdup_printf("mbox:%s/mail/local#%s/%s", mail_component_peek_base_directory(NULL), folderparent, folder);
		}

		printf("importing to uri %s\n", uri);
		mail_importer_import_mbox_sync(filefull, uri, m->cancel);
		g_free(uri);

		/* This little gem re-uses the stat buffer and filefull to automagically scan mozilla-format folders */
		if (!m->elmfmt) {
			char *tmp = g_strdup_printf("%s.sbd", filefull);

			g_free(filefull);
			filefull = tmp;
			if (stat(filefull, &st) == -1) {
				g_free(filefull);
				continue;
			}
		}

		if (S_ISDIR(st.st_mode)) {
			foldersub = folderparent?g_strdup_printf("%s/%s", folderparent, folder):g_strdup(folder);
			import_folders_rec(m, filefull, foldersub);
			g_free(foldersub);
		}

		g_free(filefull);
	}

	camel_operation_end(NULL);
}

/**
 * mail_importer_import_folders_sync:
 * @filepath: 
 * @: 
 * @flags: 
 * @cancel: 
 * 
 * import from a base path @filepath into the root local folder tree,
 * scanning all sub-folders.
 *
 * if @flags is MAIL_IMPORTER_MOZFMT, then subfolders are assumed to
 * be in mozilla/evolutoin 1.5 format, appending '.sbd' to the
 * directory name. Otherwise they are in elm/mutt/pine format, using
 * standard unix directories.
 **/
void
mail_importer_import_folders_sync(const char *filepath, MailImporterSpecial special_folders[], int flags, CamelOperation *cancel)
{
	struct _import_folders_data m;
	CamelOperation *oldcancel = NULL;

	m.special_folders = special_folders;
	m.elmfmt = (flags & MAIL_IMPORTER_MOZFMT) == 0;
	m.cancel = cancel;

	if (cancel)
		oldcancel = camel_operation_register(cancel);

	import_folders_rec(&m, filepath, NULL);

	if (cancel)
		camel_operation_register(oldcancel);
}
