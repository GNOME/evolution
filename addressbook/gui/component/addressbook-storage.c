/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-ldap-storage.c
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
 * Author: Chris Toshok
 */

/* The addressbook-sources.xml file goes like this:

   <?xml version="1.0"?>
   <addressbooks>
     <contactserver>
           <name>LDAP Server</name>
	   <host>ldap.server.com</host>
	   <port>389</port>
	   <rootdn></rootdn>
	   <authmethod>simple</authmethod>
	   <emailaddr>toshok@blubag.com</emailaddr>
	   <limit>100</limit>
	   <rememberpass/>
     </contactserver>
   </addressbooks>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "addressbook-storage.h"

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <bonobo/bonobo-object.h>

#include <gal/util/e-util.h>
#include <gal/util/e-xml-utils.h>
#include <libgnome/gnome-i18n.h>

#include "evolution-shell-component.h"

#include "addressbook-config.h"

#define ADDRESSBOOK_SOURCES_XML "addressbook-sources.xml"

#ifdef HAVE_LDAP
static gboolean load_source_data (const char *file_path);
#endif

static gboolean save_source_data (const char *file_path);
static void deregister_storage (void);

static GList *sources;
static EvolutionStorage *storage;
static char *storage_path;
static GNOME_Evolution_Shell corba_shell;

void
addressbook_storage_setup (EvolutionShellComponent *shell_component,
			   const char *evolution_homedir)
{
	EvolutionShellClient *shell_client;

	shell_client = evolution_shell_component_get_owner (shell_component);
	if (shell_client == CORBA_OBJECT_NIL) {
		g_warning ("We have no shell!?");
		return;
	}

	corba_shell = evolution_shell_client_corba_objref (shell_client);

	sources = NULL;

	if (storage_path)
		g_free (storage_path);
	storage_path = g_build_filename (evolution_homedir, ADDRESSBOOK_SOURCES_XML, NULL);
#ifdef HAVE_LDAP
	if (!load_source_data (storage_path))
		deregister_storage ();
#endif 
}

void
addressbook_storage_cleanup (void)
{
	if (storage != NULL) {
		bonobo_object_unref (storage);
		storage = NULL;
	}
}

#ifdef HAVE_LDAP
static void
notify_listener (const Bonobo_Listener listener, 
		 GNOME_Evolution_Storage_Result corba_result)
{
	CORBA_any any;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	any._type = TC_GNOME_Evolution_Storage_Result;
	any._value = &corba_result;

	Bonobo_Listener_event (listener, "result", &any, &ev);

	CORBA_exception_free (&ev);
}

static void
remove_ldap_folder (EvolutionStorage *storage, const Bonobo_Listener listener,
		    const CORBA_char *path, const CORBA_char *physical_uri,
		    gpointer data)
{
	
	addressbook_storage_remove_source (path + 1);
	addressbook_storage_write_sources();

	notify_listener (listener, GNOME_Evolution_Storage_OK);
}

static void
create_ldap_folder (EvolutionStorage *storage, const Bonobo_Listener listener,
		    const CORBA_char *path, const CORBA_char *type,
		    const CORBA_char *description, const CORBA_char *parent_physical_uri,
		    gpointer data)
{
	if (strcmp (type, "contacts")) {
		notify_listener (listener, GNOME_Evolution_Storage_UNSUPPORTED_TYPE);
		return;
	}

	if (strcmp (parent_physical_uri, "")) {/* ldap servers can't have subfolders */
		notify_listener (listener, GNOME_Evolution_Storage_INVALID_URI);
		return;
	}
	addressbook_config_create_new_source (path + 1, NULL);

	notify_listener (listener, GNOME_Evolution_Storage_OK);
}
#endif


