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

#ifndef E_CONFIG_LOOKUP_RESULT_H
#define E_CONFIG_LOOKUP_RESULT_H

#include <libedataserver/libedataserver.h>

#include <e-util/e-util-enums.h>

/* Standard GObject macros */
#define E_TYPE_CONFIG_LOOKUP_RESULT \
	(e_config_lookup_result_get_type ())
#define E_CONFIG_LOOKUP_RESULT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONFIG_LOOKUP_RESULT, EConfigLookupResult))
#define E_CONFIG_LOOKUP_RESULT_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONFIG_LOOKUP_RESULT, EConfigLookupResultInterface))
#define E_IS_CONFIG_LOOKUP_RESULT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONFIG_LOOKUP_RESULT))
#define E_IS_CONFIG_LOOKUP_RESULT_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONFIG_LOOKUP_RESULT))
#define E_CONFIG_LOOKUP_RESULT_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_CONFIG_LOOKUP_RESULT, EConfigLookupResultInterface))

#define E_CONFIG_LOOKUP_RESULT_PRIORITY_IMAP	1000
#define E_CONFIG_LOOKUP_RESULT_PRIORITY_POP3	2000
#define E_CONFIG_LOOKUP_RESULT_PRIORITY_SMTP	1000

G_BEGIN_DECLS

struct _EConfigLookup;

typedef struct _EConfigLookupResult EConfigLookupResult;
typedef struct _EConfigLookupResultInterface EConfigLookupResultInterface;

struct _EConfigLookupResultInterface {
	GTypeInterface parent_interface;

	EConfigLookupResultKind
			(* get_kind)			(EConfigLookupResult *lookup_result);
	gint		(* get_priority)		(EConfigLookupResult *lookup_result);
	gboolean	(* get_is_complete)		(EConfigLookupResult *lookup_result);
	const gchar *	(* get_protocol)		(EConfigLookupResult *lookup_result);
	const gchar *	(* get_display_name)		(EConfigLookupResult *lookup_result);
	const gchar *	(* get_description)		(EConfigLookupResult *lookup_result);
	const gchar *	(* get_password)		(EConfigLookupResult *lookup_result);
	gboolean	(* configure_source)		(EConfigLookupResult *lookup_result,
							 struct _EConfigLookup *config_lookup,
							 ESource *source);
};

GType		e_config_lookup_result_get_type		(void);
EConfigLookupResultKind
		e_config_lookup_result_get_kind		(EConfigLookupResult *lookup_result);
gint		e_config_lookup_result_get_priority	(EConfigLookupResult *lookup_result);
gboolean	e_config_lookup_result_get_is_complete	(EConfigLookupResult *lookup_result);
const gchar *	e_config_lookup_result_get_protocol	(EConfigLookupResult *lookup_result);
const gchar *	e_config_lookup_result_get_display_name	(EConfigLookupResult *lookup_result);
const gchar *	e_config_lookup_result_get_description	(EConfigLookupResult *lookup_result);
const gchar *	e_config_lookup_result_get_password	(EConfigLookupResult *lookup_result);
gboolean	e_config_lookup_result_configure_source	(EConfigLookupResult *lookup_result,
							 struct _EConfigLookup *config_lookup,
							 ESource *source);
gint		e_config_lookup_result_compare		(gconstpointer lookup_result_a,
							 gconstpointer lookup_result_b);
gboolean	e_config_lookup_result_equal		(gconstpointer lookup_result_a,
							 gconstpointer lookup_result_b);

G_END_DECLS

#endif /* E_CONFIG_LOOKUP_RESULT_H */
