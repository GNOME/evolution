/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-source-list.c
 *
 * Copyright (C) 2003  Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#include <config.h>

#include "e-source-list.h"

#include "e-util-marshal.h"

#include <string.h>
#include <gal/util/e-util.h>


#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;

struct _ESourceListPrivate {
	GConfClient *gconf_client;
	char *gconf_path;

	int gconf_notify_id;

	GSList *groups;

	gboolean ignore_group_changed;
	int sync_idle_id;
};


/* Signals.  */

enum {
	CHANGED,
	GROUP_REMOVED,
	GROUP_ADDED,
	LAST_SIGNAL
};
static unsigned int signals[LAST_SIGNAL] = { 0 };


/* Forward declarations.  */

static gboolean  sync_idle_callback      (ESourceList  *list);
static void      group_changed_callback  (ESourceGroup *group,
					  ESourceList  *list);
static void      conf_changed_callback   (GConfClient  *client,
					  unsigned int  connection_id,
					  GConfEntry   *entry,
					  ESourceList  *list);


/* Utility functions.  */

static void
load_from_gconf (ESourceList *list)
{
	GSList *conf_list, *p, *q;
	GSList *new_groups_list;
	GHashTable *new_groups_hash;
	gboolean changed = FALSE;
	int pos;

	conf_list = gconf_client_get_list (list->priv->gconf_client,
					   list->priv->gconf_path,
					   GCONF_VALUE_STRING, NULL);

	new_groups_list = NULL;
	new_groups_hash = g_hash_table_new (g_direct_hash, g_direct_equal);

	for (p = conf_list, pos = 0; p != NULL; p = p->next, pos++) {
		const char *xml = p->data;
		xmlDocPtr xmldoc = xmlParseDoc ((char *) xml);
		char *group_uid = e_source_group_uid_from_xmldoc (xmldoc);
		ESourceGroup *existing_group;

		if (group_uid == NULL)
			continue;

		existing_group = e_source_list_peek_group_by_uid (list, group_uid);
		if (g_hash_table_lookup (new_groups_hash, existing_group) != NULL)
			continue;

		if (existing_group == NULL) {
			ESourceGroup *new_group = e_source_group_new_from_xmldoc (xmldoc);

			if (new_group != NULL) {
				g_signal_connect (new_group, "changed", G_CALLBACK (group_changed_callback), list);
				new_groups_list = g_slist_prepend (new_groups_list, new_group);

				g_hash_table_insert (new_groups_hash, new_group, new_group);
				g_signal_emit (list, signals[GROUP_ADDED], 0, new_group);
				changed = TRUE;
			}
		} else {
			gboolean group_changed;

			list->priv->ignore_group_changed ++;

			if (e_source_group_update_from_xmldoc (existing_group, xmldoc, &group_changed)) {
				new_groups_list = g_slist_prepend (new_groups_list, existing_group);
				g_object_ref (existing_group);
				g_hash_table_insert (new_groups_hash, existing_group, existing_group);

				if (group_changed)
					changed = TRUE;
			}

			list->priv->ignore_group_changed --;
		}

		g_free (group_uid);
	}

	new_groups_list = g_slist_reverse (new_groups_list);

	g_slist_foreach (conf_list, (GFunc) g_free, NULL);
	g_slist_free (conf_list);

	/* Emit "group_removed" and disconnect the "changed" signal for all the
	   groups that we haven't found in the new list.  Also, check if the
	   order has changed.  */
	q = new_groups_list;
	for (p = list->priv->groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);

		if (g_hash_table_lookup (new_groups_hash, group) == NULL) {
			changed = TRUE;
			g_signal_emit (list, signals[GROUP_REMOVED], 0, group);
			g_signal_handlers_disconnect_by_func (group, group_changed_callback, list);
		}

		if (! changed && q != NULL) {
			if (q->data != p->data)
				changed = TRUE;
			q = q->next;
		}
	}

	g_hash_table_destroy (new_groups_hash);

	/* Replace the original group list with the new one.  */

	g_slist_foreach (list->priv->groups, (GFunc) g_object_unref, NULL);
	g_slist_free (list->priv->groups);

	list->priv->groups = new_groups_list;

	/* FIXME if the order changes, the function doesn't notice.  */

	if (changed)
		g_signal_emit (list, signals[CHANGED], 0);
}

