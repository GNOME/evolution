/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* addressbook-component.c
 *
 * Copyright (C) 2003  Ettore Perazzoli
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

/* EPFIXME: Add autocompletion setting.  */


#include <config.h>

#include "addressbook-component.h"

#include "addressbook.h"
#include "addressbook-config.h"

#include "widgets/misc/e-source-selector.h"
#include "addressbook/gui/widgets/eab-gui-util.h"

#include "e-task-bar.h"

#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtklabel.h>	/* FIXME */
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>
#include <gconf/gconf-client.h>
#include <gal/util/e-util.h>


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

struct _AddressbookComponentPrivate {
	GConfClient *gconf_client;
	ESourceList *source_list;
	GtkWidget *source_selector;

	EActivityHandler *activity_handler;
};


/* Utility functions.  */

static void
load_uri_for_selection (ESourceSelector *selector,
			BonoboControl *view_control)
{
	ESource *selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (selector));

	if (selected_source != NULL) {
		bonobo_control_set_property (view_control, NULL, "source_uid", TC_CORBA_string,
					     e_source_peek_uid (selected_source), NULL);
	}
}

static void
add_popup_menu_item (GtkMenu *menu, const char *label, const char *pixmap,
		     GCallback callback, gpointer user_data)
{
	GtkWidget *item, *image;

	if (pixmap) {
		item = gtk_image_menu_item_new_with_label (label);

		/* load the image */
		image = gtk_image_new_from_file (pixmap);
		if (!image)
			image = gtk_image_new_from_stock (pixmap, GTK_ICON_SIZE_MENU);

		if (image)
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	} else {
		item = gtk_menu_item_new_with_label (label);
	}

	if (callback)
		g_signal_connect (G_OBJECT (item), "activate", callback, user_data);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

/* Folder popup menu callbacks */

static void
new_addressbook_cb (GtkWidget *widget, AddressbookComponent *comp)
{
	addressbook_config_create_new_source (gtk_widget_get_toplevel (widget));
}

static void
edit_addressbook_cb (GtkWidget *widget, AddressbookComponent *comp)
{
	AddressbookComponentPrivate *priv;
	ESource *selected_source;

	priv = comp->priv;

	selected_source =
		e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->source_selector));
	if (!selected_source)
		return;

	addressbook_config_edit_source (gtk_widget_get_toplevel (widget), selected_source);
}

static void
delete_addressbook_cb (GtkWidget *widget, AddressbookComponent *comp)
{
}

/* Callbacks.  */

static void
primary_source_selection_changed_callback (ESourceSelector *selector,
					   BonoboControl *view_control)
{
	load_uri_for_selection (selector, view_control);
}

static void
fill_popup_menu_callback (ESourceSelector *selector, GtkMenu *menu, AddressbookComponent *comp)
{
	add_popup_menu_item (menu, _("New Addressbook"), NULL, G_CALLBACK (new_addressbook_cb), comp);
	add_popup_menu_item (menu, _("Properties..."), NULL, G_CALLBACK (edit_addressbook_cb), comp);
	add_popup_menu_item (menu, _("Delete"), GTK_STOCK_DELETE, G_CALLBACK (delete_addressbook_cb), comp);
	add_popup_menu_item (menu, _("Rename"), NULL, NULL, NULL);
}

/* Evolution::Component CORBA methods.  */

