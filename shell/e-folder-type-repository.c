/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-folder-type-repository.c
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

#include "e-folder-type-repository.h"


#define PARENT_TYPE GTK_TYPE_OBJECT
static GtkObjectClass *parent_class = NULL;

struct _FolderType {
	char *name;
	char *icon_name;
	char *control_id;

	/* The icon, standard (48x48) and mini (16x16) versions.  */
	GdkPixbuf *icon_pixbuf;
	GdkPixbuf *mini_icon_pixbuf;
};
typedef struct _FolderType FolderType;

struct _EFolderTypeRepositoryPrivate {
	GHashTable *name_to_type;
};


/* FIXME these are hardcoded for now.  */

#ifdef USING_OAF
#    	define CALENDAR_CONTROL_ID "OAFIID:control:calendar:dd34ddae-25c6-486b-a8a8-3e8f0286b54c"
#    	define CONTACTS_CONTROL_ID "OAFIID:control:addressbook:851f883b-2fe7-4c94-a1e3-a1f2a7a03c49"
#       define MAIL_CONTROL_ID     "OAFIID:control:evolution-mail:833d5a71-a201-4a0e-b7e6-5475c5c4cb45"
#else
#   	define CALENDAR_CONTROL_ID "control:calendar"
#   	define CONTACTS_CONTROL_ID "control:addressbook"
#   	define MAIL_CONTROL_ID     "control:evolution-mail"
#endif


/* FolderType handling.  */

static FolderType *
folder_type_new (const char *name,
		 const char *icon_name,
		 const char *control_id)
{
	FolderType *new;
	char *icon_path;

	new = g_new (FolderType, 1);

	new->name       = g_strdup (name);
	new->icon_name  = g_strdup (icon_name);
	new->control_id = g_strdup (control_id);

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
	g_free (folder_type->control_id);

	if (folder_type->icon_pixbuf != NULL)
		gdk_pixbuf_unref (folder_type->icon_pixbuf);
	if (folder_type->mini_icon_pixbuf != NULL)
		gdk_pixbuf_unref (folder_type->mini_icon_pixbuf);

	g_free (folder_type);
}

static const FolderType *
get_folder_type (EFolderTypeRepository *folder_type_repository,
		 const char *type_name)
{
	EFolderTypeRepositoryPrivate *priv;

	priv = folder_type_repository->priv;

	return g_hash_table_lookup (priv->name_to_type, type_name);
}

static gboolean
add_folder_type (EFolderTypeRepository *folder_type_repository,
		 const char *name,
		 const char *icon_name,
		 const char *control_id)
{
	EFolderTypeRepositoryPrivate *priv;
	FolderType *folder_type;

	priv = folder_type_repository->priv;

	/* Make sure we don't add the same type twice.  */
	if (get_folder_type (folder_type_repository, name) != NULL)
		return FALSE;

	folder_type = folder_type_new (name, icon_name, control_id);
	g_hash_table_insert (priv->name_to_type, folder_type->name, folder_type);

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
	EFolderTypeRepository *folder_type_repository;
	EFolderTypeRepositoryPrivate *priv;

	folder_type_repository = E_FOLDER_TYPE_REPOSITORY (object);
	priv = folder_type_repository->priv;

	g_hash_table_foreach (priv->name_to_type, hash_forall_free_folder_type, NULL);
	g_hash_table_destroy (priv->name_to_type);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EFolderTypeRepositoryClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;

	parent_class = gtk_type_class (gtk_object_get_type ());
}

static void
init (EFolderTypeRepository *folder_type_repository)
{
	EFolderTypeRepositoryPrivate *priv;

	priv = g_new (EFolderTypeRepositoryPrivate, 1);
	priv->name_to_type = g_hash_table_new (g_str_hash, g_str_equal);

	folder_type_repository->priv = priv;
}


void
e_folder_type_repository_construct (EFolderTypeRepository *folder_type_repository)
{
	g_return_if_fail (folder_type_repository != NULL);
	g_return_if_fail (E_IS_FOLDER_TYPE_REPOSITORY (folder_type_repository));

	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (folder_type_repository), GTK_FLOATING);

	/* FIXME these are hardcoded for now.  */

	add_folder_type (folder_type_repository,
			 "mail", "evolution-inbox.png", MAIL_CONTROL_ID);
	add_folder_type (folder_type_repository,
			 "calendar", "evolution-calendar.png", CALENDAR_CONTROL_ID);
	add_folder_type (folder_type_repository,
			 "contacts", "evolution-contacts.png", CONTACTS_CONTROL_ID);
}

EFolderTypeRepository *
e_folder_type_repository_new (void)
{
	EFolderTypeRepository *new;

	new = gtk_type_new (e_folder_type_repository_get_type ());

	e_folder_type_repository_construct (new);

	return new;
}


const char *
e_folder_type_repository_get_icon_name_for_type (EFolderTypeRepository *folder_type_repository,
						 const char *type_name)
{
	const FolderType *folder_type;

	g_return_val_if_fail (folder_type_repository != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER_TYPE_REPOSITORY (folder_type_repository), NULL);
	g_return_val_if_fail (type_name != NULL, NULL);

	folder_type = get_folder_type (folder_type_repository, type_name);
	if (folder_type == NULL) {
		g_warning ("%s: Unknown type -- %s", __FUNCTION__, type_name);
		return NULL;
	}

	return folder_type->icon_name;
}

GdkPixbuf *
e_folder_type_repository_get_icon_for_type (EFolderTypeRepository *folder_type_repository,
					    const char *type_name,
					    gboolean mini)
{
	const FolderType *folder_type;

	g_return_val_if_fail (folder_type_repository != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER_TYPE_REPOSITORY (folder_type_repository), NULL);
	g_return_val_if_fail (type_name != NULL, NULL);

	folder_type = get_folder_type (folder_type_repository, type_name);
	if (folder_type == NULL) {
		g_warning ("%s: Unknown type -- %s", __FUNCTION__, type_name);
		return NULL;
	}

	if (mini)
		return folder_type->mini_icon_pixbuf;
	else
		return folder_type->icon_pixbuf;
}

const char *
e_folder_type_repository_get_control_id_for_type (EFolderTypeRepository *folder_type_repository,
						  const char *type_name)
{
	const FolderType *folder_type;

	g_return_val_if_fail (folder_type_repository != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER_TYPE_REPOSITORY (folder_type_repository), NULL);
	g_return_val_if_fail (type_name != NULL, NULL);

	folder_type = get_folder_type (folder_type_repository, type_name);
	if (folder_type == NULL) {
		g_warning ("%s: Unknown type -- %s", __FUNCTION__, type_name);
		return NULL;
	}

	return folder_type->control_id;
}


E_MAKE_TYPE (e_folder_type_repository, "EFolderTypeRepository", EFolderTypeRepository,
	     class_init, init, PARENT_TYPE)
