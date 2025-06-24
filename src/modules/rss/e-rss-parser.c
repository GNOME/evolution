/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include "e-rss-parser.h"

ERssEnclosure *
e_rss_enclosure_new (void)
{
	return g_slice_new0 (ERssEnclosure);
}

void
e_rss_enclosure_free (gpointer ptr)
{
	ERssEnclosure *enclosure = ptr;

	if (enclosure) {
		g_clear_pointer (&enclosure->data, g_bytes_unref);
		g_free (enclosure->title);
		g_free (enclosure->href);
		g_free (enclosure->content_type);
		g_slice_free (ERssEnclosure, enclosure);
	}
}

ERssFeed *
e_rss_feed_new (void)
{
	return g_slice_new0 (ERssFeed);
}

void
e_rss_feed_free (gpointer ptr)
{
	ERssFeed *feed = ptr;

	if (feed) {
		g_free (feed->id);
		g_free (feed->link);
		g_free (feed->author);
		g_free (feed->title);
		g_free (feed->body);
		g_slist_free_full (feed->enclosures, e_rss_enclosure_free);
		g_slice_free (ERssFeed, feed);
	}
}

static void
e_rss_read_feed_person (xmlNodePtr author,
			xmlChar **out_name,
			xmlChar **out_email)
{
	xmlNodePtr node;
	gboolean email_set = FALSE;

	for (node = author->children; node && (!*out_name || !email_set); node = node->next) {
		if (g_strcmp0 ((const gchar *) node->name, "name") == 0) {
			g_clear_pointer (out_name, xmlFree);
			*out_name = xmlNodeGetContent (node);
		} else if (g_strcmp0 ((const gchar *) node->name, "email") == 0) {
			g_clear_pointer (out_email, xmlFree);
			*out_email = xmlNodeGetContent (node);
			email_set = *out_email && **out_email;
		} else if (g_strcmp0 ((const gchar *) node->name, "uri") == 0 &&
			   (!*out_email || !**out_email)) {
			g_clear_pointer (out_email, xmlFree);
			*out_email = xmlNodeGetContent (node);
		}
	}

	if (!*out_name && !*out_email) {
		*out_name = xmlNodeGetContent (author);
		if (*out_name && !**out_name)
			g_clear_pointer (out_name, xmlFree);
	}

	if (*out_email && (
	    g_ascii_strncasecmp ((const gchar *) *out_email, "http://", 7) == 0 ||
	    g_ascii_strncasecmp ((const gchar *) *out_email, "https://", 8) == 0)) {
		/* Do not use URIs as emails */
		g_clear_pointer (out_email, xmlFree);
	}
}

static gchar *
e_rss_parser_encode_address (xmlChar *name,
			     xmlChar *email)
{
	gchar *address;

	if (!name && !email)
		return NULL;

	address = camel_internet_address_format_address ((const gchar *) name,
							 email ? (const gchar *) email : "");

	if (address && (!email || !*email) && g_str_has_suffix (address, " <>")) {
		/* avoid empty email in the string */
		address[strlen (address) - 3] = '\0';
	}

	return address;
}

static ERssEnclosure *
e_rss_read_enclosure (xmlNodePtr node)
{
	#define dup_attr(des, x) { \
		xmlChar *attr = xmlGetProp (node, (const xmlChar *) x); \
		if (attr && *attr) \
			des = g_strdup ((const gchar *) attr); \
		else \
			des = NULL; \
		g_clear_pointer (&attr, xmlFree); \
	}

	ERssEnclosure *enclosure;
	xmlChar *length;
	gchar *href;

	dup_attr (href, "href");
	if (!href)
		dup_attr (href, "url");

	if (!href || !*href) {
		g_free (href);
		return NULL;
	}

	enclosure = e_rss_enclosure_new ();

	enclosure->href = href;

	dup_attr (enclosure->title, "title");
	dup_attr (enclosure->content_type, "type");

	#undef dup_attr

	length = xmlGetProp (node, (const xmlChar *) "length");
	if (length && *length)
		enclosure->size = g_ascii_strtoull ((const gchar *) length, NULL, 10);
	g_clear_pointer (&length, xmlFree);

	return enclosure;
}

