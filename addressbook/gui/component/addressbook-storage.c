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
   <addressbooks>
     <contactserver>
           <name>LDAP Server</name>
	   <description>This is my company address book.</description>
	   <host>ldap.server.com</host>
	   <port>389</port>
	   <rootdn></rootdn>
	   <authmethod>simple</authmethod>
	   <binddn>cn=Chris Toshok,dc=helixcode,dc=com</binddn>
	   <rememberpass/>
     </contactserver>
     <contactfile>
           <name>On Disk Contacts</name>
	   <description>This is one of my private contact dbs.</description>
           <path>/home/toshok/contacts/work-contacts.db</path>
     </contactfile>
   </addressbooks>

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

#include "addressbook-storage.h"

#include <gal/util/e-util.h>
#include <gal/util/e-xml-utils.h>

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define ADDRESSBOOK_SOURCES_XML "addressbook-sources.xml"

static gboolean load_source_data (EvolutionStorage *storage, const char *file_path);
static gboolean save_source_data (const char *file_path);

GList *sources;
EvolutionStorage *storage;
static char *storage_path;

void
addressbook_storage_setup (EvolutionShellComponent *shell_component,
			   const char *evolution_homedir)
{
	EvolutionShellClient *shell_client;
	GNOME_Evolution_Shell corba_shell;

	shell_client = evolution_shell_component_get_owner (shell_component);
	if (shell_client == CORBA_OBJECT_NIL) {
		g_warning ("We have no shell!?");
		return;
	}

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));

	storage = evolution_storage_new (_("Other Contacts"), NULL, NULL);
	if (evolution_storage_register_on_shell (storage, corba_shell) != EVOLUTION_STORAGE_OK) {
		g_warning ("Cannot register storage");
		return;
	}

	sources = NULL;

	gtk_object_set_data (GTK_OBJECT (shell_component), "e-storage", storage);

	if (storage_path)
		g_free (storage_path);
	storage_path = g_strdup_printf ("%s/" ADDRESSBOOK_SOURCES_XML, evolution_homedir);
	load_source_data (storage, storage_path);
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

static char *
ldap_unparse_auth (AddressbookLDAPAuthType auth_type)
{
	switch (auth_type) {
	case ADDRESSBOOK_LDAP_AUTH_NONE:
		return "none";
	case ADDRESSBOOK_LDAP_AUTH_SIMPLE:
		return "simple";
	case ADDRESSBOOK_LDAP_AUTH_SASL:
		return "sasl";
	default:
		g_assert(0);
		return "none";
	}
}

static AddressbookLDAPAuthType
ldap_parse_auth (const char *auth)
{
	if (!auth)
		return ADDRESSBOOK_LDAP_AUTH_NONE;

	if (!strcmp (auth, "simple"))
		return ADDRESSBOOK_LDAP_AUTH_SIMPLE;
	else if (!strcmp (auth, "sasl"))
		return ADDRESSBOOK_LDAP_AUTH_SASL;
	else
		return ADDRESSBOOK_LDAP_AUTH_NONE;
}

static char *
ldap_unparse_scope (AddressbookLDAPScopeType scope_type)
{
	switch (scope_type) {
	case ADDRESSBOOK_LDAP_SCOPE_BASE:
		return "base";
	case ADDRESSBOOK_LDAP_SCOPE_ONELEVEL:
		return "one";
	case ADDRESSBOOK_LDAP_SCOPE_SUBTREE:
		return "sub";
	default:
		g_assert(0);
		return "";
	}
}

static AddressbookLDAPScopeType
ldap_parse_scope (const char *scope)
{
	if (!scope)
		return ADDRESSBOOK_LDAP_SCOPE_SUBTREE; /* XXX good default? */

	if (!strcmp (scope, "base"))
		return ADDRESSBOOK_LDAP_SCOPE_BASE;
	else if (!strcmp (scope, "one"))
		return ADDRESSBOOK_LDAP_SCOPE_ONELEVEL;
	else
		return ADDRESSBOOK_LDAP_SCOPE_SUBTREE;
}

void
addressbook_storage_init_source_uri (AddressbookSource *source)
{
	if (source->uri)
		g_free (source->uri);

	if (source->type == ADDRESSBOOK_SOURCE_LDAP)
		source->uri = g_strdup_printf  ("ldap://%s:%s/%s??%s",
						source->ldap.host, source->ldap.port,
						source->ldap.rootdn, ldap_unparse_scope(source->ldap.scope));
	else
		source->uri = g_strdup_printf ("file://%s",
					       source->file.path);
}

