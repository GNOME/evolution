/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-user-creatable-items-handler.c
 *
 * Copyright (C) 2001  Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-user-creatable-items-handler.h"

#include "e-util/e-corba-utils.h"

#include <gal/util/e-util.h>

#include <bonobo/bonobo-ui-util.h>

#include <stdlib.h>
#include <ctype.h>


#define PARENT_TYPE gtk_object_get_type ()
static GtkObjectClass *parent_class = NULL;


#define VERB_PREFIX "ShellUserCreatableItemVerb"
#define SHELL_VIEW_DATA_KEY "EShellUserCreatableItemsHandler:shell_view"

struct _Component {
	EvolutionShellComponentClient *component_client;

	GNOME_Evolution_UserCreatableItemTypeList *type_list;
};
typedef struct _Component Component;

struct _EShellUserCreatableItemsHandlerPrivate { 
	GSList *components;	/* Component */

	char *menu_xml;
};


/* Component struct handling.  */

static Component *
component_new_from_client (EvolutionShellComponentClient *client)
{
	CORBA_Environment ev;
	Component *new;
	GNOME_Evolution_ShellComponent objref;

	new = g_new (Component, 1);

	new->component_client = client;
	gtk_object_ref (GTK_OBJECT (client));

	CORBA_exception_init (&ev);

	objref = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	new->type_list = GNOME_Evolution_ShellComponent__get_userCreatableItemTypes (objref, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		new->type_list = NULL;

	CORBA_exception_free (&ev);

	return new;
}

static void
component_free (Component *component)
{
	gtk_object_unref (GTK_OBJECT (component->component_client));

	if (component->type_list != NULL)
		CORBA_free (component->type_list);

	g_free (component);
}


/* Helper functions.  */

static char *
create_verb_from_component_number_and_type_id (int component_num,
					       const char *type_id)
{
	return g_strdup_printf (VERB_PREFIX ":%d:%s", component_num, type_id);
}


/* Setting up the XML for the menus.  */

struct _MenuItem {
	const char *label;
	char shortcut;
	char *verb;
};
typedef struct _MenuItem MenuItem;

static int
item_types_sort_func (const void *a,
		      const void *b)
{
	const MenuItem *item_a;
	const MenuItem *item_b;
	const char *p1, *p2;

	item_a = (const MenuItem *) a;
	item_b = (const MenuItem *) b;

	p1 = item_a->label;
	p2 = item_b->label;

	while (*p1 != '\0' && *p2 != '\0') {
		if (*p1 == '_') {
			p1 ++;
			continue;
		}

		if (*p2 == '_') {
			p2 ++;
			continue;
		}

		if (toupper ((int) *p1) < toupper ((int) *p2))
			return -1;
		else if (toupper ((int) *p1) > toupper ((int) *p2))
			return +1;

		p1 ++, p2 ++;
	}

	if (*p1 == '\0') {
		if (*p2 == '\0')
			return 0;
		else
			return -1;
	} else {
		return +1;
	}
	
}

static char *
create_xml_from_menu_items (GSList *items)
{
	GString *xml;
	GSList *p;
	char *str;

	xml = g_string_new ("");

	g_string_append (xml, "<Root> <menu> <submenu name=\"File\"> <submenu name=\"New\"> <placeholder name=\"NewItems\">");

	g_string_append (xml, "<separator/> ");

	for (p = items; p != NULL; p = p->next) {
		const MenuItem *item;
		char *encoded_label;

		item = (const MenuItem *) p->data;

		encoded_label = bonobo_ui_util_encode_str (item->label);

		g_string_sprintfa (xml, "<menuitem name=\"New:%s\" verb=\"%s\" label=\"%s\"",
				   item->verb, item->verb, encoded_label);

		if (item->shortcut != '\0')
			g_string_sprintfa (xml, " accel=\"*Control**Shift*%c\"", item->shortcut);

		g_string_append (xml, "/> ");

		g_free (encoded_label);
	}

	g_string_append (xml, "</placeholder> </submenu> </submenu> </menu> </Root>");

	str = xml->str;
	g_string_free (xml, FALSE);

	return str;
}

static void
setup_menu_xml (EShellUserCreatableItemsHandler *handler)
{
	EShellUserCreatableItemsHandlerPrivate *priv;
	GSList *menu_items;
	GSList *p;
	int component_num;

	priv = handler->priv;
	g_assert (priv->menu_xml == NULL);

	menu_items = NULL;
	component_num = 0;
	for (p = priv->components; p != NULL; p = p->next) {
		const Component *component;
		int i;

		component = (const Component *) p->data;
		if (component->type_list != NULL) {
			for (i = 0; i < component->type_list->_length; i ++) {
				const GNOME_Evolution_UserCreatableItemType *type;
				MenuItem *item;

				type = (const GNOME_Evolution_UserCreatableItemType *) component->type_list->_buffer + i;

				item = g_new (MenuItem, 1);
				item->label    = type->menuDescription;
				item->shortcut = type->menuShortcut;
				item->verb     = create_verb_from_component_number_and_type_id (component_num, type->id);

				menu_items = g_slist_prepend (menu_items, item);
			}
		}

		component_num ++;
	}

	if (menu_items == NULL) {
		priv->menu_xml = g_strdup ("");
		return;
	}

	menu_items = g_slist_sort (menu_items, item_types_sort_func);

	priv->menu_xml = create_xml_from_menu_items (menu_items);

	for (p = menu_items; p != NULL; p = p->next) {
		MenuItem *item;

		item = (MenuItem *) p->data;
		g_free (item->verb);
		g_free (item);
	}
	g_slist_free (menu_items);
}


/* Verb handling.  */

static void
verb_fn (BonoboUIComponent *ui_component,
	 void *data,
	 const char *verb_name)
{
	EShellUserCreatableItemsHandler *handler;
	EShellUserCreatableItemsHandlerPrivate *priv;
	EShellView *shell_view;
	const Component *component;
	int component_number;
	const char *p;
	const char *id;
	GSList *component_list_item;
	int i;

	shell_view = gtk_object_get_data (GTK_OBJECT (ui_component), SHELL_VIEW_DATA_KEY);
	g_assert (E_IS_SHELL_VIEW (shell_view));

	handler = E_SHELL_USER_CREATABLE_ITEMS_HANDLER (data);
	priv = handler->priv;

	p = strchr (verb_name, ':');
	g_assert (p != NULL);
	component_number = atoi (p + 1);

	p = strchr (p + 1, ':');
	g_assert (p != NULL);
	id = p + 1;

	component_list_item = g_slist_nth (priv->components, component_number);
	g_assert (component_list_item != NULL);

	component = (const Component *) component_list_item->data;

	if (component->type_list == NULL)
		return;

	for (i = 0; i < component->type_list->_length; i ++) {
		if (strcmp (component->type_list->_buffer[i].id, id) == 0) {
			CORBA_Environment ev;

			CORBA_exception_init (&ev);

			GNOME_Evolution_ShellComponent_userCreateNewItem
				(bonobo_object_corba_objref (BONOBO_OBJECT (component->component_client)),
				 id,
				 e_safe_corba_string (e_shell_view_get_current_physical_uri (shell_view)),
				 e_safe_corba_string (e_shell_view_get_current_folder_type (shell_view)),
				 &ev);

			if (ev._major != CORBA_NO_EXCEPTION)
				g_warning ("Error in userCreateNewItem -- %s", ev._repo_id);

			CORBA_exception_free (&ev);
			return;
		}
	}
}

static void
add_verbs_to_ui_component (EShellUserCreatableItemsHandler *handler,
			   BonoboUIComponent *ui_component)
{
	EShellUserCreatableItemsHandlerPrivate *priv;
	int component_num;
	GSList *p;

	priv = handler->priv;

	component_num = 0;
	for (p = priv->components; p != NULL; p = p->next) {
		const Component *component;
		int i;

		component = (const Component *) p->data;

		if (component->type_list != NULL) {
			for (i = 0; i < component->type_list->_length; i ++) {
				char *verb_name;

				verb_name = create_verb_from_component_number_and_type_id
					(component_num, component->type_list->_buffer[i].id);

				bonobo_ui_component_add_verb (ui_component, verb_name, verb_fn, handler);

				g_free (verb_name);
			}
		}

		component_num ++;
	}
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EShellUserCreatableItemsHandler *handler;
	EShellUserCreatableItemsHandlerPrivate *priv;
	GSList *p;

	handler = E_SHELL_USER_CREATABLE_ITEMS_HANDLER (object);
	priv = handler->priv;

	for (p = priv->components; p != NULL; p = p->next)
		component_free ((Component *) p->data);

	g_slist_free (priv->components);

	g_free (priv->menu_xml);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (GtkObjectClass *object_class)
{
	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = impl_destroy;
}

static void
init (EShellUserCreatableItemsHandler *shell_user_creatable_items_handler)
{
	EShellUserCreatableItemsHandlerPrivate *priv;

	priv = g_new (EShellUserCreatableItemsHandlerPrivate, 1);
	priv->components = NULL;
	priv->menu_xml   = NULL;

	shell_user_creatable_items_handler->priv = priv;
}


EShellUserCreatableItemsHandler *
e_shell_user_creatable_items_handler_new (void)
{
	EShellUserCreatableItemsHandler *new;

	new = gtk_type_new (e_shell_user_creatable_items_handler_get_type ());

	return new;
}

void
e_shell_user_creatable_items_handler_add_component  (EShellUserCreatableItemsHandler *handler,
						     EvolutionShellComponentClient   *shell_component_client)
{
	EShellUserCreatableItemsHandlerPrivate *priv;

	g_return_if_fail (handler != NULL);
	g_return_if_fail (E_IS_SHELL_USER_CREATABLE_ITEMS_HANDLER (handler));
	g_return_if_fail (shell_component_client != NULL);
	g_return_if_fail (EVOLUTION_IS_SHELL_COMPONENT_CLIENT (shell_component_client));

	priv = handler->priv;
	g_return_if_fail (priv->menu_xml == NULL);

	priv->components = g_slist_prepend (priv->components, component_new_from_client (shell_component_client));
}

void
e_shell_user_creatable_items_handler_setup_menus (EShellUserCreatableItemsHandler *handler,
						  EShellView *shell_view)
{
	EShellUserCreatableItemsHandlerPrivate *priv;
	BonoboUIComponent *ui_component;

	g_return_if_fail (handler != NULL);
	g_return_if_fail (E_IS_SHELL_USER_CREATABLE_ITEMS_HANDLER (handler));
	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	priv = handler->priv;

	if (priv->menu_xml == NULL)
		setup_menu_xml (handler);

	ui_component = e_shell_view_get_bonobo_ui_component (shell_view);
	g_assert (ui_component);

	add_verbs_to_ui_component (handler, ui_component);

	gtk_object_set_data (GTK_OBJECT (ui_component), SHELL_VIEW_DATA_KEY, shell_view);	/* Yuck.  */

	bonobo_ui_component_set (ui_component, "/", priv->menu_xml, NULL);
}


E_MAKE_TYPE (e_shell_user_creatable_items_handler,
	     "EShellUserCreatableItemsHandler", EShellUserCreatableItemsHandler,
	     class_init, init, PARENT_TYPE)
