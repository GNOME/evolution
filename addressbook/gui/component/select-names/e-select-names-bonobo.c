/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-select-names-bonobo.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-control.h>

#include "Evolution-Addressbook-SelectNames.h"

#include "e-util/e-util.h"
#include "e-select-names-manager.h"

#include "e-select-names-bonobo.h"


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

struct _ESelectNamesBonoboPrivate {
	ESelectNamesManager *manager;
};

enum _EntryPropertyID {
	ENTRY_PROPERTY_ID_TEXT
};
typedef enum _EntryPropertyID EntryPropertyID;


/* PropertyBag implementation for the entry widgets.  */

static void
entry_get_property_fn (BonoboPropertyBag *bag,
		       BonoboArg *arg,
		       unsigned int arg_id,
		       void *user_data)
{
	GtkWidget *widget;
	char *text;

	widget = GTK_WIDGET (user_data);

	switch (arg_id) {
	case ENTRY_PROPERTY_ID_TEXT:
		gtk_object_get (GTK_OBJECT (widget), "text", &text, NULL);
		BONOBO_ARG_SET_STRING (arg, text);
		break;
	default:
		break;
	}
}


/* CORBA interface implementation.  */

static POA_Evolution_Addressbook_SelectNames__vepv SelectNames_vepv;

static POA_Evolution_Addressbook_SelectNames *
create_servant (void)
{
	POA_Evolution_Addressbook_SelectNames *servant;
	CORBA_Environment ev;

	servant = (POA_Evolution_Addressbook_SelectNames *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &SelectNames_vepv;

	CORBA_exception_init (&ev);

	POA_Evolution_Addressbook_SelectNames__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}

static void
impl_SelectNames_add_section (PortableServer_Servant servant,
			      const CORBA_char *id,
			      const CORBA_char *title,
			      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ESelectNamesBonobo *select_names;
	ESelectNamesBonoboPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	select_names = E_SELECT_NAMES_BONOBO (bonobo_object);
	priv = select_names->priv;

	e_select_names_manager_add_section (priv->manager, id, title);
}

static Bonobo_Control
impl_SelectNames_get_entry_for_section (PortableServer_Servant servant,
					const CORBA_char *section_id,
					CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ESelectNamesBonobo *select_names;
	ESelectNamesBonoboPrivate *priv;
	GtkWidget *entry_widget;
	BonoboControl *control;
	BonoboPropertyBag *property_bag;

	bonobo_object = bonobo_object_from_servant (servant);
	select_names = E_SELECT_NAMES_BONOBO (bonobo_object);
	priv = select_names->priv;

	entry_widget = e_select_names_manager_create_entry (priv->manager, section_id);
	gtk_widget_show (entry_widget);

	if (entry_widget == NULL) {
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_Evolution_Addressbook_SelectNames_SectionNotFound,
				     NULL);
		return CORBA_OBJECT_NIL;
	}

	control = bonobo_control_new (entry_widget);

	property_bag = bonobo_property_bag_new (entry_get_property_fn, NULL, entry_widget);
	bonobo_property_bag_add (property_bag, "text", ENTRY_PROPERTY_ID_TEXT,
				 BONOBO_ARG_STRING, NULL, NULL, BONOBO_PROPERTY_READABLE);

	bonobo_control_set_property_bag (control, property_bag);

	return bonobo_object_corba_objref (BONOBO_OBJECT (control));
}

static void
impl_SelectNames_activate_dialog (PortableServer_Servant servant,
				  const CORBA_char *section_id,
				  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ESelectNamesBonobo *select_names;
	ESelectNamesBonoboPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	select_names = E_SELECT_NAMES_BONOBO (bonobo_object);
	priv = select_names->priv;

	e_select_names_manager_activate_dialog (priv->manager, section_id);
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	ESelectNamesBonobo *select_names;
	ESelectNamesBonoboPrivate *priv;

	select_names = E_SELECT_NAMES_BONOBO (object);
	priv = select_names->priv;

	gtk_object_unref (GTK_OBJECT (priv->manager));

	g_free (priv);
}


static void
corba_class_init ()
{
	POA_Evolution_Addressbook_SelectNames__vepv *vepv;
	POA_Evolution_Addressbook_SelectNames__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_Evolution_Addressbook_SelectNames__epv, 1);
	epv->add_section           = impl_SelectNames_add_section;
	epv->get_entry_for_section = impl_SelectNames_get_entry_for_section;
	epv->activate_dialog       = impl_SelectNames_activate_dialog;

	vepv = &SelectNames_vepv;
	vepv->Bonobo_Unknown_epv                    = bonobo_object_get_epv ();
	vepv->Evolution_Addressbook_SelectNames_epv = epv;
}

static void
class_init (ESelectNamesBonoboClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	parent_class = gtk_type_class (bonobo_object_get_type ());

	object_class->destroy = impl_destroy;

	corba_class_init ();
}

static void
init (ESelectNamesBonobo *select_names)
{
	ESelectNamesBonoboPrivate *priv;

	priv = g_new (ESelectNamesBonoboPrivate, 1);

	priv->manager = e_select_names_manager_new ();

	select_names->priv = priv;
}


void
e_select_names_bonobo_construct (ESelectNamesBonobo *select_names,
				 Evolution_Addressbook_SelectNames corba_object)
{
	g_return_if_fail (select_names != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_BONOBO (select_names));

	bonobo_object_construct (BONOBO_OBJECT (select_names), corba_object);
}

ESelectNamesBonobo *
e_select_names_bonobo_new (void)
{
	POA_Evolution_Addressbook_SelectNames *servant;
	Evolution_Addressbook_SelectNames corba_object;
	ESelectNamesBonobo *select_names;

	servant = create_servant ();
	if (servant == NULL)
		return NULL;
	
	select_names = gtk_type_new (e_select_names_bonobo_get_type ());

	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (select_names), servant);
	e_select_names_bonobo_construct (select_names, corba_object);

	return select_names;
}


E_MAKE_TYPE (e_select_names_bonobo, "ESelectNamesBonobo", ESelectNamesBonobo, class_init, init, PARENT_TYPE)
