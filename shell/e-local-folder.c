/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-local-folder.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 *
 * Author: Ettore Perazzoli
 */

/* The metafile goes like this:

   <?xml version="1.0"?>
   <efolder>
           <type>mail</type>

	   <name>Inbox</name>
	   <name locale="it">Posta in Arrivo</name>

	   <description>This is the default folder for incoming messages</description>
	   <description locale="it">Cartella che contiene i messaggi in arrivo</description>

	   <homepage>http://www.somewhere.net</homepage>
   </efolder>

   FIXME: Do we want to use a namespace for this?
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <unistd.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <libgnome/gnome-util.h>

#include <gal/util/e-util.h>
#include <gal/util/e-xml-utils.h>

#include <libgnome/gnome-util.h>

#include "e-local-folder.h"


#define PARENT_TYPE E_TYPE_FOLDER
static EFolderClass *parent_class = NULL;

#define URI_PREFIX     "file://"
#define URI_PREFIX_LEN 7

/* This provides the name and the description for a specific locale.  */
struct _I18nInfo {
	char *language_id;
	char *name;
	char *description;
};
typedef struct _I18nInfo I18nInfo;

struct _ELocalFolderPrivate {
	GHashTable *language_id_to_i18n_info;
};


/* Locale information.  */

static char *global_language_id = NULL;


/* I18nInfo handling.  */

static I18nInfo *
i18n_info_new (const char *language_id,
	       const char *name,
	       const char *description)
{
	I18nInfo *info;

	info = g_new (I18nInfo, 1);
	info->language_id = g_strdup (language_id);
	info->name        = g_strdup (name);
	info->description = g_strdup (description);

	return info;
}

static void
i18n_info_free (I18nInfo *info)
{
	g_free (info->language_id);
	g_free (info->name);
	g_free (info->description);

	g_free (info);
}


/* Language ID -> I18nInfo hash table handling.  */

static void
add_i18n_info_to_hash (GHashTable *language_id_to_i18n_info_hash,
		       I18nInfo *i18n_info)
{
	I18nInfo *existing_i18n_info;

	existing_i18n_info = (I18nInfo *) g_hash_table_lookup (language_id_to_i18n_info_hash,
							       i18n_info->language_id);
	if (existing_i18n_info != NULL) {
		g_hash_table_remove (language_id_to_i18n_info_hash,
				     i18n_info->language_id);
		i18n_info_free (existing_i18n_info);
	}

	g_hash_table_insert (language_id_to_i18n_info_hash, i18n_info->language_id, i18n_info);
}

static void
language_id_to_i18n_info_hash_foreach_free (void *key,
					    void *value,
					    void *data)
{
	i18n_info_free ((I18nInfo *) value);
}

static I18nInfo *
get_i18n_info_for_language (ELocalFolder *local_folder,
			    const char *language_id)
{
	ELocalFolderPrivate *priv;
	I18nInfo *i18n_info;

	priv = local_folder->priv;

	if (language_id == NULL)
		language_id = global_language_id;

	i18n_info = g_hash_table_lookup (priv->language_id_to_i18n_info, language_id);

	/* For locale info like `en_UK@yadda', we try to use `en' as a backup.  */
	/* Note: this is exactly the same thing that gnome-config does with the
	   I18N value handling.  I hope it works.  */
	if (i18n_info == NULL) {
		size_t n;

		n = strcspn (language_id, "@_");
		if (language_id[n] != '\0') {
			char *simplified_language_id;

			simplified_language_id = g_strndup (language_id, n);
			i18n_info = g_hash_table_lookup (priv->language_id_to_i18n_info,
							 simplified_language_id);
		}
	}

	return i18n_info;
}


/* Locale handling.  */

static void
setup_global_language_id (void)
{
	/* FIXME: Implement.  */
	global_language_id = "C";
}

/* Update the EFolder attributes according to the current locale.  */
static void
update_for_global_locale (ELocalFolder *local_folder)
{
	I18nInfo *i18n_info;

	i18n_info = get_i18n_info_for_language (local_folder, NULL);

	if (i18n_info == NULL)
		i18n_info = get_i18n_info_for_language (local_folder, "C");

	g_assert (i18n_info != NULL);

	e_folder_set_name        (E_FOLDER (local_folder), i18n_info->name);
	e_folder_set_description (E_FOLDER (local_folder), i18n_info->description);
}


/* XML tree handling.  */

static char *
get_string_value (xmlNode *node,
		  const char *name)
{
	xmlNode *p;
	xmlChar *xml_string;
	char *retval;

	p = e_xml_get_child_by_name (node, (xmlChar *) name);
	if (p == NULL)
		return NULL;

	p = e_xml_get_child_by_name (p, (xmlChar *) "text");
	if (p == NULL)
		return NULL;

	xml_string = xmlNodeListGetString (node->doc, p, TRUE);
	retval = g_strdup ((char *) xml_string);
	xmlFree (xml_string);

	return retval;
}

