/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shortcuts.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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

/* The shortcut list goes like this:

   <?xml version="1.0"?>
   <shortcuts>
           <group title="Evolution shortcuts">
	           <item name="Inbox" type="mail">evolution:/local/Inbox</item>
	           <item name="Trash" type="vtrash">evolution:/local/Trash</item>
	           <item name="Calendar" type="calendar">evolution:/local/Calendar</item>
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

#include "e-shortcuts.h"

#include <string.h>

#include <glib.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkobject.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktypeutils.h>

#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>
#include <gal/util/e-xml-utils.h>
#include <gal/widgets/e-unicode.h>
#include <gal/shortcut-bar/e-shortcut-bar.h>

#include "e-shortcuts-view.h"

#include "e-shell-constants.h"


#define PARENT_TYPE GTK_TYPE_OBJECT
static GtkObjectClass *parent_class = NULL;

struct _ShortcutGroup {
	/* Title of the group.  */
	char *title;

	/* A list of shortcuts.  */
	GSList *shortcuts;
};
typedef struct _ShortcutGroup ShortcutGroup;

struct _EShortcutsPrivate {
	/* Name of the file associated with these shortcuts.  Changes in the shortcuts
           will update this file automatically.  */
	char *file_name;

	/* ID of the idle function that will be called to save the shortcuts when they are
           changed.  */
	int save_idle_id;

	/* Whether these shortcuts need to be saved to disk.  */
	gboolean dirty;

	/* The storage set to which these shortcuts are associated.  */
	EStorageSet *storage_set;

	/* The folder type registry.  */
	EFolderTypeRegistry *folder_type_registry;

	/* Total number of groups.  */
	int num_groups;

	/* A list of ShortcutGroups.  */
	GSList *groups;

	/* A list of ShortcutViews.  */
	GSList *views;
};

