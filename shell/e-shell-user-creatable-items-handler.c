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
	char *component_id;
	char *folder_type;
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
	g_object_ref (client);

	CORBA_exception_init (&ev);

	objref = evolution_shell_component_client_corba_objref (client);
	new->type_list = GNOME_Evolution_ShellComponent__get_userCreatableItemTypes (objref, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		new->type_list = NULL;

	CORBA_exception_free (&ev);

	return new;
}

static void
component_free (Component *component)
{
	g_object_unref (component->component_client);

	if (component->type_list != NULL)
		CORBA_free (component->type_list);

	g_free (component);
}


/* Helper functions.  */

static gboolean
item_is_default (const MenuItem *item,
		 const char *folder_type,
		 const char *component_id)
{
	if (component_id == NULL || folder_type == NULL)
		return FALSE;

	if (item->folder_type != NULL && *item->folder_type != 0) {
		if (strcmp (item->folder_type, folder_type) == 0)
			return TRUE;
		else
			return FALSE;
	}

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
				item->label        = type->menuDescription;
				item->shortcut     = type->menuShortcut;
				item->verb         = create_verb_from_component_number_and_type_id (component_num, type->id);
				item->tooltip      = type->tooltip;
				item->component_id = g_strdup (evolution_shell_component_client_get_id (component->component_client));
				item->folder_type  = g_strdup (type->folderType);

				if (strcmp (item->component_id, EVOLUTION_MAIL_OAFIID) == 0
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

	priv->menu_items = menu_items;

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
get_default_action_for_view (EShellUserCreatableItemsHandler *handler,
			     EShellView *shell_view)
{
	EShellUserCreatableItemsHandlerPrivate *priv;
	const char *view_component_id;
	const GSList *p;

	priv = handler->priv;

	view_component_id = e_shell_view_get_current_component_id (shell_view);
	if (view_component_id == NULL)
		return priv->default_menu_item;

	for (p = priv->menu_items; p != NULL; p = p->next) {
		const MenuItem *item;

		item = (const MenuItem *) p->data;
		if (item_is_default (item,
				     e_shell_view_get_current_folder_type (shell_view),
				     e_shell_view_get_current_component_id (shell_view)))
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
create_menu_xml (EShellUserCreatableItemsHandler *handler,
		 const char *folder_type,
		 const char *component_id)
{
	EShellUserCreatableItemsHandlerPrivate *priv;
	GString *xml;
	GSList *p;
	GSList *non_default_items;
	char *retval;

	priv = handler->priv;

	ensure_menu_items (handler);

	xml = g_string_new ("");

	g_string_append (xml, "<placeholder name=\"ComponentItems\">");
	g_string_append (xml, "<placeholder name=\"EShellUserCreatableItemsPlaceholder\">");

	/* 1. Add all the elements that are default for this component.  (Note
	   that we don't need to do any sorting since the items are already
	   sorted alphabetically.)  */

	if (component_id != NULL) {
		gboolean first = TRUE;

		for (p = priv->menu_items; p != NULL; p = p->next) {
			const MenuItem *item;

			item = (const MenuItem *) p->data;
			if (item_is_default (item, folder_type, component_id)) {
				append_xml_for_menu_item (xml, item, first);
				first = FALSE;
			}
		}
	}

	/* 2. Add a separator. */

	if (component_id != NULL)
		g_string_append_printf (xml,
					"<separator f=\"\" name=\"EShellUserCreatableItemsHandlerSeparator\"/>");

	/* 3. Add the elements that are not default for this component.  */

	non_default_items = NULL;
	for (p = priv->menu_items; p != NULL; p = p->next) {
		const MenuItem *item;

		item = (const MenuItem *) p->data;
		if (! item_is_default (item, folder_type, component_id))
			non_default_items = g_slist_prepend (non_default_items, (void *) item);
	}

	non_default_items = g_slist_sort (non_default_items, item_types_sort_func);
	for (p = non_default_items; p != NULL; p = p->next)
		append_xml_for_menu_item (xml, (const MenuItem *) p->data, FALSE);
	g_slist_free (non_default_items);

	/* Done...  */

	g_string_append (xml, "</placeholder>"); /* EShellUserCreatableItemsPlaceholder */
	g_string_append (xml, "</placeholder>"); /* ComponentItems */

	retval = xml->str;
	g_string_free (xml, FALSE);

	return retval;
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
				(evolution_shell_component_client_corba_objref (component->component_client),
				 id,
				 e_safe_corba_string (e_shell_view_get_current_physical_uri (shell_view)),
				 e_safe_corba_string (e_shell_view_get_current_folder_type (shell_view)),
				 &ev);

			if (ev._major != CORBA_NO_EXCEPTION)
				g_warning ("Error in userCreateNewItem -- %s", BONOBO_EX_REPOID (&ev));

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

	shell_view = g_object_get_data (G_OBJECT (ui_component), SHELL_VIEW_KEY);
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
	g_object_set_data (G_OBJECT (ui_component), SHELL_VIEW_KEY, shell_view);

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

	g_signal_connect (combo_button, "activate_default",
			  G_CALLBACK (combo_button_activate_default_callback),
			  shell_view);

	ui_component = e_shell_view_get_bonobo_ui_component (shell_view);
	bonobo_window_add_popup (BONOBO_WINDOW (shell_view), GTK_MENU (menu), "/popups/NewPopup");

	control = bonobo_control_new (combo_button);

	bonobo_ui_component_object_set (ui_component, "/Toolbar/NewComboButton",
					BONOBO_OBJREF (control), NULL);

	g_object_set_data (G_OBJECT (shell_view), COMBO_BUTTON_WIDGET_KEY, combo_button);

	tooltips = gtk_tooltips_new ();
	g_object_set_data (G_OBJECT (combo_button), TOOLTIPS_KEY, tooltips);
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
	BonoboUIComponent *ui_component;
	const MenuItem *default_menu_item;
	char *menu_xml;

	handler = E_SHELL_USER_CREATABLE_ITEMS_HANDLER (data);
	priv = handler->priv;

	combo_button_widget = g_object_get_data (G_OBJECT (shell_view), COMBO_BUTTON_WIDGET_KEY);
	g_assert (E_IS_COMBO_BUTTON (combo_button_widget));

	tooltips = g_object_get_data (G_OBJECT (combo_button_widget), TOOLTIPS_KEY);
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

	ui_component = e_shell_view_get_bonobo_ui_component (shell_view);
	bonobo_ui_component_rm (ui_component, "/menu/File/New/ComponentItems/EShellUserCreatableItemsPlaceholder", NULL);
	bonobo_ui_component_rm (ui_component, "/popups/NewPopup/ComponentItems/EShellUserCreatableItemsPlaceholder", NULL);

	menu_xml = create_menu_xml (handler,
				    e_shell_view_get_current_folder_type (shell_view),
				    e_shell_view_get_current_component_id (shell_view));
	bonobo_ui_component_set (ui_component, "/menu/File/New", menu_xml, NULL);
	bonobo_ui_component_set (ui_component, "/popups/NewPopup", menu_xml, NULL);
	g_free (menu_xml);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EShellUserCreatableItemsHandler *handler;
	EShellUserCreatableItemsHandlerPrivate *priv;
	GSList *p;

	handler = E_SHELL_USER_CREATABLE_ITEMS_HANDLER (object);
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
	EShellUserCreatableItemsHandler *handler;
	EShellUserCreatableItemsHandlerPrivate *priv;

	handler = E_SHELL_USER_CREATABLE_ITEMS_HANDLER (object);
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
init (EShellUserCreatableItemsHandler *shell_user_creatable_items_handler)
{
	EShellUserCreatableItemsHandlerPrivate *priv;

	priv = g_new (EShellUserCreatableItemsHandlerPrivate, 1);
	priv->components 	= NULL;
	priv->menu_items 	= NULL;
	priv->default_menu_item = NULL;

	shell_user_creatable_items_handler->priv = priv;
}


EShellUserCreatableItemsHandler *
e_shell_user_creatable_items_handler_new (void)
{
	EShellUserCreatableItemsHandler *new;

	new = g_object_new (e_shell_user_creatable_items_handler_get_type (), NULL);

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
	char *menu_xml;

	g_return_if_fail (handler != NULL);
	g_return_if_fail (E_IS_SHELL_USER_CREATABLE_ITEMS_HANDLER (handler));
	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	priv = handler->priv;

	setup_toolbar_button (handler, shell_view);
	g_signal_connect (shell_view, "view_changed",
			  G_CALLBACK (shell_view_view_changed_callback), handler);

	add_verbs (handler, shell_view);
	menu_xml = create_menu_xml (handler,
				    e_shell_view_get_current_component_id (shell_view),
				    e_shell_view_get_current_folder_type (shell_view));

	ui_component = e_shell_view_get_bonobo_ui_component (shell_view);
	bonobo_ui_component_set (ui_component, "/menu/File/New", menu_xml, NULL);
	bonobo_ui_component_set (ui_component, "/popups/NewPopup", menu_xml, NULL);

	g_free (menu_xml);
}


E_MAKE_TYPE (e_shell_user_creatable_items_handler,
	     "EShellUserCreatableItemsHandler", EShellUserCreatableItemsHandler,
	     class_init, init, PARENT_TYPE)
