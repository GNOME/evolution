/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shortcuts.c
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

/* The shortcut list goes this:

   <?xml version="1.0"?>
   <shortcuts>
           <group title="Evolution shortcuts">
	           <item>evolution:/local/Inbox</item>
	           <item>evolution:/local/Trash</item>
	           <item>evolution:/local/Calendar</item>
	   </group>

	   <group title="Personal shortcuts">
	           <item>evolution:/local/Personal</item>
	   </group>
   </shortcuts>

   FIXME: Do we want to use a namespace for this?
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkobject.h>
#include <gtk/gtktypeutils.h>

#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include <string.h>

#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"

#include "shortcut-bar/e-shortcut-bar.h"
#include "e-shortcuts-view.h"

#include "e-shortcuts.h"


#define PARENT_TYPE GTK_TYPE_OBJECT
static GtkObjectClass *parent_class = NULL;

struct _ShortcutGroup {
	/* Title of the group.  */
	char *title;

	/* A list of strings with the URI for the shortcut.  */
	GList *shortcuts;
};
typedef struct _ShortcutGroup ShortcutGroup;

struct _EShortcutsPrivate {
	/* The storage set to which these shortcuts are associated.  */
	EStorageSet *storage_set;

	/* The folder type repository.  */
	EFolderTypeRepository *folder_type_repository;

	/* A list of ShortcutGroups.  */
	GList *groups;

	/* A list of ShortcutViews.  */
	GList *views;
};


static void
unload_shortcuts (EShortcuts *shortcuts)
{
	EShortcutsPrivate *priv;
	GList *p, *q;

	priv = shortcuts->priv;

	for (p = priv->groups; p != NULL; p = p->next) {
		ShortcutGroup *group;

		group = (ShortcutGroup *) p->data;
		g_free (group->title);

		for (q = group->shortcuts; q != NULL; q = q->next)
			g_free (q->data);

		g_list_free (group->shortcuts);
	}

	if (priv->groups != NULL)
		g_list_free (priv->groups);

	priv->groups = NULL;

	/* FIXME update the views.  */
}

static gboolean
load_shortcuts (EShortcuts *shortcuts,
		const char *path)
{
	EShortcutsPrivate *priv;
	xmlDoc *doc;
	xmlNode *root;
	xmlNode *p, *q;

	priv = shortcuts->priv;

	doc = xmlParseFile (path);
	if (doc == NULL)
		return FALSE;

	root = xmlDocGetRootElement (doc);
	if (root == NULL || strcmp (root->name, "shortcuts") != 0) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	unload_shortcuts (shortcuts);

	for (p = root->childs; p != NULL; p = p->next) {
		ShortcutGroup *shortcut_group;
		char *shortcut_group_title;

		if (strcmp ((char *) p->name, "group") != 0)
			continue;

		shortcut_group_title = (char *) xmlGetProp (p, "title");
		if (shortcut_group_title == NULL)
			continue;

		shortcut_group = g_new (ShortcutGroup, 1);
		shortcut_group->title = g_strdup (shortcut_group_title);
		xmlFree (shortcut_group_title);
		shortcut_group->shortcuts = NULL;

		for (q = p->childs; q != NULL; q = q->next) {
			xmlNode *link_node;

			if (strcmp ((char *) q->name, "item") != 0)
				continue;

			link_node = e_xml_get_child_by_name (q, (xmlChar *) "text");
			if (link_node == NULL)
				continue;

			shortcut_group->shortcuts = g_list_prepend (shortcut_group->shortcuts,
								    g_strdup (link_node->content));
		}

		shortcut_group->shortcuts = g_list_reverse (shortcut_group->shortcuts);

		priv->groups = g_list_prepend (priv->groups, shortcut_group);
	}

	priv->groups = g_list_reverse (priv->groups);

	xmlFreeDoc (doc);

	return TRUE;
}