enum {
	NEW_GROUP,
	REMOVE_GROUP,
	RENAME_GROUP,
	NEW_SHORTCUT,
	REMOVE_SHORTCUT,
	UPDATE_SHORTCUT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static void make_dirty (EShortcuts *shortcuts);


static EShortcutItem *
shortcut_item_new (const char *uri,
		   const char *name,
		   int unread_count,
		   const char *type)
{
	EShortcutItem *new;

	if (name == NULL)
		name = g_basename (uri);

	new = g_new (EShortcutItem, 1);
	new->uri  = g_strdup (uri);
	new->name = g_strdup (name);
	new->type = g_strdup (type);
	new->unread_count = unread_count;

	return new;
}

static gboolean
shortcut_item_update (EShortcutItem *shortcut_item,
		      const char *uri,
		      const char *name,
		      int unread_count,
		      const char *type)
{
	gboolean changed = FALSE;

	if (name == NULL)
		name = g_basename (uri);

	if (shortcut_item->uri == NULL || uri == NULL ||
	    strcmp (shortcut_item->uri, uri) != 0) {
		g_free (shortcut_item->uri);
		shortcut_item->uri  = g_strdup (uri);
		changed = TRUE;
	}

	if (shortcut_item->name == NULL || name == NULL ||
	    strcmp (shortcut_item->name, name) != 0) {
		g_free (shortcut_item->name);
		shortcut_item->name  = g_strdup (name);
		changed = TRUE;
	}

	if (shortcut_item->unread_count != unread_count) {
		shortcut_item->unread_count = unread_count;
		changed = FALSE;
	}

	if (shortcut_item->type == NULL || type == NULL ||
	    strcmp (shortcut_item->type, type) != 0) {
		g_free (shortcut_item->type);
		shortcut_item->type  = g_strdup (type);
		changed = TRUE;
	}

	return changed;
}

static void
shortcut_item_free (EShortcutItem *shortcut_item)
{
	g_free (shortcut_item->uri);
	g_free (shortcut_item->name);
	g_free (shortcut_item->type);

	g_free (shortcut_item);
}

static ShortcutGroup *
shortcut_group_new (const char *title)
{
	ShortcutGroup *new;

	new = g_new (ShortcutGroup, 1);
	new->title     = g_strdup (title);
	new->shortcuts = NULL;

	return new;
}

static void
shortcut_group_free (ShortcutGroup *group)
{
	GSList *p;

	g_free (group->title);

	for (p = group->shortcuts; p != NULL; p = p->next)
		shortcut_item_free ((EShortcutItem *) p->data);
	g_slist_free (group->shortcuts);

	g_free (group);
}


/* Utility functions.  */

static gboolean
update_shortcut_and_emit_signal (EShortcuts *shortcuts,
				 EShortcutItem *shortcut_item,
				 int group_num,
				 int num,
				 const char *uri,
				 const char *name,
				 int unread_count,
				 const char *type)
{
	/* Only thing that changed was the unread count */
	if (shortcut_item->unread_count != unread_count
	    && !shortcut_item_update (shortcut_item, uri, name, unread_count, type)) {
		gtk_signal_emit (GTK_OBJECT (shortcuts), signals[UPDATE_SHORTCUT], group_num, num);
		return FALSE;
	}

	/* Unread count is the same, but other stuff changed */
	else if (shortcut_item_update (shortcut_item, uri, name, unread_count, type)) {
		gtk_signal_emit (GTK_OBJECT (shortcuts), signals[UPDATE_SHORTCUT], group_num, num);
		return TRUE;
	}

	/* Nothing at all changed, return false only */
	else
		return FALSE;
}

static void
unload_shortcuts (EShortcuts *shortcuts)
{
	EShortcutsPrivate *priv;
	GSList *orig_groups;
	GSList *p;

	priv = shortcuts->priv;
	orig_groups = priv->groups;

	for (p = priv->groups; p != NULL; p = p->next) {
		ShortcutGroup *group;

		gtk_signal_emit (GTK_OBJECT (shortcuts), signals[REMOVE_GROUP], 0);

		group = (ShortcutGroup *) p->data;

		shortcut_group_free (group);

		priv->groups = priv->groups->next;
	}

	if (orig_groups != NULL)
		g_slist_free (orig_groups);

	priv->groups = NULL;
}

static gboolean
load_shortcuts (EShortcuts *shortcuts,
		const char *file_name)
{
	EShortcutsPrivate *priv;
	xmlDoc *doc;
	xmlNode *root;
	xmlNode *p, *q;

	/* FIXME: Update the views by emitting the appropriate signals.  */

	priv = shortcuts->priv;

	doc = xmlParseFile (file_name);
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
		xmlChar *shortcut_group_title;

		if (strcmp ((char *) p->name, "group") != 0)
			continue;

		shortcut_group_title = xmlGetProp (p, "title");
		if (shortcut_group_title == NULL)
			continue;

		shortcut_group = shortcut_group_new (shortcut_group_title);
		xmlFree (shortcut_group_title);

		for (q = p->childs; q != NULL; q = q->next) {
			EShortcutItem *shortcut_item;
			xmlChar *uri;
			xmlChar *name;
			xmlChar *type;

			if (strcmp ((char *) q->name, "item") != 0)
				continue;

			uri  = xmlNodeListGetString (doc, q->childs, 1);
			if (uri == NULL)
				continue;

			if (strncmp (uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) == 0) {
				EFolder *folder;

				folder = e_storage_set_get_folder (priv->storage_set, uri + E_SHELL_URI_PREFIX_LEN);
				if (folder != NULL) {
					name = xmlMemStrdup (e_folder_get_name (folder));
					type = xmlMemStrdup (e_folder_get_type_string (folder));
				} else {
					name = xmlGetProp (q, "name");
					type = xmlGetProp (q, "type");
				}

				shortcut_item = shortcut_item_new (uri, name, 0, type);
				shortcut_group->shortcuts = g_slist_prepend (shortcut_group->shortcuts,
									     shortcut_item);

				if (name != NULL)
					xmlFree (name);
				if (type != NULL)
					xmlFree (type);
			}

			xmlFree (uri);
		}

		shortcut_group->shortcuts = g_slist_reverse (shortcut_group->shortcuts);

		priv->groups = g_slist_prepend (priv->groups, shortcut_group);
		priv->num_groups ++;
	}

	priv->groups = g_slist_reverse (priv->groups);

	xmlFreeDoc (doc);

	/* After loading, we always have to re-save ourselves as we have merged
	   the information we have with the information we got from the
	   StorageSet.  */
	/* FIXME: Obviously, this sucks.  */
	make_dirty (shortcuts);

	return TRUE;
}

