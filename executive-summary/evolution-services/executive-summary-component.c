/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* executive-summary-component.c - Bonobo implementation of 
 *                                 SummaryComponent.idl
 *
 * Authors: Iain Holmes <iain@helixcode.com>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo.h>
#include <gnome.h>
#include <gal/util/e-util.h>

#include "Executive-Summary.h"
#include "executive-summary-component.h"

static void executive_summary_component_destroy (GtkObject *object);
static void executive_summary_component_init (ExecutiveSummaryComponent *component);
static void executive_summary_component_class_init (ExecutiveSummaryComponentClass *esc_class);

#define PARENT_TYPE (bonobo_object_get_type ())
#define FACTORY_PARENT_TYPE (bonobo_object_get_type ())

static BonoboObjectClass *parent_class;
static BonoboObjectClass *factory_parent_class;

struct _ExecutiveSummaryComponentPrivate {
	int dummy;
};

struct _ExecutiveSummaryComponentFactoryPrivate {
	EvolutionServicesCreateViewFn create_view;
	void *closure;
};

/* CORBA interface */
static POA_GNOME_Evolution_Summary_Component__vepv SummaryComponent_vepv;
static POA_GNOME_Evolution_Summary_ComponentFactory__vepv ComponentFactory_vepv;

static POA_GNOME_Evolution_Summary_Component *
create_servant (void)
{
	POA_GNOME_Evolution_Summary_Component *servant;
	CORBA_Environment ev;
	
	servant = (POA_GNOME_Evolution_Summary_Component *)g_new0 (BonoboObjectServant, 1);
	servant->vepv = &SummaryComponent_vepv;
	
	CORBA_exception_init (&ev);
	POA_GNOME_Evolution_Summary_Component__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}
	
	CORBA_exception_free (&ev);
	
	return servant;
}

static void
executive_summary_component_destroy (GtkObject *object)
{
	ExecutiveSummaryComponent *component;
	ExecutiveSummaryComponentPrivate *priv;
	
	component = EXECUTIVE_SUMMARY_COMPONENT (object);
	priv = component->private;
	
	if (priv == NULL)
		return;
	
	g_free (priv);
	component->private = NULL;
	
	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* Init */
static void
corba_class_init (void)
{
	POA_GNOME_Evolution_Summary_Component__vepv *vepv;
	POA_GNOME_Evolution_Summary_Component__epv *epv;
	PortableServer_ServantBase__epv *base_epv;
	
	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private = NULL;
	base_epv->finalize = NULL;
	base_epv->default_POA = NULL;
	
	epv = g_new0 (POA_GNOME_Evolution_Summary_Component__epv, 1);
		
	vepv = &SummaryComponent_vepv;
	vepv->_base_epv = base_epv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_Summary_Component_epv = epv;
}

static void
executive_summary_component_class_init (ExecutiveSummaryComponentClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = executive_summary_component_destroy;

	parent_class = gtk_type_class (PARENT_TYPE);
	
	corba_class_init ();
}

static void
executive_summary_component_init (ExecutiveSummaryComponent *component)
{
	ExecutiveSummaryComponentPrivate *priv;
	
	priv = g_new (ExecutiveSummaryComponentPrivate, 1);
	
	component->private = priv;
}

E_MAKE_TYPE (executive_summary_component, "ExecutiveSummaryComponent", 
	     ExecutiveSummaryComponent, executive_summary_component_class_init,
	     executive_summary_component_init, PARENT_TYPE);


static void
executive_summary_component_construct (ExecutiveSummaryComponent *component,
				       GNOME_Evolution_Summary_Component corba_object)
{
	g_return_if_fail (component != NULL);
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);
	
	bonobo_object_construct (BONOBO_OBJECT (component), corba_object);
}


/*** Public API ***/
/**
 * executive_summary_component_new:
 *
 * Creates a BonoboObject that implements the Summary::Component interface.
 *
 * Returns: A pointer to a BonoboObject.
 */
BonoboObject *
executive_summary_component_new (void)
{
	ExecutiveSummaryComponent *component;
	POA_GNOME_Evolution_Summary_Component *servant;
	GNOME_Evolution_Summary_Component corba_object;
	
	servant = create_servant ();
	if (servant == NULL)
		return NULL;
	
	component = gtk_type_new (executive_summary_component_get_type ());
	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (component),
						       servant);
	
	executive_summary_component_construct (component, corba_object);
	
	return BONOBO_OBJECT (component);
}


/**** ComponentFactory implementation ****/

static POA_GNOME_Evolution_Summary_ComponentFactory *
create_factory_servant (void)
{
	POA_GNOME_Evolution_Summary_ComponentFactory *servant;
	CORBA_Environment ev;
	
	servant = (POA_GNOME_Evolution_Summary_ComponentFactory *)g_new0 (BonoboObjectServant, 1);
	servant->vepv = &ComponentFactory_vepv;
	
	CORBA_exception_init (&ev);
	POA_GNOME_Evolution_Summary_ComponentFactory__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}
	
	CORBA_exception_free (&ev);
	
	return servant;
}