typedef struct _FeedDefaults {
	GUri *base_uri; /* 'base' as a GUri */
	xmlChar *base;
	xmlChar *author_name;
	xmlChar *author_email;
	gint64 publish_date;
	xmlChar *link;
	xmlChar *alt_link;
	xmlChar *title;
	xmlChar *icon;
} FeedDefaults;

static void
e_rss_ensure_uri_absolute (GUri *base_uri,
			   gchar **inout_uri)
{
	GUri *abs_uri;
	const gchar *uri;

	if (!base_uri || !inout_uri)
		return;

	uri = *inout_uri;

	if (!uri || *uri != '/')
		return;

	abs_uri = g_uri_parse_relative (base_uri, uri,
		G_URI_FLAGS_PARSE_RELAXED |
		G_URI_FLAGS_HAS_PASSWORD |
		G_URI_FLAGS_ENCODED_PATH |
		G_URI_FLAGS_ENCODED_QUERY |
		G_URI_FLAGS_ENCODED_FRAGMENT |
		G_URI_FLAGS_SCHEME_NORMALIZE, NULL);

	if (abs_uri) {
		g_free (*inout_uri);
		*inout_uri = g_uri_to_string_partial (abs_uri, G_URI_HIDE_PASSWORD);

		g_uri_unref (abs_uri);
	}
}

static void
e_rss_read_item (xmlNodePtr item,
		 const FeedDefaults *defaults,
		 GSList **out_feeds)
{
	ERssFeed *feed = e_rss_feed_new ();
	xmlNodePtr node;
	gboolean has_author = FALSE;

	for (node = item->children; node; node = node->next) {
		xmlChar *value = NULL;

		if (g_strcmp0 ((const gchar *) node->name, "title") == 0) {
			value = xmlNodeGetContent (node);
			g_clear_pointer (&feed->title, g_free);
			feed->title = g_strdup ((const gchar *) value);
		} else if (g_strcmp0 ((const gchar *) node->name, "link") == 0) {
			xmlChar *rel = xmlGetProp (node, (const xmlChar *) "rel");
			if (!rel ||
			    g_strcmp0 ((const gchar *) rel, "self") == 0 ||
			    g_strcmp0 ((const gchar *) rel, "alternate") == 0) {
				value = xmlGetProp (node, (const xmlChar *) "href");
				if (!value)
					value = xmlNodeGetContent (node);
				g_clear_pointer (&feed->link, g_free);
				feed->link = g_strdup ((const gchar *) value);

				/* Use full URI-s, not relative */
				if (feed->link && *feed->link == '/' && defaults->base_uri)
					e_rss_ensure_uri_absolute (defaults->base_uri, &feed->link);
			} else if (g_strcmp0 ((const gchar *) rel, "enclosure") == 0) {
				ERssEnclosure *enclosure = e_rss_read_enclosure (node);

				if (enclosure)
					feed->enclosures = g_slist_prepend (feed->enclosures, enclosure);
			}
			g_clear_pointer (&rel, xmlFree);
		} else if (g_strcmp0 ((const gchar *) node->name, "id") == 0 ||
			   g_strcmp0 ((const gchar *) node->name, "guid") == 0) {
			value = xmlNodeGetContent (node);
			g_clear_pointer (&feed->id, g_free);
			feed->id = g_strdup ((const gchar *) value);
		} else if (g_strcmp0 ((const gchar *) node->name, "content") == 0) {
			if (node->ns && node->ns->href &&
			    g_ascii_strcasecmp ((const gchar *) node->ns->href, "http://www.w3.org/2005/Atom") != 0) {
				/* the <content> element is valid in the Atom namespace  */
				continue;
			}

			value = xmlNodeGetContent (node);
			g_clear_pointer (&feed->body, g_free);
			feed->body = g_strdup ((const gchar *) value);
		} else if (g_strcmp0 ((const gchar *) node->name, "description") == 0 ||
			   g_strcmp0 ((const gchar *) node->name, "summary") == 0) {
			if (!feed->body || !*feed->body) {
				value = xmlNodeGetContent (node);
				g_clear_pointer (&feed->body, g_free);
				feed->body = g_strdup ((const gchar *) value);
			}
		} else if (g_strcmp0 ((const gchar *) node->name, "enclosure") == 0) {
			ERssEnclosure *enclosure = e_rss_read_enclosure (node);

			if (enclosure)
				feed->enclosures = g_slist_prepend (feed->enclosures, enclosure);
		} else if (g_strcmp0 ((const gchar *) node->name, "author") == 0 ||
			   (!has_author && g_strcmp0 ((const gchar *) node->name, "creator") == 0)) {
			xmlChar *name = NULL, *email = NULL;

			e_rss_read_feed_person (node, &name, &email);

			if (name || email) {
				g_clear_pointer (&feed->author, g_free);
				feed->author = e_rss_parser_encode_address (name, email);

				has_author = g_strcmp0 ((const gchar *) node->name, "author") == 0;

				g_clear_pointer (&name, xmlFree);
				g_clear_pointer (&email, xmlFree);
			}
		} else if (g_strcmp0 ((const gchar *) node->name, "pubDate") == 0) {
			value = xmlNodeGetContent (node);

			if (value && *value)
				feed->last_modified = camel_header_decode_date ((const gchar *) value, NULL);
		} else if (g_strcmp0 ((const gchar *) node->name, "updated") == 0 ||
			   g_strcmp0 ((const gchar *) node->name, "date") == 0) {
			value = xmlNodeGetContent (node);

			if (value && *value) {
				GDateTime *dt;

				dt = g_date_time_new_from_iso8601 ((const gchar *) value, NULL);

				if (dt)
					feed->last_modified = g_date_time_to_unix (dt);

				g_clear_pointer (&dt, g_date_time_unref);
			}
		}

		g_clear_pointer (&value, xmlFree);
	}

	if (feed->title) {
		if (!feed->author) {
			if (defaults->author_name || defaults->author_email) {
				feed->author = e_rss_parser_encode_address (defaults->author_name, defaults->author_email);
			} else {
				feed->author = g_strdup (_("Unknown author"));
			}
		}

		if (!feed->last_modified)
			feed->last_modified = defaults->publish_date;

		feed->enclosures = g_slist_reverse (feed->enclosures);

		*out_feeds = g_slist_prepend (*out_feeds, feed);
	} else {
		e_rss_feed_free (feed);
	}
}