static gboolean
save_shortcuts (EShortcuts *shortcuts,
		const char *file_name)
{
	EShortcutsPrivate *priv;
	xmlDoc *doc;
	xmlNode *root;
	GSList *p, *q;

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
			EShortcutItem *shortcut;
			xmlNode *shortcut_node;

			shortcut = (EShortcutItem *) q->data;
			shortcut_node = xmlNewTextChild (group_node, NULL, (xmlChar *) "item",
							 (xmlChar *) shortcut->uri);

			if (shortcut->name != NULL)
				xmlSetProp (shortcut_node, (xmlChar *) "name", shortcut->name);

			if (shortcut->type != NULL)
				xmlSetProp (shortcut_node, (xmlChar *) "type", shortcut->type);

		}
	}

	if (xmlSaveFile (file_name, doc) < 0) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	xmlFreeDoc (doc);
	return TRUE;
}


/* Idle function to update the file on disk.  */

static int
idle_cb (void *data)
{
	EShortcuts *shortcuts;
	EShortcutsPrivate *priv;

	shortcuts = E_SHORTCUTS (data);
	priv = shortcuts->priv;

	if (priv->dirty) {
		if (! e_shortcuts_save (shortcuts, NULL))
			g_warning ("Saving of shortcuts failed -- %s", priv->file_name);
		else
			priv->dirty = FALSE;
	}

	priv->save_idle_id = 0;

	return FALSE;
}

static void
schedule_idle (EShortcuts *shortcuts)
{
	EShortcutsPrivate *priv;

	priv = shortcuts->priv;

	if (priv->save_idle_id != 0)
		return;

	priv->save_idle_id = gtk_idle_add (idle_cb, shortcuts);
}

static void
make_dirty (EShortcuts *shortcuts)
{
	EShortcutsPrivate *priv;

	priv = shortcuts->priv;

	priv->dirty = TRUE;
	schedule_idle (shortcuts);
}

static void
update_shortcuts_by_path (EShortcuts *shortcuts,
			  const char *path)
{
	EShortcutsPrivate *priv;
	EFolder *folder;
	const GSList *p, *q;
	char *evolution_uri;
	int group_num, num;
	gboolean changed = FALSE;

	priv = shortcuts->priv;
	folder = e_storage_set_get_folder (priv->storage_set, path);

	evolution_uri = g_strconcat (E_SHELL_URI_PREFIX, path, NULL);

	group_num = 0;
	for (p = priv->groups; p != NULL; p = p->next, group_num++) {
		ShortcutGroup *group;

		group = (ShortcutGroup *) p->data;
		num = 0;
		for (q = group->shortcuts; q != NULL; q = q->next, num++) {
			EShortcutItem *shortcut_item;

			shortcut_item = (EShortcutItem *) q->data;

			if (strcmp (shortcut_item->uri, evolution_uri) == 0) {
				changed = update_shortcut_and_emit_signal (shortcuts,
									   shortcut_item,
									   group_num,
									   num,
									   evolution_uri,
									   shortcut_item->name,
									   e_folder_get_unread_count (folder),
									   e_folder_get_type_string (folder));
			}
		}
	}

	g_free (evolution_uri);

	if (changed)
		make_dirty (shortcuts);
}


static EShortcutItem *
get_item (EShortcuts *shortcuts,
	  int group_num,
	  int num)
{
	EShortcutsPrivate *priv;
	ShortcutGroup *group;
	GSList *group_element;
	GSList *shortcut_element;

	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	priv = shortcuts->priv;

	group_element = g_slist_nth (priv->groups, group_num);
	if (group_element == NULL)
		return NULL;

	group = (ShortcutGroup *) group_element->data;

	shortcut_element = g_slist_nth (group->shortcuts, num);
	if (shortcut_element == NULL)
		return NULL;

	return (EShortcutItem *) shortcut_element->data;
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

	priv->views = g_slist_remove (priv->views, object);
}


/* Signal handlers for the EStorageSet.  */

static void
storage_set_new_folder_callback (EStorageSet *storage_set,
				 const char *path,
				 void *data)
{
	EShortcuts *shortcuts;

	shortcuts = E_SHORTCUTS (data);

	update_shortcuts_by_path (shortcuts, path);
}

