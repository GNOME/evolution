/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-provider.c: provider framework */

/*
 *
 * Authors:
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *  Dan Winship <danw@ximian.com>
 *  Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright 1999, 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <gmodule.h>

#include "camel-provider.h"
#include "camel-exception.h"
#include "camel-string-utils.h"


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
	CamelProviderModule *m;

	providers = g_hash_table_new (camel_strcase_hash, camel_strcase_equal);
	
	dir = opendir (CAMEL_PROVIDERDIR);
	if (!dir) {
		g_error ("Could not open camel provider directory (%s): %s",
			 CAMEL_PROVIDERDIR, g_strerror (errno));
		return NULL;
	}
	
	while ((d = readdir (dir))) {
		FILE *fp;
		
		p = strchr (d->d_name, '.');
		if (!p || strcmp (p, ".urls") != 0)
			continue;
		
		name = g_strdup_printf ("%s/%s", CAMEL_PROVIDERDIR, d->d_name);
		fp = fopen (name, "r");
		if (!fp) {
			g_warning ("Could not read provider info file %s: %s",
				   name, g_strerror (errno));
			g_free (name);
			continue;
		}
		
		p = strrchr (name, '.');
		strcpy (p, ".so");

		m = g_malloc0(sizeof(*m));
		m->path = name;

		while ((fgets (buf, sizeof (buf), fp))) {
			buf[sizeof (buf) - 1] = '\0';
			p = strchr (buf, '\n');
			if (p)
				*p = '\0';
			
			if (*buf) {
				char *protocol = g_strdup(buf);

				m->types = g_slist_prepend(m->types, protocol);
				g_hash_table_insert(providers, protocol, m);
			}
		}

		fclose (fp);
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
				      _("Could not load %s: Module loading "
				      "not supported on this system."),
				      path);
		return;
	}

	module = g_module_open (path, 0);
	if (!module) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not load %s: %s"),
				      path, g_module_error ());
		return;
	}

	if (!g_module_symbol (module, "camel_provider_module_init",
			      (gpointer *)&camel_provider_module_init)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not load %s: No initialization "
					"code in module."), path);
		g_module_close (module);
		return;
	}

	camel_provider_module_init (session);
}


/**
 * camel_provider_auto_detect:
 * @provider: camel provider
 * @settings: currently set settings
 * @auto_detected: output hash table of auto-detected values
 * @ex: exception
 *
 * After filling in the standard Username/Hostname/Port/Path settings
 * (which must be set in @settings), if the provider supports it, you
 * may wish to have the provider auto-detect further settings based on
 * the aformentioned settings.
 *
 * If the provider does not support auto-detection, @auto_detected
 * will be set to %NULL. Otherwise the provider will attempt to
 * auto-detect whatever it can and file them into @auto_detected. If
 * for some reason it cannot auto-detect anything (not enough
 * information provided in @settings?) then @auto_deetected will be
 * set to %NULL and an exception may be set to explain why it failed.
 *
 * Returns 0 on success or -1 on fail.
 **/
int
camel_provider_auto_detect (CamelProvider *provider, CamelURL *url,
			    GHashTable **auto_detected, CamelException *ex)
{
	g_return_val_if_fail (provider != NULL, -1);
	
	if (provider->auto_detect) {
		return provider->auto_detect (url, auto_detected, ex);
	} else {
		*auto_detected = NULL;
		return 0;
	}
}