static gboolean
load_source_data (EvolutionStorage *storage,
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
				g_error ("Failed to rename %s: %s\n",
					 ADDRESSBOOK_SOURCES_XML,
					 strerror(errno));
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
	if (root == NULL || strcmp (root->name, "addressbooks") != 0) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	for (child = root->childs; child; child = child->next) {
		char *path;
		AddressbookSource *source;

		source = g_new0 (AddressbookSource, 1);

		if (!strcmp (child->name, "contactserver")) {
			source->type        = ADDRESSBOOK_SOURCE_LDAP;
			source->ldap.port   = get_string_value (child, "port");
			source->ldap.host   = get_string_value (child, "host");
			source->ldap.rootdn = get_string_value (child, "rootdn");
			source->ldap.scope  = ldap_parse_scope (get_string_value (child, "scope"));
			source->ldap.auth   = ldap_parse_auth (get_string_value (child, "authmethod"));
			source->ldap.binddn = get_string_value (child, "binddn");
		}
		else if (!strcmp (child->name, "contactfile")) {
			source->type        = ADDRESSBOOK_SOURCE_FILE;
			source->file.path = get_string_value (child, "path");
		}
		else {
			g_warning ("unknown node '%s' in %s", child->name, file_path);
			g_free (source);
			continue;
		}

		addressbook_storage_init_source_uri (source);

		source->name = get_string_value (child, "name");
		source->description = get_string_value (child, "description");

		path = g_strdup_printf ("/%s", source->name);
		evolution_storage_new_folder (storage, path, source->name,
					      "contacts", source->uri,
					      source->description, FALSE);

		sources = g_list_append (sources, source);

		g_free (path);
	}

	xmlFreeDoc (doc);

	return TRUE;
}

static void
ldap_source_foreach(AddressbookSource *source, xmlNode *root)
{
	xmlNode *source_root = xmlNewNode (NULL,
					   (xmlChar *) "contactserver");

	xmlAddChild (root, source_root);

	xmlNewChild (source_root, NULL, (xmlChar *) "name",
		     (xmlChar *) source->name);
	xmlNewChild (source_root, NULL, (xmlChar *) "description",
		     (xmlChar *) source->description);

	xmlNewChild (source_root, NULL, (xmlChar *) "port",
		     (xmlChar *) source->ldap.port);
	xmlNewChild (source_root, NULL, (xmlChar *) "host",
		     (xmlChar *) source->ldap.host);
	xmlNewChild (source_root, NULL, (xmlChar *) "rootdn",
		     (xmlChar *) source->ldap.rootdn);
	xmlNewChild (source_root, NULL, (xmlChar *) "scope",
		     (xmlChar *) ldap_unparse_scope(source->ldap.scope));
	xmlNewChild (source_root, NULL, (xmlChar *) "authmethod",
		     (xmlChar *) ldap_unparse_auth(source->ldap.auth));
	if (source->ldap.auth == ADDRESSBOOK_LDAP_AUTH_SIMPLE) {
		xmlNewChild (source_root, NULL, (xmlChar *) "binddn",
			     (xmlChar *) source->ldap.binddn);
		if (source->ldap.remember_passwd)
			xmlNewChild (source_root, NULL, (xmlChar *) "rememberpass",
				     NULL);
	}
}

static void
file_source_foreach (AddressbookSource *source, xmlNode *root)
{
	xmlNode *source_root = xmlNewNode (NULL,
					   (xmlChar *) "contactfile");

	xmlAddChild (root, source_root);

	xmlNewChild (source_root, NULL, (xmlChar *) "name",
		     (xmlChar *) source->name);
	xmlNewChild (source_root, NULL, (xmlChar *) "description",
		     (xmlChar *) source->description);

	xmlNewChild (source_root, NULL, (xmlChar *) "path",
		     (xmlChar *) source->file.path);
}

static void
source_foreach(gpointer value, gpointer user_data)
{
	AddressbookSource *source = value;
	xmlNode *root = user_data;

	if (source->type == ADDRESSBOOK_SOURCE_LDAP)
		ldap_source_foreach(source, root);
	else
		file_source_foreach(source, root);
}

