/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CONFIG_LOOKUP_RESULT_SIMPLE_H
#define E_CONFIG_LOOKUP_RESULT_SIMPLE_H

#include <libedataserver/libedataserver.h>
#include <e-util/e-config-lookup-result.h>

/* Standard GObject macros */
#define E_TYPE_CONFIG_LOOKUP_RESULT_SIMPLE \
	(e_config_lookup_result_simple_get_type ())
#define E_CONFIG_LOOKUP_RESULT_SIMPLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONFIG_LOOKUP_RESULT_SIMPLE, EConfigLookupResultSimple))
#define E_CONFIG_LOOKUP_RESULT_SIMPLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONFIG_LOOKUP_RESULT_SIMPLE, EConfigLookupResultSimpleClass))
#define E_IS_CONFIG_LOOKUP_RESULT_SIMPLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONFIG_LOOKUP_RESULT_SIMPLE))
#define E_IS_CONFIG_LOOKUP_RESULT_SIMPLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONFIG_LOOKUP_RESULT_SIMPLE))
#define E_CONFIG_LOOKUP_RESULT_SIMPLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONFIG_LOOKUP_RESULT_SIMPLE, EConfigLookupResultSimpleClass))

G_BEGIN_DECLS

struct _EConfigLookup;

typedef struct _EConfigLookupResultSimple EConfigLookupResultSimple;
typedef struct _EConfigLookupResultSimpleClass EConfigLookupResultSimpleClass;
typedef struct _EConfigLookupResultSimplePrivate EConfigLookupResultSimplePrivate;

/**
 * EConfigLookupResultSimple:
 *
 * Contains only private data that should be read and manipulated using
 * the functions below.
 *
 * Since: 3.26
 **/
struct _EConfigLookupResultSimple {
	/*< private >*/
	GObject parent;
	EConfigLookupResultSimplePrivate *priv;
};

struct _EConfigLookupResultSimpleClass {
	/*< private >*/
	GObjectClass parent_class;

	gboolean	(* configure_source)		(EConfigLookupResult *lookup_result,
							 struct _EConfigLookup *config_lookup,
							 ESource *source);
};

GType		e_config_lookup_result_simple_get_type	(void) G_GNUC_CONST;
EConfigLookupResult *
		e_config_lookup_result_simple_new	(EConfigLookupResultKind kind,
							 gint priority,
							 gboolean is_complete,
							 const gchar *protocol,
							 const gchar *display_name,
							 const gchar *description,
							 const gchar *password);
void		e_config_lookup_result_simple_add_value	(EConfigLookupResult *lookup_result,
							 const gchar *extension_name,
							 const gchar *property_name,
							 const GValue *value);
void		e_config_lookup_result_simple_add_boolean
							(EConfigLookupResult *lookup_result,
							 const gchar *extension_name,
							 const gchar *property_name,
							 gboolean value);
void		e_config_lookup_result_simple_add_int	(EConfigLookupResult *lookup_result,
							 const gchar *extension_name,
							 const gchar *property_name,
							 gint value);
void		e_config_lookup_result_simple_add_uint	(EConfigLookupResult *lookup_result,
							 const gchar *extension_name,
							 const gchar *property_name,
							 guint value);
void		e_config_lookup_result_simple_add_int64	(EConfigLookupResult *lookup_result,
							 const gchar *extension_name,
							 const gchar *property_name,
							 gint64 value);
void		e_config_lookup_result_simple_add_uint64(EConfigLookupResult *lookup_result,
							 const gchar *extension_name,
							 const gchar *property_name,
							 guint64 value);
void		e_config_lookup_result_simple_add_double(EConfigLookupResult *lookup_result,
							 const gchar *extension_name,
							 const gchar *property_name,
							 gdouble value);
void		e_config_lookup_result_simple_add_string(EConfigLookupResult *lookup_result,
							 const gchar *extension_name,
							 const gchar *property_name,
							 const gchar *value);
void		e_config_lookup_result_simple_add_enum	(EConfigLookupResult *lookup_result,
							 const gchar *extension_name,
							 const gchar *property_name,
							 GType enum_type,
							 gint value);

G_END_DECLS

#endif /* E_CONFIG_LOOKUP_RESULT_SIMPLE_H */
