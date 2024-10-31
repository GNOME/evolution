/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef CAMEL_RSS_SETTINGS_H
#define CAMEL_RSS_SETTINGS_H

#include <camel/camel.h>

/* Standard GObject macros */
#define CAMEL_TYPE_RSS_SETTINGS \
	(camel_rss_settings_get_type ())
#define CAMEL_RSS_SETTINGS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_RSS_SETTINGS, CamelRssSettings))
#define CAMEL_RSS_SETTINGS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_RSS_SETTINGS, CamelRssSettingsClass))
#define CAMEL_IS_RSS_SETTINGS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_RSS_SETTINGS))
#define CAMEL_IS_RSS_SETTINGS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_RSS_SETTINGS))
#define CAMEL_RSS_SETTINGS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_RSS_SETTINGS, CamelRssSettingsClass))

G_BEGIN_DECLS

typedef struct _CamelRssSettings CamelRssSettings;
typedef struct _CamelRssSettingsClass CamelRssSettingsClass;
typedef struct _CamelRssSettingsPrivate CamelRssSettingsPrivate;

struct _CamelRssSettings {
	CamelOfflineSettings parent;
	CamelRssSettingsPrivate *priv;
};

struct _CamelRssSettingsClass {
	CamelOfflineSettingsClass parent_class;
};

GType		camel_rss_settings_get_type
					(void) G_GNUC_CONST;
gboolean	camel_rss_settings_get_filter_all
					(CamelRssSettings *settings);
void		camel_rss_settings_set_filter_all
					(CamelRssSettings *settings,
					 gboolean filter_all);
gboolean	camel_rss_settings_get_complete_articles
					(CamelRssSettings *settings);
void		camel_rss_settings_set_complete_articles
					(CamelRssSettings *settings,
					 gboolean value);
gboolean	camel_rss_settings_get_feed_enclosures
					(CamelRssSettings *settings);
void		camel_rss_settings_set_feed_enclosures
					(CamelRssSettings *settings,
					 gboolean value);
gboolean	camel_rss_settings_get_limit_feed_enclosure_size
					(CamelRssSettings *settings);
void		camel_rss_settings_set_limit_feed_enclosure_size
					(CamelRssSettings *settings,
					 gboolean value);
guint32		camel_rss_settings_get_max_feed_enclosure_size
					(CamelRssSettings *settings);
void		camel_rss_settings_set_max_feed_enclosure_size
					(CamelRssSettings *settings,
					 guint32 value);

G_END_DECLS

#endif /* CAMEL_RSS_SETTINGS_H */
