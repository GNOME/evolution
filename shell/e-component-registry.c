/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-component-registry.c
 *
 * Copyright (C) 2000, 2001, 2002  Ximian, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-component-registry.h"

#include <glib.h>
#include <gtk/gtktypeutils.h>

#include <gal/util/e-util.h>

#include <bonobo-activation/bonobo-activation.h>

#include "Evolution.h"

#include "e-shell-utils.h"
#include "evolution-shell-component-client.h"
#include "e-folder-type-registry.h"


#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;

typedef struct _Component Component;

struct _Component {
	char *id;
	
	EvolutionShellComponentClient *client;

	/* Names of the folder types we support (normal ASCII strings).  */
	GList *folder_type_names;
};

struct _EComponentRegistryPrivate {
	EShell *shell;

	GHashTable *component_id_to_component;
};


/* Utility functions.  */

static int
sleep_with_g_main_loop_timeout_callback (void *data)
{
	GMainLoop *loop;

	loop = (GMainLoop *) data;
	g_main_loop_quit (loop);

	return FALSE;
}

/* This function is like `sleep()', but it uses the GMainLoop so CORBA
   invocations can get through.  */
static void
sleep_with_g_main_loop (int num_seconds)
{
	GMainLoop *loop;

	loop = g_main_loop_new (NULL, TRUE);
	g_timeout_add (1000 * num_seconds, sleep_with_g_main_loop_timeout_callback, loop);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
}

static void
wait_for_corba_object_to_die (Bonobo_Unknown corba_objref,
			      const char *id)
{
	gboolean alive;
	int count;

	count = 1;
	while (1) {
		alive = bonobo_unknown_ping (corba_objref, NULL);
		if (! alive)
			break;

		g_print ("Waiting for component to die -- %s (%d)\n", id, count);
		sleep_with_g_main_loop (1);
		count ++;
	}
}


/* Component information handling.  */

static Component *
component_new (const char *id,
	       EvolutionShellComponentClient *client)
{
	Component *new;

	g_object_ref (client);

	new = g_new (Component, 1);
	new->id                = g_strdup (id);
	new->folder_type_names = NULL;
	new->client            = client;

	return new;
}

static gboolean
component_free (Component *component)
{
	GNOME_Evolution_ShellComponent corba_shell_component;
	CORBA_Environment ev;
	gboolean retval;

	CORBA_exception_init (&ev);

	corba_shell_component = evolution_shell_component_client_corba_objref (component->client);
	corba_shell_component = CORBA_Object_duplicate (corba_shell_component, &ev);

	GNOME_Evolution_ShellComponent_unsetOwner (corba_shell_component, &ev);
	if (ev._major == CORBA_NO_EXCEPTION)
		retval = TRUE;
	else
		retval = FALSE;
	CORBA_exception_free (&ev);

	g_object_unref (component->client);

	/* If the component is out-of-proc, wait for the process to die first.  */
	if (bonobo_object (ORBit_small_get_servant (corba_shell_component)) == NULL)
		wait_for_corba_object_to_die ((Bonobo_Unknown) corba_shell_component, component->id);

	CORBA_Object_release (corba_shell_component, &ev);

	e_free_string_list (component->folder_type_names);
	g_free (component->id);

	g_free (component);

	return retval;
}

static gboolean
register_type (EComponentRegistry *component_registry,
	       const char *name,
	       const char *icon_name,
	       const char *display_name,
	       const char *description,
	       gboolean user_creatable,
	       int num_exported_dnd_types,
	       const char **exported_dnd_types,
	       int num_accepted_dnd_types,
	       const char **accepted_dnd_types,
	       Component *handler,
	       gboolean override_duplicate)
{
	EComponentRegistryPrivate *priv;
	EFolderTypeRegistry *folder_type_registry;

	priv = component_registry->priv;

	folder_type_registry = e_shell_get_folder_type_registry (priv->shell);
	g_assert (folder_type_registry != NULL);

	if (override_duplicate
	    && e_folder_type_registry_type_registered (folder_type_registry, name))
		e_folder_type_registry_unregister_type (folder_type_registry, name);

	if (! e_folder_type_registry_register_type (folder_type_registry,
						    name, icon_name, 
						    display_name, description,
						    user_creatable,
						    num_exported_dnd_types,
						    exported_dnd_types,
						    num_accepted_dnd_types,
						    accepted_dnd_types)) {
		g_warning ("Trying to register duplicate folder type -- %s", name);
		return FALSE;
	}

	e_folder_type_registry_set_handler_for_type (folder_type_registry, name, handler->client);

	return TRUE;
}

