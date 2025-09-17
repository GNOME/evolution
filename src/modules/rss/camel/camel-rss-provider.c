/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <camel/camel.h>

#include "camel-rss-store.h"

static CamelProvider rss_provider = {
	.protocol = "rss",
	.name = N_("News and Blogs"),
	.description = N_("This is a provider for reading RSS news and blogs."),
	.domain = "mail",
	.flags = CAMEL_PROVIDER_IS_LOCAL | CAMEL_PROVIDER_IS_SOURCE | CAMEL_PROVIDER_IS_STORAGE,
	.url_flags = CAMEL_URL_NEED_HOST,
	.extra_conf = NULL,
	.port_entries = NULL,
};

void
camel_provider_module_init (void)
{
	rss_provider.object_types[CAMEL_PROVIDER_STORE] = camel_rss_store_get_type ();
	rss_provider.authtypes = NULL;
	rss_provider.translation_domain = GETTEXT_PACKAGE;

	camel_provider_register (&rss_provider);
}