static GNOME_Evolution_Summary_Component
impl_GNOME_Evolution_Summary_ComponentFactory_createView (PortableServer_Servant servant,
							  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	BonoboObject *view;
	ExecutiveSummaryComponentFactory *factory;
	ExecutiveSummaryComponentFactoryPrivate *priv;
	GNOME_Evolution_Summary_Component component, component_dup;

	bonobo_object = bonobo_object_from_servant (servant);
	factory = EXECUTIVE_SUMMARY_COMPONENT_FACTORY (bonobo_object);
	priv = factory->private;

	view = (* priv->create_view) (factory, priv->closure);
	g_return_val_if_fail (view != NULL, CORBA_OBJECT_NIL);

	component = bonobo_object_corba_objref (BONOBO_OBJECT (view));

	component_dup = CORBA_Object_duplicate (component, ev);

	return component_dup;
}

static void
corba_factory_init (void)
{
	POA_GNOME_Evolution_Summary_ComponentFactory__vepv *vepv;
	POA_GNOME_Evolution_Summary_ComponentFactory__epv *epv;
	PortableServer_ServantBase__epv *base_epv;
	
	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private = NULL;
	base_epv->finalize = NULL;
	base_epv->default_POA = NULL;
	
	epv = g_new0 (POA_GNOME_Evolution_Summary_ComponentFactory__epv, 1);
	epv->createView = impl_GNOME_Evolution_Summary_ComponentFactory_createView;

	vepv = &ComponentFactory_vepv;
	vepv->_base_epv = base_epv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_Summary_ComponentFactory_epv = epv;
}

/* GtkObject methods */
static void
executive_summary_component_factory_destroy (GtkObject *object)
{
	ExecutiveSummaryComponentFactory *factory;
	ExecutiveSummaryComponentFactoryPrivate *priv;

	factory = EXECUTIVE_SUMMARY_COMPONENT_FACTORY (object);
	priv = factory->private;

	if (priv == NULL)
		return;

	g_free (priv);
	factory->private = NULL;

	(* GTK_OBJECT_CLASS (factory_parent_class)->destroy) (object);
}

static void
executive_summary_component_factory_class_init (ExecutiveSummaryComponentFactoryClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = executive_summary_component_factory_destroy;

	factory_parent_class = gtk_type_class (FACTORY_PARENT_TYPE);
	corba_factory_init ();
}

static void
executive_summary_component_factory_init (ExecutiveSummaryComponentFactory *factory)
{
	ExecutiveSummaryComponentFactoryPrivate *priv;

	priv = g_new (ExecutiveSummaryComponentFactoryPrivate, 1);

	priv->create_view = NULL;
	priv->closure = NULL;
	factory->private = priv;
}

E_MAKE_TYPE (executive_summary_component_factory, 
	     "ExecutiveSummaryComponentFactory",
	     ExecutiveSummaryComponentFactory,
	     executive_summary_component_factory_class_init,
	     executive_summary_component_factory_init, FACTORY_PARENT_TYPE);

static void
executive_summary_component_factory_construct (ExecutiveSummaryComponentFactory *factory,
					       GNOME_Evolution_Summary_ComponentFactory corba_object,
					       EvolutionServicesCreateViewFn create_view,
					       void *closure)
{
	ExecutiveSummaryComponentFactoryPrivate *priv;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);

	bonobo_object_construct (BONOBO_OBJECT (factory), corba_object);
	priv = factory->private;

	priv->create_view = create_view;
	priv->closure = closure;
}


/*** Public API ***/
/**
 * executive_summary_component_factory_new:
 * @create_view: A pointer to the function to create a new view.
 * @closure: The data to be passed to the @create_view function when it is 
 * called.
 *
 * Creates a BonoboObject that implements the Summary::ComponentFactory
 * interface.
 *
 * Returns: A pointer to a BonoboObject.
 */
BonoboObject *
executive_summary_component_factory_new (EvolutionServicesCreateViewFn create_view,
					 void *closure)
{
	ExecutiveSummaryComponentFactory *factory;
	POA_GNOME_Evolution_Summary_ComponentFactory *servant;
	GNOME_Evolution_Summary_ComponentFactory corba_object;

	servant = create_factory_servant ();
	if (servant == NULL)
		return NULL;

	factory = gtk_type_new (executive_summary_component_factory_get_type ());
	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (factory),
						       servant);
	executive_summary_component_factory_construct (factory, corba_object,
						       create_view, closure);
	return BONOBO_OBJECT (factory);
}
	
