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

#include "e-user-creatable-items-handler.h"

#include "e-shell-utils.h"

#include "widgets/misc/e-combo-button.h"

#include "e-util/e-corba-utils.h"

#include <gal/util/e-util.h>

#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-exception.h>

#include <libgnome/gnome-i18n.h>

#include <gtk/gtksignal.h>
#include <gtk/gtktooltips.h>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>


#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;


#define VERB_PREFIX "ShellUserCreatableItemVerb"

#define EVOLUTION_MAIL_OAFIID "OAFIID:GNOME_Evolution_Mail_ShellComponent"

#define SHELL_WINDOW_KEY          "EUserCreatableItemsHandler:shell_window"
#define COMBO_BUTTON_WIDGET_KEY "EUserCreatableItemsHandler:combo_button"
#define TOOLTIPS_KEY            "EUserCreatableItemsHandler:tooltips"

struct _Component {
	char *id;
	GNOME_Evolution_Component component;
	GNOME_Evolution_CreatableItemTypeList *type_list;
};
typedef struct _Component Component;

/* Representation of a single menu item.  */
struct _MenuItem {
	const char *label;
	char shortcut;
	char *verb;
	char *tooltip;
	char *component_id;
	GdkPixbuf *icon;
};
typedef struct _MenuItem MenuItem;

struct _EUserCreatableItemsHandlerPrivate {
	/* The components that register user creatable items.  */
	GSList *components;	/* Component */

	/* The "New ..." menu items.  */
	GSList *menu_items;     /* MenuItem */

	/* The default item (the mailer's "message" item).  To be used when the
	   component in the view we are in doesn't provide a default user
	   creatable type.  This pointer always points to one of the menu items
	   in ->menu_items.  */
	const MenuItem *default_menu_item;
};


/* Component struct handling.  */

static Component *
component_new (const char *id,
	       GNOME_Evolution_Component component)
{
	CORBA_Environment ev;
	Component *new;

	CORBA_exception_init (&ev);

	new = g_new (Component, 1);
	new->id = g_strdup (id);

	new->type_list = GNOME_Evolution_Component__get_userCreatableItems (component, &ev);
	if (BONOBO_EX (&ev))
		new->type_list = NULL;

	new->component = component;
	Bonobo_Unknown_ref (new->component, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		new->type_list = NULL;

	CORBA_exception_free (&ev);

	return new;
}

static void
component_free (Component *component)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Bonobo_Unknown_unref (component->component, &ev);

	g_free (component->id);

	if (component->type_list != NULL)
		CORBA_free (component->type_list);

	CORBA_exception_free (&ev);

	g_free (component);
}

static void
get_components_from_registry (EUserCreatableItemsHandler *handler,
			      EComponentRegistry *registry)
{
	GSList *registry_list = e_component_registry_peek_list (registry);
	GSList *p;

	for (p = registry_list; p != NULL; p = p->next) {
		EComponentInfo *info = p->data;
		Component *component;

		e_component_registry_activate (registry, info->id, NULL);

		if (info->iface != CORBA_OBJECT_NIL)
			component = component_new (info->id, info->iface);

		handler->priv->components = g_slist_prepend (handler->priv->components, component);
	}
}


/* Helper functions.  */

static gboolean
item_is_default (const MenuItem *item,
		 const char *component_id)
{
	if (component_id == NULL)
		return FALSE;

	if (strcmp (item->component_id, component_id) == 0)
		return TRUE;
	else
		return FALSE;
}

static char *
create_verb_from_component_number_and_type_id (int component_num,
					       const char *type_id)
{
	return g_strdup_printf (VERB_PREFIX ":%d:%s", component_num, type_id);
}


/* Setting up menu items for the "File -> New" submenu and the "New" toolbar
   button.  */