static void
impl_createControls (PortableServer_Servant servant,
		     Bonobo_Control *corba_sidebar_control,
		     Bonobo_Control *corba_view_control,
		     Bonobo_Control *corba_statusbar_control,
		     CORBA_Environment *ev)
{
	AddressbookComponent *addressbook_component = ADDRESSBOOK_COMPONENT (bonobo_object_from_servant (servant));
	GtkWidget *selector;
	GtkWidget *selector_scrolled_window;
	GtkWidget *statusbar_widget;
	BonoboControl *sidebar_control;
	BonoboControl *view_control;
	BonoboControl *statusbar_control;

	selector = e_source_selector_new (addressbook_component->priv->source_list);
	e_source_selector_show_selection (E_SOURCE_SELECTOR (selector), FALSE);
	gtk_widget_show (selector);

	addressbook_component->priv->source_selector = selector;

	selector_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (selector_scrolled_window), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (selector_scrolled_window), selector);
	gtk_widget_show (selector_scrolled_window);

	sidebar_control = bonobo_control_new (selector_scrolled_window);

	view_control = addressbook_new_control ();
	g_signal_connect_object (selector, "primary_selection_changed",
				 G_CALLBACK (primary_source_selection_changed_callback),
				 G_OBJECT (view_control), 0);
	g_signal_connect_object (selector, "fill_popup_menu",
				 G_CALLBACK (fill_popup_menu_callback),
				 G_OBJECT (addressbook_component), 0);
	load_uri_for_selection (E_SOURCE_SELECTOR (selector), view_control);

	statusbar_widget = e_task_bar_new ();
	gtk_widget_show (statusbar_widget);
	statusbar_control = bonobo_control_new (statusbar_widget);

	e_activity_handler_attach_task_bar (addressbook_component->priv->activity_handler,
					    E_TASK_BAR (statusbar_widget));

	*corba_sidebar_control = CORBA_Object_duplicate (BONOBO_OBJREF (sidebar_control), ev);
	*corba_view_control = CORBA_Object_duplicate (BONOBO_OBJREF (view_control), ev);
	*corba_statusbar_control = CORBA_Object_duplicate (BONOBO_OBJREF (statusbar_control), ev);
}

static GNOME_Evolution_CreatableItemTypeList *
impl__get_userCreatableItems (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	GNOME_Evolution_CreatableItemTypeList *list = GNOME_Evolution_CreatableItemTypeList__alloc ();

	list->_length  = 2;
	list->_maximum = list->_length;
	list->_buffer  = GNOME_Evolution_CreatableItemTypeList_allocbuf (list->_length);

	CORBA_sequence_set_release (list, FALSE);

	list->_buffer[0].id = "contact";
	list->_buffer[0].description = _("New Contact");
	list->_buffer[0].menuDescription = _("_Contact");
	list->_buffer[0].tooltip = _("Create a new contact");
	list->_buffer[0].menuShortcut = 'c';
	list->_buffer[0].iconName = "evolution-contacts-mini.png";

	list->_buffer[1].id = "contact_list";
	list->_buffer[1].description = _("New Contact List");
	list->_buffer[1].menuDescription = _("Contact _List");
	list->_buffer[1].tooltip = _("Create a new contact list");
	list->_buffer[1].menuShortcut = 'l';
	list->_buffer[1].iconName = "contact-list-16.png";

	return list;
}