static void
retrieve_info_item (ELocalFolder *local_folder,
		    xmlNode *node)
{
	xmlChar *lang;
	char *name;
	char *description;

	lang        = xmlGetProp (node, "lang");
	name        = get_string_value (node, "name");
	description = get_string_value (node, "description");

	if (lang == NULL) {
		e_local_folder_add_i18n_info (local_folder, "C", name, description);
	} else {
		e_local_folder_add_i18n_info (local_folder, lang, name, description);
		xmlFree (lang);
	}

	g_free (name);
	g_free (description);
}

static void
retrieve_info (ELocalFolder *local_folder,
	       xmlNode *root_xml_node)
{
	ELocalFolderPrivate *priv;
	xmlNode *p;

	priv = local_folder->priv;

	for (p = root_xml_node->children; p != NULL; p = p->next) {
		if (xmlStrcmp (p->name, "info") == 0)
			retrieve_info_item (local_folder, p);
	}
}

static gboolean
construct_loading_metadata (ELocalFolder *local_folder,
			    const char *path)
{
	EFolder *folder;
	xmlDoc *doc;
	xmlNode *root;
	char *type;
	char *metadata_path;
	char *physical_uri;

	folder = E_FOLDER (local_folder);

	metadata_path = g_concat_dir_and_file (path, E_LOCAL_FOLDER_METADATA_FILE_NAME);

	doc = xmlParseFile (metadata_path);
	if (doc == NULL) {
		g_free (metadata_path);
		return FALSE;
	}

	root = xmlDocGetRootElement (doc);
	if (root == NULL || root->name == NULL || strcmp (root->name, "efolder") != 0) {
		g_free (metadata_path);
		xmlFreeDoc (doc);
		return FALSE;
	}

	type = get_string_value (root, "type");
	if (type == NULL) {
		g_free (metadata_path);
		xmlFreeDoc (doc);
		return FALSE;
	}

	e_local_folder_construct (local_folder, g_basename (path), type, NULL);
	g_free (type);

	retrieve_info (local_folder, root);

	xmlFreeDoc (doc);

	physical_uri = g_strconcat (URI_PREFIX, path, NULL);
	e_folder_set_physical_uri (folder, physical_uri);
	g_free (physical_uri);

	g_free (metadata_path);

	return TRUE;
}

