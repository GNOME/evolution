/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef CAMEL_RSS_STORE_SUMMARY_H
#define CAMEL_RSS_STORE_SUMMARY_H

#include <camel/camel.h>

/* Standard GObject macros */
#define CAMEL_TYPE_RSS_STORE_SUMMARY \
	(camel_rss_store_summary_get_type ())
#define CAMEL_RSS_STORE_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_RSS_STORE_SUMMARY, CamelRssStoreSummary))
#define CAMEL_RSS_STORE_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_RSS_STORE_SUMMARY, CamelRssStoreSummaryClass))
#define CAMEL_IS_RSS_STORE_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_RSS_STORE_SUMMARY))
#define CAMEL_IS_RSS_STORE_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_RSS_STORE_SUMMARY))
#define CAMEL_RSS_STORE_SUMMARY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_RSS_STORE_SUMMARY, CamelRssStoreSummaryClass))

G_BEGIN_DECLS

typedef enum {
	CAMEL_RSS_CONTENT_TYPE_HTML,
	CAMEL_RSS_CONTENT_TYPE_PLAIN_TEXT,
	CAMEL_RSS_CONTENT_TYPE_MARKDOWN
} CamelRssContentType;

typedef struct _CamelRssStoreSummary CamelRssStoreSummary;
typedef struct _CamelRssStoreSummaryClass CamelRssStoreSummaryClass;
typedef struct _CamelRssStoreSummaryPrivate CamelRssStoreSummaryPrivate;

struct _CamelRssStoreSummary {
	GObject object;
	CamelRssStoreSummaryPrivate *priv;
};

struct _CamelRssStoreSummaryClass {
	GObjectClass object_class;
};

GType		camel_rss_store_summary_get_type	(void);
CamelRssStoreSummary *
		camel_rss_store_summary_new		(const gchar *filename);
void		camel_rss_store_summary_lock		(CamelRssStoreSummary *self);
void		camel_rss_store_summary_unlock		(CamelRssStoreSummary *self);
gboolean	camel_rss_store_summary_load		(CamelRssStoreSummary *self,
							 GError **error);
gboolean	camel_rss_store_summary_save		(CamelRssStoreSummary *self,
							 GError **error);
const gchar *	camel_rss_store_summary_add		(CamelRssStoreSummary *self,
							 const gchar *href,
							 const gchar *display_name,
							 const gchar *icon_filename,
							 CamelRssContentType content_type);
gboolean	camel_rss_store_summary_remove		(CamelRssStoreSummary *self,
							 const gchar *id);
gboolean	camel_rss_store_summary_contains	(CamelRssStoreSummary *self,
							 const gchar *id);
GSList *	camel_rss_store_summary_dup_feeds	(CamelRssStoreSummary *self); /* gchar *id */
CamelFolderInfo *
		camel_rss_store_summary_dup_folder_info	(CamelRssStoreSummary *self,
							 const gchar *id);
CamelFolderInfo *
		camel_rss_store_summary_dup_folder_info_for_display_name
							(CamelRssStoreSummary *self,
							 const gchar *display_name);
const gchar *	camel_rss_store_summary_get_href	(CamelRssStoreSummary *self,
							 const gchar *id);
const gchar *	camel_rss_store_summary_get_display_name(CamelRssStoreSummary *self,
							 const gchar *id);
void		camel_rss_store_summary_set_display_name(CamelRssStoreSummary *self,
							 const gchar *id,
							 const gchar *display_name);
const gchar *	camel_rss_store_summary_get_icon_filename
							(CamelRssStoreSummary *self,
							 const gchar *id);
void		camel_rss_store_summary_set_icon_filename
							(CamelRssStoreSummary *self,
							 const gchar *id,
							 const gchar *filename);
CamelRssContentType
		camel_rss_store_summary_get_content_type(CamelRssStoreSummary *self,
							 const gchar *id);
void		camel_rss_store_summary_set_content_type(CamelRssStoreSummary *self,
							 const gchar *id,
							 CamelRssContentType content_type);
guint32		camel_rss_store_summary_get_total_count	(CamelRssStoreSummary *self,
							 const gchar *id);
void		camel_rss_store_summary_set_total_count	(CamelRssStoreSummary *self,
							 const gchar *id,
							 guint32 total_count);
guint32		camel_rss_store_summary_get_unread_count(CamelRssStoreSummary *self,
							 const gchar *id);
void		camel_rss_store_summary_set_unread_count(CamelRssStoreSummary *self,
							 const gchar *id,
							 guint32 unread_count);
gint64		camel_rss_store_summary_get_last_updated(CamelRssStoreSummary *self,
							 const gchar *id);
void		camel_rss_store_summary_set_last_updated(CamelRssStoreSummary *self,
							 const gchar *id,
							 gint64 last_updated);
const gchar *	camel_rss_store_summary_get_last_etag	(CamelRssStoreSummary *self,
							 const gchar *id);
void		camel_rss_store_summary_set_last_etag	(CamelRssStoreSummary *self,
							 const gchar *id,
							 const gchar *last_etag);
const gchar *	camel_rss_store_summary_get_last_modified
							(CamelRssStoreSummary *self,
							 const gchar *id);
void		camel_rss_store_summary_set_last_modified
							(CamelRssStoreSummary *self,
							 const gchar *id,
							 const gchar *last_modified);

G_END_DECLS

#endif /* CAMEL_RSS_STORE_SUMMARY_H */
