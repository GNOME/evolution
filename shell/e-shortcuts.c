/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shortcuts.c
 *
 * Copyright (C) 2000, 2001, 2002 Ximian, Inc.
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

	/* Whether to use small icons for this group.  */
	unsigned int use_small_icons : 1;
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

	/* The shell that is associated with these shortcuts.  */
	EShell *shell;

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
	GROUP_CHANGE_ICON_SIZE,
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
		   const char *type,
		   const char *custom_icon_name)
{
	EShortcutItem *new;

	if (name == NULL)
		name = g_basename (uri);

	new = g_new (EShortcutItem, 1);
	new->uri              = g_strdup (uri);
	new->name             = g_strdup (name);
	new->type             = g_strdup (type);
	new->custom_icon_name = g_strdup (custom_icon_name);
	new->unread_count     = unread_count;

	return new;
}

static gboolean
shortcut_item_update (EShortcutItem *shortcut_item,
		      const char *uri,
		      const char *name,
		      int unread_count,
		      const char *type,
		      const char *custom_icon_name)
{
	gboolean changed = FALSE;

	if (name == NULL)
		name = g_basename (uri);

	if (shortcut_item->unread_count != unread_count) {
		shortcut_item->unread_count = unread_count;
		changed = TRUE;
	}

#define UPDATE_STRING(member)					\
	if (shortcut_item->member == NULL || member == NULL ||	\
	    strcmp (shortcut_item->member, member) != 0) {	\
		g_free (shortcut_item->member);			\
		shortcut_item->member  = g_strdup (member);	\
		changed = TRUE;					\
	}

	UPDATE_STRING (uri);
	UPDATE_STRING (name);
	UPDATE_STRING (type);
	UPDATE_STRING (custom_icon_name);

#undef UPDATE_STRING

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
	new->title           = g_strdup (title);
	new->shortcuts       = NULL;
	new->use_small_icons = FALSE;

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
				 const char *type,
				 const char *custom_icon_name)
{
	gboolean shortcut_changed;

	shortcut_changed = shortcut_item_update (shortcut_item, uri, name, unread_count, type, custom_icon_name);
	if (shortcut_changed) {
		gtk_signal_emit (GTK_OBJECT (shortcuts), signals[UPDATE_SHORTCUT], group_num, num);
		return TRUE;
	}

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
		xmlChar *icon_size;

		if (strcmp ((char *) p->name, "group") != 0)
			continue;

		shortcut_group_title = xmlGetProp (p, "title");
		if (shortcut_group_title == NULL)
			continue;

		shortcut_group = shortcut_group_new (shortcut_group_title);
		xmlFree (shortcut_group_title);

		icon_size = xmlGetProp (p, "icon_size");
		if (icon_size != NULL && strcmp (icon_size, "small") == 0)
			shortcut_group->use_small_icons = TRUE;
		else
			shortcut_group->use_small_icons = FALSE;
		xmlFree (icon_size);

		for (q = p->childs; q != NULL; q = q->next) {
			EShortcutItem *shortcut_item;
			xmlChar *uri;
			xmlChar *name;
			xmlChar *type;
			xmlChar *icon;
			char *path;

			if (strcmp ((char *) q->name, "item") != 0)
				continue;

			uri  = xmlNodeListGetString (doc, q->childs, 1);
			if (uri == NULL)
				continue;

			if (e_shell_parse_uri (priv->shell, uri, &path, NULL)) {
				EFolder *folder;

				folder = e_storage_set_get_folder (e_shell_get_storage_set (priv->shell), path);
				if (folder != NULL) {
					name = xmlMemStrdup (e_folder_get_name (folder));
					type = xmlMemStrdup (e_folder_get_type_string (folder));

					if (e_folder_get_custom_icon_name (folder) != NULL)
						icon = xmlMemStrdup (e_folder_get_custom_icon_name (folder));
					else
						icon = NULL;
				} else {
					name = xmlGetProp (q, "name");
					type = xmlGetProp (q, "type");
					icon = xmlGetProp (q, "icon");
				}

				shortcut_item = shortcut_item_new (uri, name, 0, type, icon);
				shortcut_group->shortcuts = g_slist_prepend (shortcut_group->shortcuts,
									     shortcut_item);

				if (name != NULL)
					xmlFree (name);
				if (type != NULL)
					xmlFree (type);
				if (icon != NULL)
					xmlFree (icon);
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

		if (group->use_small_icons)
			xmlSetProp (group_node, (xmlChar *) "icon_size", "small");
		else
			xmlSetProp (group_node, (xmlChar *) "icon_size", "large");

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

			if (shortcut->custom_icon_name != NULL)
				xmlSetProp (shortcut_node, (xmlChar *) "icon", shortcut->custom_icon_name);
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
	int group_num, num;
	gboolean changed = FALSE;

	priv = shortcuts->priv;
	folder = e_storage_set_get_folder (e_shell_get_storage_set (priv->shell), path);

	group_num = 0;
	for (p = priv->groups; p != NULL; p = p->next, group_num++) {
		ShortcutGroup *group;

		group = (ShortcutGroup *) p->data;
		num = 0;
		for (q = group->shortcuts; q != NULL; q = q->next, num++) {
			EShortcutItem *shortcut_item;
			char *shortcut_path;

			shortcut_item = (EShortcutItem *) q->data;

			if (! e_shell_parse_uri (priv->shell, shortcut_item->uri, &shortcut_path, NULL)) {
				/* Ignore bogus URIs.  */
				continue;
			}

			if (strcmp (shortcut_path, path) == 0) {
				changed = update_shortcut_and_emit_signal (shortcuts,
									   shortcut_item,
									   group_num,
									   num,
									   shortcut_item->uri,
									   shortcut_item->name,
									   e_folder_get_unread_count (folder),
									   e_folder_get_type_string (folder),
									   e_folder_get_custom_icon_name (folder));
			}

			g_free (shortcut_path);
		}
	}

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
impl_destroy (GtkObject *object)
{
	EShortcuts *shortcuts;
	EShortcutsPrivate *priv;

	shortcuts = E_SHORTCUTS (object);
	priv = shortcuts->priv;

	g_free (priv->file_name);

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
	object_class->destroy = impl_destroy;

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

	signals[GROUP_CHANGE_ICON_SIZE]
		= gtk_signal_new ("group_change_icon_size",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EShortcutsClass, group_change_icon_size),
				  gtk_marshal_NONE__INT_INT,
				  GTK_TYPE_NONE, 2,
				  GTK_TYPE_INT,
				  GTK_TYPE_INT);

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
	priv->num_groups     = 0;
	priv->groups         = NULL;
	priv->views          = NULL;
	priv->dirty          = 0;
	priv->save_idle_id   = 0;
	priv->shell          = NULL;

	shortcuts->priv = priv;
}


void
e_shortcuts_construct (EShortcuts *shortcuts,
		       EShell *shell)
{
	EShortcutsPrivate *priv;
	EStorageSet *storage_set;

	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));
	g_return_if_fail (E_IS_SHELL (shell));

	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (shortcuts), GTK_FLOATING);

	priv = shortcuts->priv;

	/* Don't ref it so we don't create a circular dependency.  */
	priv->shell = shell;

	storage_set = e_shell_get_storage_set (shell);

	gtk_signal_connect_while_alive (GTK_OBJECT (storage_set), "new_folder",
					GTK_SIGNAL_FUNC (storage_set_new_folder_callback),
					shortcuts, GTK_OBJECT (shortcuts));
	gtk_signal_connect_while_alive (GTK_OBJECT (storage_set), "updated_folder",
					GTK_SIGNAL_FUNC (storage_set_updated_folder_callback),
					shortcuts, GTK_OBJECT (shortcuts));
}

