/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef E_CID_REQUEST_H
#define E_CID_REQUEST_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_CID_RESOLVER \
	(e_cid_resolver_get_type ())
#define E_CID_RESOLVER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CID_RESOLVER, ECidResolver))
#define E_CID_RESOLVER_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CID_RESOLVER, ECidResolverInterface))
#define E_IS_CID_RESOLVER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CID_RESOLVER))
#define E_IS_CID_RESOLVER_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CID_RESOLVER))
#define E_CID_RESOLVER_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_CID_RESOLVER, ECidResolverInterface))

#define E_TYPE_CID_REQUEST \
	(e_cid_request_get_type ())
#define E_CID_REQUEST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CID_REQUEST, ECidRequest))
#define E_CID_REQUEST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CID_REQUEST, ECidRequestClass))
#define E_IS_CID_REQUEST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CID_REQUEST))
#define E_IS_CID_REQUEST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CID_REQUEST))
#define E_CID_REQUEST_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CID_REQUEST, ECidRequestClass))

G_BEGIN_DECLS

typedef struct _ECidResolver ECidResolver;
typedef struct _ECidResolverInterface ECidResolverInterface;

struct _ECidResolverInterface {
	GTypeInterface parent_interface;

	CamelMimePart *	(* ref_part)		(ECidResolver *resolver,
						 const gchar *uri);

	gchar *		(* dup_mime_type)	(ECidResolver *resolver,
						 const gchar *uri);
};

GType		e_cid_resolver_get_type		(void);
CamelMimePart *	e_cid_resolver_ref_part		(ECidResolver *resolver,
						 const gchar *uri);
gchar *		e_cid_resolver_dup_mime_type	(ECidResolver *resolver,
						 const gchar *uri);

typedef struct _ECidRequest ECidRequest;
typedef struct _ECidRequestClass ECidRequestClass;
typedef struct _ECidRequestPrivate ECidRequestPrivate;

struct _ECidRequest {
	GObject parent;
	ECidRequestPrivate *priv;
};

struct _ECidRequestClass {
	GObjectClass parent;
};

GType		e_cid_request_get_type		(void) G_GNUC_CONST;
EContentRequest *
		e_cid_request_new		(void);

G_END_DECLS

#endif /* E_CID_REQUEST_H */
