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
	   <host>ldap.server.com</host>
	   <port>389</port>
	   <rootdn></rootdn>
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

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define LDAPSERVER_XML "ldapservers.xml"

static gboolean load_ldap_data (EvolutionStorage *storage, const char *file_path);
static gboolean save_ldap_data (const char *file_path);

GHashTable *servers;
EvolutionStorage *storage;

void
setup_ldap_storage (EvolutionShellComponent *shell_component)
{
	EvolutionShellClient *shell_client;
	Evolution_Shell corba_shell;
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
	servers = g_hash_table_new (g_str_hash, g_str_equal);

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
	if (p == NULL) /* there's no text between the tags, return the empty string */
		return g_strdup("");

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

 tryagain:
	doc = xmlParseFile (file_path);
	if (doc == NULL) {
		/* check to see if a ldapserver.xml.new file is
                   there.  if it is, rename it and run with it */
		char *new_path = g_strdup_printf ("%s.new", file_path);
		struct stat sb;

		if (stat (new_path, &sb) == 0) {
			int rv;

			rv = rename (new_path, file_path);
			g_free (new_path);

			if (0 > rv) {
				g_error ("Failed to rename ldapserver.xml: %s\n", strerror(errno));
				return FALSE;
			}
			else {
				goto tryagain;
			}
		}

		g_free (new_path);
		return TRUE;
	}

	root = xmlDocGetRootElement (doc);
	if (root == NULL || strcmp (root->name, "contactservers") != 0) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	for (child = root->childs; child; child = child->next) {
		char *path;
		ELDAPServer *server;

		if (strcmp (child->name, "contactserver")) {
			g_warning ("unknown node '%s' in %s", child->name, file_path);
			continue;
		}

		server = g_new (ELDAPServer, 1);

		server->name = get_string_value (child, "name");
		server->description = get_string_value (child, "description");
		server->port = get_string_value (child, "port");
		server->host = get_string_value (child, "host");
		server->rootdn = get_string_value (child, "rootdn");
		server->scope = get_string_value (child, "scope");
		server->uri = g_strdup_printf ("ldap://%s:%s/%s??%s", server->host, server->port, server->rootdn, server->scope);

		path = g_strdup_printf ("/%s", server->name);
		evolution_storage_new_folder (storage, path, "contacts", server->uri, server->description);

		g_hash_table_insert (servers, server->name, server);

		g_free (path);
	}

	xmlFreeDoc (doc);

	return TRUE;
}

static void
ldap_server_foreach(gpointer key, gpointer value, gpointer user_data)
{
	ELDAPServer *server = (ELDAPServer*)value;
	xmlNode *root = (xmlNode*)user_data;
	xmlNode *server_root = xmlNewNode (NULL,
					   (xmlChar *) "contactserver");

	xmlAddChild (root, server_root);

	xmlNewChild (server_root, NULL, (xmlChar *) "name",
		     (xmlChar *) server->name);
	xmlNewChild (server_root, NULL, (xmlChar *) "description",
		     (xmlChar *) server->description);

	xmlNewChild (server_root, NULL, (xmlChar *) "port",
		     (xmlChar *) server->port);
	xmlNewChild (server_root, NULL, (xmlChar *) "host",
		     (xmlChar *) server->host);
	xmlNewChild (server_root, NULL, (xmlChar *) "rootdn",
		     (xmlChar *) server->rootdn);
	xmlNewChild (server_root, NULL, (xmlChar *) "scope",
		     (xmlChar *) server->scope);
}

static gboolean
save_ldap_data (const char *file_path)
{
	xmlDoc *doc;
	xmlNode *root;
	int fd, rv;
	xmlChar *buf;
	int buf_size;
	char *new_path = g_strdup_printf ("%s.new", file_path);

	doc = xmlNewDoc ((xmlChar *) "1.0");
	root = xmlNewDocNode (doc, NULL, (xmlChar *) "contactservers", NULL);
	xmlDocSetRootElement (doc, root);

	g_hash_table_foreach (servers, ldap_server_foreach, root);

	fd = open (new_path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	fchmod (fd, 0600);

	xmlDocDumpMemory (doc, &buf, &buf_size);

	if (buf == NULL) {
		g_error ("Failed to write ldapserver.xml: xmlBufferCreate() == NULL");
		return FALSE;
	}

	rv = write (fd, buf, buf_size);
	xmlFree (buf);
	close (fd);

	if (0 > rv) {
		g_error ("Failed to write new ldapserver.xml: %s\n", strerror(errno));
		unlink (new_path);
		return FALSE;
	}
	else {
		if (0 > rename (new_path, file_path)) {
			g_error ("Failed to rename ldapserver.xml: %s\n", strerror(errno));
			unlink (new_path);
			return FALSE;
		}
		return TRUE;
	}
}

void
e_ldap_storage_add_server (ELDAPServer *server)
{
	char *path;
	/* add it to our hashtable */
	g_hash_table_insert (servers, server->name, server);

	/* and then to the ui */
	path = g_strdup_printf ("/%s", server->name);
	evolution_storage_new_folder (storage, path, "contacts", server->uri, server->description);

	g_free (path);

	path = g_strdup_printf ("%s/evolution/" LDAPSERVER_XML, g_get_home_dir());
	save_ldap_data (path);
	g_free (path);
}

void
e_ldap_storage_remove_server (char *name)
{
	char *path;
	ELDAPServer *server;

	/* remove it from our hashtable */
	server = (ELDAPServer*)g_hash_table_lookup (servers, name);
	g_hash_table_remove (servers, name);

	g_free (server->name);
	g_free (server->description);
	g_free (server->host);
	g_free (server->port);
	g_free (server->rootdn);
	g_free (server->scope);
	g_free (server->uri);

	g_free (server);

	/* and then from the ui */
	path = g_strdup_printf ("/%s", name);
	evolution_storage_removed_folder (storage, path);

	g_free (path);

	path = g_strdup_printf ("%s/evolution/" LDAPSERVER_XML, g_get_home_dir());
	save_ldap_data (path);
	g_free (path);
}