EShortcuts *
e_shortcuts_new_from_file (EShell *shell,
			   const char *file_name)
{
	EShortcuts *new;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (file_name != NULL, NULL);

	new = gtk_type_new (e_shortcuts_get_type ());
	e_shortcuts_construct (new, shell);

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


EShell *
e_shortcuts_get_shell (EShortcuts *shortcuts)
{
	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	return shortcuts->priv->shell;
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
			  const char *type,
			  const char *custom_icon_name)
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

	item = shortcut_item_new (uri, name, unread_count, type, custom_icon_name);

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
			     const char *type,
			     const char *custom_icon_name)
{
	EShortcutItem *shortcut_item;

	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));

	shortcut_item = get_item (shortcuts, group_num, num);

	update_shortcut_and_emit_signal (shortcuts, shortcut_item, group_num, num,
					 uri, name, unread_count, type, custom_icon_name);

	make_dirty (shortcuts);
}


void
e_shortcuts_add_default_shortcuts (EShortcuts *shortcuts,
				   int group_num)
{
	char *utf;

	utf = e_utf8_from_locale_string (_("Summary"));
	e_shortcuts_add_shortcut (shortcuts, 0, -1, E_SUMMARY_URI, utf, 0, "summary", NULL);
	g_free (utf);

	utf = e_utf8_from_locale_string (_("Inbox"));
	e_shortcuts_add_shortcut (shortcuts, 0, -1, "default:mail", utf, 0, "mail", "inbox");
	g_free (utf);

	utf = e_utf8_from_locale_string (_("Calendar"));
	e_shortcuts_add_shortcut (shortcuts, 0, -1, "default:calendar", utf, 0, "calendar", NULL);
	g_free (utf);

	utf = e_utf8_from_locale_string (_("Tasks"));
	e_shortcuts_add_shortcut (shortcuts, 0, -1, "default:tasks", utf, 0, "tasks", NULL);
	g_free (utf);

	utf = e_utf8_from_locale_string (_("Contacts"));
	e_shortcuts_add_shortcut (shortcuts, 0, -1, "default:contacts", utf, 0, "contacts", NULL);
	g_free (utf);
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

	e_shortcuts_add_default_shortcuts (shortcuts, -1);
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

void
e_shortcuts_set_group_uses_small_icons  (EShortcuts *shortcuts,
					 int group_num,
					 gboolean use_small_icons)
{
	EShortcutsPrivate *priv;
	ShortcutGroup *group;
	GSList *group_element;

	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));

	priv = shortcuts->priv;

	group_element = g_slist_nth (priv->groups, group_num);
	if (group_element == NULL)
		return;

	group = (ShortcutGroup *) group_element->data;

	use_small_icons = !! use_small_icons;
	if (group->use_small_icons != use_small_icons) {
		group->use_small_icons = use_small_icons;
		gtk_signal_emit (GTK_OBJECT (shortcuts), signals[GROUP_CHANGE_ICON_SIZE],
				 group_num, use_small_icons);

		make_dirty (shortcuts);
	}
}

