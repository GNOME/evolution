/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Iain Holmes <iain@ximian.com>
 *      Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <gmodule.h>
#include <glib/gi18n.h>
#include <camel/camel-folder.h>
#include <camel/camel-store.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-mime-parser.h>
#include <camel/camel-exception.h>
#include <camel/camel-stream-mem.h>

#include "e-util/e-util-private.h"

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
gchar *
mail_importer_make_local_folder(const gchar *folderpath)
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
			const gchar *str,
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
	info = camel_message_info_new(NULL);
	camel_message_info_set_flags(info, CAMEL_MESSAGE_SEEN, ~0);

	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg),
						  CAMEL_STREAM (importer->mstream));

	camel_object_unref (importer->mstream);
	importer->mstream = NULL;

	ex = camel_exception_new ();
	camel_folder_append_message (importer->folder, msg, info, NULL, ex);
	camel_object_unref (msg);

	camel_exception_free (ex);
	camel_message_info_free(info);
}

struct _BonoboObject *mail_importer_factory_cb(struct _BonoboGenericFactory *factory, const gchar *iid, gpointer data)
{
#if 0
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
#endif
	return NULL;
}

struct _import_mbox_msg {
	MailMsg base;

	gchar *path;
	gchar *uri;
	CamelOperation *cancel;

	void (*done)(gpointer data, CamelException *ex);
	gpointer done_data;
};

static gchar *
import_mbox_desc (struct _import_mbox_msg *m)
{
	return g_strdup (_("Importing mailbox"));
}

static struct {
	gchar tag;
	guint32 mozflag;
	guint32 flag;
} status_flags[] = {
	{ 'F', MSG_FLAG_MARKED, CAMEL_MESSAGE_FLAGGED },
	{ 'A', MSG_FLAG_REPLIED, CAMEL_MESSAGE_ANSWERED },
	{ 'D', MSG_FLAG_EXPUNGED, CAMEL_MESSAGE_DELETED },
	{ 'R', MSG_FLAG_READ, CAMEL_MESSAGE_SEEN },
};

static guint32
decode_status(const gchar *status)
{
	const gchar *p;
	guint32 flags = 0;
	gint i;

	p = status;
	while ((*p++)) {
		for (i=0;i<sizeof(status_flags)/sizeof(status_flags[0]);i++)
			if (status_flags[i].tag == *p)
				flags |= status_flags[i].flag;
	}

	return flags;
}

static guint32
decode_mozilla_status(const gchar *tmp)
{
	unsigned long status = strtoul(tmp, NULL, 16);
	guint32 flags = 0;
	gint i;

	for (i=0;i<sizeof(status_flags)/sizeof(status_flags[0]);i++)
		if (status_flags[i].mozflag & status)
			flags |= status_flags[i].flag;
	return flags;
}

