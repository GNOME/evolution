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
#include "addressbook-config.h"
#include "addressbook-migrate.h"
#include "addressbook-view.h"
#include "addressbook/gui/contact-editor/eab-editor.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "e-util/e-plugin.h"
#include "addressbook/gui/widgets/eab-popup.h"
#include "addressbook/gui/widgets/eab-menu.h"

#include "widgets/misc/e-task-bar.h"
#include "widgets/misc/e-info-label.h"

#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkimage.h>
#include <gconf/gconf-client.h>
#include <gal/util/e-util.h>

#ifdef ENABLE_SMIME
#include "smime/gui/component.h"
#endif


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

struct _AddressbookComponentPrivate {
	GConfClient *gconf_client;
	char *base_directory;
};

/* Evolution::Component CORBA methods.  */

static void
impl_createControls (PortableServer_Servant servant,
		     Bonobo_Control *corba_sidebar_control,
		     Bonobo_Control *corba_view_control,
		     Bonobo_Control *corba_statusbar_control,
		     CORBA_Environment *ev)
{
	AddressbookView *view = addressbook_view_new ();
	BonoboControl *sidebar_control;
	BonoboControl *view_control;
	BonoboControl *statusbar_control;

	sidebar_control = bonobo_control_new (addressbook_view_peek_sidebar (view));
	view_control = addressbook_view_peek_folder_view (view);
	statusbar_control = bonobo_control_new (addressbook_view_peek_statusbar (view));

	*corba_sidebar_control = CORBA_Object_duplicate (BONOBO_OBJREF (sidebar_control), ev);
	*corba_view_control = CORBA_Object_duplicate (BONOBO_OBJREF (view_control), ev);
	*corba_statusbar_control = CORBA_Object_duplicate (BONOBO_OBJREF (statusbar_control), ev);
}

static GNOME_Evolution_CreatableItemTypeList *
impl__get_userCreatableItems (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	GNOME_Evolution_CreatableItemTypeList *list = GNOME_Evolution_CreatableItemTypeList__alloc ();

	list->_length  = 3;
	list->_maximum = list->_length;
	list->_buffer  = GNOME_Evolution_CreatableItemTypeList_allocbuf (list->_length);

	CORBA_sequence_set_release (list, FALSE);

	list->_buffer[0].id = "contact";
	list->_buffer[0].description = _("New Contact");
	list->_buffer[0].menuDescription = _("_Contact");
	list->_buffer[0].tooltip = _("Create a new contact");
	list->_buffer[0].menuShortcut = 'c';
	list->_buffer[0].iconName = "stock_contact";
	list->_buffer[0].type = GNOME_Evolution_CREATABLE_OBJECT;

	list->_buffer[1].id = "contact_list";
	list->_buffer[1].description = _("New Contact List");
	list->_buffer[1].menuDescription = _("Contact _List");
	list->_buffer[1].tooltip = _("Create a new contact list");
	list->_buffer[1].menuShortcut = 'l';
	list->_buffer[1].iconName = "stock_contact-list";
	list->_buffer[1].type = GNOME_Evolution_CREATABLE_OBJECT;

	list->_buffer[2].id = "address_book";
	list->_buffer[2].description = _("New Address Book");
	list->_buffer[2].menuDescription = _("Address _Book");
	list->_buffer[2].tooltip = _("Create a new address book");
	list->_buffer[2].menuShortcut = 'b';
	list->_buffer[2].iconName = "stock_addressbook";
	list->_buffer[2].type = GNOME_Evolution_CREATABLE_FOLDER;

	return list;
}

static void
book_loaded_cb (EBook *book, EBookStatus status, gpointer data)
{
	EContact *contact;
	char *item_type_name = data;

	if (status != E_BOOK_ERROR_OK) {
		/* XXX we really need a dialog here, but we don't have
		   access to the ESource so we can't use
		   eab_load_error_dialog.  fun! */
		return;
	}

	contact = e_contact_new ();

	if (!strcmp (item_type_name, "contact")) {
		eab_show_contact_editor (book, contact, TRUE, TRUE);
	}
	else if (!strcmp (item_type_name, "contact_list")) {
		eab_show_contact_list_editor (book, contact, TRUE, TRUE);
	}

	g_object_unref (book);
	g_object_unref (contact);

	g_free (item_type_name);
}

static void
impl_requestCreateItem (PortableServer_Servant servant,
			const CORBA_char *item_type_name,
			CORBA_Environment *ev)
{
	AddressbookComponent *addressbook_component = ADDRESSBOOK_COMPONENT (bonobo_object_from_servant (servant));
	AddressbookComponentPrivate *priv;
	EBook *book;

	priv = addressbook_component->priv;

	if (!item_type_name ||
	    (strcmp (item_type_name, "address_book") &&
	     strcmp (item_type_name, "contact") &&
	     strcmp (item_type_name, "contact_list"))) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_UnknownType, NULL);
		return;
	}

	if (!strcmp (item_type_name, "address_book")) {
		addressbook_config_create_new_source (NULL);
		return;
	}

	book = e_book_new_default_addressbook (NULL);
	e_book_async_open (book, FALSE, book_loaded_cb, g_strdup (item_type_name));
}

static void
impl_upgradeFromVersion (PortableServer_Servant servant, short major, short minor, short revision, CORBA_Environment *ev)
{
	GError *err = NULL;

	if (!addressbook_migrate (addressbook_component_peek (), major, minor, revision, &err)) {
		GNOME_Evolution_Component_UpgradeFailed *failedex;

		failedex = GNOME_Evolution_Component_UpgradeFailed__alloc();
		failedex->what = CORBA_string_dup(_("Failed upgrading Addressbook settings or folders."));
		failedex->why = CORBA_string_dup(err->message);
		CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_UpgradeFailed, failedex);
	}

	if (err)
		g_error_free(err);
}

static CORBA_boolean
impl_requestQuit (PortableServer_Servant servant, CORBA_Environment *ev)
{
	return eab_editor_request_close_all ();
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	AddressbookComponentPrivate *priv = ADDRESSBOOK_COMPONENT (object)->priv;

	if (priv->gconf_client != NULL) {
		g_object_unref (priv->gconf_client);
		priv->gconf_client = NULL;
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
	epv->upgradeFromVersion      = impl_upgradeFromVersion;
	epv->requestQuit             = impl_requestQuit;

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_peek_parent (class);
}

static void
addressbook_component_init (AddressbookComponent *component)
{
	AddressbookComponentPrivate *priv;
	static int first = TRUE;

	priv = g_new0 (AddressbookComponentPrivate, 1);

	/* EPFIXME: Should use a custom one instead? */
	priv->gconf_client = gconf_client_get_default ();

	priv->base_directory = g_build_filename (g_get_home_dir (), ".evolution", NULL);

	component->priv = priv;

#ifdef ENABLE_SMIME
	smime_component_init ();
#endif

	if (first) {
		first = FALSE;
		e_plugin_hook_register_type(eab_popup_hook_get_type());
		e_plugin_hook_register_type(eab_menu_hook_get_type());
	}
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

GConfClient*
addressbook_component_peek_gconf_client (AddressbookComponent *component)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_COMPONENT (component), NULL);

	return component->priv->gconf_client;
}

const char *
addressbook_component_peek_base_directory (AddressbookComponent *component)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_COMPONENT (component), NULL);

	return component->priv->base_directory;
}

BONOBO_TYPE_FUNC_FULL (AddressbookComponent, GNOME_Evolution_Component, PARENT_TYPE, addressbook_component)