static gboolean
save_source_data (const char *file_path)
{
	xmlDoc *doc;
	xmlNode *root;
	int fd, rv;
	xmlChar *buf;
	int buf_size;
	char *new_path = g_strdup_printf ("%s.new", file_path);

	doc = xmlNewDoc ((xmlChar *) "1.0");
	root = xmlNewDocNode (doc, NULL, (xmlChar *) "addressbooks", NULL);
	xmlDocSetRootElement (doc, root);

	g_list_foreach (sources, source_foreach, root);

	fd = open (new_path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	fchmod (fd, 0600);

	xmlDocDumpMemory (doc, &buf, &buf_size);

	if (buf == NULL) {
		g_error ("Failed to write %s: xmlBufferCreate() == NULL", ADDRESSBOOK_SOURCES_XML);
		return FALSE;
	}

	rv = write (fd, buf, buf_size);
	xmlFree (buf);
	close (fd);

	if (0 > rv) {
		g_error ("Failed to write new %s: %s\n", ADDRESSBOOK_SOURCES_XML, strerror(errno));
		unlink (new_path);
		return FALSE;
	}
	else {
		if (0 > rename (new_path, file_path)) {
			g_error ("Failed to rename %s: %s\n", ADDRESSBOOK_SOURCES_XML, strerror(errno));
			unlink (new_path);
			return FALSE;
		}
		return TRUE;
	}
}

void
addressbook_storage_add_source (AddressbookSource *source)
{
	char *path;

	sources = g_list_append (sources, source);

	/* and then to the ui */
	path = g_strdup_printf ("/%s", source->name);
	evolution_storage_new_folder (storage, path, source->name, "contacts",
				      source->uri, source->description, FALSE);

	g_free (path);

	save_source_data (storage_path);
}

void
addressbook_storage_remove_source (const char *name)
{
	char *path;
	AddressbookSource *source = NULL;
	GList *l;

	/* remove it from our hashtable */
	for (l = sources; l; l = l->next) {
		AddressbookSource *s = l->data;
		if (!strcmp (s->name, name)) {
			source = s;
			break;
		}
	}

	if (!source)
		return;

	sources = g_list_remove_link (sources, l);
	g_list_free_1 (l);

	addressbook_source_free (source);

	/* and then from the ui */
	path = g_strdup_printf ("/%s", name);
	evolution_storage_removed_folder (storage, path);

	g_free (path);

	save_source_data (storage_path);
}

GList *
addressbook_storage_get_sources ()
{
	return sources;
}

void
addressbook_source_free (AddressbookSource *source)
{
	g_free (source->name);
	g_free (source->description);
	g_free (source->uri);
	if (source->type == ADDRESSBOOK_SOURCE_LDAP) {
		g_free (source->ldap.host);
		g_free (source->ldap.port);
		g_free (source->ldap.rootdn);
	}
	else {
		g_free (source->file.path);
	}

	g_free (source);
}

static void
addressbook_source_foreach (AddressbookSource *source, gpointer data)
{
	char *path = g_strdup_printf ("/%s", source->name);

	evolution_storage_removed_folder (storage, path);

	g_free (path);

	addressbook_source_free (source);
}

void
addressbook_storage_clear_sources ()
{
	g_list_foreach (sources, (GFunc)addressbook_source_foreach, NULL);
	g_list_free (sources);
	sources = NULL;
}

AddressbookSource *
addressbook_source_copy (const AddressbookSource *source)
{
	AddressbookSource *copy;

	copy = g_new0 (AddressbookSource, 1);
	copy->name = g_strdup (source->name);
	copy->description = g_strdup (source->description);
	copy->type = source->type;
	copy->uri = g_strdup (source->uri);

	if (copy->type == ADDRESSBOOK_SOURCE_LDAP) {
		copy->ldap.host = g_strdup (source->ldap.host);
		copy->ldap.port = g_strdup (source->ldap.port);
		copy->ldap.rootdn = g_strdup (source->ldap.rootdn);
		copy->ldap.scope = source->ldap.scope;
		copy->ldap.auth = source->ldap.auth;
		copy->ldap.binddn = source->ldap.binddn;
		copy->ldap.remember_passwd = source->ldap.remember_passwd;
	}
	else {
		copy->file.path = g_strdup (source->file.path);
	}
	return copy;
}
