/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 * Authors:
 * Chris Toshok <toshok@ximian.com>
 * Chris Lahey <clahey@ximian.com>
 **/

#ifndef __ADDRESSBOOK_CONFIG_H__
#define __ADDRESSBOOK_CONFIG_H__

#include "evolution-config-control.h"

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

GtkWidget* addressbook_config_edit_source        (GtkWidget *parent, ESource *source);
GtkWidget* addressbook_config_create_new_source  (GtkWidget *parent);

#endif /* __ADDRESSBOOK_CONFIG_H__ */
