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
 *   Peter Williams <peterw@ximian.com>
 *   Michael Zucchi <notzed@ximian.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

#ifndef MAIL_FOLDER_CACHE_H
#define MAIL_FOLDER_CACHE_H

#include <camel/camel.h>

/* Standard GObject macros */
#define MAIL_TYPE_FOLDER_CACHE \
	(mail_folder_cache_get_type ())
#define MAIL_FOLDER_CACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), MAIL_TYPE_FOLDER_CACHE, MailFolderCache))
#define MAIL_FOLDER_CACHE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), MAIL_TYPE_FOLDER_CACHE, MailFolderCacheClass))
#define MAIL_IS_FOLDER_CACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), MAIL_TYPE_FOLDER_CACHE))
#define MAIL_IS_FOLDER_CACHE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), MAIL_TYPE_FOLDER_CACHE))
#define MAIL_FOLDER_CACHE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), MAIL_TYPE_FOLDER_CACHE, MailFolderCacheClass))

G_BEGIN_DECLS

typedef struct _MailFolderCache MailFolderCache;
typedef struct _MailFolderCacheClass MailFolderCacheClass;
typedef struct _MailFolderCachePrivate MailFolderCachePrivate;

/**
 * NoteDoneFunc:
 *
 * The signature of a function to be registered as a callback for
 * mail_folder_cache_note_store()
 */
typedef gboolean	(*NoteDoneFunc)		(CamelStore *store,
						 CamelFolderInfo *info,
						 gpointer data);

/**
 * MailFolderCache:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 */
struct _MailFolderCache {
	GObject parent;
	MailFolderCachePrivate *priv;
};

struct _MailFolderCacheClass {
	GObjectClass parent_class;
};

GType		mail_folder_cache_get_type	(void) G_GNUC_CONST;
MailFolderCache *
		mail_folder_cache_get_default	(void);
void		mail_folder_cache_note_store	(MailFolderCache *self,
						 CamelStore *store,
						 CamelOperation *op,
						 NoteDoneFunc done,
						 gpointer data);
void		mail_folder_cache_note_store_remove
						(MailFolderCache *self,
						 CamelStore *store);
void		mail_folder_cache_note_folder	(MailFolderCache *self,
						 CamelFolder *folder);
gboolean	mail_folder_cache_get_folder_from_uri
						(MailFolderCache *self,
						 const gchar *uri,
						 CamelFolder **folderp);
gboolean	mail_folder_cache_get_folder_info_flags
						(MailFolderCache *self,
						 CamelFolder *folder,
						 gint *flags);

gboolean	mail_folder_cache_get_folder_has_children
						(MailFolderCache *self,
						 CamelFolder *folder,
						 gboolean *found);

G_END_DECLS

#endif /* MAIL_FOLDER_CACHE_H */
