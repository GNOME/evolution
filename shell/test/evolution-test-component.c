/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <glib/gi18n.h>
#include <bonobo/bonobo-shlib-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <gtk/gtk.h>
#include "e-task-bar.h"
#include "evolution-test-component.h"

#define FACTORY_ID "OAFIID:GNOME_Evolution_Test_Factory:" BASE_VERSION
#define TEST_COMPONENT_ID  "OAFIID:GNOME_Evolution_Test_Component:" BASE_VERSION
#define CREATE_TEST_ID      "test"

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

struct _EvolutionTestComponentPrivate {
	BonoboControl *view_control;
	BonoboControl *sidebar_control;
	BonoboControl *status_control;
};

/* GObject methods */

static void
impl_dispose (GObject *object)
{
	EvolutionTestComponentPrivate *priv;

	priv = EVOLUTION_TEST_COMPONENT (object)->priv;

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EvolutionTestComponentPrivate *priv = EVOLUTION_TEST_COMPONENT (object)->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Evolution::Component CORBA methods */

static void
impl_upgradeFromVersion (PortableServer_Servant servant,
			 const CORBA_short major,
			 const CORBA_short minor,
			 const CORBA_short revision,
			 CORBA_Environment *ev)
{
	EvolutionTestComponent *component = EVOLUTION_TEST_COMPONENT (bonobo_object_from_servant (servant));
	EvolutionTestComponentPrivate *priv;

	priv = component->priv;

	g_message ("Upgrading from %d.%d.%d", major, minor, revision);
}

static GNOME_Evolution_CreatableItemTypeList *
impl__get_userCreatableItems (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	GNOME_Evolution_CreatableItemTypeList *list = GNOME_Evolution_CreatableItemTypeList__alloc ();

	list->_length  = 1;
	list->_maximum = list->_length;
	list->_buffer  = GNOME_Evolution_CreatableItemTypeList_allocbuf (list->_length);

	CORBA_sequence_set_release (list, FALSE);

	list->_buffer[0].id = CREATE_TEST_ID;
	list->_buffer[0].description = _("New Test");
	list->_buffer[0].menuDescription = (gchar *) C_("New", "_Test");
	list->_buffer[0].tooltip = _("Create a new test item");
	list->_buffer[0].menuShortcut = 'i';
	list->_buffer[0].iconName = "";

	return list;
}

static void
impl_requestCreateItem (PortableServer_Servant servant,
			const CORBA_char *item_type_name,
			CORBA_Environment *ev)
{
	EvolutionTestComponent *evolution_test_component = EVOLUTION_TEST_COMPONENT (bonobo_object_from_servant (servant));
	EvolutionTestComponentPrivate *priv;

	priv = evolution_test_component->priv;

	if (strcmp (item_type_name, CREATE_TEST_ID) == 0) {
		g_message ("Creating test item");
	} else {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_UnknownType);
		return;
	}
}

/* Initialization */

static void
evolution_test_component_class_init (EvolutionTestComponentClass *klass)
{
	POA_GNOME_Evolution_Component__epv *epv = &klass->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	epv->upgradeFromVersion      = impl_upgradeFromVersion;
	epv->_get_userCreatableItems = impl__get_userCreatableItems;
	epv->requestCreateItem       = impl_requestCreateItem;

	object_class->dispose = impl_dispose;
	object_class->finalize = impl_finalize;
}

static void
evolution_test_component_init (EvolutionTestComponent *component, EvolutionTestComponentClass *klass)
{
	EvolutionTestComponentPrivate *priv;

	priv = g_new0 (EvolutionTestComponentPrivate, 1);

	component->priv = priv;
}

BONOBO_TYPE_FUNC_FULL (EvolutionTestComponent, GNOME_Evolution_Component, PARENT_TYPE, evolution_test_component)

static BonoboObject *
factory (BonoboGenericFactory *factory,
	 const gchar *component_id,
	 gpointer closure)
{
	if (strcmp (component_id, TEST_COMPONENT_ID) == 0) {
		BonoboObject *object = BONOBO_OBJECT (g_object_new (EVOLUTION_TEST_TYPE_COMPONENT, NULL));
		bonobo_object_ref (object);
		return object;
	}

	g_warning (FACTORY_ID ": Don't know what to do with %s", component_id);

	return NULL;
}

BONOBO_ACTIVATION_SHLIB_FACTORY (FACTORY_ID, "Evolution Calendar component factory", factory, NULL)
