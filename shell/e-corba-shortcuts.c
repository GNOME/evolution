/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-corba-shortcuts.c
 *
 * Copyright (C) 2001  Ximian, Inc.
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

/* FIXME: Doesn't throw exceptions properly.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-corba-shortcuts.h"

#include "e-util/e-corba-utils.h"

#include <gal/util/e-util.h>


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

struct _ECorbaShortcutsPrivate {
	EShortcuts *shortcuts;
};


/* Utility functions.  */

static const char *
string_from_corba (CORBA_char *corba_string)
{
	if (corba_string[0] == '\0')
		return NULL;

	return corba_string;
}

static void
shortcut_list_to_corba (const GSList *shortcut_list,
			GNOME_Evolution_Shortcuts_ShortcutList *shortcut_list_return)
{
	GNOME_Evolution_Shortcuts_Shortcut *buffer;
	const GSList *p;
	int num_shortcuts;
	int i;

	num_shortcuts = g_slist_length ((GSList *) shortcut_list); /* safe cast, GLib sucks */

	shortcut_list_return->_maximum = num_shortcuts;
	shortcut_list_return->_length = num_shortcuts;

	buffer = CORBA_sequence_GNOME_Evolution_Shortcuts_Shortcut_allocbuf (num_shortcuts);
	shortcut_list_return->_buffer = buffer;

	for (p = shortcut_list, i = 0; p != NULL; p = p->next, i++) {
		const EShortcutItem *item;

		item = (const EShortcutItem *) p->data;

		buffer[i].uri  		 = CORBA_string_dup (e_safe_corba_string (item->uri));
		buffer[i].name 		 = CORBA_string_dup (e_safe_corba_string (item->name));
		buffer[i].type 		 = CORBA_string_dup (e_safe_corba_string (item->type));
		buffer[i].customIconName = CORBA_string_dup (e_safe_corba_string (item->custom_icon_name));
	}

	CORBA_sequence_set_release (shortcut_list_return, TRUE);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	ECorbaShortcuts *corba_shortcuts;
	ECorbaShortcutsPrivate *priv;

	corba_shortcuts = E_CORBA_SHORTCUTS (object);
	priv = corba_shortcuts->priv;

	if (priv->shortcuts != NULL) {
		g_object_unref (priv->shortcuts);
		priv->shortcuts = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	ECorbaShortcuts *corba_shortcuts;

	corba_shortcuts = E_CORBA_SHORTCUTS (object);

	g_free (corba_shortcuts->priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Evolution::Shortcuts CORBA methods.  */

static void
impl_add (PortableServer_Servant servant,
	  const CORBA_short group_num,
	  const CORBA_short position,
	  const GNOME_Evolution_Shortcuts_Shortcut *shortcut,
	  CORBA_Environment *ev)
{
	ECorbaShortcuts *corba_shortcuts;
	ECorbaShortcutsPrivate *priv;

	corba_shortcuts = E_CORBA_SHORTCUTS (bonobo_object_from_servant (servant));
	priv = corba_shortcuts->priv;

	e_shortcuts_add_shortcut (priv->shortcuts, group_num, position,
				  string_from_corba (shortcut->uri),
				  string_from_corba (shortcut->name),
				  0,
				  string_from_corba (shortcut->type),
				  string_from_corba (shortcut->customIconName));
}

static void
impl_remove (PortableServer_Servant servant,
	     const CORBA_short group_num,
	     const CORBA_short item_num,
	     CORBA_Environment *ev)
{
	ECorbaShortcuts *corba_shortcuts;
	ECorbaShortcutsPrivate *priv;

	corba_shortcuts = E_CORBA_SHORTCUTS (bonobo_object_from_servant (servant));
	priv = corba_shortcuts->priv;

	e_shortcuts_remove_shortcut (priv->shortcuts, group_num, item_num);
}

static GNOME_Evolution_Shortcuts_Shortcut *
impl_get (PortableServer_Servant servant,
	  const CORBA_short group_num,
	  const CORBA_short item_num,
	  CORBA_Environment *ev)
{
	ECorbaShortcuts *corba_shortcuts;
	ECorbaShortcutsPrivate *priv;
	GNOME_Evolution_Shortcuts_Shortcut *retval;
	const EShortcutItem *item;

	corba_shortcuts = E_CORBA_SHORTCUTS (bonobo_object_from_servant (servant));
	priv = corba_shortcuts->priv;

	item = e_shortcuts_get_shortcut (priv->shortcuts, group_num, item_num);
	if (item == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Shortcuts_NotFound, NULL);
		return NULL;
	}

	retval = GNOME_Evolution_Shortcuts_Shortcut__alloc ();
	retval->uri  	       = CORBA_string_dup (e_safe_corba_string (item->uri));
	retval->name 	       = CORBA_string_dup (e_safe_corba_string (item->name));
	retval->type 	       = CORBA_string_dup (e_safe_corba_string (item->type));
	retval->customIconName = CORBA_string_dup (e_safe_corba_string (item->custom_icon_name));

	return retval;
}

static void
impl_addGroup (PortableServer_Servant servant,
	       const CORBA_short position,
	       const CORBA_char *name,
	       CORBA_Environment *ev)
{
	ECorbaShortcuts *corba_shortcuts;
	ECorbaShortcutsPrivate *priv;

	corba_shortcuts = E_CORBA_SHORTCUTS (bonobo_object_from_servant (servant));
	priv = corba_shortcuts->priv;

	if (position == 0) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Shortcuts_InvalidPosition, NULL);
		return;
	}

	e_shortcuts_add_group (priv->shortcuts, position, name);
}

