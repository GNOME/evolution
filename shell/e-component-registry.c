/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-component-registry.c
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkobject.h>
#include <gtk/gtktypeutils.h>
#include <bonobo.h>

#include "e-util/e-util.h"
#include "e-shell-utils.h"

#include "Evolution.h"

#include "e-component-registry.h"


#define PARENT_TYPE GTK_TYPE_OBJECT
static GtkObjectClass *parent_class = NULL;

typedef struct _Component Component;

struct _Component {
	char *id;
	
	BonoboObjectClient *client;

	/* Names of the folder types we support (normal ASCII strings).  */
	GList *folder_type_names;
};

struct _EComponentRegistryPrivate {
	EShell *shell;

	GHashTable *component_id_to_component;
};


/* Component information handling.  */

static Component *
component_new (const char *id,
	       BonoboObjectClient *client)
{
	Component *new;

	bonobo_object_ref (BONOBO_OBJECT (client));

	new = g_new (Component, 1);
	new->id                = g_strdup (id);
	new->folder_type_names = NULL;
	new->client            = client;

	return new;
}

static void
component_free (Component *component)
{
	Evolution_ShellComponent corba_shell_component;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	corba_shell_component = bonobo_object_corba_objref (BONOBO_OBJECT (component->client));
	Evolution_ShellComponent_unset_owner (corba_shell_component, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_warning ("Cannot unregister component -- %s", component->id);
	else
		g_print ("Component unregistered successfully -- %s\n", component->id);
	CORBA_exception_free (&ev);

	g_free (component->id);

	bonobo_object_unref (BONOBO_OBJECT (component->client));

	e_free_string_list (component->folder_type_names);

	g_free (component);
}

static gboolean
register_type (EComponentRegistry *component_registry,
	       const char *name,
	       const char *icon_name,
	       Component *handler)
{
	EComponentRegistryPrivate *priv;
	EFolderTypeRegistry *folder_type_registry;

	priv = component_registry->priv;

	folder_type_registry = e_shell_get_folder_type_registry (priv->shell);
	g_assert (folder_type_registry != NULL);

	if (! e_folder_type_registry_register_type (folder_type_registry, name, icon_name)) {
		g_warning ("Trying to register duplicate folder type -- %s", name);
		return FALSE;
	}

	e_folder_type_registry_set_handler_for_type (folder_type_registry, name, handler->client);

	return TRUE;
}

static gboolean
register_component (EComponentRegistry *component_registry,
		    const char *id)
{
	EComponentRegistryPrivate *priv;
	Evolution_ShellComponent component_corba_interface;
	Evolution_Shell shell_corba_interface;
	Evolution_FolderTypeList *supported_types;
	Component *component;
	BonoboObjectClient *client;
	CORBA_Environment ev;
	CORBA_unsigned_long i;

	priv = component_registry->priv;

	if (g_hash_table_lookup (priv->component_id_to_component, id) != NULL) {
		g_warning ("Trying to register component twice -- %s", id);
		return FALSE;
	}

	client = bonobo_object_activate (id, 0);
	if (client == NULL)
		return FALSE;

	CORBA_exception_init (&ev);

	component_corba_interface = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	shell_corba_interface = bonobo_object_corba_objref (BONOBO_OBJECT (priv->shell));

	Evolution_ShellComponent_set_owner (component_corba_interface, shell_corba_interface, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		bonobo_object_unref (BONOBO_OBJECT (client));
		CORBA_exception_free (&ev);
		return FALSE;
	}

	supported_types = Evolution_ShellComponent__get_supported_types (component_corba_interface, &ev);
	if (ev._major != CORBA_NO_EXCEPTION || supported_types->_length == 0) {
		bonobo_object_unref (BONOBO_OBJECT (client));
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	component = component_new (id, client);
	g_hash_table_insert (priv->component_id_to_component, component->id, component);

	for (i = 0; i < supported_types->_length; i++) {
		const Evolution_FolderType *type;

		type = supported_types->_buffer + i;

		if (! register_type (component_registry, type->name, type->icon_name, component)) {
			g_warning ("Cannot register type `%s' for component %s",
				   type->name, component->id);
		} else {
			g_print ("Registered type for component -- (%s) %s\n",
				 type->name, component->id);
		}
	}

	CORBA_free (supported_types);

	return TRUE;
}


/* GtkObject methods.  */

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
destroy (GtkObject *object)
{
	EComponentRegistry *component_registry;
	EComponentRegistryPrivate *priv;

	component_registry = E_COMPONENT_REGISTRY (object);
	priv = component_registry->priv;

	bonobo_object_unref (BONOBO_OBJECT (priv->shell));

	g_hash_table_foreach (priv->component_id_to_component, component_id_foreach_free, NULL);
	g_hash_table_destroy (priv->component_id_to_component);

	g_free (priv);
}


static void
class_init (EComponentRegistryClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	parent_class = gtk_type_class (gtk_object_get_type ());
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

	bonobo_object_ref (BONOBO_OBJECT (shell));
	priv->shell = shell;
}

EComponentRegistry *
e_component_registry_new (EShell *shell)
{
	EComponentRegistry *component_registry;

	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	component_registry = gtk_type_new (e_component_registry_get_type ());
	e_component_registry_construct (component_registry, shell);

	return component_registry;
}


gboolean
e_component_registry_register_component (EComponentRegistry *component_registry,
					 const char *id)
{
	g_return_val_if_fail (component_registry != NULL, FALSE);
	g_return_val_if_fail (E_IS_COMPONENT_REGISTRY (component_registry), FALSE);
	g_return_val_if_fail (id != NULL, FALSE);

	return register_component (component_registry, id);
}


E_MAKE_TYPE (e_component_registry, "EComponentRegistry", EComponentRegistry,
	     class_init, init, PARENT_TYPE)
