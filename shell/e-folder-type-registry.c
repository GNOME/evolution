/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-folder-type-registry.c
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

#include <glib.h>
#include <gtk/gtktypeutils.h>

#include "e-util/e-util.h"

#include "e-shell-utils.h"

#include "e-folder-type-registry.h"


#define PARENT_TYPE GTK_TYPE_OBJECT
static GtkObjectClass *parent_class = NULL;

struct _FolderType {
	char *name;
	char *icon_name;

	EvolutionShellComponentClient *handler;

	/* The icon, standard (48x48) and mini (16x16) versions.  */
	GdkPixbuf *icon_pixbuf;
	GdkPixbuf *mini_icon_pixbuf;
};
typedef struct _FolderType FolderType;

struct _EFolderTypeRegistryPrivate {
	GHashTable *name_to_type;
};


/* FolderType handling.  */

static FolderType *
folder_type_new (const char *name,
		 const char *icon_name)
{
	FolderType *new;
	char *icon_path;

	new = g_new (FolderType, 1);

	new->name      = g_strdup (name);
	new->icon_name = g_strdup (icon_name);
	new->handler   = NULL;

	icon_path = e_shell_get_icon_path (icon_name, FALSE);
	if (icon_path == NULL)
		new->icon_pixbuf = NULL;
	else
		new->icon_pixbuf = gdk_pixbuf_new_from_file (icon_path);

	g_free (icon_path);

	icon_path = e_shell_get_icon_path (icon_name, TRUE);
	if (icon_path != NULL) {
		new->mini_icon_pixbuf = gdk_pixbuf_new_from_file (icon_path);
	} else {
		if (new->icon_pixbuf != NULL)
			new->mini_icon_pixbuf = gdk_pixbuf_ref (new->icon_pixbuf);
		else
			new->mini_icon_pixbuf = NULL;
	}

	g_free (icon_path);

	return new;
}

static void
folder_type_free (FolderType *folder_type)
{
	g_free (folder_type->name);
	g_free (folder_type->icon_name);

	if (folder_type->icon_pixbuf != NULL)
		gdk_pixbuf_unref (folder_type->icon_pixbuf);
	if (folder_type->mini_icon_pixbuf != NULL)
		gdk_pixbuf_unref (folder_type->mini_icon_pixbuf);

	if (folder_type->handler != NULL)
		bonobo_object_unref (BONOBO_OBJECT (folder_type->handler));

	g_free (folder_type);
}

static FolderType *
get_folder_type (EFolderTypeRegistry *folder_type_registry,
		 const char *type_name)
{
	EFolderTypeRegistryPrivate *priv;

	priv = folder_type_registry->priv;

	return g_hash_table_lookup (priv->name_to_type, type_name);
}

static gboolean
register_folder_type (EFolderTypeRegistry *folder_type_registry,
		      const char *name,
		      const char *icon_name)
{
	EFolderTypeRegistryPrivate *priv;
	FolderType *folder_type;

	priv = folder_type_registry->priv;

	/* Make sure we don't add the same type twice.  */
	if (get_folder_type (folder_type_registry, name) != NULL)
		return FALSE;

	folder_type = folder_type_new (name, icon_name);
	g_hash_table_insert (priv->name_to_type, folder_type->name, folder_type);

	return TRUE;
}

static gboolean
set_handler (EFolderTypeRegistry *folder_type_registry,
	     const char *name,
	     EvolutionShellComponentClient *handler)
{
	EFolderTypeRegistryPrivate *priv;
	FolderType *folder_type;

	priv = folder_type_registry->priv;

	folder_type = get_folder_type (folder_type_registry, name);
	if (folder_type == NULL)
		return FALSE;
	if (folder_type->handler != NULL) {
		g_warning ("Folder type already has a handler -- %s",
			   folder_type->name);
		return FALSE;
	}

	bonobo_object_ref (BONOBO_OBJECT (handler));
	folder_type->handler = handler;

	return TRUE;
}


/* GtkObject methods.  */

static void
hash_forall_free_folder_type (gpointer key,
			      gpointer value,
			      gpointer data)
{
	FolderType *folder_type;

	folder_type = (FolderType *) value;
	folder_type_free (folder_type);
}

static void
destroy (GtkObject *object)
{
	EFolderTypeRegistry *folder_type_registry;
	EFolderTypeRegistryPrivate *priv;

	folder_type_registry = E_FOLDER_TYPE_REGISTRY (object);
	priv = folder_type_registry->priv;

	g_hash_table_foreach (priv->name_to_type,
			      hash_forall_free_folder_type, NULL);
	g_hash_table_destroy (priv->name_to_type);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EFolderTypeRegistryClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;

	parent_class = gtk_type_class (gtk_object_get_type ());
}