static void
storage_set_updated_folder_callback (EStorageSet *storage_set,
				     const char *path,
				     void *data)
{
	EShortcuts *shortcuts;

	shortcuts = E_SHORTCUTS (data);

	update_shortcuts_by_path (shortcuts, path);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EShortcuts *shortcuts;
	EShortcutsPrivate *priv;

	shortcuts = E_SHORTCUTS (object);
	priv = shortcuts->priv;

	g_free (priv->file_name);

	if (priv->storage_set != NULL)
		gtk_object_unref (GTK_OBJECT (priv->storage_set));

	if (priv->folder_type_registry != NULL)
		gtk_object_unref (GTK_OBJECT (priv->folder_type_registry));

	unload_shortcuts (shortcuts);

	if (priv->save_idle_id != 0)
		gtk_idle_remove (priv->save_idle_id);

	if (priv->dirty) {
		if (! e_shortcuts_save (shortcuts, NULL))
			g_warning (_("Error saving shortcuts.")); /* FIXME */
	}

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EShortcutsClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;
	object_class->destroy = destroy;

	parent_class = gtk_type_class (gtk_object_get_type ());

	signals[NEW_GROUP]
		= gtk_signal_new ("new_group",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EShortcutsClass, new_group),
				  gtk_marshal_NONE__INT,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_INT);

	signals[REMOVE_GROUP]
		= gtk_signal_new ("remove_group",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EShortcutsClass, remove_group),
				  gtk_marshal_NONE__INT,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_INT);

	signals[RENAME_GROUP]
		= gtk_signal_new ("rename_group",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EShortcutsClass, rename_group),
				  gtk_marshal_NONE__INT_POINTER,
				  GTK_TYPE_NONE, 2,
				  GTK_TYPE_INT,
				  GTK_TYPE_STRING);

	signals[NEW_SHORTCUT]
		= gtk_signal_new ("new_shortcut",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EShortcutsClass, new_shortcut),
				  gtk_marshal_NONE__INT_INT,
				  GTK_TYPE_NONE, 2,
				  GTK_TYPE_INT,
				  GTK_TYPE_INT);

	signals[REMOVE_SHORTCUT]
		= gtk_signal_new ("remove_shortcut",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EShortcutsClass, remove_shortcut),
				  gtk_marshal_NONE__INT_INT,
				  GTK_TYPE_NONE, 2,
				  GTK_TYPE_INT,
				  GTK_TYPE_INT);

	signals[UPDATE_SHORTCUT]
		= gtk_signal_new ("update_shortcut",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EShortcutsClass, update_shortcut),
				  gtk_marshal_NONE__INT_INT,
				  GTK_TYPE_NONE, 2,
				  GTK_TYPE_INT,
				  GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}


static void
init (EShortcuts *shortcuts)
{
	EShortcutsPrivate *priv;

	priv = g_new (EShortcutsPrivate, 1);

	priv->file_name      = NULL;
	priv->storage_set    = NULL;
	priv->num_groups     = 0;
	priv->groups         = NULL;
	priv->views          = NULL;
	priv->dirty          = 0;
	priv->save_idle_id   = 0;

	shortcuts->priv = priv;
}


void
e_shortcuts_construct (EShortcuts  *shortcuts,
		       EStorageSet *storage_set,
		       EFolderTypeRegistry *folder_type_registry)
{
	EShortcutsPrivate *priv;

	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));
	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));

	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (shortcuts), GTK_FLOATING);

	priv = shortcuts->priv;

	gtk_object_ref (GTK_OBJECT (storage_set));
	priv->storage_set = storage_set;

	gtk_signal_connect_while_alive (GTK_OBJECT (storage_set), "new_folder",
					GTK_SIGNAL_FUNC (storage_set_new_folder_callback),
					shortcuts, GTK_OBJECT (shortcuts));
	gtk_signal_connect_while_alive (GTK_OBJECT (storage_set), "updated_folder",
					GTK_SIGNAL_FUNC (storage_set_updated_folder_callback),
					shortcuts, GTK_OBJECT (shortcuts));

	gtk_object_ref (GTK_OBJECT (folder_type_registry));
	priv->folder_type_registry = folder_type_registry;
}

EShortcuts *
e_shortcuts_new (EStorageSet *storage_set,
		 EFolderTypeRegistry *folder_type_registry,
		 const char *file_name)
{
	EShortcuts *new;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);

	new = gtk_type_new (e_shortcuts_get_type ());
	e_shortcuts_construct (new, storage_set, folder_type_registry);

	if (! e_shortcuts_load (new, file_name))
		new->priv->file_name = g_strdup (file_name);

	return new;
}


int
e_shortcuts_get_num_groups (EShortcuts *shortcuts)
{
	g_return_val_if_fail (shortcuts != NULL, 0);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), 0);

	return shortcuts->priv->num_groups;
}

GSList *
e_shortcuts_get_group_titles (EShortcuts *shortcuts)
{
	EShortcutsPrivate *priv;
	ShortcutGroup *group;
	GSList *list;
	GSList *p;

	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	priv = shortcuts->priv;

	list = NULL;

	for (p = priv->groups; p != NULL; p = p->next) {
		group = (ShortcutGroup *) p->data;
		list = g_slist_prepend (list, g_strdup (group->title));
	}

	return g_slist_reverse (list);
}