static void
e_rss_read_defaults_rdf (xmlNodePtr root,
			 FeedDefaults *defaults)
{
	xmlNodePtr node;

	defaults->base = xmlGetProp (root, (const xmlChar *) "base");

	for (node = root->children; node; node = node->next) {
		if (g_strcmp0 ((const gchar *) node->name, "channel") == 0) {
			xmlNodePtr subnode;
			gboolean has_author = FALSE, has_link = FALSE, has_title = FALSE, has_image = FALSE, has_date = FALSE;

			for (subnode = node->children; subnode && (!has_author || !has_link || !has_title || !has_image || !has_date); subnode = subnode->next) {
				if (!has_author && g_strcmp0 ((const gchar *) subnode->name, "creator") == 0) {
					g_clear_pointer (&defaults->author_name, xmlFree);
					defaults->author_name = xmlNodeGetContent (subnode);
					has_author = TRUE;
				} else if (!has_author && g_strcmp0 ((const gchar *) subnode->name, "publisher") == 0) {
					g_clear_pointer (&defaults->author_name, xmlFree);
					defaults->author_name = xmlNodeGetContent (subnode);
					/* do not set has_author here, creator is more suitable */
				}

				if (!has_link && g_strcmp0 ((const gchar *) subnode->name, "link") == 0) {
					defaults->link = xmlNodeGetContent (subnode);
					has_link = TRUE;
				}

				if (!has_title && g_strcmp0 ((const gchar *) subnode->name, "title") == 0) {
					defaults->title = xmlNodeGetContent (subnode);
					has_title = TRUE;
				}

				if (!has_image && g_strcmp0 ((const gchar *) subnode->name, "image") == 0) {
					defaults->icon = xmlGetProp (subnode, (const xmlChar *) "resource");
					has_image = TRUE;
				}

				if (!has_date && g_strcmp0 ((const gchar *) subnode->name, "date") == 0) {
					xmlChar *value = xmlNodeGetContent (subnode);

					if (value && *value) {
						GDateTime *dt;

						dt = g_date_time_new_from_iso8601 ((const gchar *) value, NULL);

						if (dt)
							defaults->publish_date = g_date_time_to_unix (dt);

						g_clear_pointer (&dt, g_date_time_unref);
					}

					g_clear_pointer (&value, xmlFree);
					has_date = TRUE;
				}
			}

			break;
		}
	}
}