static void
impl_requestCreateItem (PortableServer_Servant servant,
			const CORBA_char *item_type_name,
			CORBA_Environment *ev)
{
	AddressbookComponent *addressbook_component = ADDRESSBOOK_COMPONENT (bonobo_object_from_servant (servant));
	AddressbookComponentPrivate *priv;
	EBook *book;
	EContact *contact = e_contact_new ();
	ESource *selected_source;
	gchar *uri;

	priv = addressbook_component->priv;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->source_selector));
	if (!selected_source) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_Failed, NULL);
		return;
	}

	uri = e_source_get_uri (selected_source);
	if (!uri) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_Failed, NULL);
		return;
	}

	book = e_book_new ();
	if (!e_book_load_uri (book, uri, TRUE, NULL)) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_Failed, NULL);
		g_object_unref (book);
		g_free (uri);
		return;
	}

	contact = e_contact_new ();

	if (!item_type_name) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_UnknownType, NULL);
	} else if (!strcmp (item_type_name, "contact")) {
		eab_show_contact_editor (book, contact, TRUE, TRUE);
	} else if (!strcmp (item_type_name, "contact_list")) {
		eab_show_contact_list_editor (book, contact, TRUE, TRUE);
	} else {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_UnknownType, NULL);
	}

	g_object_unref (book);
	g_object_unref (contact);
	g_free (uri);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	AddressbookComponentPrivate *priv = ADDRESSBOOK_COMPONENT (object)->priv;

	if (priv->source_selector != NULL) {
		g_object_unref (priv->source_selector);
		priv->source_selector = NULL;
	}

	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}

	if (priv->gconf_client != NULL) {
		g_object_unref (priv->gconf_client);
		priv->gconf_client = NULL;
	}

	if (priv->activity_handler != NULL) {
		g_object_unref (priv->activity_handler);
		priv->activity_handler = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	AddressbookComponentPrivate *priv = ADDRESSBOOK_COMPONENT (object)->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Initialization.  */

static void
addressbook_component_class_init (AddressbookComponentClass *class)
{
	POA_GNOME_Evolution_Component__epv *epv = &class->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	epv->createControls          = impl_createControls;
	epv->_get_userCreatableItems = impl__get_userCreatableItems;
	epv->requestCreateItem       = impl_requestCreateItem;

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_peek_parent (class);
}

static void
addressbook_component_init (AddressbookComponent *component)
{
	AddressbookComponentPrivate *priv;
	GSList *groups;

	priv = g_new0 (AddressbookComponentPrivate, 1);

	/* EPFIXME: Should use a custom one instead?  Also we should add
	   addressbook_component_peek_gconf_client().  */
	priv->gconf_client = gconf_client_get_default ();

	priv->source_list = e_source_list_new_for_gconf (priv->gconf_client,
							 "/apps/evolution/addressbook/sources");

	priv->activity_handler = e_activity_handler_new ();

	/* Create default addressbooks if there are no groups */
	groups = e_source_list_peek_groups (priv->source_list);
	if (!groups) {
		ESourceGroup *group;
		ESource *source;
		char *base_uri, *base_uri_proto, *new_dir;

		/* create the local source group */
		base_uri = g_build_filename (g_get_home_dir (),
					     "/.evolution/addressbook/local/OnThisComputer/",
					     NULL);

		base_uri_proto = g_strconcat ("file://", base_uri, NULL);

		group = e_source_group_new (_("On This Computer"), base_uri_proto);
		e_source_list_add_group (priv->source_list, group, -1);

		g_free (base_uri_proto);

		/* FIXME: Migrate addressbooks from older setup? */

		/* Create default addressbooks */
		new_dir = g_build_filename (base_uri, "Personal/", NULL);
		if (!e_mkdir_hier (new_dir, 0700)) {
			source = e_source_new (_("Personal"), "Personal");
			e_source_group_add_source (group, source, -1);
		}
		g_free (new_dir);

		new_dir = g_build_filename (base_uri, "Work/", NULL);
		if (!e_mkdir_hier (new_dir, 0700)) {
			source = e_source_new (_("Work"), "Work");
			e_source_group_add_source (group, source, -1);
		}
		g_free (new_dir);

		g_free (base_uri);

		/* Create the LDAP source group */
		group = e_source_group_new (_("On LDAP Servers"), "ldap://");
		e_source_list_add_group (priv->source_list, group, -1);
	}

	component->priv = priv;
}


/* Public API.  */

AddressbookComponent *
addressbook_component_peek (void)
{
	static AddressbookComponent *component = NULL;

	if (component == NULL)
		component = g_object_new (addressbook_component_get_type (), NULL);

	return component;
}


EActivityHandler *
addressbook_component_peek_activity_handler (AddressbookComponent *component)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_COMPONENT (component), NULL);

	return component->priv->activity_handler;
}


BONOBO_TYPE_FUNC_FULL (AddressbookComponent, GNOME_Evolution_Component, PARENT_TYPE, addressbook_component)