const GSList *
e_shortcuts_get_shortcuts_in_group (EShortcuts *shortcuts,
				    int group_num)
{
	EShortcutsPrivate *priv;
	ShortcutGroup *shortcut_group;
	GSList *shortcut_group_list_item;

	priv = shortcuts->priv;

	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	shortcut_group_list_item = g_slist_nth (priv->groups, group_num);
	if (shortcut_group_list_item == NULL)
		return NULL;

	shortcut_group = (ShortcutGroup *) shortcut_group_list_item->data;

	return shortcut_group->shortcuts;
}


EStorageSet *
e_shortcuts_get_storage_set (EShortcuts *shortcuts)
{
	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	return shortcuts->priv->storage_set;
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
	priv->views = g_slist_prepend (priv->views, new);

	gtk_signal_connect (GTK_OBJECT (new), "destroy", view_destroyed_cb, shortcuts);

	return new;
}


gboolean
e_shortcuts_load (EShortcuts *shortcuts,
		  const char *file_name)
{
	EShortcutsPrivate *priv;
	char *tmp;

	g_return_val_if_fail (shortcuts != NULL, FALSE);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), FALSE);
	g_return_val_if_fail (file_name == NULL || g_path_is_absolute (file_name), FALSE);

	priv = shortcuts->priv;

	if (file_name == NULL) {
		if (priv->file_name == NULL)
			return FALSE;
		file_name = priv->file_name;
	}

	if (! load_shortcuts (shortcuts, file_name))
		return FALSE;

	tmp = g_strdup (file_name);
	g_free (priv->file_name);
	priv->file_name = tmp;

	return TRUE;
}

gboolean
e_shortcuts_save (EShortcuts *shortcuts,
		  const char *file_name)
{
	EShortcutsPrivate *priv;
	char *tmp;

	g_return_val_if_fail (shortcuts != NULL, FALSE);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), FALSE);
	g_return_val_if_fail (file_name == NULL || g_path_is_absolute (file_name), FALSE);

	priv = shortcuts->priv;

	if (file_name == NULL) {
		if (priv->file_name == NULL)
			return FALSE;
		file_name = priv->file_name;
	}

	if (! save_shortcuts (shortcuts, file_name))
		return FALSE;

	tmp = g_strdup (file_name);
	g_free (priv->file_name);
	priv->file_name = tmp;

	return TRUE;
}


const EShortcutItem *
e_shortcuts_get_shortcut (EShortcuts *shortcuts,
			  int group_num,
			  int num)
{
	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	return (const EShortcutItem *) get_item (shortcuts, group_num, num);
}


void
e_shortcuts_remove_shortcut (EShortcuts *shortcuts,
			     int group_num,
			     int num)
{
	EShortcutsPrivate *priv;
	ShortcutGroup *group;
	GSList *p;
	EShortcutItem *item;

	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));

	priv = shortcuts->priv;

	p = g_slist_nth (priv->groups, group_num);
	g_return_if_fail (p != NULL);

	group = (ShortcutGroup *) p->data;

	p = g_slist_nth (group->shortcuts, num);
	g_return_if_fail (p != NULL);

	gtk_signal_emit (GTK_OBJECT (shortcuts), signals[REMOVE_SHORTCUT], group_num, num);

	item = (EShortcutItem *) p->data;
	shortcut_item_free (item);

	group->shortcuts = g_slist_remove_link (group->shortcuts, p);

	make_dirty (shortcuts);
}

void
e_shortcuts_add_shortcut (EShortcuts *shortcuts,
			  int group_num,
			  int num,
			  const char *uri,
			  const char *name,
			  int unread_count,
			  const char *type)
{
	EShortcutsPrivate *priv;
	ShortcutGroup *group;
	EShortcutItem *item;
	GSList *p;

	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));

	priv = shortcuts->priv;

	p = g_slist_nth (priv->groups, group_num);
	g_return_if_fail (p != NULL);

	group = (ShortcutGroup *) p->data;

	if (num == -1)
		num = g_slist_length (group->shortcuts);

	item = shortcut_item_new (uri, name, unread_count, type);

	group->shortcuts = g_slist_insert (group->shortcuts, item, num);

	gtk_signal_emit (GTK_OBJECT (shortcuts), signals[NEW_SHORTCUT], group_num, num);

	make_dirty (shortcuts);
}

