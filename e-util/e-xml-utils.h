/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-xml-utils.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __E_XML_UTILS__
#define __E_XML_UTILS__

#include <glib.h>

#include <libxml/tree.h>

#include <libedataserver/e-xml-utils.h>

G_BEGIN_DECLS

/* lang set to NULL means use the current locale. */
xmlNode *e_xml_get_child_by_name_by_lang             (const xmlNode *parent,
                                                      const xmlChar *child_name,
                                                      const gchar   *lang);
/* lang_list set to NULL means use the current locale. */
xmlNode *e_xml_get_child_by_name_by_lang_list        (const xmlNode *parent,
                                                      const gchar   *name,
                                                      const GList   *lang_list);
xmlNode *e_xml_get_child_by_name_no_lang             (const xmlNode *parent,
                                                      const gchar   *name);


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
void      e_xml_set_double_prop_by_name              ( xmlNode       *parent,
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

gchar    *e_xml_get_translated_string_prop_by_name   (const xmlNode *parent,
                                                      const xmlChar *prop_name);

int e_xml_save_file (const char *filename, xmlDocPtr doc);

G_END_DECLS

#endif /* __E_XML_UTILS__ */
