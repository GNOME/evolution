/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* addressbook-storage.h
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

#ifndef __ADDRESSBOOK_STORAGE_H__
#define __ADDRESSBOOK_STORAGE_H__

#include "evolution-shell-component.h"
#include "evolution-storage.h"

typedef enum {
	ADDRESSBOOK_LDAP_AUTH_NONE,
	ADDRESSBOOK_LDAP_AUTH_SIMPLE_EMAIL,
	ADDRESSBOOK_LDAP_AUTH_SIMPLE_BINDDN,
} AddressbookLDAPAuthType;

typedef enum {
	ADDRESSBOOK_LDAP_SCOPE_ONELEVEL,
	ADDRESSBOOK_LDAP_SCOPE_SUBTREE,
	ADDRESSBOOK_LDAP_SCOPE_BASE,
	ADDRESSBOOK_LDAP_SCOPE_LAST
} AddressbookLDAPScopeType;

typedef enum {
	ADDRESSBOOK_LDAP_SSL_ALWAYS,
	ADDRESSBOOK_LDAP_SSL_WHENEVER_POSSIBLE,
	ADDRESSBOOK_LDAP_SSL_NEVER
} AddressbookLDAPSSLType;

typedef struct {
	char *name;
	char *description;
	char *host;
	char *port;
	char *rootdn;
	AddressbookLDAPScopeType scope;
	AddressbookLDAPAuthType auth;
	AddressbookLDAPSSLType ssl;
	char *email_addr;                   /* used in AUTH_SIMPLE_EMAIL */
	char *binddn;                       /* used in AUTH_SIMPLE_BINDDN */
	gboolean remember_passwd;
	int limit;

	char *uri; /* filled in from the above */
} AddressbookSource;

void addressbook_storage_setup (EvolutionShellComponent *shell_component,
				const char *evolution_homedir);
void addressbook_storage_cleanup (void);

EvolutionStorage  *addressbook_get_other_contact_storage (void);
GList             *addressbook_storage_get_sources (void);
AddressbookSource *addressbook_storage_get_source_by_uri (const char *uri);
void               addressbook_storage_clear_sources (void);
void               addressbook_storage_write_sources (void);
AddressbookSource *addressbook_source_copy (const AddressbookSource *source);
void               addressbook_source_free (AddressbookSource *source);
void               addressbook_storage_init_source_uri (AddressbookSource *source);

void               addressbook_storage_add_source (AddressbookSource *source);
void               addressbook_storage_remove_source (const char *name);
const char*        addressbook_storage_auth_type_to_string (AddressbookLDAPAuthType auth_type);

#endif /* __ADDRESSBOOK_STORAGE_H__ */
