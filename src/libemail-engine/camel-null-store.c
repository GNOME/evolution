/*
 * camel-null-store.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "camel-null-store.h"

G_DEFINE_TYPE (CamelNullStore, camel_null_store, CAMEL_TYPE_STORE)
G_DEFINE_TYPE (CamelNullTransport, camel_null_transport, CAMEL_TYPE_TRANSPORT)

static CamelProvider null_provider = {
	.protocol = "none",
	.name = N_("None"),
	.description = NULL,
	.domain = "mail",
	/* XXX This provider is not really a "source", the
	 *     flag just gets it shown in the account editor. */
	.flags = CAMEL_PROVIDER_IS_SOURCE,
	.url_flags = 0,
	.extra_conf = NULL,
	.port_entries = NULL,
	.object_types = { 0, 0 },  /* see below */
	.authtypes = NULL,
	.translation_domain = GETTEXT_PACKAGE
};

static void
camel_null_store_class_init (CamelNullStoreClass *class)
{
	CamelServiceClass *service_class;

	/* We should never be invoking methods on a CamelNullStore,
	 * but thankfully, in case we do, CamelStore has NULL function
	 * pointer checks in all of its wrapper functions.  So it will
	 * emit a runtime warning, which is what we want, and frees us
	 * from having to override any class methods here. */

	service_class = CAMEL_SERVICE_CLASS (class);
	service_class->settings_type = CAMEL_TYPE_SETTINGS;
}

static void
camel_null_store_init (CamelNullStore *store)
{
	/* nothing to do */
}

static void
camel_null_transport_class_init (CamelNullTransportClass *class)
{
	CamelServiceClass *service_class;

	/* We should never be invoking methods on a CamelNullTransport,
	 * but thankfully, in case we do, CamelTransport has NULL function
	 * pointer checks in all of its wrapper functions.  So it will
	 * emit a runtime warning, which is what we want, and frees us
	 * from having to override any class methods here. */

	service_class = CAMEL_SERVICE_CLASS (class);
	service_class->settings_type = CAMEL_TYPE_SETTINGS;
}

static void
camel_null_transport_init (CamelNullTransport *transport)
{
	/* nothing to do */
}

void
camel_null_store_register_provider (void)
{
	null_provider.object_types[CAMEL_PROVIDER_STORE] = CAMEL_TYPE_NULL_STORE;

	null_provider.object_types[CAMEL_PROVIDER_TRANSPORT] = CAMEL_TYPE_NULL_TRANSPORT;

	camel_provider_register (&null_provider);
}
