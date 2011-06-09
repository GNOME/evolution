/*
 * camel-sasl-xoauth.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef CAMEL_SASL_XOAUTH_H
#define CAMEL_SASL_XOAUTH_H

#include <camel/camel.h>

/* Standard GObject macros */
#define CAMEL_TYPE_SASL_XOAUTH \
	(camel_sasl_xoauth_get_type ())
#define CAMEL_SASL_XOAUTH(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_SASL_XOAUTH, CamelSaslXOAuth))
#define CAMEL_SASL_XOAUTH_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_SASL_XOAUTH, CamelSaslXOAuthClass))
#define CAMEL_IS_SASL_XOAUTH(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_SASL_XOAUTH))
#define CAMEL_IS_SASL_XOAUTH_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_SASL_XOAUTH))
#define CAMEL_SASL_XOAUTH_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_SASL_XOAUTH, CamelSaslXOAuthClass))

G_BEGIN_DECLS

typedef struct _CamelSaslXOAuth CamelSaslXOAuth;
typedef struct _CamelSaslXOAuthClass CamelSaslXOAuthClass;
typedef struct _CamelSaslXOAuthPrivate CamelSaslXOAuthPrivate;

struct _CamelSaslXOAuth {
	CamelSasl parent;
	CamelSaslXOAuthPrivate *priv;
};

struct _CamelSaslXOAuthClass {
	CamelSaslClass parent_class;
};

GType		camel_sasl_xoauth_get_type	(void);
void		camel_sasl_xoauth_type_register	(GTypeModule *type_module);

G_END_DECLS

#endif /* CAMEL_SASL_XOAUTH_H */