static gboolean
register_component (EComponentRegistry *component_registry,
		    const char *id,
		    gboolean override_duplicate,
		    CORBA_Environment *ev)
{
	EComponentRegistryPrivate *priv;
	GNOME_Evolution_ShellComponent component_corba_interface;
	GNOME_Evolution_Shell shell_corba_interface;
	GNOME_Evolution_FolderTypeList *supported_types;
	GNOME_Evolution_URISchemaList *supported_schemas;
	Component *component;
	EvolutionShellComponentClient *client;
	CORBA_Environment my_ev;
	CORBA_unsigned_long i;

	priv = component_registry->priv;

	if (! override_duplicate && g_hash_table_lookup (priv->component_id_to_component, id) != NULL) {
		g_warning ("Trying to register component twice -- %s", id);
		return FALSE;
	}

	client = evolution_shell_component_client_new (id, ev);
	if (client == NULL)
		return FALSE;

	/* FIXME we could use the EvolutionShellComponentClient API here instead, but for
           now we don't care.  */

	component_corba_interface = evolution_shell_component_client_corba_objref (client);
	shell_corba_interface = BONOBO_OBJREF (priv->shell);

	CORBA_exception_init (&my_ev);

	/* Register the supported folder types.  */

	supported_types = GNOME_Evolution_ShellComponent__get_supportedTypes (component_corba_interface, &my_ev);
	if (my_ev._major != CORBA_NO_EXCEPTION || supported_types->_length == 0) {
		g_object_unref (client);
		CORBA_exception_free (&my_ev);
		return FALSE;
	}

	CORBA_exception_free (&my_ev);

	component = component_new (id, client);
	g_hash_table_insert (priv->component_id_to_component, component->id, component);
	g_object_unref (client);

	for (i = 0; i < supported_types->_length; i++) {
		const GNOME_Evolution_FolderType *type;

		type = supported_types->_buffer + i;

		if (! register_type (component_registry,
				     type->name, type->iconName, 
				     type->displayName, type->description,
				     type->userCreatable,
				     type->exportedDndTypes._length,
				     (const char **) type->exportedDndTypes._buffer,
				     type->acceptedDndTypes._length,
				     (const char **) type->acceptedDndTypes._buffer,
				     component,
				     override_duplicate)) {
			g_warning ("Cannot register type `%s' for component %s",
				   type->name, component->id);
		}
	}

	CORBA_free (supported_types);

	/* Register the supported external URI schemas.  */

	supported_schemas = GNOME_Evolution_ShellComponent__get_externalUriSchemas (component_corba_interface, &my_ev);
	if (my_ev._major == CORBA_NO_EXCEPTION) {
		EUriSchemaRegistry *uri_schema_registry;

		uri_schema_registry = e_shell_get_uri_schema_registry (priv->shell); 

		for (i = 0; i < supported_schemas->_length; i++) {
			const CORBA_char *schema;

			schema = supported_schemas->_buffer[i];
			e_uri_schema_registry_set_handler_for_schema (uri_schema_registry, schema, component->client);
		}

		CORBA_free (supported_schemas);
	}

	return TRUE;
}


/* GObject methods.  */

static void
component_id_foreach_free (void *key,
			   void *value,
			   void *user_data)
{
	Component *component;

	component = (Component *) value;
	component_free (component);
}

static void
impl_dispose (GObject *object)
{
	EComponentRegistry *component_registry;
	EComponentRegistryPrivate *priv;

	component_registry = E_COMPONENT_REGISTRY (object);
	priv = component_registry->priv;

	if (priv->component_id_to_component != NULL) {
		g_hash_table_foreach (priv->component_id_to_component, component_id_foreach_free, NULL);
		g_hash_table_destroy (priv->component_id_to_component);
		priv->component_id_to_component = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EComponentRegistry *component_registry;
	EComponentRegistryPrivate *priv;

	component_registry = E_COMPONENT_REGISTRY (object);
	priv = component_registry->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
class_init (EComponentRegistryClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_ref(PARENT_TYPE);
}


static void
init (EComponentRegistry *component_registry)
{
	EComponentRegistryPrivate *priv;

	priv = g_new (EComponentRegistryPrivate, 1);
	priv->shell                     = NULL;
	priv->component_id_to_component = g_hash_table_new (g_str_hash, g_str_equal);

	component_registry->priv = priv;
}


void
e_component_registry_construct (EComponentRegistry *component_registry,
				EShell *shell)
{
	EComponentRegistryPrivate *priv;

	g_return_if_fail (component_registry != NULL);
	g_return_if_fail (E_IS_COMPONENT_REGISTRY (component_registry));
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));

	priv = component_registry->priv;
	priv->shell = shell;
}