EvolutionStorage *
addressbook_get_other_contact_storage (void) 
{
#ifdef HAVE_LDAP
	EvolutionStorageResult result;

	if (storage == NULL) {
		storage = evolution_storage_new (_("Other Contacts"), FALSE);
		g_signal_connect (storage,
				  "remove_folder",
				  G_CALLBACK (remove_ldap_folder), NULL);
		g_signal_connect (storage,
				  "create_folder",
				  G_CALLBACK (create_ldap_folder), NULL);
		result = evolution_storage_register_on_shell (storage, corba_shell);
		switch (result) {
		case EVOLUTION_STORAGE_OK:
			break;
		case EVOLUTION_STORAGE_ERROR_GENERIC : 
			g_warning("register_storage: generic error");
			break;
		case EVOLUTION_STORAGE_ERROR_CORBA : 
			g_warning("register_storage: corba error");
			break;
		case EVOLUTION_STORAGE_ERROR_ALREADYREGISTERED :
			g_warning("register_storage: already registered error");
			break;
		case EVOLUTION_STORAGE_ERROR_EXISTS :
			g_warning("register_storage: already exists error");
			break;
		default:
			g_warning("register_storage: other error");
			break;
		}
	}
#endif

	return storage;
}

static void 
deregister_storage (void)
{
	if (evolution_storage_deregister_on_shell (storage, corba_shell) != 
	    EVOLUTION_STORAGE_OK) {
		g_warning("couldn't deregister storage");
	}

	storage = NULL;
}

#ifdef HAVE_LDAP
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

static int
get_integer_value (xmlNode *node,
		   const char *name,
		   int defval)
{
	xmlNode *p;
	xmlChar *xml_string;
	int retval;

	p = e_xml_get_child_by_name (node, (xmlChar *) name);
	if (p == NULL)
		return defval;

	p = e_xml_get_child_by_name (p, (xmlChar *) "text");
	if (p == NULL) /* there's no text between the tags, return the default */
		return defval;

	xml_string = xmlNodeListGetString (node->doc, p, 1);
	retval = atoi (xml_string);
	xmlFree (xml_string);

	return retval;
}
#endif

static char *
ldap_unparse_auth (AddressbookLDAPAuthType auth_type)
{
	switch (auth_type) {
	case ADDRESSBOOK_LDAP_AUTH_NONE:
		return "none";
	case ADDRESSBOOK_LDAP_AUTH_SIMPLE_EMAIL:
		return "ldap/simple-email";
	case ADDRESSBOOK_LDAP_AUTH_SIMPLE_BINDDN:
		return "ldap/simple-binddn";
	default:
		g_assert(0);
		return "none";
	}
}

#ifdef HAVE_LDAP
static AddressbookLDAPAuthType
ldap_parse_auth (const char *auth)
{
	if (!auth)
		return ADDRESSBOOK_LDAP_AUTH_NONE;

	if (!strcmp (auth, "ldap/simple-email") || !strcmp (auth, "simple"))
		return ADDRESSBOOK_LDAP_AUTH_SIMPLE_EMAIL;
	else if (!strcmp (auth, "ldap/simple-binddn"))
		return ADDRESSBOOK_LDAP_AUTH_SIMPLE_BINDDN;
	else
		return ADDRESSBOOK_LDAP_AUTH_NONE;
}
#endif

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

#ifdef HAVE_LDAP
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
#endif

static char *
ldap_unparse_ssl (AddressbookLDAPSSLType ssl_type)
{
	switch (ssl_type) {
	case ADDRESSBOOK_LDAP_SSL_NEVER:
		return "never";
	case ADDRESSBOOK_LDAP_SSL_WHENEVER_POSSIBLE:
		return "whenever_possible";
	case ADDRESSBOOK_LDAP_SSL_ALWAYS:
		return "always";
	default:
		g_assert(0);
		return "";
	}
}

#ifdef HAVE_LDAP
static AddressbookLDAPSSLType
ldap_parse_ssl (const char *ssl)
{
	if (!ssl)
		return ADDRESSBOOK_LDAP_SSL_WHENEVER_POSSIBLE; /* XXX good default? */

	if (!strcmp (ssl, "always"))
		return ADDRESSBOOK_LDAP_SSL_ALWAYS;
	else if (!strcmp (ssl, "never"))
		return ADDRESSBOOK_LDAP_SSL_NEVER;
	else
		return ADDRESSBOOK_LDAP_SSL_WHENEVER_POSSIBLE;
}
#endif

