/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-provider.c :  provider framework   */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org> .
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


/* 
   A provider can be added "by hand" or by loading a module. 


   Adding providers with modules. 
   ------------------------------

   The modules are shared libraries which must contain the 
   function

   CamelProvider *camel_provider_module_init ();
  
   returning the provider object defined in the module 

   
*/

/* FIXME: Shouldn't we add a version number to providers ? */

#include "config.h"
#include "camel-provider.h"
#include "camel-log.h"


static GList *_provider_list = NULL;
static gchar *_last_error;

static gint        
_provider_name_cmp (gconstpointer a, gconstpointer b)
{
	CamelProvider *provider_a = CAMEL_PROVIDER (a);
	CamelProvider *provider_b = CAMEL_PROVIDER (b);

	return strcmp ( provider_a->name,  provider_b->name);
}

void
camel_provider_register (CamelProvider *provider)
{
	GList *old_provider_node = NULL;

	g_assert (provider);
	
	if (_provider_list)
		old_provider_node = g_list_find_custom (_provider_list, provider, _provider_name_cmp);

	if (old_provider_node != NULL) {
		// camel_provider_unref (CAMEL_PROVIDER (old_provider_node->data));
		old_provider_node->data = provider;
	} else {
		/* be careful, we use prepend here, so that last registered
		   providers come first */
		_provider_list = g_list_prepend (_provider_list, provider);
	}
	// camel_provider_ref (provider);
	
}


const CamelProvider *
camel_provider_register_as_module (const gchar *module_path)
{
	
	CamelProvider *new_provider = NULL;
	GModule *new_module = NULL;
	CamelProvider * (*camel_provider_module_init) ();
	gboolean has_module_init;
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelProvider::register_as_module\n");
	
	g_return_val_if_fail (module_path, NULL);

	if (!g_module_supported ()) {
		CAMEL_LOG_WARNING ("CamelProvider::register_as_module module loading not supported on this system\n");
		CAMEL_LOG_FULL_DEBUG ("Leaving CamelProvider::register_as_module\n");
		return NULL;
	}
	

	new_module = g_module_open (module_path, 0);
	if (!new_module) {
		CAMEL_LOG_WARNING ("CamelProvider::register_as_module Unable to load module %s\n", module_path);
		CAMEL_LOG_FULL_DEBUG ("Leaving CamelProvider::register_as_module\n");
		return NULL;
	}
		
	has_module_init = g_module_symbol (new_module, "camel_provider_module_init", (gpointer *)&camel_provider_module_init);
	if (!has_module_init){
		CAMEL_LOG_WARNING ("CamelProvider::register_as_module loading of module %s failed,\n"
				   "\t Symbol camel_provider_module_init not defined in it\n", module_path);
		CAMEL_LOG_FULL_DEBUG ("Leaving CamelProvider::register_as_module\n");
		return NULL;
	}

	new_provider = camel_provider_module_init();
	new_provider->gmodule = new_module;
	camel_provider_register (new_provider);

	CAMEL_LOG_FULL_DEBUG ("Leaving CamelProvider::register_as_module\n");
	return new_provider;

 
} 




/* 
   be careful to this function, @a is expected to be
   a provider, @b a protocol name (const gchar *)
*/
static gint        
_provider_protocol_find (gconstpointer a, gconstpointer b)
{
	CamelProvider *provider_a = CAMEL_PROVIDER (a);
	const gchar *name_b = (const gchar *)b;

	return g_strcasecmp ( provider_a->name, name_b);
}

/**
 * camel_provider_get_for_protocol: get a registered provider for a protocol
 * @protocol: protocol name (case insensitive)
 * @type: provider type (transport, store, ...)
 * 
 * Look into the list of registered provider if 
 * one correspond both to the protocol name 
 * and to the protocol type. When several providers
 * exist for a same protocol, the last registered
 * is returned.
 * 
 * Return value: Matching provider or NULL if none exists. 
 **/
const CamelProvider *
camel_provider_get_for_protocol (const gchar *protocol, ProviderType type)
{
	CamelProvider *current_provider = NULL;
	GList *current_provider_node;
	gboolean protocol_is_found;
	gboolean provider_is_found;

	g_assert (protocol);
	g_return_val_if_fail (_provider_list, NULL);

	current_provider_node = _provider_list;
	provider_is_found = FALSE;

	while ((!provider_is_found) && current_provider_node) {
		current_provider = (CamelProvider *)current_provider_node->data;
		
		protocol_is_found = (g_strcasecmp (protocol, current_provider->protocol) == 0);
		if (protocol_is_found) 
			provider_is_found = (current_provider->provider_type == type);
		
		g_list_next (current_provider_node);
		}

	if (provider_is_found) return current_provider;
	else return NULL;
}
