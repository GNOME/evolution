/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors:
 *	Rodrigo Moya <rodrigo@ximian.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "e-xml-utils.h"
#include "e-xmlhash.h"

struct _EXmlHash {
	gchar *filename;
	GHashTable *objects;
};

EXmlHash *
e_xmlhash_new (const char *filename)
{
	EXmlHash *hash;

	g_return_val_if_fail (filename != NULL, NULL);

	hash = g_new0 (EXmlHash, 1);
	hash->filename = g_strdup (filename);
	hash->objects = e_xml_file_to_hash (filename, E_XML_FILE_TYPE_OBJECT_UID);

	return hash;
}

void
e_xmlhash_add (EXmlHash *hash, const char *key, const char *data)
{
	g_return_if_fail (hash != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (data != NULL);

	e_xmlhash_remove (hash, key);
	g_hash_table_insert (hash->objects, g_strdup (key), g_strdup (data));
}

void
e_xmlhash_remove (EXmlHash *hash, const char *key)
{
	void *orig_key;
	void *orig_value;

	g_return_if_fail (hash != NULL);
	g_return_if_fail (key != NULL);

	if (g_hash_table_lookup_extended (hash->objects, key, &orig_key, &orig_value)) {
		g_hash_table_remove (hash->objects, key);
		g_free (orig_key);
		g_free (orig_value);
	}
}

EXmlHashStatus
e_xmlhash_compare (EXmlHash *hash, const char *key, const char *compare_data)
{
	char *data;
	int rc;

	g_return_val_if_fail (hash != NULL, E_XMLHASH_STATUS_NOT_FOUND);
	g_return_val_if_fail (key != NULL, E_XMLHASH_STATUS_NOT_FOUND);
	g_return_val_if_fail (compare_data != NULL, E_XMLHASH_STATUS_NOT_FOUND);

	data = g_hash_table_lookup (hash->objects, key);
	if (!data)
		return E_XMLHASH_STATUS_NOT_FOUND;

	rc = strcmp (data, compare_data);
	if (rc == 0)
		return E_XMLHASH_STATUS_SAME;

	return E_XMLHASH_STATUS_DIFFERENT;
}

typedef struct {
	EXmlHashFunc func;
	gpointer user_data;
} _foreach_data_t;

static void
foreach_hash_func (gpointer key, gpointer value, gpointer user_data)
{
	_foreach_data_t *data = (_foreach_data_t *) user_data;

	g_return_if_fail (data != NULL);
	g_return_if_fail (data->func != NULL);

	data->func ((const char *) key, data->user_data);
}

void
e_xmlhash_foreach_key (EXmlHash *hash, EXmlHashFunc func, gpointer user_data)
{
	_foreach_data_t data;

	g_return_if_fail (hash != NULL);
	g_return_if_fail (func != NULL);

	data.func = func;
	data.user_data = user_data;
	g_hash_table_foreach (hash->objects, (GHFunc) foreach_hash_func, &data);
}

void
e_xmlhash_write (EXmlHash *hash)
{
	g_return_if_fail (hash != NULL);

	e_xml_file_from_hash (hash->filename, E_XML_FILE_TYPE_OBJECT_UID,
				"xmlhash", hash->objects);
}

void
e_xmlhash_destroy (EXmlHash *hash)
{
	g_return_if_fail (hash != NULL);

	g_free (hash->filename);
	e_xml_file_destroy_hash (hash->objects);

	g_free (hash);
}
