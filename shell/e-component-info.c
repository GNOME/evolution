/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-component-info.c - Load/save information about Evolution components.
 *
 * Copyright (C) 2002 Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-component-info.h"

#include "e-util/e-lang-utils.h"

#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include <string.h>
#include <stdlib.h>


static char *
get_value_for_node (xmlNode *node)
{
	xmlChar *xml_value;
	char *glib_value;

	xml_value = xmlNodeGetContent (node);
	glib_value = g_strdup (xml_value);
	xmlFree (xml_value);

	return glib_value;
}

static xmlNode *
lookup_node (xmlNode *parent_node,
	     const char *node_name)
{
	xmlNode *p;

	for (p = parent_node->childs; p != NULL; p = p->next) {
		if (strcmp ((const char *) p->name, node_name) == 0)
			return p;
	}

	return NULL;
}

static char *
get_value (xmlNode *parent_node,
	   const char *node_name)
{
	xmlNode *node;

	node = lookup_node (parent_node, node_name);
	if (node == NULL)
		return NULL;

	return get_value_for_node (node);
}

static xmlNode *
lookup_node_for_language (xmlNode *parent_node,
			  const char *node_name,
			  const char *language_id)
{
	xmlNode *p;

	for (p = parent_node->childs; p != NULL; p = p->next) {
		xmlChar *node_language_id;

		if (strcmp ((const char *) p->name, node_name) != 0)
			continue;

		node_language_id = xmlNodeGetLang (p);
		if (node_language_id == NULL)
			continue;

		if (strcmp (node_language_id, language_id) == 0) {
			xmlFree (node_language_id);
			return p;
		}
	}

	return NULL;
}

static char *
get_i18n_value (xmlNode *parent_node,
		const char *node_name,
		GSList *language_list)
{
	GSList *p;

	for (p = language_list; p != NULL; p = p->next) {
		xmlNode *node;
		const char *language_id;

		language_id = (const char *) p->data;
		node = lookup_node_for_language (parent_node, node_name, language_id);

		if (node != NULL) {
			xmlChar *xml_value;
			char *glib_value;

			xml_value = xmlNodeGetContent (node);
			glib_value = g_strdup (xml_value);
			xmlFree (xml_value);

			return glib_value;
		}
	}

	return get_value (parent_node, node_name);
}


static void
add_folder_type (EComponentInfo *info,
		 xmlNode *parent_node,
		 GSList *language_list)
{
	EComponentInfoFolderType *folder_type;
	char *user_creatable_string;

	folder_type = g_new (EComponentInfoFolderType, 1);

	folder_type->name           = get_value (parent_node, "name");
	folder_type->icon_file_name = get_value (parent_node, "icon_file_name");
	folder_type->display_name   = get_i18n_value (parent_node, "display_name", language_list);
	folder_type->description    = get_i18n_value (parent_node, "description", language_list);

	/* FIXME dnd types. */

	folder_type->accepted_dnd_types = NULL;
	folder_type->exported_dnd_types = NULL;

	user_creatable_string = get_value (parent_node, "user_creatable");
	if (user_creatable_string == NULL || atoi (user_creatable_string) == 0)
		folder_type->is_user_creatable = FALSE;
	else
		folder_type->is_user_creatable = TRUE;

	info->folder_types = g_slist_prepend (info->folder_types, folder_type);
}

static void
add_user_creatable_item_type (EComponentInfo *info,
			      xmlNode *parent_node,
			      GSList *language_list)
{
	EComponentInfoUserCreatableItemType *type;

	type = g_new (EComponentInfoUserCreatableItemType, 1);

	type->id               = get_value (parent_node, "id");
	type->description      = get_i18n_value (parent_node, "description", language_list);
	type->icon_file_name   = get_value (parent_node, "icon_file_name");
	type->menu_description = get_i18n_value (parent_node, "menu_description", language_list);
	type->menu_shortcut    = get_value (parent_node, "menu_shortcut");

	info->user_creatable_item_types = g_slist_prepend (info->user_creatable_item_types, type);
}

static void
add_uri_schema (EComponentInfo *info,
		xmlNode *parent_node)
{
	info->uri_schemas = g_slist_prepend (info->uri_schemas, get_value_for_node (parent_node));
}


EComponentInfo *
e_component_info_load (const char *file_name)
{
	EComponentInfo *new;
	xmlDoc *doc;
	xmlNode *root;
	xmlNode *p;
	GSList *language_list;

	g_return_val_if_fail (file_name != NULL, NULL);

	doc = xmlParseFile (file_name);
	if (doc == NULL)
		return NULL;

	root = xmlDocGetRootElement (doc);
	if (root == NULL || strcmp (root->name, "evolution_component") != 0) {
		xmlFreeDoc (doc);
		return NULL;
	}

	language_list = e_get_language_list ();

	new = g_new (EComponentInfo, 1);

	new->id                        = get_value (root, "id");
	new->description               = get_i18n_value (root, "description", language_list);
	new->icon_file_name            = get_value (root, "icon_file_name");

	new->folder_types              = NULL;
	new->uri_schemas               = NULL;
	new->user_creatable_item_types = NULL;

	for (p = root->childs; p != NULL; p = p->next) {
		if (strcmp ((char *) p->name, "folder_type") == 0) 
			add_folder_type (new, p, language_list);
		else if (strcmp ((char *) p->name, "user_creatable_item_type") == 0)
			add_user_creatable_item_type (new, p, language_list);
		else if (strcmp ((char *) p->name, "uri_schema") == 0)
			add_uri_schema (new, p);
	}

	xmlFreeDoc (doc);
	e_free_language_list (language_list);

	return new;
}

void
e_component_info_free (EComponentInfo *component_info)
{
	GSList *p;

	g_return_if_fail (component_info != NULL);

	g_free (component_info->id);
	g_free (component_info->description);
	g_free (component_info->icon_file_name);

	for (p = component_info->folder_types; p != NULL; p = p->next) {
		EComponentInfoFolderType *folder_type;
		GSList *q;

		folder_type = (EComponentInfoFolderType *) p->data;
		g_free (folder_type->name);
		g_free (folder_type->icon_file_name);
		g_free (folder_type->display_name);
		g_free (folder_type->description);

		for (q = folder_type->accepted_dnd_types; q != NULL; q = q->next)
			g_free ((char *) q->data);
		g_slist_free (folder_type->accepted_dnd_types);

		for (q = folder_type->exported_dnd_types; q != NULL; q = q->next)
			g_free ((char *) q->data);
		g_slist_free (folder_type->exported_dnd_types);

		g_free (folder_type);
	}
	g_free (component_info->folder_types);

	for (p = component_info->uri_schemas; p != NULL; p = p->next)
		g_free ((char *) p->data);
	g_slist_free (component_info->uri_schemas);

	for (p = component_info->user_creatable_item_types; p != NULL; p = p->next) {
		EComponentInfoUserCreatableItemType *type;

		type = (EComponentInfoUserCreatableItemType *) p->data;

		g_free (type->id);
		g_free (type->description);
		g_free (type->icon_file_name);
		g_free (type->menu_description);
		g_free (type->menu_shortcut);
	}
	g_slist_free (component_info->user_creatable_item_types);

	g_free (component_info);
}
