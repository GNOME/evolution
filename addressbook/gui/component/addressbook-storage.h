/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* addressbook-storage.h
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

#ifndef __ADDRESSBOOK_STORAGE_H__
#define __ADDRESSBOOK_STORAGE_H__

#include "evolution-shell-component.h"

typedef enum {
	ADDRESSBOOK_SOURCE_FILE,
	ADDRESSBOOK_SOURCE_LDAP,
	ADDRESSBOOK_SOURCE_LAST
} AddressbookSourceType;

typedef enum {
	ADDRESSBOOK_LDAP_AUTH_NONE,
	ADDRESSBOOK_LDAP_AUTH_SIMPLE,
	ADDRESSBOOK_LDAP_AUTH_SASL, /* XXX currently unsupported */
	ADDRESSBOOK_LDAP_AUTH_LAST
} AddressbookLDAPAuthType;

typedef enum {
	ADDRESSBOOK_LDAP_SCOPE_SUBTREE,
	ADDRESSBOOK_LDAP_SCOPE_BASE,
	ADDRESSBOOK_LDAP_SCOPE_ONELEVEL,
	ADDRESSBOOK_LDAP_SCOPE_LAST
} AddressbookLDAPScopeType;

typedef struct {
	AddressbookSourceType type;
	char *name;
	char *description;
	struct {
		char *path;
	} file;
	struct {
		char *host;
		char *port;
		char *rootdn;
		AddressbookLDAPScopeType scope;
		AddressbookLDAPAuthType auth;
		char *binddn;                   /* used in AUTH_SIMPLE */
		gboolean remember_passwd;
	} ldap;
	char *uri; /* filled in from the above */
} AddressbookSource;

void addressbook_storage_setup (EvolutionShellComponent *shell_component,
				const char *evolution_homedir);

GList             *addressbook_storage_get_sources (void);
AddressbookSource *addressbook_storage_get_source_by_uri (const char *uri);
void               addressbook_storage_clear_sources (void);
AddressbookSource *addressbook_source_copy (const AddressbookSource *source);
void               addressbook_source_free (AddressbookSource *source);
void               addressbook_storage_init_source_uri (AddressbookSource *source);

void               addressbook_storage_add_source (AddressbookSource *source);
void               addressbook_storage_remove_source (const char *name);

#endif /* __ADDRESSBOOK_STORAGE_H__ */
