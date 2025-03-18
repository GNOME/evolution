/*
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
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __E_XML_UTILS__
#define __E_XML_UTILS__

#include <glib.h>

#include <libxml/tree.h>

G_BEGIN_DECLS

gint     e_xml_get_integer_prop_by_name              (const xmlNode *parent,
                                                      const xmlChar *prop_name);
gint     e_xml_get_integer_prop_by_name_with_default (const xmlNode *parent,
                                                      const xmlChar *prop_name,
                                                      gint           def);
void     e_xml_set_integer_prop_by_name              (xmlNode       *parent,
                                                      const xmlChar *prop_name,
                                                      gint           value);

guint    e_xml_get_uint_prop_by_name                 (const xmlNode *parent,
                                                      const xmlChar *prop_name);
guint    e_xml_get_uint_prop_by_name_with_default    (const xmlNode *parent,
                                                      const xmlChar *prop_name,
                                                      guint          def);
void     e_xml_set_uint_prop_by_name                 (xmlNode       *parent,
                                                      const xmlChar *prop_name,
                                                      guint          value);

gboolean e_xml_get_bool_prop_by_name                 (const xmlNode *parent,
                                                      const xmlChar *prop_name);
gboolean e_xml_get_bool_prop_by_name_with_default    (const xmlNode *parent,
                                                      const xmlChar *prop_name,
                                                      gboolean       def);
void     e_xml_set_bool_prop_by_name                 (xmlNode       *parent,
                                                      const xmlChar *prop_name,
                                                      gboolean       value);

gdouble  e_xml_get_double_prop_by_name               (const xmlNode *parent,
                                                      const xmlChar *prop_name);
gdouble  e_xml_get_double_prop_by_name_with_default  (const xmlNode *parent,
                                                      const xmlChar *prop_name,
                                                      gdouble        def);
void      e_xml_set_double_prop_by_name              (xmlNode       *parent,
                                                      const xmlChar *prop_name,
                                                      gdouble        value);

gchar    *e_xml_get_string_prop_by_name              (const xmlNode *parent,
                                                      const xmlChar *prop_name);
gchar    *e_xml_get_string_prop_by_name_with_default (const xmlNode *parent,
                                                      const xmlChar *prop_name,
                                                      const gchar   *def);
void      e_xml_set_string_prop_by_name              (xmlNode       *parent,
                                                      const xmlChar *prop_name,
                                                      const gchar   *value);

G_END_DECLS

#endif /* __E_XML_UTILS__ */
