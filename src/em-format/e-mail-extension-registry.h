/*
 * e-mail-extension-registry.h
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

#ifndef E_MAIL_EXTENSION_REGISTRY_H
#define E_MAIL_EXTENSION_REGISTRY_H

#include <glib-object.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_EXTENSION_REGISTRY \
	(e_mail_extension_registry_get_type ())
#define E_MAIL_EXTENSION_REGISTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_EXTENSION_REGISTRY, EMailExtensionRegistry))
#define E_MAIL_EXTENSION_REGISTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_EXTENSION_REGISTRY, EMailExtensionRegistryClass))
#define E_IS_MAIL_EXTENSION_REGISTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_EXTENSION_REGISTRY))
#define E_IS_MAIL_EXTENSION_REGISTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_EXTENSION_REGISTRY))
#define E_MAIL_EXTENSION_REGISTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_EXTENSION_REGISTRY, EMailExtensionRegistryClass))

G_BEGIN_DECLS

typedef struct _EMailExtensionRegistry EMailExtensionRegistry;
typedef struct _EMailExtensionRegistryClass EMailExtensionRegistryClass;
typedef struct _EMailExtensionRegistryPrivate EMailExtensionRegistryPrivate;

struct _EMailExtensionRegistry {
	GObject parent;
	EMailExtensionRegistryPrivate *priv;
};

struct _EMailExtensionRegistryClass {
	GObjectClass parent_class;
};

GType		e_mail_extension_registry_get_type
					(void) G_GNUC_CONST;
GQueue *	e_mail_extension_registry_get_for_mime_type
					(EMailExtensionRegistry *registry,
					 const gchar *mime_type);
GQueue *	e_mail_extension_registry_get_fallback
					(EMailExtensionRegistry *registry,
					 const gchar *mime_type);

G_END_DECLS

/******************************************************************************/

/* Standard GObject macros */
#define E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY \
	(e_mail_parser_extension_registry_get_type ())
#define E_MAIL_PARSER_EXTENSION_REGISTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY, EMailParserExtensionRegistry))
#define E_MAIL_PARSER_EXTENSION_REGISTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY, EMailParserExtensionRegistryClass))
#define E_IS_MAIL_PARSER_EXTENSION_REGISTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY))
#define E_IS_MAIL_PARSER_EXTENSION_REGISTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY))
#define E_MAIL_PARSER_EXTENSION_REGISTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY, EMailParserExtensionRegistryClass))

G_BEGIN_DECLS

typedef struct _EMailParserExtensionRegistry EMailParserExtensionRegistry;
typedef struct _EMailParserExtensionRegistryClass EMailParserExtensionRegistryClass;
typedef struct _EMailParserExtensionRegistryPrivate EMailParserExtensionRegistryPrivate;

struct _EMailParserExtensionRegistry {
	EMailExtensionRegistry parent;
	EMailParserExtensionRegistryPrivate *priv;
};

struct _EMailParserExtensionRegistryClass {
	EMailExtensionRegistryClass parent_class;
};

GType		e_mail_parser_extension_registry_get_type
					(void) G_GNUC_CONST;
void		e_mail_parser_extension_registry_load
					(EMailParserExtensionRegistry *registry);

G_END_DECLS

/******************************************************************************/

/* Standard GObject macros */
#define E_TYPE_MAIL_FORMATTER_EXTENSION_REGISTRY \
	(e_mail_formatter_extension_registry_get_type ())
#define E_MAIL_FORMATTER_EXTENSION_REGISTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_FORMATTER_EXTENSION_REGISTRY, EMailFormatterExtensionRegistry))
#define E_MAIL_FORMATTER_EXTENSION_REGISTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_FORMATTER_EXTENSION_REGISTRY, EMailFormatterExtensionRegistryClass))
#define E_IS_MAIL_FORMATTER_EXTENSION_REGISTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_FORMATTER_EXTENSION_REGISTRY))
#define E_IS_MAIL_FORMATTER_EXTENSION_REGISTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_FORMATTER_EXTENSION_REGISTRY))
#define E_MAIL_FORMATTER_EXTENSION_REGISTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_FORMATTER_EXTENSION_REGISTRY, EMailFormatterExtensionRegistryClass))

G_BEGIN_DECLS

typedef struct _EMailFormatterExtensionRegistry EMailFormatterExtensionRegistry;
typedef struct _EMailFormatterExtensionRegistryClass EMailFormatterExtensionRegistryClass;
typedef struct _EMailFormatterExtensionRegistryPrivate EMailFormatterExtensionRegistryPrivate;

struct _EMailFormatterExtensionRegistry {
	EMailExtensionRegistry parent;
	EMailFormatterExtensionRegistryPrivate *priv;
};

struct _EMailFormatterExtensionRegistryClass {
	EMailExtensionRegistryClass parent_class;
};

GType		e_mail_formatter_extension_registry_get_type
					(void) G_GNUC_CONST;
void		e_mail_formatter_extension_registry_load
					(EMailFormatterExtensionRegistry *registry,
					 GType base_extension_type);

G_END_DECLS

#endif /* E_MAIL_EXTENSION_REGISTRY_H */
