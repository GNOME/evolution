/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2001-2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __E_XML_HASH_UTILS_H__
#define __E_XML_HASH_UTILS_H__

#include <glib.h>
#include <libxml/parser.h>

typedef enum {
	E_XML_HASH_TYPE_OBJECT_UID,
	E_XML_HASH_TYPE_PROPERTY
} EXmlHashType;

GHashTable *e_xml_to_hash      (xmlDoc       *doc,
				EXmlHashType  type);
xmlDoc     *e_xml_from_hash    (GHashTable   *hash,
				EXmlHashType  type,
				const char   *root_node);

void        e_xml_destroy_hash (GHashTable   *hash);



typedef enum {
	E_XMLHASH_STATUS_SAME,
	E_XMLHASH_STATUS_DIFFERENT,
	E_XMLHASH_STATUS_NOT_FOUND
} EXmlHashStatus;

typedef void (* EXmlHashFunc) (const char *key, gpointer user_data);

typedef struct EXmlHash EXmlHash;

EXmlHash      *e_xmlhash_new         (const char   *filename);

void           e_xmlhash_add         (EXmlHash     *hash,
				      const char   *key,
				      const char   *data);
void           e_xmlhash_remove      (EXmlHash     *hash,
				      const char   *key);

EXmlHashStatus e_xmlhash_compare     (EXmlHash     *hash,
				      const char   *key,
				      const char   *compare_data);
void           e_xmlhash_foreach_key (EXmlHash     *hash,
				      EXmlHashFunc  func,
				      gpointer      user_data);

void           e_xmlhash_write       (EXmlHash     *hash);
void           e_xmlhash_destroy     (EXmlHash     *hash);

#endif