static void
import_mbox_exec (struct _import_mbox_msg *m)
{
	CamelFolder *folder;
	CamelMimeParser *mp = NULL;
	struct stat st;
	gint fd;
	CamelMessageInfo *info;

	if (g_stat(m->path, &st) == -1) {
		g_warning("cannot find source file to import '%s': %s", m->path, g_strerror(errno));
		return;
	}

	if (m->uri == NULL || m->uri[0] == 0)
		folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_INBOX);
	else
		folder = mail_tool_uri_to_folder(m->uri, CAMEL_STORE_FOLDER_CREATE, &m->base.ex);

	if (folder == NULL)
		return;

	if (S_ISREG(st.st_mode)) {
		CamelOperation *oldcancel = NULL;

		fd = g_open(m->path, O_RDONLY|O_BINARY, 0);
		if (fd == -1) {
			g_warning("cannot find source file to import '%s': %s", m->path, g_strerror(errno));
			goto fail1;
		}

		mp = camel_mime_parser_new();
		camel_mime_parser_scan_from(mp, TRUE);
		if (camel_mime_parser_init_with_fd(mp, fd) == -1) { /* will never happen - 0 is unconditionally returned */
			goto fail2;
		}

		if (m->cancel)
			oldcancel = camel_operation_register(m->cancel);

		camel_operation_start(NULL, _("Importing `%s'"), folder->full_name);
		camel_folder_freeze(folder);
		while (camel_mime_parser_step(mp, NULL, NULL) == CAMEL_MIME_PARSER_STATE_FROM) {
			CamelMimeMessage *msg;
			const gchar *tmp;
			gint pc = 0;
			guint32 flags = 0;

			if (st.st_size > 0)
				pc = (gint)(100.0 * ((double)camel_mime_parser_tell(mp) / (double)st.st_size));
			camel_operation_progress(NULL, pc);

			msg = camel_mime_message_new();
			if (camel_mime_part_construct_from_parser((CamelMimePart *)msg, mp) == -1) {
				/* set exception? */
				camel_object_unref(msg);
				break;
			}

			info = camel_message_info_new(NULL);

			tmp = camel_medium_get_header((CamelMedium *)msg, "X-Mozilla-Status");
			if (tmp)
				flags |= decode_mozilla_status(tmp);
			tmp = camel_medium_get_header((CamelMedium *)msg, "Status");
			if (tmp)
				flags |= decode_status(tmp);
			tmp = camel_medium_get_header((CamelMedium *)msg, "X-Status");
			if (tmp)
				flags |= decode_status(tmp);

			camel_message_info_set_flags(info, flags, ~0);
			camel_folder_append_message(folder, msg, info, NULL, &m->base.ex);
			camel_message_info_free(info);
			camel_object_unref(msg);

			if (camel_exception_is_set(&m->base.ex))
				break;

			camel_mime_parser_step(mp, NULL, NULL);
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
import_mbox_done (struct _import_mbox_msg *m)
{
	if (m->done)
		m->done(m->done_data, &m->base.ex);
}

static void
import_mbox_free (struct _import_mbox_msg *m)
{
	if (m->cancel)
		camel_operation_unref(m->cancel);
	g_free(m->uri);
	g_free(m->path);
}

static MailMsgInfo import_mbox_info = {
	sizeof (struct _import_mbox_msg),
	(MailMsgDescFunc) import_mbox_desc,
	(MailMsgExecFunc) import_mbox_exec,
	(MailMsgDoneFunc) import_mbox_done,
	(MailMsgFreeFunc) import_mbox_free
};

gint
mail_importer_import_mbox(const gchar *path, const gchar *folderuri, CamelOperation *cancel, void (*done)(gpointer data, CamelException *), gpointer data)
{
	struct _import_mbox_msg *m;
	gint id;

	m = mail_msg_new(&import_mbox_info);
	m->path = g_strdup(path);
	m->uri = g_strdup(folderuri);
	m->done = done;
	m->done_data = data;
	if (cancel) {
		m->cancel = cancel;
		camel_operation_ref(cancel);
	}

	id = m->base.seq;
	mail_msg_fast_ordered_push (m);

	return id;
}

void
mail_importer_import_mbox_sync(const gchar *path, const gchar *folderuri, CamelOperation *cancel)
{
	struct _import_mbox_msg *m;

	m = mail_msg_new(&import_mbox_info);
	m->path = g_strdup(path);
	m->uri = g_strdup(folderuri);
	if (cancel) {
		m->cancel = cancel;
		camel_operation_ref(cancel);
	}

	import_mbox_exec(m);
	import_mbox_done(m);
	mail_msg_unref(m);
}

struct _import_folders_data {
	MailImporterSpecial *special_folders;
	CamelOperation *cancel;

	guint elmfmt:1;
};

static void
import_folders_rec(struct _import_folders_data *m, const gchar *filepath, const gchar *folderparent)
{
	GDir *dir;
	const gchar *d;
	struct stat st;
	gchar *filefull, *foldersub, *uri, *utf8_filename;
	const gchar *folder;

	dir = g_dir_open(filepath, 0, NULL);
	if (dir == NULL)
		return;

	utf8_filename = g_filename_to_utf8 (filepath, -1, NULL, NULL, NULL);
	camel_operation_start(NULL, _("Scanning %s"), utf8_filename);
	g_free (utf8_filename);

	while ( (d=g_dir_read_name(dir))) {
		if (d[0] == '.')
			continue;

		filefull = g_build_filename(filepath, d, NULL);

		/* skip non files and directories, and skip directories in mozilla mode */
		if (g_stat(filefull, &st) == -1
		    || !(S_ISREG(st.st_mode)
			 || (m->elmfmt && S_ISDIR(st.st_mode)))) {
			g_free(filefull);
			continue;
		}

		folder = d;
		if (folderparent == NULL) {
			gint i;

			for (i=0;m->special_folders[i].orig;i++)
				if (strcmp(m->special_folders[i].orig, folder) == 0) {
					folder = m->special_folders[i].new;
					break;
				}
			/* FIXME: need a better way to get default store location */
			uri = g_strdup_printf("mbox:%s/local#%s", mail_component_peek_base_directory(NULL), folder);
		} else {
			uri = g_strdup_printf("mbox:%s/local#%s/%s", mail_component_peek_base_directory(NULL), folderparent, folder);
		}

		printf("importing to uri %s\n", uri);
		mail_importer_import_mbox_sync(filefull, uri, m->cancel);
		g_free(uri);

		/* This little gem re-uses the stat buffer and filefull to automagically scan mozilla-format folders */
		if (!m->elmfmt) {
			gchar *tmp = g_strdup_printf("%s.sbd", filefull);

			g_free(filefull);
			filefull = tmp;
			if (g_stat(filefull, &st) == -1) {
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
	g_dir_close(dir);

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
mail_importer_import_folders_sync(const gchar *filepath, MailImporterSpecial special_folders[], gint flags, CamelOperation *cancel)
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