static void
e_rss_read_rdf (xmlNodePtr node,
		const FeedDefaults *defaults,
		GSList **out_feeds)
{
	if (g_strcmp0 ((const gchar *) node->name, "item") == 0) {
		e_rss_read_item (node, defaults, out_feeds);
	}
}

static void
e_rss_read_defaults_rss (xmlNodePtr root,
			 FeedDefaults *defaults)
{
	xmlNodePtr channel_node;

	defaults->base = xmlGetProp (root, (const xmlChar *) "base");

	for (channel_node = root->children; channel_node; channel_node = channel_node->next) {
		if (g_strcmp0 ((const gchar *) channel_node->name, "channel") == 0) {
			xmlNodePtr node;
			gboolean has_pubdate = FALSE, has_link = FALSE, has_title = FALSE, has_image = FALSE;

			for (node = channel_node->children; node && (!has_pubdate || !has_link || !has_title || !has_image); node = node->next) {
				/* coming from "itunes:name" http://www.itunes.com/dtds/podcast-1.0.dtd */
				if (g_strcmp0 ((const gchar *) node->name, "owner") == 0) {
					xmlNodePtr owner_node;

					for (owner_node = node->children; owner_node; owner_node = owner_node->next) {
						if (g_strcmp0 ((const gchar *) owner_node->name, "name") == 0) {
							g_clear_pointer (&defaults->author_name, xmlFree);
							defaults->author_name = xmlNodeGetContent (owner_node);
						} else if (g_strcmp0 ((const gchar *) owner_node->name, "email") == 0) {
							g_clear_pointer (&defaults->author_email, xmlFree);
							defaults->author_email = xmlNodeGetContent (owner_node);
						}
					}
				}

				if (!has_pubdate && g_strcmp0 ((const gchar *) node->name, "pubDate") == 0) {
					xmlChar *value = xmlNodeGetContent (node);

					if (value && *value)
						defaults->publish_date = camel_header_decode_date ((const gchar *) value, NULL);

					g_clear_pointer (&value, xmlFree);

					has_pubdate = TRUE;
				}

				if (!has_link && g_strcmp0 ((const gchar *) node->name, "link") == 0) {
					xmlChar *value = xmlNodeGetContent (node);

					if (value && *value) {
						defaults->link = value;
						has_link = TRUE;
					} else {
						g_clear_pointer (&value, xmlFree);
					}
				}

				if (!has_title && g_strcmp0 ((const gchar *) node->name, "title") == 0) {
					xmlChar *value = xmlNodeGetContent (node);

					if (value && *value)
						defaults->title = value;
					else
						g_clear_pointer (&value, xmlFree);

					has_title = TRUE;
				}

				if (!has_image && g_strcmp0 ((const gchar *) node->name, "image") == 0) {
					xmlNodePtr image_node;

					for (image_node = node->children; image_node; image_node = image_node->next) {
						if (g_strcmp0 ((const gchar *) image_node->name, "url") == 0) {
							xmlChar *value = xmlNodeGetContent (image_node);

							if (value && *value)
								defaults->icon = value;
							else
								g_clear_pointer (&value, xmlFree);
							break;
						}
					}

					/* try href attribute from itunes:image http://www.itunes.com/dtds/podcast-1.0.dtd */
					if (!defaults->icon)
						defaults->icon = xmlGetProp (node, (const xmlChar *) "href");

					has_image = TRUE;
				}
			}
			/* read only the first channel */
			break;
		}
	}
}

