/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-ldap-storage.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Author: Chris Toshok
 */

/* The ldap server file goes like this:

   <?xml version="1.0"?>
   <contactservers>
     <contactserver>
           <name>LDAP Server</name>
	   <description>This is my company address book.</description>
	   <uri>ldap://ldap.somewhere.net/</uri>
     </contactserver>
   </contactservers>

   FIXME: Do we want to use a namespace for this?
   FIXME: Do we want to have an internationalized description?
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo.h>

#include "evolution-shell-component.h"
#include "evolution-storage.h"

#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include "e-ldap-storage.h"
#include "e-ldap-server-dialog.h"

#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"

#define LDAPSERVER_XML "ldapservers.xml"

static gboolean load_ldap_data (EvolutionStorage *storage, const char *file_path);
static gboolean save_ldap_data (const char *file_path);

GHashTable *servers;

void
setup_ldap_storage (EvolutionShellComponent *shell_component)
{
	EvolutionShellClient *shell_client;
	Evolution_Shell corba_shell;
	EvolutionStorage *storage;
	char *path;

	shell_client = evolution_shell_component_get_owner (shell_component);
	if (shell_client == CORBA_OBJECT_NIL) {
		g_warning ("We have no shell!?");
		return;
	}

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));

	storage = evolution_storage_new (_("External Directories"));
	if (evolution_storage_register_on_shell (storage, corba_shell) != EVOLUTION_STORAGE_OK) {
		g_warning ("Cannot register storage");
		return;
	}

	/* save the storage for later */
	gtk_object_set_data (GTK_OBJECT (shell_component), "e-storage", storage);

	path = g_strdup_printf ("%s/evolution/" LDAPSERVER_XML, g_get_home_dir());
	load_ldap_data (storage, path);
	g_free (path);
}

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

	xml_string = xmlNodeListGetString (node->doc, p, 1);
	retval = g_strdup ((char *) xml_string);
	xmlFree (xml_string);

	return retval;
}

static gboolean
load_ldap_data (EvolutionStorage *storage,
		const char *file_path)
{
	xmlDoc *doc;
	xmlNode *root;
	xmlNode *child;

	doc = xmlParseFile (file_path);
	if (doc == NULL) {
		return FALSE;
	}

	root = xmlDocGetRootElement (doc);
	if (root == NULL || strcmp (root->name, "contactservers") != 0) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	for (child = root->childs; child; child = child->next) {
		char *path;
		char *name;
		char *uri;
		char *description;

		if (strcmp (child->name, "contactserver")) {
			g_warning ("unknown node '%s' in %s", child->name, file_path);
			continue;
		}

		name = get_string_value (child, "name");
		uri = get_string_value (child, "uri");
		description = get_string_value (child, "description");

		path = g_strdup_printf ("/%s", name);
		evolution_storage_new_folder (storage, path, "contacts", uri, description);

		g_free (path);
		g_free (name);
		g_free (uri);
		g_free (description);
	}

	xmlFreeDoc (doc);

	return TRUE;
}

static void
ldap_server_foreach(gpointer key, gpointer value, gpointer user_data)
{
	ELDAPServer *server = (ELDAPServer*)value;
	xmlNode *root = (xmlNode*)user_data;

	xmlNewChild (root, NULL, (xmlChar *) "name",
		     (xmlChar *) server->name);

	xmlNewChild (root, NULL, (xmlChar *) "uri",
		     (xmlChar *) server->uri);

	if (server->description)
		xmlNewChild (root, NULL, (xmlChar *) "description",
			     (xmlChar *) server->description);
}

static gboolean
save_ldap_data (const char *file_path)
{
	xmlDoc *doc;
	xmlNode *root;

	doc = xmlNewDoc ((xmlChar *) "1.0");
	root = xmlNewDocNode (doc, NULL, (xmlChar *) "contactservers", NULL);
	xmlDocSetRootElement (doc, root);

	g_hash_table_foreach (servers, ldap_server_foreach, root);

	if (xmlSaveFile (file_path, doc) < 0) {
		unlink (file_path);
		xmlFreeDoc (doc);
		return FALSE;
	}

	xmlFreeDoc (doc);
	return TRUE;
}