void
e_shortcuts_update_shortcut (EShortcuts *shortcuts,
			     int         group_num,
			     int         num,
			     const char *uri,
			     const char *name,
			     int unread_count,
			     const char *type)
{
	EShortcutItem *shortcut_item;

	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));

	shortcut_item = get_item (shortcuts, group_num, num);

	update_shortcut_and_emit_signal (shortcuts, shortcut_item, group_num, num, uri, name, unread_count, type);

	make_dirty (shortcuts);
}


void
e_shortcuts_add_default_group (EShortcuts *shortcuts)
{
	char *utf;

	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));

	utf = e_utf8_from_locale_string (_("Shortcuts"));
	e_shortcuts_add_group (shortcuts, -1, utf);
	g_free (utf);

	/* FIXME: Inbox shortcut should point to something else for
           people who won't care about using /Local Folders/Inbox */
	utf = e_utf8_from_locale_string (_("Summary"));
	e_shortcuts_add_shortcut (shortcuts, 0, -1, E_SUMMARY_URI, utf, 0, "summary");
	g_free (utf);
	utf = e_utf8_from_locale_string (_("Inbox"));
	e_shortcuts_add_shortcut (shortcuts, 0, -1, E_LOCAL_INBOX_URI, utf, 0, "mail");
	g_free (utf);
	utf = e_utf8_from_locale_string (_("Calendar"));
	e_shortcuts_add_shortcut (shortcuts, 0, -1, E_LOCAL_CALENDAR_URI, utf, 0, "calendar");
	g_free (utf);
	utf = e_utf8_from_locale_string (_("Tasks"));
	e_shortcuts_add_shortcut (shortcuts, 0, -1, E_LOCAL_TASKS_URI, utf, 0, "tasks");
	g_free (utf);
	utf = e_utf8_from_locale_string (_("Contacts"));
	e_shortcuts_add_shortcut (shortcuts, 0, -1, E_LOCAL_CONTACTS_URI, utf, 0, "contacts");
	g_free (utf);
}

void
e_shortcuts_remove_group (EShortcuts *shortcuts,
			  int group_num)
{
	EShortcutsPrivate *priv;
	GSList *p;

	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));

	priv = shortcuts->priv;

	p = g_slist_nth (priv->groups, group_num);
	g_return_if_fail (p != NULL);

	gtk_signal_emit (GTK_OBJECT (shortcuts), signals[REMOVE_GROUP], group_num);

	shortcut_group_free ((ShortcutGroup *) p->data);

	priv->groups = g_slist_remove_link (priv->groups, p);
	priv->num_groups --;

	make_dirty (shortcuts);
}

void
e_shortcuts_rename_group (EShortcuts *shortcuts,
			  int group_num,
			  const char *new_title)
{
	EShortcutsPrivate *priv;
	GSList *p;
	ShortcutGroup *group;

	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));

	priv = shortcuts->priv;

	p = g_slist_nth (priv->groups, group_num);
	g_return_if_fail (p != NULL);

	group = (ShortcutGroup *) p->data;
	if (strcmp (group->title, new_title)) {
		g_free (group->title);
		group->title = g_strdup (new_title);
	} else
		return;

	gtk_signal_emit (GTK_OBJECT (shortcuts), signals[RENAME_GROUP], group_num, new_title);

	make_dirty (shortcuts);
}

void
e_shortcuts_add_group (EShortcuts *shortcuts,
		       int group_num,
		       const char *group_name)
{
	EShortcutsPrivate *priv;
	ShortcutGroup *group;

	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));

	priv = shortcuts->priv;

	group = shortcut_group_new (group_name);

	if (group_num == -1)
		group_num = g_slist_length (priv->groups);

	priv->groups = g_slist_insert (priv->groups, group, group_num);
	priv->num_groups ++;

	gtk_signal_emit (GTK_OBJECT (shortcuts), signals[NEW_GROUP], group_num);

	make_dirty (shortcuts);
}


const char *
e_shortcuts_get_group_title (EShortcuts *shortcuts,
			     int group_num)
{
	EShortcutsPrivate *priv;
	GSList *group_element;
	const ShortcutGroup *group;

	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	priv = shortcuts->priv;

	group_element = g_slist_nth (priv->groups, group_num);
	if (group_element == NULL)
		return NULL;

	group = (ShortcutGroup *) group_element->data;

	return group->title;
}


E_MAKE_TYPE (e_shortcuts, "EShortcuts", EShortcuts, class_init, init, PARENT_TYPE)