static void
remove_group (ESourceList *list,
	      ESourceGroup *group)
{
	list->priv->groups = g_slist_remove (list->priv->groups, group);

	g_signal_emit (list, signals[GROUP_REMOVED], 0, group);
	g_object_unref (group);

	g_signal_emit (list, signals[CHANGED], 0);
}


/* Callbacks.  */

static gboolean
sync_idle_callback (ESourceList *list)
{
	GError *error = NULL;

	if (! e_source_list_sync (list, &error)) {
		g_warning ("Cannot update \"%s\": %s", list->priv->gconf_path, error->message);
		g_error_free (error);
	}

	return FALSE;
}

static void
group_changed_callback (ESourceGroup *group,
			ESourceList *list)
{
	if (! list->priv->ignore_group_changed)
		g_signal_emit (list, signals[CHANGED], 0);

	if (list->priv->sync_idle_id == 0)
		list->priv->sync_idle_id = g_idle_add ((GSourceFunc) sync_idle_callback, list);
}

static void
conf_changed_callback (GConfClient *client,
		       unsigned int connection_id,
		       GConfEntry *entry,
		       ESourceList *list)
{
	load_from_gconf (list);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	ESourceListPrivate *priv = E_SOURCE_LIST (object)->priv;

	if (priv->sync_idle_id != 0) {
		GError *error = NULL;

		g_source_remove (priv->sync_idle_id);
		priv->sync_idle_id = 0;
		
		if (! e_source_list_sync (E_SOURCE_LIST (object), &error))
			g_warning ("Could not update \"%s\": %s",
				   priv->gconf_path, error->message);
	}

	if (priv->groups != NULL) {
		GSList *p;

		for (p = priv->groups; p != NULL; p = p->next)
			g_object_unref (p->data);

		g_slist_free (priv->groups);
		priv->groups = NULL;
	}

	if (priv->gconf_client != NULL) {
		if (priv->gconf_notify_id != 0) {
			gconf_client_notify_remove (priv->gconf_client,
						    priv->gconf_notify_id);
			priv->gconf_notify_id = 0;
		}

		g_object_unref (priv->gconf_client);
		priv->gconf_client = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	ESourceListPrivate *priv = E_SOURCE_LIST (object)->priv;

	if (priv->gconf_notify_id != 0) {
		gconf_client_notify_remove (priv->gconf_client,
					    priv->gconf_notify_id);
		priv->gconf_notify_id = 0;
	}

	g_free (priv->gconf_path);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Initialization.  */

static void
class_init (ESourceListClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_peek_parent (class);

	signals[CHANGED] = 
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESourceListClass, changed),
			      NULL, NULL,
			      e_util_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[GROUP_REMOVED] = 
		g_signal_new ("group_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESourceListClass, group_removed),
			      NULL, NULL,
			      e_util_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	signals[GROUP_ADDED] = 
		g_signal_new ("group_added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESourceListClass, group_added),
			      NULL, NULL,
			      e_util_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
}

static void
init (ESourceList *source_list)
{
	ESourceListPrivate *priv;

	priv = g_new0 (ESourceListPrivate, 1);

	source_list->priv = priv;
}


/* Public methods.  */

ESourceList *
e_source_list_new (void)
{
	ESourceList *list = g_object_new (e_source_list_get_type (), NULL);

	return list;
}

ESourceList *
e_source_list_new_for_gconf (GConfClient *client,
			     const char *path)
{
	ESourceList *list;

	g_return_val_if_fail (GCONF_IS_CLIENT (client), NULL);
	g_return_val_if_fail (path != NULL, NULL);

	list = g_object_new (e_source_list_get_type (), NULL);

	list->priv->gconf_path = g_strdup (path);
	list->priv->gconf_client = client;
	g_object_ref (client);

	gconf_client_add_dir (client, path, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	list->priv->gconf_notify_id
		= gconf_client_notify_add (client, path,
					   (GConfClientNotifyFunc) conf_changed_callback, list,
					   NULL, NULL);
	load_from_gconf (list);

	return list;
}


GSList *
e_source_list_peek_groups (ESourceList *list)
{
	g_return_val_if_fail (E_IS_SOURCE_LIST (list), NULL);

	return list->priv->groups;
}

ESourceGroup *
e_source_list_peek_group_by_uid (ESourceList *list,
				 const char *uid)
{
	GSList *p;

	g_return_val_if_fail (E_IS_SOURCE_LIST (list), NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	for (p = list->priv->groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);

		if (strcmp (e_source_group_peek_uid (group), uid) == 0)
			return group;
	}

	return NULL;
}

ESource *
e_source_list_peek_source_by_uid (ESourceList *list,
				  const char *uid)
{
	GSList *p;

	g_return_val_if_fail (E_IS_SOURCE_LIST (list), NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	for (p = list->priv->groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);
		ESource *source;
		
		source = e_source_group_peek_source_by_uid (group, uid);
		if (source)
			return source;
	}

	return NULL;
}


gboolean
e_source_list_add_group (ESourceList *list,
			 ESourceGroup *group,
			 int position)
{
	g_return_val_if_fail (E_IS_SOURCE_LIST (list), FALSE);
	g_return_val_if_fail (E_IS_SOURCE_GROUP (group), FALSE);

	if (e_source_list_peek_group_by_uid (list, e_source_group_peek_uid (group)) != NULL)
		return FALSE;

	list->priv->groups = g_slist_insert (list->priv->groups, group, position);
	g_object_ref (group);

	g_signal_connect (group, "changed", G_CALLBACK (group_changed_callback), list);

	g_signal_emit (list, signals[GROUP_ADDED], 0, group);
	g_signal_emit (list, signals[CHANGED], 0);

	return TRUE;
}

gboolean
e_source_list_remove_group (ESourceList *list,
			    ESourceGroup *group)
{
	g_return_val_if_fail (E_IS_SOURCE_LIST (list), FALSE);
	g_return_val_if_fail (E_IS_SOURCE_GROUP (group), FALSE);

	if (e_source_list_peek_group_by_uid (list, e_source_group_peek_uid (group)) == NULL)
		return FALSE;

	remove_group (list, group);
	return TRUE;
}

gboolean
e_source_list_remove_group_by_uid (ESourceList *list,
				    const char *uid)
{
	ESourceGroup *group;
	
	g_return_val_if_fail (E_IS_SOURCE_LIST (list), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);

	group = e_source_list_peek_group_by_uid (list, uid);
	if (group== NULL)
		return FALSE;

	remove_group (list, group);
	return TRUE;
}

gboolean
e_source_list_remove_source_by_uid (ESourceList *list,
				     const char *uid)
{
	GSList *p;
	
	g_return_val_if_fail (E_IS_SOURCE_LIST (list), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);

	for (p = list->priv->groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);
		ESource *source;
		
		source = e_source_group_peek_source_by_uid (group, uid);
		if (source)
			return e_source_group_remove_source_by_uid (group, uid);
	}

	return FALSE;
}


gboolean
e_source_list_sync (ESourceList *list,
		    GError **error)
{
	GSList *conf_list;
	GSList *p;
	gboolean retval;

	g_return_val_if_fail (E_IS_SOURCE_LIST (list), FALSE);

	conf_list = NULL;
	for (p = list->priv->groups; p != NULL; p = p->next)
		conf_list = g_slist_prepend (conf_list, e_source_group_to_xml (E_SOURCE_GROUP (p->data)));
	conf_list = g_slist_reverse (conf_list);

	retval = gconf_client_set_list (list->priv->gconf_client,
					list->priv->gconf_path,
					GCONF_VALUE_STRING,
					conf_list,
					error);

	g_slist_foreach (conf_list, (GFunc) g_free, NULL);
	g_slist_free (conf_list);

	if (list->priv->gconf_notify_id != 0) {
		gconf_client_notify_remove (list->priv->gconf_client,
					    list->priv->gconf_notify_id);
		list->priv->gconf_notify_id = 0;
	}

	return retval;
}


E_MAKE_TYPE (e_source_list, "ESourceList", ESourceList, class_init, init, PARENT_TYPE)
