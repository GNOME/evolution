/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-user-creatable-items-handler.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-user-creatable-items-handler.h"

#include "e-shell-corba-icon-utils.h"

#include "widgets/misc/e-combo-button.h"

#include "e-util/e-corba-utils.h"

#include <gal/util/e-util.h>

#include <bonobo/bonobo-ui-util.h>

#include <libgnome/gnome-i18n.h>

#include <gtk/gtksignal.h>
#include <gtk/gtktooltips.h>

#include <stdlib.h>
#include <ctype.h>


#define PARENT_TYPE gtk_object_get_type ()
static GtkObjectClass *parent_class = NULL;


#define VERB_PREFIX "ShellUserCreatableItemVerb"

#define EVOLUTION_MAIL_OAFIID "OAFIID:GNOME_Evolution_Mail_ShellComponent"

#define SHELL_VIEW_KEY          "EShellUserCreatableItemsHandler:shell_view"
#define COMBO_BUTTON_WIDGET_KEY "EShellUserCreatableItemsHandler:combo_button"
#define TOOLTIPS_KEY            "EShellUserCreatableItemsHandler:tooltips"

struct _Component {
	EvolutionShellComponentClient *component_client;

	GNOME_Evolution_UserCreatableItemTypeList *type_list;
};
typedef struct _Component Component;

/* Representation of a single menu item.  */
struct _MenuItem {
	const char *label;
	char shortcut;
	char *verb;
	char *tooltip;
	GdkPixbuf *icon;
};
typedef struct _MenuItem MenuItem;

struct _EShellUserCreatableItemsHandlerPrivate {
	/* The components that register user creatable items.  */
	GSList *components;	/* Component */

	/* The "New ..." menu items.  */
	GSList *menu_items;     /* MenuItem */

	/* The default item (the mailer's "message" item).  To be used when the
	   component in the view we are in doesn't provide a default user
	   creatable type.  This pointer always points to one of the menu items
	   in ->menu_items.  */
	const MenuItem *default_menu_item;

	/* XML to describe the menu.  */
	char *menu_xml;
};


/* Component struct handling.  */

static Component *
component_new (const char *id,
	       EvolutionShellComponentClient *client)
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


/* Setting up menu items for the "File -> New" submenu and the "New" toolbar
   button.  */

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

static void
ensure_menu_items (EShellUserCreatableItemsHandler *handler)
{
	EShellUserCreatableItemsHandlerPrivate *priv;
	GSList *menu_items;
	GSList *p;
	int component_num;
	const char *default_verb;

	priv = handler->priv;
	if (priv->menu_items != NULL)
		return;

	menu_items = NULL;
	component_num = 0;
	default_verb = NULL;
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
				item->tooltip  = type->tooltip;

				if (strcmp (evolution_shell_component_client_get_id (component->component_client),
					    EVOLUTION_MAIL_OAFIID) == 0
				    && strcmp (type->id, "message") == 0)
					default_verb = item->verb;

				if (type->icon.width == 0 || type->icon.height == 0)
					item->icon = NULL;
				else
					item->icon = e_new_gdk_pixbuf_from_corba_icon (& type->icon, 16, 16);

				menu_items = g_slist_prepend (menu_items, item);
			}
		}

		component_num ++;
	}

	if (menu_items == NULL)
		priv->menu_items = NULL;
	else
		priv->menu_items = g_slist_sort (menu_items, item_types_sort_func);

	priv->default_menu_item = NULL;
	if (default_verb != NULL) {
		for (p = priv->menu_items; p != NULL; p = p->next) {
			const MenuItem *item;

			item = (const MenuItem *) p->data;
			if (strcmp (item->verb, default_verb) == 0)
				priv->default_menu_item = item;
		}
	}
}

static void
free_menu_items (GSList *menu_items)
{
	GSList *p;

	if (menu_items == NULL)
		return;

	for (p = menu_items; p != NULL; p = p->next) {
		MenuItem *item;

		item = (MenuItem *) p->data;
		g_free (item->verb);

		if (item->icon != NULL)
			gdk_pixbuf_unref (item->icon);

		g_free (item);
	}

	g_slist_free (menu_items);
}

static const MenuItem *
find_menu_item_for_verb (EShellUserCreatableItemsHandler *handler,
			 const char *verb)
{
	EShellUserCreatableItemsHandlerPrivate *priv;
	GSList *p;

	priv = handler->priv;

	for (p = priv->menu_items; p != NULL; p = p->next) {
		const MenuItem *item;

		item = (const MenuItem *) p->data;
		if (strcmp (item->verb, verb) == 0)
			return item;
	}

	return NULL;
}

