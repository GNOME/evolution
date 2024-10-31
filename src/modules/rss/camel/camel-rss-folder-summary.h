/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef CAMEL_RSS_FOLDER_SUMMARY_H
#define CAMEL_RSS_FOLDER_SUMMARY_H

#include <camel/camel.h>

#include "camel-rss-store-summary.h"
#include "e-rss-parser.h"

/* Standard GObject macros */
#define CAMEL_TYPE_RSS_FOLDER_SUMMARY \
	(camel_rss_folder_summary_get_type ())
#define CAMEL_RSS_FOLDER_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_RSS_FOLDER_SUMMARY, CamelRssFolderSummary))
#define CAMEL_RSS_FOLDER_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_RSS_FOLDER_SUMMARY, CamelRssFolderSummaryClass))
#define CAMEL_IS_RSS_FOLDER_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_RSS_FOLDER_SUMMARY))
#define CAMEL_IS_RSS_FOLDER_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_RSS_FOLDER_SUMMARY))
#define CAMEL_RSS_FOLDER_SUMMARY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_RSS_FOLDER_SUMMARY, CamelRssFolderSummaryClass))

G_BEGIN_DECLS

typedef struct _CamelRssFolderSummary CamelRssFolderSummary;
typedef struct _CamelRssFolderSummaryClass CamelRssFolderSummaryClass;
typedef struct _CamelRssFolderSummaryPrivate CamelRssFolderSummaryPrivate;

struct _CamelRssFolderSummary {
	CamelFolderSummary parent;
	CamelRssFolderSummaryPrivate *priv;
};

struct _CamelRssFolderSummaryClass {
	CamelFolderSummaryClass parent_class;
};

GType		camel_rss_folder_summary_get_type		(void);
CamelFolderSummary *
		camel_rss_folder_summary_new			(CamelFolder *folder);
CamelMimeMessage *
		camel_rss_folder_summary_dup_message		(CamelRssFolderSummary *self,
								 const gchar *uid,
								 CamelDataCache **out_rss_cache,
								 CamelRssContentType *out_content_type,
								 GCancellable *cancellable,
								 GError **error);
gboolean	camel_rss_folder_summary_add_or_update_feed_sync(CamelRssFolderSummary *self,
								 const gchar *href,
								 ERssFeed *feed,
								 GBytes *complete_article,
								 CamelFolderChangeInfo **inout_changes,
								 GCancellable *cancellable,
								 GError **error);

G_END_DECLS

#endif /* CAMEL_RSS_FOLDER_SUMMARY_H */
