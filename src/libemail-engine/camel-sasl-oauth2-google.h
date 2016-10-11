/*
 * camel-sasl-oauth2-google.h
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

#if !defined (__LIBEMAIL_ENGINE_H_INSIDE__) && !defined (LIBEMAIL_ENGINE_COMPILATION)
#error "Only <libemail-engine/libemail-engine.h> should be included directly."
#endif

#ifndef CAMEL_SASL_OAUTH2_GOOGLE_H
#define CAMEL_SASL_OAUTH2_GOOGLE_H

#include <camel/camel.h>

/* Standard GObject macros */
#define CAMEL_TYPE_SASL_OAUTH2_GOOGLE \
	(camel_sasl_oauth2_google_get_type ())
#define CAMEL_SASL_OAUTH2_GOOGLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CAMEL_TYPE_SASL_OAUTH2_GOOGLE, CamelSaslOAuth2Google))
#define CAMEL_SASL_OAUTH2_GOOGLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CAMEL_TYPE_SASL_OAUTH2_GOOGLE, CamelSaslOAuth2GoogleClass))
#define CAMEL_IS_SASL_OAUTH2_GOOGLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CAMEL_TYPE_SASL_OAUTH2_GOOGLE))
#define CAMEL_IS_SASL_OAUTH2_GOOGLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CAMEL_TYPE_SASL_OAUTH2_GOOGLE))
#define CAMEL_SASL_OAUTH2_GOOGLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CAMEL_TYPE_SASL_OAUTH2_GOOGLE, CamelSaslOAuth2GoogleClass))

G_BEGIN_DECLS

typedef struct _CamelSaslOAuth2Google CamelSaslOAuth2Google;
typedef struct _CamelSaslOAuth2GoogleClass CamelSaslOAuth2GoogleClass;
typedef struct _CamelSaslOAuth2GooglePrivate CamelSaslOAuth2GooglePrivate;

struct _CamelSaslOAuth2Google {
	CamelSasl parent;
	CamelSaslOAuth2GooglePrivate *priv;
};

struct _CamelSaslOAuth2GoogleClass {
	CamelSaslClass parent_class;
};

GType		camel_sasl_oauth2_google_get_type	(void) G_GNUC_CONST;

G_END_DECLS

#endif /* CAMEL_SASL_OAUTH2_GOOGLE_H */
