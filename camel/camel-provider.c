/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 * camel-provider.c: provider framework
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
#include "camel-vee-store.h"
#include "libedataserver/e-msgport.h"
#include "camel-i18n.h"

/* table of CamelProviderModule's */
static GHashTable *module_table;
/* table of CamelProvider's */
static GHashTable *provider_table;
static EMutex *provider_lock;

#define LOCK() e_mutex_lock(provider_lock);
#define UNLOCK() e_mutex_unlock(provider_lock);

/* The vfolder provider is always available */
static CamelProvider vee_provider = {
	"vfolder",
	N_("Virtual folder email provider"),
	
	N_("For reading mail as a query of another set of folders"),
	
	"vfolder",
	
	CAMEL_PROVIDER_IS_STORAGE,
	CAMEL_URL_NEED_PATH | CAMEL_URL_PATH_IS_ABSOLUTE | CAMEL_URL_FRAGMENT_IS_PATH,
	
	/* ... */
};

static pthread_once_t setup_once = PTHREAD_ONCE_INIT;

static void
provider_setup(void)
{
	provider_lock = e_mutex_new(E_MUTEX_REC);
	module_table = g_hash_table_new(camel_strcase_hash, camel_strcase_equal);
	provider_table = g_hash_table_new(camel_strcase_hash, camel_strcase_equal);

	vee_provider.object_types[CAMEL_PROVIDER_STORE] = camel_vee_store_get_type ();
	vee_provider.url_hash = camel_url_hash;
	vee_provider.url_equal = camel_url_equal;
	camel_provider_register(&vee_provider);
}

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
 * TODO: This should be pathed?
 * TODO: This should be plugin-d?
 **/
void
camel_provider_init (void)
{
	DIR *dir;
	struct dirent *d;
	char *p, *name, buf[80];
	CamelProviderModule *m;
	static int loaded = 0;

	pthread_once(&setup_once, provider_setup);

	if (loaded)
		return;

	loaded = 1;

	dir = opendir (CAMEL_PROVIDERDIR);
	if (!dir) {
		g_warning("Could not open camel provider directory (%s): %s",
			  CAMEL_PROVIDERDIR, g_strerror (errno));
		return;
	}
	
	while ((d = readdir (dir))) {
		FILE *fp;
		
		p = strrchr (d->d_name, '.');
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
				g_hash_table_insert(module_table, protocol, m);
			}
		}

		fclose (fp);
	}

	closedir (dir);
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
camel_provider_load(const char *path, CamelException *ex)
{
	GModule *module;
	CamelProvider *(*camel_provider_module_init) (void);

	pthread_once(&setup_once, provider_setup);

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

	camel_provider_module_init ();
}

/**
 * camel_provider_register:
 * @provider: provider object
 *
 * Registers a provider.
 **/
void
camel_provider_register(CamelProvider *provider)
{
	int i;
	CamelProviderConfEntry *conf;
	GList *l;

	g_return_if_fail (provider != NULL);

	g_assert(provider_table);

	LOCK();

	if (g_hash_table_lookup(provider_table, provider->protocol) != NULL) {
		g_warning("Trying to re-register camel provider for protocol '%s'", provider->protocol);
		UNLOCK();
		return;
	}
	
	for (i = 0; i < CAMEL_NUM_PROVIDER_TYPES; i++) {
		if (provider->object_types[i])
			provider->service_cache[i] = camel_object_bag_new (provider->url_hash, provider->url_equal,
									   (CamelCopyFunc)camel_url_copy, (GFreeFunc)camel_url_free);
	}

	/* Translate all strings here */
#define P_(string) dgettext (provider->translation_domain, string)

	provider->name = P_(provider->name);
	provider->description = P_(provider->description);
	conf = provider->extra_conf;
	if (conf) {
		for (i=0;conf[i].type != CAMEL_PROVIDER_CONF_END;i++) {
			if (conf[i].text)
				conf[i].text = P_(conf[i].text);
		}
	}

	l = provider->authtypes;
	while (l) {
		CamelServiceAuthType *auth = l->data;

		auth->name = P_(auth->name);
		auth->description = P_(auth->description);
		l = l->next;
	}

	g_hash_table_insert(provider_table, provider->protocol, provider);

	UNLOCK();
}

static gint
provider_compare (gconstpointer a, gconstpointer b)
{
	const CamelProvider *cpa = (const CamelProvider *)a;
	const CamelProvider *cpb = (const CamelProvider *)b;

	return strcmp (cpa->name, cpb->name);
}

static void
add_to_list (gpointer key, gpointer value, gpointer user_data)
{
	GList **list = user_data;

	*list = g_list_prepend(*list, value);
}

/**
 * camel_session_list_providers:
 * @session: the session
 * @load: whether or not to load in providers that are not already loaded
 *
 * This returns a list of available providers in this session. If @load
 * is %TRUE, it will first load in all available providers that haven't
 * yet been loaded.
 *
 * Return value: a GList of providers, which the caller must free.
 **/
GList *
camel_provider_list(gboolean load)
{
	GList *list = NULL;

	g_assert(provider_table);

	LOCK();

	if (load) {
		GList *w;

		g_hash_table_foreach(module_table, add_to_list, &list);
		for (w = list;w;w = w->next) {
			CamelProviderModule *m = w->data;

			if (!m->loaded) {
				camel_provider_load(m->path, NULL);
				m->loaded = 1;
			}
		}
		g_list_free(list);
		list = NULL;
	}

	g_hash_table_foreach(provider_table, add_to_list, &list);

	UNLOCK();

	list = g_list_sort(list, provider_compare);

	return list;
}

/**
 * camel_provider_get:
 * @url_string: the URL for the service whose provider you want
 * @ex: a CamelException
 *
 * This returns the CamelProvider that would be used to handle
 * @url_string, loading it in from disk if necessary.
 *
 * Return value: the provider, or %NULL, in which case @ex will be set.
 **/
CamelProvider *
camel_provider_get(const char *url_string, CamelException *ex)
{
	CamelProvider *provider = NULL;
	char *protocol;
	size_t len;

	g_return_val_if_fail(url_string != NULL, NULL);
	g_assert(provider_table);

	len = strcspn(url_string, ":");
	protocol = g_alloca(len+1);
	memcpy(protocol, url_string, len);
	protocol[len] = 0;

	LOCK();

	provider = g_hash_table_lookup(provider_table, protocol);
	if (!provider) {
		CamelProviderModule *m;

		m = g_hash_table_lookup(module_table, protocol);
		if (m && !m->loaded) {
			m->loaded = 1;
			camel_provider_load(m->path, ex);
			if (camel_exception_is_set (ex))
				goto fail;
		}
		provider = g_hash_table_lookup(provider_table, protocol);
	}

	if (provider == NULL)
		camel_exception_setv(ex, CAMEL_EXCEPTION_SERVICE_URL_INVALID,
				     _("No provider available for protocol `%s'"),
				     protocol);
fail:
	UNLOCK();

	return provider;
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