gboolean
e_shortcuts_get_group_uses_small_icons  (EShortcuts *shortcuts,
					 int group_num)
{
	EShortcutsPrivate *priv;
	ShortcutGroup *group;
	GSList *group_element;

	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), FALSE);

	priv = shortcuts->priv;

	group_element = g_slist_nth (priv->groups, group_num);
	if (group_element == NULL)
		return FALSE;

	group = (ShortcutGroup *) group_element->data;
	return group->use_small_icons;
}


void
e_shortcuts_update_shortcuts_for_changed_uri (EShortcuts *shortcuts,
					      const char *old_uri,
					      const char *new_uri)
{
	EShortcutsPrivate *priv;
	GSList *p;

	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));
	g_return_if_fail (old_uri != NULL);
	g_return_if_fail (new_uri != NULL);

	priv = shortcuts->priv;

	for (p = priv->groups; p != NULL; p = p->next) {
		ShortcutGroup *group;
		GSList *q;

		group = (ShortcutGroup *) p->data;
		for (q = group->shortcuts; q != NULL; q = q->next) {
			EShortcutItem *item;

			item = (EShortcutItem *) q->data;

			if (strcmp (item->uri, old_uri) == 0) {
				g_free (item->uri);
				item->uri = g_strdup (new_uri);

				make_dirty (shortcuts);
			}
		}
	}
}


E_MAKE_TYPE (e_shortcuts, "EShortcuts", EShortcuts, class_init, init, PARENT_TYPE)