static void
impl_removeGroup (PortableServer_Servant servant,
		  const CORBA_short group_num,
		  CORBA_Environment *ev)
{
	ECorbaShortcuts *corba_shortcuts;
	ECorbaShortcutsPrivate *priv;

	corba_shortcuts = E_CORBA_SHORTCUTS (bonobo_object_from_servant (servant));
	priv = corba_shortcuts->priv;

	if (group_num == 0) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Shortcuts_CannotRemove, NULL);
		return;
	}

	e_shortcuts_remove_group (priv->shortcuts, group_num);
}

static GNOME_Evolution_Shortcuts_Group *
impl_getGroup (PortableServer_Servant servant,
	       const CORBA_short group_num,
	       CORBA_Environment *ev)
{
	ECorbaShortcuts *corba_shortcuts;
	ECorbaShortcutsPrivate *priv;
	GNOME_Evolution_Shortcuts_Group *group;
	const GSList *list;

	corba_shortcuts = E_CORBA_SHORTCUTS (bonobo_object_from_servant (servant));
	priv = corba_shortcuts->priv;

	list = e_shortcuts_get_shortcuts_in_group (priv->shortcuts, group_num);

	group = GNOME_Evolution_Shortcuts_Group__alloc ();

	group->name = CORBA_string_dup (e_shortcuts_get_group_title (priv->shortcuts, group_num));

	shortcut_list_to_corba (list, & group->shortcuts);

	return group;
}

static CORBA_sequence_GNOME_Evolution_Shortcuts_Group *
impl__get_groups (PortableServer_Servant servant,
		  CORBA_Environment *ev)
{
	GNOME_Evolution_Shortcuts_GroupList *list;
	ECorbaShortcuts *corba_shortcuts;
	ECorbaShortcutsPrivate *priv;
	GSList *group_titles;
	const GSList *p;
	int i;

	corba_shortcuts = E_CORBA_SHORTCUTS (bonobo_object_from_servant (servant));
	priv = corba_shortcuts->priv;

	list = GNOME_Evolution_Shortcuts_GroupList__alloc ();
	list->_length = e_shortcuts_get_num_groups (priv->shortcuts);
	list->_maximum = list->_length;
	list->_buffer = CORBA_sequence_GNOME_Evolution_Shortcuts_Group_allocbuf (list->_maximum);

	CORBA_sequence_set_release (list, TRUE);

	group_titles = e_shortcuts_get_group_titles (priv->shortcuts);
	for (p = group_titles, i = 0; p != NULL; p = p->next, i ++) {
		char *group_title;
		const GSList *shortcuts;

		group_title = (char *) p->data;

		shortcuts = e_shortcuts_get_shortcuts_in_group (priv->shortcuts, i);

		list->_buffer[i].name = CORBA_string_dup (group_title);
		shortcut_list_to_corba (shortcuts, &list->_buffer[i].shortcuts);

		g_free (group_title);
	}

	g_slist_free (group_titles);

	return list;
}


static void
e_corba_shortcuts_class_init (GObjectClass *object_class)
{
	ECorbaShortcutsClass *corba_shortcuts_class;
	POA_GNOME_Evolution_Shortcuts__epv *epv;

	parent_class = g_type_class_ref(PARENT_TYPE);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	corba_shortcuts_class = E_CORBA_SHORTCUTS_CLASS (object_class);

	epv = & corba_shortcuts_class->epv;
	epv->add         = impl_add;
	epv->remove      = impl_remove;
	epv->get         = impl_get;
	epv->addGroup    = impl_addGroup;
	epv->removeGroup = impl_removeGroup;
	epv->getGroup    = impl_getGroup;
	epv->_get_groups = impl__get_groups;
}

static void
e_corba_shortcuts_init (ECorbaShortcuts *corba_shortcuts)
{
	ECorbaShortcutsPrivate *priv;

	priv = g_new (ECorbaShortcutsPrivate, 1);
	priv->shortcuts = NULL;

	corba_shortcuts->priv = priv;
}


ECorbaShortcuts *
e_corba_shortcuts_new (EShortcuts *shortcuts)
{
	ECorbaShortcuts *corba_shortcuts;

	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	corba_shortcuts = g_object_new (e_corba_shortcuts_get_type (), NULL);

	g_object_ref (shortcuts);
	corba_shortcuts->priv->shortcuts = shortcuts;

	return corba_shortcuts;
}


BONOBO_TYPE_FUNC_FULL (ECorbaShortcuts,
		       GNOME_Evolution_Shortcuts,
		       PARENT_TYPE,
		       e_corba_shortcuts)
