/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-provider.c: provider framework */

/*
 *
 * Authors:
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *  Dan Winship <danw@helixcode.com>
 *  Jeffrey Stedfast <fejj@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


/* FIXME: Shouldn't we add a version number to providers ? */

#include "config.h"
#include "camel-provider.h"
#include "camel-exception.h"
#include "hash-table-utils.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <gmodule.h>

char *camel_provider_type_name[CAMEL_NUM_PROVIDER_TYPES] = {
	"store",
	"transport"
};

/**
 * camel_provider_init:
 *
 * Initialize the Camel provider system by reading in the .urls
 * files in the provider directory and creating a hash table mapping
 * URLs to module names.
 *
 * A .urls file has the same initial prefix as the shared library it
 * correspond to, and consists of a series of lines containing the URL
 * protocols that that library handles.
 *
 * Return value: a hash table mapping URLs to module names
 **/
GHashTable *
camel_provider_init (void)
{
	GHashTable *providers;
	DIR *dir;
	struct dirent *d;
	char *p, *name, buf[80];
	FILE *f;

	providers = g_hash_table_new (g_strcase_hash, g_strcase_equal);

	dir = opendir (CAMEL_PROVIDERDIR);
	if (!dir) {
		g_error ("Could not open camel provider directory: %s",
			 g_strerror (errno));
		return NULL;
	}

	while ((d = readdir (dir))) {
		p = strchr (d->d_name, '.');
		if (!p || strcmp (p, ".urls") != 0)
			continue;

		name = g_strdup_printf ("%s/%s", CAMEL_PROVIDERDIR, d->d_name);
		f = fopen (name, "r");
		if (!f) {
			g_warning ("Could not read provider info file %s: %s",
				   name, g_strerror (errno));
			g_free (name);
			continue;
		}

		p = strrchr (name, '.');
		strcpy (p, ".so");
		while ((fgets (buf, sizeof (buf), f))) {
			buf[sizeof (buf) - 1] = '\0';
			p = strchr (buf, '\n');
			if (p)
				*p = '\0';

			g_hash_table_insert (providers, g_strdup (buf), name);
		}
		fclose (f);
	}

	closedir (dir);
	return providers;
}

/**
 * camel_provider_load:
 * @session: the current session
 * @path: the path to a shared library
 * @ex: a CamelException
 *
 * Loads the provider at @path, and calls its initialization function,
 * passing @session as an argument. The provider should then register
 * itself with @session.
 **/ 
void
camel_provider_load (CamelSession *session, const char *path, CamelException *ex)
{
	GModule *module;
	CamelProvider *(*camel_provider_module_init) ();

	if (!g_module_supported ()) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not load %s: Module loading "
				      "not supported on this system.",
				      path);
		return;
	}

	module = g_module_open (path, 0);
	if (!module) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not load %s: %s",
				      path, g_module_error ());
		return;
	}

	if (!g_module_symbol (module, "camel_provider_module_init",
			      (gpointer *)&camel_provider_module_init)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not load %s: No initialization "
				      "routine in module.", path);
		g_module_close (module);
		return;
	}

	camel_provider_module_init (session);
}