static void
ensure_menu_items (EUserCreatableItemsHandler *handler)
{
	EUserCreatableItemsHandlerPrivate *priv;
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
				const GNOME_Evolution_CreatableItemType *type;
				MenuItem *item;

				type = (const GNOME_Evolution_CreatableItemType *) component->type_list->_buffer + i;

				item = g_new (MenuItem, 1);
				item->label        = type->menuDescription;
				item->shortcut     = type->menuShortcut;
				item->verb         = create_verb_from_component_number_and_type_id (component_num, type->id);
				item->tooltip      = type->tooltip;
				item->component_id = g_strdup (component->id);

				if (strcmp (item->component_id, EVOLUTION_MAIL_OAFIID) == 0
				    && strcmp (type->id, "message") == 0)
					default_verb = item->verb;

				if (type->iconName == "") {
					item->icon = NULL;
				} else {
					char *icon_path = e_shell_get_icon_path (type->iconName, TRUE);
					item->icon = gdk_pixbuf_new_from_file (icon_path, NULL);
					g_free (icon_path);
				}

				menu_items = g_slist_prepend (menu_items, item);
			}
		}

		component_num ++;
	}

	priv->menu_items = g_slist_reverse (menu_items);

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
			g_object_unref (item->icon);

		g_free (item->component_id);
		g_free (item);
	}

	g_slist_free (menu_items);
}

static const MenuItem *
get_default_action_for_view (EUserCreatableItemsHandler *handler,
			     EShellWindow *window)
{
	EUserCreatableItemsHandlerPrivate *priv;
	const char *window_component_id;
	const GSList *p;

	priv = handler->priv;

	window_component_id = e_shell_window_peek_current_component_id (window);
	if (window_component_id == NULL)
		return priv->default_menu_item;

	for (p = priv->menu_items; p != NULL; p = p->next) {
		const MenuItem *item;

		item = (const MenuItem *) p->data;
		if (item_is_default (item, window_component_id))
			return item;
	}

	return priv->default_menu_item;
}


/* The XML description for "File -> New".  */

/* This adds a menu item for @item.  If @first is true, the keyboard shortcut
   is going to be "Control-N" instead of whatever the component defines.  */
static void
append_xml_for_menu_item (GString *xml,
			  const MenuItem *item,
			  gboolean first)
{
	char *encoded_label;
	char *encoded_tooltip;

	encoded_label = bonobo_ui_util_encode_str (item->label);
	g_string_append_printf (xml, "<menuitem name=\"New:%s\" verb=\"%s\" label=\"%s\"",
				item->verb, item->verb, encoded_label);

	if (first)
		g_string_append_printf (xml, " accel=\"*Control*N\"");
	else if (item->shortcut != '\0')
		g_string_append_printf (xml, " accel=\"*Control**Shift*%c\"", item->shortcut);

	if (item->icon != NULL) {
		char *icon_xml;

		icon_xml = bonobo_ui_util_pixbuf_to_xml (item->icon);
		g_string_append_printf (xml, " pixtype=\"pixbuf\" pixname=\"%s\"", icon_xml);
		g_free (icon_xml);
	}

	encoded_tooltip = bonobo_ui_util_encode_str (item->tooltip);
	g_string_append_printf (xml, " tip=\"%s\"", encoded_tooltip);

	g_string_append (xml, "/> ");

	g_free (encoded_label);
	g_free (encoded_tooltip);
}

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
create_menu_xml (EUserCreatableItemsHandler *handler,
		 const char *component_id)
{
	EUserCreatableItemsHandlerPrivate *priv;
	GString *xml;
	GSList *p;
	GSList *non_default_items;
	char *retval;

	priv = handler->priv;

	ensure_menu_items (handler);

	xml = g_string_new ("");

	g_string_append (xml, "<placeholder name=\"ComponentItems\">");
	g_string_append (xml, "<placeholder name=\"EUserCreatableItemsPlaceholder\">");

	/* 1. Add all the elements that are default for this component.  (Note
	   that we don't need to do any sorting since the items are already
	   sorted alphabetically.)  */

	if (component_id != NULL) {
		gboolean first = TRUE;

		for (p = priv->menu_items; p != NULL; p = p->next) {
			const MenuItem *item;

			item = (const MenuItem *) p->data;
			if (item_is_default (item, component_id)) {
				append_xml_for_menu_item (xml, item, first);
				first = FALSE;
			}
		}
	}

	/* 2. Add a separator. */

	if (component_id != NULL)
		g_string_append_printf (xml,
					"<separator f=\"\" name=\"EUserCreatableItemsHandlerSeparator\"/>");

	/* 3. Add the elements that are not default for this component.  */

	non_default_items = NULL;
	for (p = priv->menu_items; p != NULL; p = p->next) {
		const MenuItem *item;

		item = (const MenuItem *) p->data;
		if (! item_is_default (item, component_id))
			non_default_items = g_slist_prepend (non_default_items, (void *) item);
	}

	non_default_items = g_slist_sort (non_default_items, item_types_sort_func);
	for (p = non_default_items; p != NULL; p = p->next)
		append_xml_for_menu_item (xml, (const MenuItem *) p->data, FALSE);
	g_slist_free (non_default_items);

	/* Done...  */

	g_string_append (xml, "</placeholder>"); /* EUserCreatableItemsPlaceholder */
	g_string_append (xml, "</placeholder>"); /* ComponentItems */

	retval = xml->str;
	g_string_free (xml, FALSE);

	return retval;
}