static const MenuItem *
get_default_action_for_view (EShellUserCreatableItemsHandler *handler,
			     EShellView *shell_view)
{
	EShellUserCreatableItemsHandlerPrivate *priv;
	const char *view_component_id;
	const GSList *p;
	int component_num;

	priv = handler->priv;

	/* FIXME-1.2: This should be based on the folder type not the component
	   that handles it.  For this, we are going to have to make the IDL a
	   little more complex.  Also, this is a pretty brutal and ugly hack.  */

	view_component_id = e_shell_view_get_current_component_id (shell_view);
	if (view_component_id == NULL)
		return NULL;

	for (p = priv->components, component_num = 0; p != NULL; p = p->next, component_num ++) {
		const Component *component;
		const GNOME_Evolution_UserCreatableItemType *type;
		const char *component_id;

		component = (const Component *) p->data;
		if (component->type_list->_length == 0)
			continue;

		type = & component->type_list->_buffer[0];
		component_id = evolution_shell_component_client_get_id (component->component_client);

		if (strcmp (component_id, view_component_id) == 0) {
			const MenuItem *item;
			char *verb;

			verb = create_verb_from_component_number_and_type_id (component_num, type->id);
			item = find_menu_item_for_verb (handler, verb);
			g_free (verb);

			return item;
		}
	}

	return priv->default_menu_item;
}


/* The XML description for "File -> New".  */

static void
ensure_menu_xml (EShellUserCreatableItemsHandler *handler)
{
	EShellUserCreatableItemsHandlerPrivate *priv;
	GString *xml;
	GSList *p;

	priv = handler->priv;
	if (priv->menu_xml != NULL)
		return;

	ensure_menu_items (handler);

	xml = g_string_new ("");

	g_string_append (xml, "<placeholder name=\"ComponentItems\">");

	for (p = priv->menu_items; p != NULL; p = p->next) {
		const MenuItem *item;
		char *encoded_label;
		char *encoded_tooltip;

		item = (const MenuItem *) p->data;

		encoded_label = bonobo_ui_util_encode_str (item->label);
		g_string_sprintfa (xml, "<menuitem name=\"New:%s\" verb=\"%s\" label=\"%s\"",
				   item->verb, item->verb, encoded_label);

		if (item->shortcut != '\0')
			g_string_sprintfa (xml, " accel=\"*Control**Shift*%c\"", item->shortcut);

		if (item->icon != NULL)
			g_string_sprintfa (xml, " pixtype=\"pixbuf\" pixname=\"%s\"",
					   bonobo_ui_util_pixbuf_to_xml (item->icon));

		encoded_tooltip = bonobo_ui_util_encode_str (item->tooltip);
		g_string_sprintfa (xml, " tip=\"%s\"", encoded_tooltip);

		g_string_append (xml, "/> ");

		g_free (encoded_label);
		g_free (encoded_tooltip);
	}

	g_string_append (xml, "</placeholder>");

	priv->menu_xml = xml->str;
	g_string_free (xml, FALSE);
}


/* Verb handling.  */

