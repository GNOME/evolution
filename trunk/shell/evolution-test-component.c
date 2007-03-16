/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* tasks-component.c
 *
 * Copyright (C) 2004  Novell, Inc.
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
 * Author: JP Rosevear <jpr@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <bonobo/bonobo-shlib-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-exception.h>
#include <gtk/gtklabel.h>
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

static CORBA_boolean
impl_upgradeFromVersion (PortableServer_Servant servant,
			 CORBA_short major,
			 CORBA_short minor,
			 CORBA_short revision,
			 CORBA_Environment *ev)
{
	EvolutionTestComponent *component = EVOLUTION_TEST_COMPONENT (bonobo_object_from_servant (servant));
	EvolutionTestComponentPrivate *priv;

	priv = component->priv;

	g_message ("Upgrading from %d.%d.%d", major, minor, revision);

	return CORBA_TRUE;
}

static void
impl_createControls (PortableServer_Servant servant,
		     Bonobo_Control *corba_sidebar_control,
		     Bonobo_Control *corba_view_control,
		     Bonobo_Control *corba_statusbar_control,
		     CORBA_Environment *ev)
{
	EvolutionTestComponent *component = EVOLUTION_TEST_COMPONENT (bonobo_object_from_servant (servant));
	EvolutionTestComponentPrivate *priv;
	GtkWidget *label, *bar;
	
	priv = component->priv;

	/* Sidebar */
	label = gtk_label_new ("Side Bar Control");
	gtk_widget_show (label);
	priv->sidebar_control = bonobo_control_new (label);

	/* View */
	label = gtk_label_new ("View Control");
	gtk_widget_show (label);
 	priv->view_control = bonobo_control_new (label);

	/* Status bar */
	bar = e_task_bar_new ();
	gtk_widget_show (bar);	
	priv->status_control =  bonobo_control_new (bar);

	/* Return the controls */
	*corba_sidebar_control = CORBA_Object_duplicate (BONOBO_OBJREF (priv->sidebar_control), ev);
	*corba_view_control = CORBA_Object_duplicate (BONOBO_OBJREF (priv->view_control), ev);
	*corba_statusbar_control = CORBA_Object_duplicate (BONOBO_OBJREF (priv->status_control), ev);
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
	list->_buffer[0].menuDescription = _("_Test");
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
	epv->createControls          = impl_createControls;
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
	 const char *component_id,
	 void *closure)
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
