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

#include <gal/util/e-util.h>
#include <gal/e-text/e-entry.h>
#include "e-select-names-manager.h"
#include "e-select-names-model.h"
#include "e-select-names-text-model.h"

#include "e-select-names-bonobo.h"


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

struct _ESelectNamesBonoboPrivate {
	ESelectNamesManager *manager;
};

enum _EntryPropertyID {
	ENTRY_PROPERTY_ID_TEXT,
	ENTRY_PROPERTY_ID_ENTRY_CHANGED
};
typedef enum _EntryPropertyID EntryPropertyID;


/* PropertyBag implementation for the entry widgets.  */

static void
entry_get_property_fn (BonoboPropertyBag *bag,
		       BonoboArg *arg,
		       unsigned int arg_id,
		       CORBA_Environment *ev,
		       void *user_data)
{
	GtkWidget *w;
	const char *text;

	w = GTK_WIDGET (user_data);

	switch (arg_id) {
	case ENTRY_PROPERTY_ID_TEXT:
		{
			ESelectNamesTextModel *text_model;
			ESelectNamesModel *model;

			gtk_object_get(GTK_OBJECT(w),
				       "model", &text_model,
				       NULL);
			gtk_object_get(GTK_OBJECT(text_model),
				       "source", &model,
				       NULL);
			text = e_select_names_model_get_address_text (model);
			BONOBO_ARG_SET_STRING (arg, text);
		}
		break;
	default:
		break;
	}
}

static void
entry_set_property_fn (BonoboPropertyBag *bag,
		       const BonoboArg *arg,
		       guint arg_id,
		       CORBA_Environment *ev,
		       gpointer user_data)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (user_data);

	switch (arg_id) {

	case ENTRY_PROPERTY_ID_TEXT:
		e_entry_set_text (E_ENTRY (widget), BONOBO_ARG_GET_STRING (arg));
		break;

	case ENTRY_PROPERTY_ID_ENTRY_CHANGED:
		gtk_object_set_data (GTK_OBJECT (widget), "entry_property_id_changed", GUINT_TO_POINTER (1));
		break;
	default:
		break;
	}
}


/* CORBA interface implementation.  */

static POA_GNOME_Evolution_Addressbook_SelectNames__vepv SelectNames_vepv;

static POA_GNOME_Evolution_Addressbook_SelectNames *
create_servant (void)
{
	POA_GNOME_Evolution_Addressbook_SelectNames *servant;
	CORBA_Environment ev;

	servant = (POA_GNOME_Evolution_Addressbook_SelectNames *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &SelectNames_vepv;

	CORBA_exception_init (&ev);

	POA_GNOME_Evolution_Addressbook_SelectNames__init ((PortableServer_Servant) servant, &ev);
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

static void
entry_changed (GtkWidget *widget, BonoboControl *control)
{
	gboolean changed = GPOINTER_TO_UINT (gtk_object_get_data (GTK_OBJECT (widget), "entry_property_id_changed"));

	if (!changed)
		bonobo_control_set_property (control, "entry_changed", TRUE, NULL);
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
				     ex_GNOME_Evolution_Addressbook_SelectNames_SectionNotFound,
				     NULL);
		return CORBA_OBJECT_NIL;
	}

	control = bonobo_control_new (entry_widget);

	property_bag = bonobo_property_bag_new (entry_get_property_fn, entry_set_property_fn, entry_widget);
	bonobo_property_bag_add (property_bag, "text", ENTRY_PROPERTY_ID_TEXT,
				 BONOBO_ARG_STRING, NULL, NULL,
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (property_bag, "entry_changed", ENTRY_PROPERTY_ID_ENTRY_CHANGED,
				 BONOBO_ARG_BOOLEAN, NULL, NULL,
				 BONOBO_PROPERTY_WRITEABLE);

	bonobo_control_set_properties (control, property_bag);

	gtk_signal_connect (GTK_OBJECT (entry_widget), "changed",
			    GTK_SIGNAL_FUNC (entry_changed), control);

	return CORBA_Object_duplicate (bonobo_object_corba_objref (BONOBO_OBJECT (control)), ev);
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
	POA_GNOME_Evolution_Addressbook_SelectNames__vepv *vepv;
	POA_GNOME_Evolution_Addressbook_SelectNames__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_GNOME_Evolution_Addressbook_SelectNames__epv, 1);
	epv->addSection        = impl_SelectNames_add_section;
	epv->getEntryBySection = impl_SelectNames_get_entry_for_section;
	epv->activateDialog    = impl_SelectNames_activate_dialog;

	vepv = &SelectNames_vepv;
	vepv->Bonobo_Unknown_epv                    = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_Addressbook_SelectNames_epv = epv;
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
				 GNOME_Evolution_Addressbook_SelectNames corba_object)
{
	g_return_if_fail (select_names != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_BONOBO (select_names));

	bonobo_object_construct (BONOBO_OBJECT (select_names), corba_object);
}

ESelectNamesBonobo *
e_select_names_bonobo_new (void)
{
	POA_GNOME_Evolution_Addressbook_SelectNames *servant;
	GNOME_Evolution_Addressbook_SelectNames corba_object;
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