static gboolean
save_shortcuts (EShortcuts *shortcuts,
		const char *path)
{
	EShortcutsPrivate *priv;
	xmlDoc *doc;
	xmlNode *root;
	GList *p, *q;

	priv = shortcuts->priv;

	doc = xmlNewDoc ((xmlChar *) "1.0");
	root = xmlNewDocNode (doc, NULL, (xmlChar *) "shortcuts", NULL);
	xmlDocSetRootElement (doc, root);

	for (p = priv->groups; p != NULL; p = p->next) {
		ShortcutGroup *group;
		xmlNode *group_node;

		group = (ShortcutGroup *) p->data;
		group_node = xmlNewChild (root, NULL, (xmlChar *) "group", NULL);

		xmlSetProp (group_node, (xmlChar *) "title", group->title);

		for (q = group->shortcuts; q != NULL; q = q->next) {
			const char *shortcut;

			shortcut = (const char *) q->data;
			xmlNewChild (group_node, NULL, (xmlChar *) "item", (xmlChar *) shortcut);
		}
	}

	if (xmlSaveFile (path, doc) < 0) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	xmlFreeDoc (doc);
	return TRUE;
}


/* View initialization.  */

static const char *
get_storage_set_path_from_uri (const char *uri)
{
	const char *colon;

	if (g_path_is_absolute (uri))
		return NULL;

	colon = strchr (uri, ':');
	if (colon == NULL || colon == uri || colon[1] == '\0')
		return NULL;

	if (! g_path_is_absolute (colon + 1))
		return NULL;

	if (g_strncasecmp (uri, "evolution", colon - uri) != 0)
		return NULL;

	return colon + 1;
}

static void
load_folders_into_view (EShortcuts *shortcuts,
			EShortcutBar *view,
			ShortcutGroup *group,
			int group_num)
{
	EStorageSet *storage_set;
	GList *p;

	storage_set = shortcuts->priv->storage_set;

	for (p = group->shortcuts; p != NULL; p = p->next) {
		EFolder *folder;
		const char *path;
		const char *uri;
		const char *name;

		uri = (const char *) p->data;
		path = get_storage_set_path_from_uri (uri);
		if (path != NULL)
			folder = e_storage_set_get_folder (storage_set, path);

		if (path == NULL || folder == NULL) {
			/* FIXME */
			g_warning ("Invalid link while loading shortcut bar view -- %s\n",
				   uri);
			continue;
		}

		name = e_folder_get_name (folder);
		e_shortcut_bar_add_item (view, group_num, uri, name);
	}
}

static void
load_shortcuts_into_view (EShortcuts *shortcuts,
			  EShortcutBar *view)
{
	EShortcutsPrivate *priv;
	GList *p;
	int group_num;

	priv = shortcuts->priv;

	for (p = priv->groups; p != NULL; p = p->next) {
		ShortcutGroup *group;

		group = (ShortcutGroup *) p->data;
		group_num = e_shortcut_bar_add_group (view, group->title);

		load_folders_into_view (shortcuts, view, group, group_num);
	}
}

/* Icon callback for the shortcut bar.  */
static GdkPixbuf *
icon_callback (EShortcutBar *shortcut_bar,
	       const char *uri,
	       gpointer data)
{
	EFolderTypeRepository *folder_type_repository;
	EShortcuts *shortcuts;
	EStorageSet *storage_set;
	EFolder *folder;
	GdkPixbuf *pixbuf;
	const char *type;

	shortcuts = E_SHORTCUTS (data);

	storage_set = shortcuts->priv->storage_set;
	folder_type_repository = shortcuts->priv->folder_type_repository;

	folder = e_storage_set_get_folder (storage_set,
					   get_storage_set_path_from_uri (uri));

	if (folder == NULL)
		return NULL;

	type = e_folder_get_type_string (folder);
	if (type == NULL)
		return NULL;

	pixbuf = e_folder_type_repository_get_icon_for_type (folder_type_repository, type);
	if (pixbuf != NULL)
		gdk_pixbuf_ref (pixbuf);

	return pixbuf;
}