static void
e_rss_read_rss (xmlNodePtr node,
		const FeedDefaults *defaults,
		GSList **out_feeds)
{
	if (g_strcmp0 ((const gchar *) node->name, "channel") == 0) {
		xmlNodePtr subnode;

		for (subnode = node->children; subnode; subnode = subnode->next) {
			if (g_strcmp0 ((const gchar *) subnode->name, "item") == 0) {
				e_rss_read_item (subnode, defaults, out_feeds);
			}
		}
	}
}

static void
e_rss_read_defaults_feed (xmlNodePtr root,
			  FeedDefaults *defaults)
{
	xmlNodePtr node;
	gboolean has_author = FALSE, has_published = FALSE, has_link = FALSE, has_alt_link = FALSE, has_title = FALSE, has_icon = FALSE;

	defaults->base = xmlGetProp (root, (const xmlChar *) "base");
	if (!defaults->base) {
		for (node = root->children; node && !defaults->base; node = node->next) {
			if (g_strcmp0 ((const gchar *) node->name, "link") == 0)
				defaults->base = xmlGetProp (root, (const xmlChar *) "rel");
			else if (g_strcmp0 ((const gchar *) node->name, "alternate") == 0)
				defaults->base = xmlGetProp (root, (const xmlChar *) "href");
		}
	}

	for (node = root->children; node && (!has_author || !has_published || !has_link || !has_alt_link || !has_title || !has_icon); node = node->next) {
		if (!has_author && g_strcmp0 ((const gchar *) node->name, "author") == 0) {
			g_clear_pointer (&defaults->author_name, xmlFree);
			g_clear_pointer (&defaults->author_email, xmlFree);
			e_rss_read_feed_person (node, &defaults->author_name, &defaults->author_email);
			has_author = TRUE;
		}

		if (!has_published && (
		    g_strcmp0 ((const gchar *) node->name, "published") == 0 ||
		    g_strcmp0 ((const gchar *) node->name, "updated") == 0)) {
			xmlChar *value = xmlNodeGetContent (node);

			if (value && *value) {
				GDateTime *dt;

				dt = g_date_time_new_from_iso8601 ((const gchar *) value, NULL);

				if (dt) {
					defaults->publish_date = g_date_time_to_unix (dt);
					has_published = TRUE;
				}

				g_clear_pointer (&dt, g_date_time_unref);
			}

			g_clear_pointer (&value, xmlFree);
		}

		if ((!has_link || !has_alt_link) && g_strcmp0 ((const gchar *) node->name, "link") == 0) {
			xmlChar *rel, *href;

			rel = xmlGetProp (node, (const xmlChar *) "rel");
			href = xmlGetProp (node, (const xmlChar *) "href");

			if (!has_link && href && *href && g_strcmp0 ((const gchar *) rel, "self") == 0) {
				defaults->link = href;
				href = NULL;
				has_link = TRUE;
			}

			if (!has_alt_link && href && *href && g_strcmp0 ((const gchar *) rel, "alternate") == 0) {
				defaults->alt_link = href;
				href = NULL;
				has_alt_link = TRUE;
			}

			g_clear_pointer (&rel, xmlFree);
			g_clear_pointer (&href, xmlFree);
		}

		if (!has_title && g_strcmp0 ((const gchar *) node->name, "title") == 0) {
			xmlChar *value = xmlNodeGetContent (node);

			if (value && *value)
				defaults->title = value;
			else
				g_clear_pointer (&value, xmlFree);

			has_title = TRUE;
		}

		if (!has_icon && (
		    g_strcmp0 ((const gchar *) node->name, "icon") == 0 ||
		    g_strcmp0 ((const gchar *) node->name, "logo") == 0)) {
			xmlChar *value = xmlNodeGetContent (node);

			if (value && *value) {
				g_clear_pointer (&defaults->icon, xmlFree);
				defaults->icon = value;
			} else {
				g_clear_pointer (&value, xmlFree);
			}

			/* Prefer "icon", but if not available, then use "logo" */
			has_icon = g_strcmp0 ((const gchar *) node->name, "icon") == 0;
		}
	}
}