const char*
addressbook_storage_auth_type_to_string (AddressbookLDAPAuthType auth_type)
{
	return ldap_unparse_auth (auth_type);
}

void
addressbook_storage_init_source_uri (AddressbookSource *source)
{
	GString *str;

	if (source->uri)
		g_free (source->uri);

	str = g_string_new ("ldap://");

	g_string_append_printf (str, "%s:%s/%s?"/*trigraph prevention*/"?%s",
				source->host,
				source->port,
				source->rootdn,
				ldap_unparse_scope (source->scope));

	g_string_append_printf (str, ";limit=%d", source->limit);

	g_string_append_printf (str, ";ssl=%s", ldap_unparse_ssl (source->ssl));

#if 0
	g_string_append_printf (str, ";timeout=%d", source->timeout);
#endif

	source->uri = str->str;

	g_string_free (str, FALSE);
}

#ifdef HAVE_LDAP
static gboolean
load_source_data (const char *file_path)
{
	xmlDoc *doc;
	xmlNode *root;
	xmlNode *child;

	addressbook_get_other_contact_storage();

 tryagain:
	doc = xmlParseFile (file_path);
	if (doc == NULL) {
		/* Check to see if a addressbook-sources.xml.new file
                   exists.  If it does, rename it and try loading it */
		char *new_path = g_strdup_printf ("%s.new", file_path);
		struct stat sb;

		if (stat (new_path, &sb) == 0) {
			int rv;

			rv = rename (new_path, file_path);
			g_free (new_path);

			if (rv < 0) {
				g_error ("Failed to rename %s: %s\n",
					 file_path,
					 strerror(errno));
				return FALSE;
			} else
				goto tryagain;
		}

		g_free (new_path);
		return FALSE;
	}

	root = xmlDocGetRootElement (doc);
	if (root == NULL || strcmp (root->name, "addressbooks") != 0) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	for (child = root->children; child; child = child->next) {
		char *path, *value;
		AddressbookSource *source;

		source = g_new0 (AddressbookSource, 1);

		if (!strcmp (child->name, "contactserver")) {
			source->port   = get_string_value (child, "port");
			source->host   = get_string_value (child, "host");
			source->rootdn = get_string_value (child, "rootdn");
			value = get_string_value (child, "scope");
			source->scope  = ldap_parse_scope (value);
			g_free (value);
			value = get_string_value (child, "authmethod");
			source->auth   = ldap_parse_auth (value);
			g_free (value);
			value = get_string_value (child, "ssl");
			source->ssl    = ldap_parse_ssl (value);
			g_free (value);
			source->email_addr = get_string_value (child, "emailaddr");
			source->binddn = get_string_value (child, "binddn");
			source->limit  = get_integer_value (child, "limit", 100);
		}
		else if (!strcmp (child->name, "text")) {
			if (child->content) {
				int i;
				for (i = 0; i < strlen (child->content); i++) {
					if (!isspace (child->content[i])) {
						g_warning ("illegal text in contactserver list.");
						break;
					}
				}
			}
			g_free (source);
			continue;
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
					      "contacts/ldap", source->uri,
					      source->description, NULL, 0, FALSE, 0);

		sources = g_list_append (sources, source);

		g_free (path);
	}

	if (g_list_length (sources) == 0)
		deregister_storage();

	xmlFreeDoc (doc);
	return TRUE;
}
#endif

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
		     (xmlChar *) source->port);
	xmlNewChild (source_root, NULL, (xmlChar *) "host",
		     (xmlChar *) source->host);
	xmlNewChild (source_root, NULL, (xmlChar *) "rootdn",
		     (xmlChar *) source->rootdn);
	xmlNewChild (source_root, NULL, (xmlChar *) "scope",
		     (xmlChar *) ldap_unparse_scope(source->scope));
	xmlNewChild (source_root, NULL, (xmlChar *) "authmethod",
		     (xmlChar *) ldap_unparse_auth(source->auth));
	xmlNewChild (source_root, NULL, (xmlChar *) "ssl",
		     (xmlChar *) ldap_unparse_ssl(source->ssl));

	if (source->limit != 100) {
		char *string;
		string = g_strdup_printf ("%d", source->limit);
		xmlNewChild (source_root, NULL, (xmlChar *) "limit",
			     (xmlChar *) string);
		g_free (string);
	}

	if (source->auth != ADDRESSBOOK_LDAP_AUTH_NONE) {
		if (source->auth == ADDRESSBOOK_LDAP_AUTH_SIMPLE_BINDDN)
			xmlNewChild (source_root, NULL, (xmlChar *) "binddn",
				     (xmlChar *) source->binddn);
		else
			xmlNewChild (source_root, NULL, (xmlChar *) "emailaddr",
				     (xmlChar *) source->email_addr);

		if (source->remember_passwd)
			xmlNewChild (source_root, NULL, (xmlChar *) "rememberpass",
				     NULL);
	}
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

	g_list_foreach (sources, (GFunc)ldap_source_foreach, root);

	fd = open (new_path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	fchmod (fd, 0600);

	xmlDocDumpMemory (doc, &buf, &buf_size);

	if (buf == NULL) {
		g_error ("Failed to write %s: xmlBufferCreate() == NULL", file_path);
		return FALSE;
	}

	rv = write (fd, buf, buf_size);
	xmlFree (buf);
	close (fd);

	if (0 > rv) {
		g_error ("Failed to write new %s: %s\n", file_path, strerror(errno));
		unlink (new_path);
		return FALSE;
	}
	else {
		if (0 > rename (new_path, file_path)) {
			g_error ("Failed to rename %s: %s\n", file_path, strerror(errno));
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

	/* And then to the ui */
	addressbook_get_other_contact_storage();
	path = g_strdup_printf ("/%s", source->name);
	evolution_storage_new_folder (storage, path, source->name, "contacts/ldap",
				      source->uri, source->description, NULL, 0, FALSE, 0);

	g_free (path);
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

	if (g_list_length (sources) == 0) 
		deregister_storage ();

	g_free (path);
}

GList *
addressbook_storage_get_sources ()
{
	return sources;
}

AddressbookSource *
addressbook_storage_get_source_by_uri (const char *uri)
{
	GList *l;

	for (l = sources; l ; l = l->next) {
		AddressbookSource *source = l->data;
		if (!strcmp (uri, source->uri))
			return source;
	}

	return NULL;
}

void
addressbook_source_free (AddressbookSource *source)
{
	g_free (source->name);
	g_free (source->description);
	g_free (source->uri);
	g_free (source->host);
	g_free (source->port);
	g_free (source->rootdn);
	g_free (source->email_addr);
	g_free (source->binddn);

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
addressbook_storage_clear_sources (void)
{
	g_list_foreach (sources, (GFunc)addressbook_source_foreach, NULL);
	g_list_free (sources);
	deregister_storage ();
	sources = NULL;
}

void
addressbook_storage_write_sources (void)
{
	save_source_data (storage_path);
}

AddressbookSource *
addressbook_source_copy (const AddressbookSource *source)
{
	AddressbookSource *copy;

	copy = g_new0 (AddressbookSource, 1);
	copy->name = g_strdup (source->name);
	copy->description = g_strdup (source->description);
	copy->uri = g_strdup (source->uri);

	copy->host = g_strdup (source->host);
	copy->port = g_strdup (source->port);
	copy->rootdn = g_strdup (source->rootdn);
	copy->scope = source->scope;
	copy->auth = source->auth;
	copy->ssl = source->ssl;
	copy->email_addr = g_strdup (source->email_addr);
	copy->binddn = g_strdup (source->binddn);
	copy->remember_passwd = source->remember_passwd;
	copy->limit = source->limit;

	return copy;
}
