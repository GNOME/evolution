/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-importer.h
 *
 * Authors: Iain Holmes <iain@ximian.com>
 *
 * Copyright (C) 2001  Ximian, Inc.
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

#ifndef __MAIL_IMPORTER_H__
#define __MAIL_IMPORTER_H__

typedef struct _MailImporter MailImporter;
struct _MailImporter {
	struct _CamelFolder *folder;
	struct _CamelStreamMem *mstream;

	gboolean frozen; /* Is folder frozen? */
};

void mail_importer_init (struct _MailComponent *mc);
void mail_importer_uninit (void);

void mail_importer_add_line (MailImporter *importer,
			     const char *str,
			     gboolean finished);
void mail_importer_create_folder (const char *parent_path,
				  const char *name,
				  const char *description);

/* creates a folder at folderpath on the local storage */
char *mail_importer_make_local_folder(const char *folderpath);

struct _BonoboObject;
struct _BonoboGenericFactory;
struct _CamelOperation;

#define ELM_INTELLIGENT_IMPORTER_IID "OAFIID:GNOME_Evolution_Mail_Elm_Intelligent_Importer:" BASE_VERSION
#define PINE_INTELLIGENT_IMPORTER_IID "OAFIID:GNOME_Evolution_Mail_Pine_Intelligent_Importer:" BASE_VERSION
#define NETSCAPE_INTELLIGENT_IMPORTER_IID "OAFIID:GNOME_Evolution_Mail_Netscape_Intelligent_Importer:" BASE_VERSION

#define MBOX_IMPORTER_IID "OAFIID:GNOME_Evolution_Mail_Mbox_Importer:" BASE_VERSION
#define OUTLOOK_IMPORTER_IID "OAFIID:GNOME_Evolution_Mail_Outlook_Importer:" BASE_VERSION

struct _BonoboObject *elm_intelligent_importer_new(void);
struct _BonoboObject *pine_intelligent_importer_new(void);
struct _BonoboObject *netscape_intelligent_importer_new(void);

struct _BonoboObject *mbox_importer_new(void);
struct _BonoboObject *outlook_importer_new(void);

struct _BonoboObject *mail_importer_factory_cb(struct _BonoboGenericFactory *factory, const char *iid, void *data);


/* Defines copied from nsMsgMessageFlags.h in Mozilla source. */
/* Evolution only cares about these headers I think */
#define MSG_FLAG_READ 0x0001
#define MSG_FLAG_REPLIED 0x0002
#define MSG_FLAG_MARKED 0x0004
#define MSG_FLAG_EXPUNGED 0x0008

int mail_importer_import_mbox(const char *path, const char *folderuri, struct _CamelOperation *cancel);
void mail_importer_import_mbox_sync(const char *path, const char *folderuri, struct _CamelOperation *cancel);

struct _MailImporterSpecial {
	char *orig, *new;
};
typedef struct _MailImporterSpecial MailImporterSpecial;

/* mozilla format subdirs */
#define MAIL_IMPORTER_MOZFMT (1<<0)

/* api in flux */
void mail_importer_import_folders_sync(const char *filepath, MailImporterSpecial special_folders[], int flags, struct _CamelOperation *cancel);

#endif
