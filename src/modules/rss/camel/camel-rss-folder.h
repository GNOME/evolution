/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef CAMEL_RSS_FOLDER_H
#define CAMEL_RSS_FOLDER_H

#include <camel/camel.h>

/* Standard GObject macros */
#define CAMEL_TYPE_RSS_FOLDER \
	(camel_rss_folder_get_type ())
#define CAMEL_RSS_FOLDER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_RSS_FOLDER, CamelRssFolder))
#define CAMEL_RSS_FOLDER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_RSS_FOLDER, CamelRssFolderClass))
#define CAMEL_IS_RSS_FOLDER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_RSS_FOLDER))
#define CAMEL_IS_RSS_FOLDER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_RSS_FOLDER))
#define CAMEL_RSS_FOLDER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_RSS_FOLDER, CamelRssFolderClass))

G_BEGIN_DECLS

typedef struct _CamelRssFolder CamelRssFolder;
typedef struct _CamelRssFolderClass CamelRssFolderClass;
typedef struct _CamelRssFolderPrivate CamelRssFolderPrivate;

struct _CamelRssFolder {
	CamelFolder parent;
	CamelRssFolderPrivate *priv;
};

struct _CamelRssFolderClass {
	CamelFolderClass parent;
};

GType		camel_rss_folder_get_type	(void);
CamelFolder *	camel_rss_folder_new		(CamelStore *parent,
						 const gchar *id,
						 GCancellable *cancellable,
						 GError **error);
const gchar *	camel_rss_folder_get_id		(CamelRssFolder *self);

G_END_DECLS

#endif /* CAMEL_RSS_FOLDER_H */