static gboolean
save_metadata (ELocalFolder *local_folder)
{
	EFolder *folder;
	xmlDoc *doc;
	xmlNode *root;
	const char *physical_directory;
	char *physical_path;

	folder = E_FOLDER (local_folder);

	doc = xmlNewDoc ((xmlChar *) "1.0");
	root = xmlNewDocNode (doc, NULL, (xmlChar *) "efolder", NULL);
	xmlDocSetRootElement (doc, root);

	xmlNewChild (root, NULL, (xmlChar *) "type",
		     (xmlChar *) e_folder_get_type_string (folder));

	if (e_folder_get_description (folder) != NULL)
		xmlNewTextChild (root, NULL, (xmlChar *) "description",
				 (xmlChar *) e_folder_get_description (folder));

	physical_directory = e_folder_get_physical_uri (folder) + URI_PREFIX_LEN - 1;
	physical_path = g_concat_dir_and_file (physical_directory, E_LOCAL_FOLDER_METADATA_FILE_NAME);

	if (xmlSaveFile (physical_path, doc) < 0) {
		unlink (physical_path);
		g_free (physical_path);
		xmlFreeDoc (doc);
		return FALSE;
	}

	g_free (physical_path);

	xmlFreeDoc (doc);
	return TRUE;
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	ELocalFolder *local_folder;
	ELocalFolderPrivate *priv;

	local_folder = E_LOCAL_FOLDER (object);
	priv = local_folder->priv;

	g_hash_table_foreach (priv->language_id_to_i18n_info,
			      language_id_to_i18n_info_hash_foreach_free,
			      NULL);
	g_hash_table_destroy (priv->language_id_to_i18n_info);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (ELocalFolderClass *klass)
{
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (e_folder_get_type ());

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	setup_global_language_id ();
}

static void
init (ELocalFolder *local_folder)
{
	ELocalFolderPrivate *priv;

	priv = g_new (ELocalFolderPrivate, 1);
	priv->language_id_to_i18n_info = g_hash_table_new (g_str_hash, g_str_equal);

	local_folder->priv = priv;
}


void
e_local_folder_construct (ELocalFolder *local_folder,
			  const char *name,
			  const char *type,
			  const char *description)
{
	ELocalFolderPrivate *priv;
	I18nInfo *i18n_info;

	g_return_if_fail (local_folder != NULL);
	g_return_if_fail (E_IS_LOCAL_FOLDER (local_folder));
	g_return_if_fail (name != NULL);
	g_return_if_fail (type != NULL);

	priv = local_folder->priv;

	e_folder_construct (E_FOLDER (local_folder), name, type, description);

	i18n_info = i18n_info_new ("C", name, description);
	add_i18n_info_to_hash (priv->language_id_to_i18n_info, i18n_info);
}

EFolder *
e_local_folder_new (const char *name,
		    const char *type,
		    const char *description)
{
	ELocalFolder *local_folder;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (type != NULL, NULL);

	local_folder = g_object_new (e_local_folder_get_type (), NULL);

	e_local_folder_construct (local_folder, name, type, description);

	return E_FOLDER (local_folder);
}

EFolder *
e_local_folder_new_from_path (const char *path)
{
	EFolder *folder;

	g_return_val_if_fail (g_path_is_absolute (path), NULL);

	folder = g_object_new (e_local_folder_get_type (), NULL);

	if (! construct_loading_metadata (E_LOCAL_FOLDER (folder), path)) {
		g_object_unref (folder);
		return NULL;
	}

	return folder;
}

gboolean
e_local_folder_save (ELocalFolder *local_folder)
{
	g_return_val_if_fail (local_folder != NULL, FALSE);
	g_return_val_if_fail (E_IS_LOCAL_FOLDER (local_folder), FALSE);
	g_return_val_if_fail (e_folder_get_physical_uri (E_FOLDER (local_folder)) != NULL, FALSE);

	return save_metadata (local_folder);
}


/**
 * e_local_folder_add_i18n_info:
 * @local_folder: A pointer to an ELocalFolder object
 * @language_id: An I1I8N locale ID
 * @name: Name for @local_folder in the specified @language_id
 * @description: Description for @local_folder in the specified @language_id
 * 
 * Set the @name and @description for the specified @language_id locale.
 **/
void
e_local_folder_add_i18n_info (ELocalFolder *local_folder,
			      const char *language_id,
			      const char *name,
			      const char *description)
{
	ELocalFolderPrivate *priv;
	I18nInfo *info;

	g_return_if_fail (local_folder != NULL);
	g_return_if_fail (E_IS_LOCAL_FOLDER (local_folder));
	g_return_if_fail (language_id != NULL);
	g_return_if_fail (name != NULL || description != NULL);

	priv = local_folder->priv;

	info = i18n_info_new (language_id, name, description);
	add_i18n_info_to_hash (priv->language_id_to_i18n_info, info);

	update_for_global_locale (local_folder);
}

/**
 * e_local_folder_get_i18n_info:
 * @local_folder: A pointer to an ELocalFolder object
 * @language_id: The ID of the language whose locale we want to retrieve name
 * and description for
 * @language_id_return: The actual locale ID that the name and description are
 * saved under (e.g. if you ask for an "en_UK@yadda", we might give you the
 * info for just "en")
 * @name_return: A pointer to a pointer that will point to the i18nized name on
 * return.  Can be NULL.
 * @description_return: A pointer to a pointer that will point to the i18n
 * description on return.  Can be NULL.
 * 
 * Retrieve the name and description for @local_folder in the specified locale.
 * 
 * Return value: %TRUE if some info is found for that @language_id, %FALSE
 * otherwise.
 **/
gboolean
e_local_folder_get_i18n_info (ELocalFolder *local_folder,
			      const char *language_id,
			      const char **language_id_return,
			      const char **name_return,
			      const char **description_return)
{
	ELocalFolderPrivate *priv;
	I18nInfo *i18n_info;

	g_return_val_if_fail (local_folder != NULL, FALSE);
	g_return_val_if_fail (E_IS_LOCAL_FOLDER (local_folder), FALSE);
	g_return_val_if_fail (language_id != NULL, FALSE);

	priv = local_folder->priv;

	i18n_info = get_i18n_info_for_language (local_folder, language_id);

	if (i18n_info == NULL) {
		if (language_id_return != NULL)
			*language_id_return = NULL;
		if (name_return != NULL)
			*name_return = NULL;
		if (description_return != NULL)
			*description_return = NULL;

		return FALSE;
	}

	if (language_id_return != NULL)
		*language_id_return = i18n_info->language_id;
	if (name_return != NULL)
		*name_return = i18n_info->name;
	if (description_return != NULL)
		*description_return = i18n_info->description;

	return TRUE;
}


E_MAKE_TYPE (e_local_folder, "ELocalFolder", ELocalFolder, class_init, init, PARENT_TYPE)