EComponentRegistry *
e_component_registry_new (EShell *shell)
{
	EComponentRegistry *component_registry;

	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	component_registry = g_object_new (e_component_registry_get_type (), NULL);
	e_component_registry_construct (component_registry, shell);

	return component_registry;
}


gboolean
e_component_registry_register_component (EComponentRegistry *component_registry,
					 const char *id,
					 CORBA_Environment *ev)
{
	g_return_val_if_fail (component_registry != NULL, FALSE);
	g_return_val_if_fail (E_IS_COMPONENT_REGISTRY (component_registry), FALSE);
	g_return_val_if_fail (id != NULL, FALSE);

	return register_component (component_registry, id, FALSE, ev);
}


static void
compose_id_list_foreach (void *key,
			 void *value,
			 void *data)
{
	GList **listp;
	const char *id;

	listp = (GList **) data;
	id = (const char *) key;

	*listp = g_list_prepend (*listp, g_strdup (id));
}

/**
 * e_component_registry_get_id_list:
 * @component_registry: 
 * 
 * Get the list of components registered.
 * 
 * Return value: A GList of strings containining the IDs for all the registered
 * components.  The list must be freed by the caller when not used anymore.
 **/
GList *
e_component_registry_get_id_list (EComponentRegistry *component_registry)
{
	EComponentRegistryPrivate *priv;
	GList *list;

	g_return_val_if_fail (component_registry != NULL, NULL);
	g_return_val_if_fail (E_IS_COMPONENT_REGISTRY (component_registry), NULL);

	priv = component_registry->priv;
	list = NULL;

	g_hash_table_foreach (priv->component_id_to_component, compose_id_list_foreach, &list);

	return list;
}

/**
 * e_component_registry_get_component_by_id:
 * @component_registry: 
 * @id: The component's OAF ID
 * 
 * Get the registered component client for the specified ID.  If that component
 * is not registered, return NULL.
 * 
 * Return value: A pointer to the ShellComponentClient for that component.
 **/
EvolutionShellComponentClient *
e_component_registry_get_component_by_id  (EComponentRegistry *component_registry,
					   const char *id)
{
	EComponentRegistryPrivate *priv;
	const Component *component;

	g_return_val_if_fail (component_registry != NULL, NULL);
	g_return_val_if_fail (E_IS_COMPONENT_REGISTRY (component_registry), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	priv = component_registry->priv;

	component = g_hash_table_lookup (priv->component_id_to_component, id);
	if (component == NULL)
		return NULL;

	return component->client;
}


EvolutionShellComponentClient *
e_component_registry_restart_component  (EComponentRegistry *component_registry,
					 const char *id,
					 CORBA_Environment *ev)
{
	EComponentRegistryPrivate *priv;
	Component *component;
	CORBA_Environment my_ev;
	CORBA_Object corba_objref;

	g_return_val_if_fail (component_registry != NULL, NULL);
	g_return_val_if_fail (E_IS_COMPONENT_REGISTRY (component_registry), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	priv = component_registry->priv;

	component = g_hash_table_lookup (priv->component_id_to_component, id);
	if (component == NULL)
		return NULL;

	CORBA_exception_init (&my_ev);

	g_hash_table_remove (priv->component_id_to_component, id);

	corba_objref = CORBA_Object_duplicate (evolution_shell_component_client_corba_objref (component->client), &my_ev);

	component_free (component);

	wait_for_corba_object_to_die (corba_objref, id);

	CORBA_exception_free (&my_ev);

	if (! register_component (component_registry, id, TRUE, ev))
		return NULL;

	return e_component_registry_get_component_by_id (component_registry, id);
}


E_MAKE_TYPE (e_component_registry, "EComponentRegistry", EComponentRegistry,
	     class_init, init, PARENT_TYPE)