/* Signal handlers for the views.  */

static void
view_destroyed_cb (GtkObject *object,
		   gpointer data)
{
	EShortcuts *shortcuts;
	EShortcutsPrivate *priv;

	shortcuts = E_SHORTCUTS (data);
	priv = shortcuts->priv;

	priv->views = g_list_remove (priv->views, object);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EShortcuts *shortcuts;
	EShortcutsPrivate *priv;

	shortcuts = E_SHORTCUTS (object);
	priv = shortcuts->priv;

	if (priv->storage_set != NULL)
		gtk_object_unref (GTK_OBJECT (priv->storage_set));

	if (priv->folder_type_repository != NULL)
		gtk_object_unref (GTK_OBJECT (priv->folder_type_repository));

	unload_shortcuts (shortcuts);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EShortcutsClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;
	object_class->destroy = destroy;

	parent_class = gtk_type_class (gtk_object_get_type ());
}


static void
init (EShortcuts *shortcuts)
{
	EShortcutsPrivate *priv;

	priv = g_new (EShortcutsPrivate, 1);
	priv->storage_set = NULL;
	priv->groups = NULL;
	priv->views = NULL;

	shortcuts->priv = priv;
}


void
e_shortcuts_construct (EShortcuts  *shortcuts,
		       EStorageSet *storage_set,
		       EFolderTypeRepository *folder_type_repository)
{
	EShortcutsPrivate *priv;

	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));
	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));

	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (storage_set), GTK_FLOATING);

	priv = shortcuts->priv;

	gtk_object_ref (GTK_OBJECT (storage_set));
	priv->storage_set = storage_set;

	gtk_object_ref (GTK_OBJECT (folder_type_repository));
	priv->folder_type_repository = folder_type_repository;
}

EShortcuts *
e_shortcuts_new (EStorageSet *storage_set,
		 EFolderTypeRepository *folder_type_repository)
{
	EShortcuts *new;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);

	new = gtk_type_new (e_shortcuts_get_type ());
	e_shortcuts_construct (new, storage_set, folder_type_repository);

	return new;
}


GtkWidget *
e_shortcuts_new_view (EShortcuts *shortcuts)
{
	EShortcutsPrivate *priv;
	GtkWidget *new;

	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	priv = shortcuts->priv;

	new = e_shortcuts_view_new (shortcuts);
	priv->views = g_list_prepend (priv->views, new);

	e_shortcut_bar_set_icon_callback (E_SHORTCUT_BAR (new),
					  icon_callback, shortcuts);

	load_shortcuts_into_view (shortcuts, E_SHORTCUT_BAR (new));

	gtk_signal_connect (GTK_OBJECT (new), "destroy", view_destroyed_cb, shortcuts);

	return new;
}


gboolean
e_shortcuts_load (EShortcuts *shortcuts,
		  const char *path)
{
	g_return_val_if_fail (shortcuts != NULL, FALSE);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (g_path_is_absolute (path), FALSE);

	return load_shortcuts (shortcuts, path);
}

gboolean
e_shortcuts_save (EShortcuts *shortcuts,
		  const char *path)
{
	g_return_val_if_fail (shortcuts != NULL, FALSE);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (g_path_is_absolute (path), FALSE);

	return save_shortcuts (shortcuts, path);
}


const char *
e_shortcuts_get_uri (EShortcuts *shortcuts, int group_num, int num)
{
	EShortcutsPrivate *priv;
	ShortcutGroup *group;
	GList *shortcut_element;

	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	priv = shortcuts->priv;

	group = g_list_nth (priv->groups, group_num)->data;
	if (group == NULL)
		return NULL;

	shortcut_element = g_list_nth (group->shortcuts, num);
	if (shortcut_element == NULL)
		return NULL;

	return shortcut_element->data;
}


E_MAKE_TYPE (e_shortcuts, "EShortcuts", EShortcuts, class_init, init, PARENT_TYPE)