/* Verb handling.  */

static void
execute_verb (EUserCreatableItemsHandler *handler,
	      EShellWindow *window,
	      const char *verb_name)
{
	EUserCreatableItemsHandlerPrivate *priv;
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

			GNOME_Evolution_Component_requestCreateItem (component->component, id, &ev);

			if (ev._major != CORBA_NO_EXCEPTION)
				g_warning ("Error in requestCreateItem -- %s", BONOBO_EX_REPOID (&ev));

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
	EUserCreatableItemsHandler *handler;
	EShellWindow *shell_window;

	shell_window = g_object_get_data (G_OBJECT (ui_component), SHELL_WINDOW_KEY);
	g_assert (E_IS_SHELL_WINDOW (shell_window));

	handler = E_USER_CREATABLE_ITEMS_HANDLER (data);

	execute_verb (handler, shell_window, verb_name);
}

static void
add_verbs (EUserCreatableItemsHandler *handler,
	   EShellWindow *window)
{
	EUserCreatableItemsHandlerPrivate *priv;
	BonoboUIComponent *ui_component;
	int component_num;
	GSList *p;

	priv = handler->priv;

	ui_component = e_shell_window_peek_bonobo_ui_component (window);
	g_object_set_data (G_OBJECT (ui_component), SHELL_WINDOW_KEY, window);

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
	EShellWindow *shell_window;
	EUserCreatableItemsHandler *handler;
	const MenuItem *menu_item;

	shell_window = E_SHELL_WINDOW (data);
	handler = e_shell_peek_user_creatable_items_handler (e_shell_window_peek_shell (shell_window));

	menu_item = get_default_action_for_view (handler, shell_window);
	execute_verb (handler, shell_window, menu_item->verb);
}

static void
setup_toolbar_button (EUserCreatableItemsHandler *handler,
		      EShellWindow *window)
{
	EUserCreatableItemsHandlerPrivate *priv;
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

	g_signal_connect (combo_button, "activate_default", G_CALLBACK (combo_button_activate_default_callback), window);

	ui_component = e_shell_window_peek_bonobo_ui_component (window);
	bonobo_window_add_popup (BONOBO_WINDOW (window), GTK_MENU (menu), "/popups/NewPopup");

	control = bonobo_control_new (combo_button);

	bonobo_ui_component_object_set (ui_component, "/Toolbar/NewComboButton",
					BONOBO_OBJREF (control), NULL);

	bonobo_object_unref (control);
	
	g_object_set_data (G_OBJECT (window), COMBO_BUTTON_WIDGET_KEY, combo_button);

	tooltips = gtk_tooltips_new ();
	g_object_set_data (G_OBJECT (combo_button), TOOLTIPS_KEY, tooltips);
}


