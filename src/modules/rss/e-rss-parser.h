/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_RSS_PARSER_H
#define E_RSS_PARSER_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _ERssEnclosure {
	gchar *title;
	gchar *href;
	gchar *content_type;
	guint64 size;
	GBytes *data;
} ERssEnclosure;

typedef struct _ERssFeed {
	gchar *id;
	gchar *link;
	gchar *author;
	gchar *title;
	gchar *body;
	gint64 last_modified;
	GSList *enclosures; /* ERssEnclosure * */
} ERssFeed;

ERssEnclosure *	e_rss_enclosure_new	(void);
void		e_rss_enclosure_free	(gpointer ptr);

ERssFeed *	e_rss_feed_new		(void);
void		e_rss_feed_free		(gpointer ptr);

gboolean	e_rss_parser_parse	(const gchar *xml,
					 gsize xml_len,
					 gchar **out_link,
					 gchar **out_alt_link,
					 gchar **out_title,
					 gchar **out_icon,
					 GSList **out_feeds); /* ERssFeed * */

G_END_DECLS

#endif /* E_RSS_PARSER_H */
