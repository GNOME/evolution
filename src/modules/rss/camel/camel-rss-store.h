/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef CAMEL_RSS_STORE_H
#define CAMEL_RSS_STORE_H

#include <camel/camel.h>

#include "camel-rss-store-summary.h"

/* Standard GObject macros */
#define CAMEL_TYPE_RSS_STORE \
	(camel_rss_store_get_type ())
#define CAMEL_RSS_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_RSS_STORE, CamelRssStore))
#define CAMEL_RSS_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_RSS_STORE, CamelRssStoreClass))
#define CAMEL_IS_RSS_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_RSS_STORE))
#define CAMEL_IS_RSS_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_RSS_STORE))
#define CAMEL_RSS_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_RSS_STORE, CamelRssStoreClass))

G_BEGIN_DECLS

typedef struct _CamelRssStore CamelRssStore;
typedef struct _CamelRssStoreClass CamelRssStoreClass;
typedef struct _CamelRssStorePrivate CamelRssStorePrivate;

struct _CamelRssStore {
	CamelStore parent;
	CamelRssStorePrivate *priv;
};

struct _CamelRssStoreClass {
	CamelStoreClass parent_class;
};

GType		camel_rss_store_get_type	(void);
CamelDataCache *camel_rss_store_get_cache	(CamelRssStore *self);
CamelRssStoreSummary *
		camel_rss_store_get_summary	(CamelRssStore *self);

G_END_DECLS

#endif /* CAMEL_RSS_STORE_H */