static void
update_for_window (EUserCreatableItemsHandler *handler,
		   EShellWindow *window)
{
	GtkWidget *combo_button_widget;
	GtkTooltips *tooltips;
	BonoboUIComponent *ui_component;
	const MenuItem *default_menu_item;
	char *menu_xml;

	combo_button_widget = g_object_get_data (G_OBJECT (window), COMBO_BUTTON_WIDGET_KEY);
	g_assert (E_IS_COMBO_BUTTON (combo_button_widget));

	tooltips = g_object_get_data (G_OBJECT (combo_button_widget), TOOLTIPS_KEY);
	g_assert (tooltips != NULL);

	default_menu_item = get_default_action_for_view (handler, window);
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

	ui_component = e_shell_window_peek_bonobo_ui_component (window);
	bonobo_ui_component_rm (ui_component, "/menu/File/New/ComponentItems/EUserCreatableItemsPlaceholder", NULL);
	bonobo_ui_component_rm (ui_component, "/popups/NewPopup/ComponentItems/EUserCreatableItemsPlaceholder", NULL);

	menu_xml = create_menu_xml (handler, e_shell_window_peek_current_component_id (window));

	bonobo_ui_component_set (ui_component, "/menu/File/New", menu_xml, NULL);
	bonobo_ui_component_set (ui_component, "/popups/NewPopup", menu_xml, NULL);
	g_free (menu_xml);
}


/* This handles the menus for a given EShellWindow.  We have to rebuild the menu
   and set the toolbar button every time the view changes, and clean up when
   the view is destroyed.  */

static void
shell_window_component_changed_callback (EShellWindow *shell_window,
					 EUserCreatableItemsHandler *handler)
{
	update_for_window (handler, shell_window);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EUserCreatableItemsHandler *handler;
	EUserCreatableItemsHandlerPrivate *priv;
	GSList *p;

	handler = E_USER_CREATABLE_ITEMS_HANDLER (object);
	priv = handler->priv;

	for (p = priv->components; p != NULL; p = p->next)
		component_free ((Component *) p->data);

	g_slist_free (priv->components);
	priv->components = NULL;

	
	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EUserCreatableItemsHandler *handler;
	EUserCreatableItemsHandlerPrivate *priv;

	handler = E_USER_CREATABLE_ITEMS_HANDLER (object);
	priv = handler->priv;

	free_menu_items (priv->menu_items);

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
class_init (GObjectClass *object_class)
{
	parent_class = g_type_class_ref(PARENT_TYPE);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;
}

static void
init (EUserCreatableItemsHandler *shell_user_creatable_items_handler)
{
	EUserCreatableItemsHandlerPrivate *priv;

	priv = g_new (EUserCreatableItemsHandlerPrivate, 1);
	priv->components 	= NULL;
	priv->menu_items 	= NULL;
	priv->default_menu_item = NULL;

	shell_user_creatable_items_handler->priv = priv;
}


EUserCreatableItemsHandler *
e_user_creatable_items_handler_new (EComponentRegistry *registry)
{
	EUserCreatableItemsHandler *new;

	new = g_object_new (e_user_creatable_items_handler_get_type (), NULL);

	get_components_from_registry (new, registry);

	return new;
}


/**
 * e_user_creatable_items_handler_attach_menus:
 * @handler: 
 * @shell_window: 
 * 
 * Set up the menus and toolbar items for @shell_window.  When the shell changes
 * view, the menu and the toolbar item will update automatically (i.e. the
 * actions for the current folder will go on top etc.).
 **/
void
e_user_creatable_items_handler_attach_menus (EUserCreatableItemsHandler *handler,
					     EShellWindow *window)
{
	BonoboUIComponent *ui_component;
	EUserCreatableItemsHandlerPrivate *priv;
	char *menu_xml;

	g_return_if_fail (E_IS_USER_CREATABLE_ITEMS_HANDLER (handler));
	g_return_if_fail (E_IS_SHELL_WINDOW (window));

	priv = handler->priv;

	setup_toolbar_button (handler, window);
	g_signal_connect (window, "component_changed", G_CALLBACK (shell_window_component_changed_callback), handler);

	add_verbs (handler, window);
	menu_xml = create_menu_xml (handler, e_shell_window_peek_current_component_id (window));

	ui_component = e_shell_window_peek_bonobo_ui_component (window);
	bonobo_ui_component_set (ui_component, "/menu/File/New", menu_xml, NULL);
	bonobo_ui_component_set (ui_component, "/popups/NewPopup", menu_xml, NULL);

	g_free (menu_xml);

	update_for_window (handler, window);
}


E_MAKE_TYPE (e_user_creatable_items_handler, "EUserCreatableItemsHandler", EUserCreatableItemsHandler, class_init, init, PARENT_TYPE)