static void
execute_verb (EShellUserCreatableItemsHandler *handler,
	      EShellView *shell_view,
	      const char *verb_name)
{
	EShellUserCreatableItemsHandlerPrivate *priv;
	const Component *component;
	int component_number;
	const char *p;
	const char *id;
	GSList *component_list_item;
	int i;

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
verb_fn (BonoboUIComponent *ui_component,
	 void *data,
	 const char *verb_name)
{
	EShellUserCreatableItemsHandler *handler;
	EShellView *shell_view;

	shell_view = gtk_object_get_data (GTK_OBJECT (ui_component), SHELL_VIEW_KEY);
	g_assert (E_IS_SHELL_VIEW (shell_view));

	handler = E_SHELL_USER_CREATABLE_ITEMS_HANDLER (data);

	execute_verb (handler, shell_view, verb_name);
}

static void
add_verbs (EShellUserCreatableItemsHandler *handler,
	   EShellView *shell_view)
{
	EShellUserCreatableItemsHandlerPrivate *priv;
	BonoboUIComponent *ui_component;
	int component_num;
	GSList *p;

	priv = handler->priv;

	ui_component = e_shell_view_get_bonobo_ui_component (shell_view);
	gtk_object_set_data (GTK_OBJECT (ui_component), SHELL_VIEW_KEY, shell_view);

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


/* The "New" button in the toolbar.  */

static void
combo_button_activate_default_callback (EComboButton *combo_button,
					void *data)
{
	EShellView *shell_view;
	EShellUserCreatableItemsHandler *handler;
	const MenuItem *menu_item;

	shell_view = E_SHELL_VIEW (data);
	handler = e_shell_get_user_creatable_items_handler (e_shell_view_get_shell (shell_view));

	menu_item = get_default_action_for_view (handler, shell_view);
	execute_verb (handler, shell_view, menu_item->verb);
}

static void
setup_toolbar_button (EShellUserCreatableItemsHandler *handler,
		      EShellView *shell_view)
{
	EShellUserCreatableItemsHandlerPrivate *priv;
	BonoboUIComponent *ui_component;
	GtkWidget *combo_button;
	GtkWidget *menu;
	GtkTooltips *tooltips;
	BonoboControl *control;

	priv = handler->priv;

	menu = gtk_menu_new ();

	combo_button = e_combo_button_new ();
	e_combo_button_set_menu (E_COMBO_BUTTON (combo_button), GTK_MENU (menu));
	e_combo_button_set_label (E_COMBO_BUTTON (combo_button), _("New"));
	gtk_widget_show (combo_button);

	gtk_signal_connect (GTK_OBJECT (combo_button), "activate_default",
			    GTK_SIGNAL_FUNC (combo_button_activate_default_callback),
			    shell_view);

	ui_component = e_shell_view_get_bonobo_ui_component (shell_view);
	bonobo_window_add_popup (BONOBO_WINDOW (shell_view), GTK_MENU (menu), "/popups/NewPopup");

	control = bonobo_control_new (combo_button);

	bonobo_ui_component_object_set (ui_component, "/Toolbar/NewComboButton",
					BONOBO_OBJREF (control), NULL);

	gtk_object_set_data (GTK_OBJECT (shell_view), COMBO_BUTTON_WIDGET_KEY, combo_button);

	tooltips = gtk_tooltips_new ();
	gtk_object_set_data (GTK_OBJECT (combo_button), TOOLTIPS_KEY, tooltips);
}


/* This handles the menus for a given EShellView.  We have to rebuild the menu
   and set the toolbar button every time the view changes, and clean up when
   the view is destroyed.  */

static void
shell_view_view_changed_callback (EShellView *shell_view,
				  const char *evolution_path,
				  const char *physical_uri,
				  const char *folder_type,
				  const char *component_id,
				  void *data)
{
	EShellUserCreatableItemsHandler *handler;
	EShellUserCreatableItemsHandlerPrivate *priv;
	GtkWidget *combo_button_widget;
	GtkTooltips *tooltips;
	const MenuItem *default_menu_item;

	handler = E_SHELL_USER_CREATABLE_ITEMS_HANDLER (data);
	priv = handler->priv;

	combo_button_widget = gtk_object_get_data (GTK_OBJECT (shell_view), COMBO_BUTTON_WIDGET_KEY);
	g_assert (E_IS_COMBO_BUTTON (combo_button_widget));

	tooltips = gtk_object_get_data (GTK_OBJECT (combo_button_widget), TOOLTIPS_KEY);
	g_assert (tooltips != NULL);

	default_menu_item = get_default_action_for_view (handler, shell_view);
	if (default_menu_item == NULL) {
		gtk_widget_set_sensitive (combo_button_widget, FALSE);
		e_combo_button_set_label (E_COMBO_BUTTON (combo_button_widget), _("New"));
		e_combo_button_set_icon (E_COMBO_BUTTON (combo_button_widget), NULL);
		gtk_tooltips_set_tip (tooltips, combo_button_widget, NULL, NULL);
		return;
	}

	gtk_widget_set_sensitive (combo_button_widget, TRUE);

	e_combo_button_set_icon (E_COMBO_BUTTON (combo_button_widget), default_menu_item->icon);
	gtk_tooltips_set_tip (tooltips, combo_button_widget, default_menu_item->tooltip, NULL);
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

	free_menu_items (priv->menu_items);

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
	priv->components 	= NULL;
	priv->menu_xml   	= NULL;
	priv->menu_items 	= NULL;
	priv->default_menu_item = NULL;

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
						     const char *id,
						     EvolutionShellComponentClient   *shell_component_client)
{
	EShellUserCreatableItemsHandlerPrivate *priv;

	g_return_if_fail (handler != NULL);
	g_return_if_fail (E_IS_SHELL_USER_CREATABLE_ITEMS_HANDLER (handler));
	g_return_if_fail (shell_component_client != NULL);
	g_return_if_fail (EVOLUTION_IS_SHELL_COMPONENT_CLIENT (shell_component_client));

	priv = handler->priv;
	g_return_if_fail (priv->menu_xml == NULL);

	priv->components = g_slist_prepend (priv->components, component_new (id, shell_component_client));
}


/**
 * e_shell_user_creatable_items_handler_attach_menus:
 * @handler: 
 * @shell_view: 
 * 
 * Set up the menus and toolbar items for @shell_view.  When the shell changes
 * view, the menu and the toolbar item will update automatically (i.e. the
 * actions for the current folder will go on top etc.).
 **/
void
e_shell_user_creatable_items_handler_attach_menus (EShellUserCreatableItemsHandler *handler,
						   EShellView *shell_view)
{
	BonoboUIComponent *ui_component;
	EShellUserCreatableItemsHandlerPrivate *priv;

	g_return_if_fail (handler != NULL);
	g_return_if_fail (E_IS_SHELL_USER_CREATABLE_ITEMS_HANDLER (handler));
	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	priv = handler->priv;

	setup_toolbar_button (handler, shell_view);
	gtk_signal_connect (GTK_OBJECT (shell_view), "view_changed",
			    GTK_SIGNAL_FUNC (shell_view_view_changed_callback), handler);

	ensure_menu_xml (handler);

	add_verbs (handler, shell_view);

	ui_component = e_shell_view_get_bonobo_ui_component (shell_view);
	bonobo_ui_component_set (ui_component, "/menu/File/New", priv->menu_xml, NULL);
	bonobo_ui_component_set (ui_component, "/popups/NewPopup", priv->menu_xml, NULL);
}


E_MAKE_TYPE (e_shell_user_creatable_items_handler,
	     "EShellUserCreatableItemsHandler", EShellUserCreatableItemsHandler,
	     class_init, init, PARENT_TYPE)