static void
e_rss_read_feed (xmlNodePtr node,
		 const FeedDefaults *defaults,
		 GSList **out_feeds)
{
	if (g_strcmp0 ((const gchar *) node->name, "entry") == 0) {
		e_rss_read_item (node, defaults, out_feeds);
	}
}

gboolean
e_rss_parser_parse (const gchar *xml,
		    gsize xml_len,
		    gchar **out_link,
		    gchar **out_alt_link,
		    gchar **out_title,
		    gchar **out_icon,
		    GSList **out_feeds) /* ERssFeed * */
{
	xmlDoc *doc;
	xmlNodePtr root;

	if (out_feeds)
		*out_feeds = NULL;

	if (!xml || !xml_len)
		return FALSE;

	doc = e_xml_parse_data (xml, xml_len);

	if (!doc)
		return FALSE;

	root = xmlDocGetRootElement (doc);
	if (root) {
		FeedDefaults defaults = { 0, };
		void (*read_func) (xmlNodePtr node,
				   const FeedDefaults *defaults,
				   GSList **out_feeds) = NULL;

		if (g_strcmp0 ((const gchar *) root->name, "RDF") == 0) {
			/* RSS 1.0 - https://web.resource.org/rss/1.0/ */
			e_rss_read_defaults_rdf (root, &defaults);
			read_func = e_rss_read_rdf;
		} else if (g_strcmp0 ((const gchar *) root->name, "rss") == 0) {
			/* RSS 2.0 - https://www.rssboard.org/rss-specification */
			e_rss_read_defaults_rss (root, &defaults);
			read_func = e_rss_read_rss;
		} else if (g_strcmp0 ((const gchar *) root->name, "feed") == 0) {
			/* Atom - https://validator.w3.org/feed/docs/atom.html */
			e_rss_read_defaults_feed (root, &defaults);
			read_func = e_rss_read_feed;
		}

		if (!defaults.publish_date)
			defaults.publish_date = g_get_real_time () / G_USEC_PER_SEC;

		if (defaults.base || defaults.link || defaults.alt_link) {
			const gchar *base;

			base = (const gchar *) defaults.base;
			if (!base || *base == '/')
				base = (const gchar *) defaults.link;
			if (!base || *base == '/')
				base = (const gchar *) defaults.alt_link;

			if (base && *base != '/') {
				defaults.base_uri = g_uri_parse (base,
					G_URI_FLAGS_PARSE_RELAXED |
					G_URI_FLAGS_HAS_PASSWORD |
					G_URI_FLAGS_ENCODED_PATH |
					G_URI_FLAGS_ENCODED_QUERY |
					G_URI_FLAGS_ENCODED_FRAGMENT |
					G_URI_FLAGS_SCHEME_NORMALIZE, NULL);
			}
		}

		if (read_func && out_feeds) {
			xmlNodePtr node;

			for (node = root->children; node; node = node->next) {
				read_func (node, &defaults, out_feeds);
			}
		}

		if (out_link) {
			*out_link = g_strdup ((const gchar *) defaults.link);
			e_rss_ensure_uri_absolute (defaults.base_uri, out_link);
		}

		if (out_alt_link) {
			*out_alt_link = g_strdup ((const gchar *) defaults.alt_link);
			e_rss_ensure_uri_absolute (defaults.base_uri, out_alt_link);
		}

		if (out_title)
			*out_title = g_strdup ((const gchar *) defaults.title);

		if (out_icon) {
			*out_icon = g_strdup ((const gchar *) defaults.icon);
			e_rss_ensure_uri_absolute (defaults.base_uri, out_icon);
		}

		g_clear_pointer (&defaults.base_uri, g_uri_unref);
		g_clear_pointer (&defaults.base, xmlFree);
		g_clear_pointer (&defaults.author_name, xmlFree);
		g_clear_pointer (&defaults.author_email, xmlFree);
		g_clear_pointer (&defaults.link, xmlFree);
		g_clear_pointer (&defaults.alt_link, xmlFree);
		g_clear_pointer (&defaults.title, xmlFree);
		g_clear_pointer (&defaults.icon, xmlFree);

		if (out_feeds)
			*out_feeds = g_slist_reverse (*out_feeds);
	}

	xmlFreeDoc (doc);

	return TRUE;
}
