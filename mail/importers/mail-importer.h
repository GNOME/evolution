/*
 *
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __MAIL_IMPORTER_H__
#define __MAIL_IMPORTER_H__

#include <e-util/e-import.h>
#include <camel/camel.h>

EImportImporter *mbox_importer_peek(void);

typedef void (*MboxImporterCreatePreviewFunc)(GObject *preview, GtkWidget **preview_widget);
typedef void (*MboxImporterFillPreviewFunc)(GObject *preview, CamelMimeMessage *msg);

/* 'create_func' is a function to create a view. 'fill_func' is to fill view with a preview of a message 'msg'
   (mail importer cannot link to em-format-html-display directly) */
void mbox_importer_set_preview_funcs (MboxImporterCreatePreviewFunc create_func, MboxImporterFillPreviewFunc fill_func);

EImportImporter *elm_importer_peek(void);
EImportImporter *pine_importer_peek(void);

/* Defines copied from nsMsgMessageFlags.h in Mozilla source. */
/* Evolution only cares about these headers I think */
#define MSG_FLAG_READ 0x0001
#define MSG_FLAG_REPLIED 0x0002
#define MSG_FLAG_MARKED 0x0004
#define MSG_FLAG_EXPUNGED 0x0008

gint mail_importer_import_mbox(const gchar *path, const gchar *folderuri, CamelOperation *cancel, void (*done)(gpointer data, GError **), gpointer data);
void mail_importer_import_mbox_sync(const gchar *path, const gchar *folderuri, CamelOperation *cancel);

struct _MailImporterSpecial {
	const gchar *orig, *new;
};
typedef struct _MailImporterSpecial MailImporterSpecial;

/* mozilla format subdirs */
#define MAIL_IMPORTER_MOZFMT (1<<0)

/* api in flux */
void mail_importer_import_folders_sync(const gchar *filepath, MailImporterSpecial special_folders[], gint flags, CamelOperation *cancel);

#endif
