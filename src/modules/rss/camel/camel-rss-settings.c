/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib.h>

#include "camel-rss-settings.h"

struct _CamelRssSettingsPrivate {
	gboolean filter_all;
	gboolean complete_articles;
	gboolean feed_enclosures;
	gboolean limit_feed_enclosure_size;
	guint32 max_feed_enclosure_size;
};

enum {
	PROP_0,
	PROP_FILTER_ALL,
	PROP_COMPLETE_ARTICLES,
	PROP_FEED_ENCLOSURES,
	PROP_LIMIT_FEED_ENCLOSURE_SIZE,
	PROP_MAX_FEED_ENCLOSURE_SIZE
};

G_DEFINE_TYPE_WITH_CODE (CamelRssSettings, camel_rss_settings, CAMEL_TYPE_OFFLINE_SETTINGS,
	G_ADD_PRIVATE (CamelRssSettings))

static void
rss_settings_set_property (GObject *object,
			   guint property_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILTER_ALL:
			camel_rss_settings_set_filter_all (
				CAMEL_RSS_SETTINGS (object),
				g_value_get_boolean (value));
			return;
		case PROP_COMPLETE_ARTICLES:
			camel_rss_settings_set_complete_articles (
				CAMEL_RSS_SETTINGS (object),
				g_value_get_boolean (value));
			return;
		case PROP_FEED_ENCLOSURES:
			camel_rss_settings_set_feed_enclosures (
				CAMEL_RSS_SETTINGS (object),
				g_value_get_boolean (value));
			return;
		case PROP_LIMIT_FEED_ENCLOSURE_SIZE:
			camel_rss_settings_set_limit_feed_enclosure_size (
				CAMEL_RSS_SETTINGS (object),
				g_value_get_boolean (value));
			return;
		case PROP_MAX_FEED_ENCLOSURE_SIZE:
			camel_rss_settings_set_max_feed_enclosure_size (
				CAMEL_RSS_SETTINGS (object),
				g_value_get_uint (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
rss_settings_get_property (GObject *object,
			   guint property_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILTER_ALL:
			g_value_set_boolean (
				value,
				camel_rss_settings_get_filter_all (
				CAMEL_RSS_SETTINGS (object)));
			return;
		case PROP_COMPLETE_ARTICLES:
			g_value_set_boolean (
				value,
				camel_rss_settings_get_complete_articles (
				CAMEL_RSS_SETTINGS (object)));
			return;
		case PROP_FEED_ENCLOSURES:
			g_value_set_boolean (
				value,
				camel_rss_settings_get_feed_enclosures (
				CAMEL_RSS_SETTINGS (object)));
			return;
		case PROP_LIMIT_FEED_ENCLOSURE_SIZE:
			g_value_set_boolean (
				value,
				camel_rss_settings_get_limit_feed_enclosure_size (
				CAMEL_RSS_SETTINGS (object)));
			return;
		case PROP_MAX_FEED_ENCLOSURE_SIZE:
			g_value_set_uint (
				value,
				camel_rss_settings_get_max_feed_enclosure_size (
				CAMEL_RSS_SETTINGS (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
camel_rss_settings_class_init (CamelRssSettingsClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = rss_settings_set_property;
	object_class->get_property = rss_settings_get_property;

	g_object_class_install_property (
		object_class,
		PROP_FILTER_ALL,
		g_param_spec_boolean (
			"filter-all",
			"Filter All",
			"Whether to apply filters in all folders",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_EXPLICIT_NOTIFY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_COMPLETE_ARTICLES,
		g_param_spec_boolean (
			"complete-articles",
			"Complete Articles",
			"Whether to download complete articles",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_EXPLICIT_NOTIFY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_FEED_ENCLOSURES,
		g_param_spec_boolean (
			"feed-enclosures",
			"Feed Enclosures",
			"Whether to download feed enclosures",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_EXPLICIT_NOTIFY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_LIMIT_FEED_ENCLOSURE_SIZE,
		g_param_spec_boolean (
			"limit-feed-enclosure-size",
			"Limit Feed Enclosure Size",
			"Whether to limit feed enclosure size",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_EXPLICIT_NOTIFY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MAX_FEED_ENCLOSURE_SIZE,
		g_param_spec_uint (
			"max-feed-enclosure-size",
			"Max Feed Enclosure Size",
			"Max size, in kB, of feed enclosure to download",
			0, G_MAXUINT32, 0,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_EXPLICIT_NOTIFY |
			G_PARAM_STATIC_STRINGS));
}

static void
camel_rss_settings_init (CamelRssSettings *settings)
{
	settings->priv = camel_rss_settings_get_instance_private (settings);
}

gboolean
camel_rss_settings_get_filter_all (CamelRssSettings *settings)
{
	g_return_val_if_fail (CAMEL_IS_RSS_SETTINGS (settings), FALSE);

	return settings->priv->filter_all;
}

void
camel_rss_settings_set_filter_all (CamelRssSettings *settings,
                                    gboolean filter_all)
{
	g_return_if_fail (CAMEL_IS_RSS_SETTINGS (settings));

	if ((!settings->priv->filter_all) == (!filter_all))
		return;

	settings->priv->filter_all = filter_all;

	g_object_notify (G_OBJECT (settings), "filter-all");
}

gboolean
camel_rss_settings_get_complete_articles (CamelRssSettings *settings)
{
	g_return_val_if_fail (CAMEL_IS_RSS_SETTINGS (settings), FALSE);

	return settings->priv->complete_articles;
}

void
camel_rss_settings_set_complete_articles (CamelRssSettings *settings,
					  gboolean value)
{
	g_return_if_fail (CAMEL_IS_RSS_SETTINGS (settings));

	if ((!settings->priv->complete_articles) == (!value))
		return;

	settings->priv->complete_articles = value;

	g_object_notify (G_OBJECT (settings), "complete-articles");
}

gboolean
camel_rss_settings_get_feed_enclosures (CamelRssSettings *settings)
{
	g_return_val_if_fail (CAMEL_IS_RSS_SETTINGS (settings), FALSE);

	return settings->priv->feed_enclosures;
}

void
camel_rss_settings_set_feed_enclosures (CamelRssSettings *settings,
					gboolean value)
{
	g_return_if_fail (CAMEL_IS_RSS_SETTINGS (settings));

	if ((!settings->priv->feed_enclosures) == (!value))
		return;

	settings->priv->feed_enclosures = value;

	g_object_notify (G_OBJECT (settings), "feed-enclosures");
}

gboolean
camel_rss_settings_get_limit_feed_enclosure_size (CamelRssSettings *settings)
{
	g_return_val_if_fail (CAMEL_IS_RSS_SETTINGS (settings), FALSE);

	return settings->priv->limit_feed_enclosure_size;
}

void
camel_rss_settings_set_limit_feed_enclosure_size (CamelRssSettings *settings,
						  gboolean value)
{
	g_return_if_fail (CAMEL_IS_RSS_SETTINGS (settings));

	if ((!settings->priv->limit_feed_enclosure_size) == (!value))
		return;

	settings->priv->limit_feed_enclosure_size = value;

	g_object_notify (G_OBJECT (settings), "limit-feed-enclosure-size");
}

guint32
camel_rss_settings_get_max_feed_enclosure_size (CamelRssSettings *settings)
{
	g_return_val_if_fail (CAMEL_IS_RSS_SETTINGS (settings), 0);

	return settings->priv->max_feed_enclosure_size;
}

void
camel_rss_settings_set_max_feed_enclosure_size (CamelRssSettings *settings,
						guint32 value)
{
	g_return_if_fail (CAMEL_IS_RSS_SETTINGS (settings));

	if (settings->priv->max_feed_enclosure_size == value)
		return;

	settings->priv->max_feed_enclosure_size = value;

	g_object_notify (G_OBJECT (settings), "max-feed-enclosure-size");
}
