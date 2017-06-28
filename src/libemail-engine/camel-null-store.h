/*
 * camel-null-store.h
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

/* Evolution kinda sorta supports multiple identities by allowing users
 * to set up so-called "transport-only" accounts by choosing "None" for
 * the account type.  This bizarre hack keeps that bizzare hack working
 * until we can support multiple identities properly. */

#if !defined (__LIBEMAIL_ENGINE_H_INSIDE__) && !defined (LIBEMAIL_ENGINE_COMPILATION)
#error "Only <libemail-engine/libemail-engine.h> should be included directly."
#endif

#ifndef CAMEL_NULL_STORE_H
#define CAMEL_NULL_STORE_H

#include <camel/camel.h>

/* Standard GObject macros */
#define CAMEL_TYPE_NULL_STORE \
	(camel_null_store_get_type ())
#define CAMEL_NULL_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_NULL_STORE, CamelNullStore))
#define CAMEL_NULL_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_NULL_STORE, CamelNullStoreClass))
#define CAMEL_IS_NULL_STORE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_NULL_STORE))
#define CAMEL_IS_NULL_STORE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_NULL_STORE))
#define CAMEL_NULL_STORE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_NULL_STORE, CamelNullStoreClass))

#define CAMEL_TYPE_NULL_TRANSPORT \
	(camel_null_transport_get_type ())
#define CAMEL_NULL_TRANSPORT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_NULL_STORE, CamelNullTransport))
#define CAMEL_NULL_TRANSPORT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_NULL_TRANSPORT, CamelNullTransportClass))
#define CAMEL_IS_NULL_TRANSPORT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_NULL_TRANSPORT))
#define CAMEL_IS_NULL_TRANSPORT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_NULL_TRANSPORT))
#define CAMEL_NULL_TRANSPORT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_NULL_TRANSPORT, CamelNullTransportClass))

G_BEGIN_DECLS

typedef struct _CamelNullStore CamelNullStore;
typedef struct _CamelNullStoreClass CamelNullStoreClass;

struct _CamelNullStore {
	CamelStore parent;
};

struct _CamelNullStoreClass {
	CamelStoreClass parent_class;
};

GType		camel_null_store_get_type		(void);

/* ------------------------------------------------------------------------- */

typedef struct _CamelNullTransport CamelNullTransport;
typedef struct _CamelNullTransportClass CamelNullTransportClass;

struct _CamelNullTransport {
	CamelTransport parent;
};

struct _CamelNullTransportClass {
	CamelTransportClass parent_class;
};

GType		camel_null_transport_get_type		(void);

/* ------------------------------------------------------------------------- */

void		camel_null_store_register_provider	(void);

#endif /* CAMEL_NULL_STORE_H */
