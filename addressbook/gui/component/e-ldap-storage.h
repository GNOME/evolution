/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-ldap-storage.h
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

#ifndef __E_LDAP_STORAGE_H__
#define __E_LDAP_STORAGE_H__

#include "evolution-shell-component.h"

typedef struct {
	char *name;
	char *description;
	char *host;
	char *port;
	char *rootdn;
	char *scope;
	char *uri; /* filled in from the above */
} ELDAPServer;

void setup_ldap_storage (EvolutionShellComponent *shell_component,
			 const char *evolution_homedir);
void e_ldap_storage_add_server (ELDAPServer *server);
void e_ldap_storage_remove_server (char *name);

#endif /* __E_LDAP_STORAGE_H__ */