static void
init (EFolderTypeRegistry *folder_type_registry)
{
	EFolderTypeRegistryPrivate *priv;

	priv = g_new (EFolderTypeRegistryPrivate, 1);
	priv->name_to_type = g_hash_table_new (g_str_hash, g_str_equal);

	folder_type_registry->priv = priv;
}


void
e_folder_type_registry_construct (EFolderTypeRegistry *folder_type_registry)
{
	g_return_if_fail (folder_type_registry != NULL);
	g_return_if_fail (E_IS_FOLDER_TYPE_REGISTRY (folder_type_registry));

	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (folder_type_registry), GTK_FLOATING);
}

EFolderTypeRegistry *
e_folder_type_registry_new (void)
{
	EFolderTypeRegistry *new;

	new = gtk_type_new (e_folder_type_registry_get_type ());

	e_folder_type_registry_construct (new);

	return new;
}


gboolean
e_folder_type_registry_register_type (EFolderTypeRegistry *folder_type_registry,
				      const char *type_name,
				      const char *icon_name)
{
	g_return_val_if_fail (folder_type_registry != NULL, FALSE);
	g_return_val_if_fail (E_IS_FOLDER_TYPE_REGISTRY (folder_type_registry), FALSE);
	g_return_val_if_fail (type_name != NULL, FALSE);
	g_return_val_if_fail (icon_name != NULL, FALSE);

	return register_folder_type (folder_type_registry, type_name, icon_name);
}

gboolean
e_folder_type_registry_set_handler_for_type  (EFolderTypeRegistry *folder_type_registry,
					      const char *type_name,
					      EvolutionShellComponentClient *handler)
{
	g_return_val_if_fail (folder_type_registry != NULL, FALSE);
	g_return_val_if_fail (E_IS_FOLDER_TYPE_REGISTRY (folder_type_registry), FALSE);
	g_return_val_if_fail (handler != NULL, FALSE);
	g_return_val_if_fail (BONOBO_IS_OBJECT_CLIENT (handler), FALSE);

	return set_handler (folder_type_registry, type_name, handler);
}


static void
get_type_names_hash_forall (void *key,
			    void *value,
			    void *data)
{
	GList **type_name_list;

	type_name_list = (GList **) data;

	*type_name_list = g_list_prepend (*type_name_list, g_strdup ((const char *) key));
}

GList *
e_folder_type_registry_get_type_names (EFolderTypeRegistry *folder_type_registry)
{
	GList *type_name_list;
	EFolderTypeRegistryPrivate *priv;

	g_return_val_if_fail (folder_type_registry != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER_TYPE_REGISTRY (folder_type_registry), NULL);

	priv = folder_type_registry->priv;

	type_name_list = NULL;
	g_hash_table_foreach (priv->name_to_type, get_type_names_hash_forall, &type_name_list);

	return type_name_list;
}


const char *
e_folder_type_registry_get_icon_name_for_type (EFolderTypeRegistry *folder_type_registry,
					       const char *type_name)
{
	const FolderType *folder_type;

	g_return_val_if_fail (folder_type_registry != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER_TYPE_REGISTRY (folder_type_registry), NULL);
	g_return_val_if_fail (type_name != NULL, NULL);

	folder_type = get_folder_type (folder_type_registry, type_name);
	if (folder_type == NULL) {
		g_warning ("%s: Unknown type -- %s", __FUNCTION__, type_name);
		return NULL;
	}

	return folder_type->icon_name;
}

GdkPixbuf *
e_folder_type_registry_get_icon_for_type (EFolderTypeRegistry *folder_type_registry,
					  const char *type_name,
					  gboolean mini)
{
	const FolderType *folder_type;

	g_return_val_if_fail (folder_type_registry != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER_TYPE_REGISTRY (folder_type_registry), NULL);
	g_return_val_if_fail (type_name != NULL, NULL);

	folder_type = get_folder_type (folder_type_registry, type_name);
	if (folder_type == NULL) {
		g_warning ("%s: Unknown type -- %s", __FUNCTION__, type_name);
		return NULL;
	}

	if (mini)
		return folder_type->mini_icon_pixbuf;
	else
		return folder_type->icon_pixbuf;
}

EvolutionShellComponentClient *
e_folder_type_registry_get_handler_for_type (EFolderTypeRegistry *folder_type_registry,
					     const char *type_name)
{
	const FolderType *folder_type;

	g_return_val_if_fail (folder_type_registry != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER_TYPE_REGISTRY (folder_type_registry), NULL);
	g_return_val_if_fail (type_name != NULL, NULL);

	folder_type = get_folder_type (folder_type_registry, type_name);
	if (folder_type == NULL) {
		g_warning ("%s: Unknown type -- %s", __FUNCTION__, type_name);
		return NULL;
	}

	return folder_type->handler;
}


E_MAKE_TYPE (e_folder_type_registry, "EFolderTypeRegistry", EFolderTypeRegistry,
	     class_init, init, PARENT_TYPE)
