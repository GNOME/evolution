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
#include "executive-summary.h"
#include "executive-summary-component.h"
#include "executive-summary-client.h"

static void executive_summary_component_destroy (GtkObject *object);
static void executive_summary_component_init (ExecutiveSummaryComponent *component);
static void executive_summary_component_class_init (ExecutiveSummaryComponentClass *esc_class);

#define PARENT_TYPE (bonobo_object_get_type ())

static BonoboObjectClass *parent_class;

struct _ExecutiveSummaryComponentPrivate {
	EvolutionServicesCreateBonoboViewFn create_bonobo_view;
	EvolutionServicesCreateHtmlViewFn create_html_view;
	EvolutionServicesConfigureFn configure;
	
	ExecutiveSummaryClient *owner_client;
	
	void *closure;
};

/* CORBA interface */
static POA_Evolution_SummaryComponent__vepv SummaryComponent_vepv;

static POA_Evolution_SummaryComponent *
create_servant (void)
{
	POA_Evolution_SummaryComponent *servant;
	CORBA_Environment ev;
	
	servant = (POA_Evolution_SummaryComponent *)g_new0 (BonoboObjectServant, 1);
	servant->vepv = &SummaryComponent_vepv;
	
	CORBA_exception_init (&ev);
	POA_Evolution_SummaryComponent__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}
	
	CORBA_exception_free (&ev);
	
	return servant;
}

static void
impl_Evolution_SummaryComponent_supports (PortableServer_Servant servant,
					  CORBA_boolean *html,
					  CORBA_boolean *bonobo,
					  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummaryComponent *component;
	ExecutiveSummaryComponentPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	component = EXECUTIVE_SUMMARY_COMPONENT (bonobo_object);
	priv = component->private;

	*html = (priv->create_html_view != NULL);
	*bonobo = (priv->create_bonobo_view != NULL);
}

static void
impl_Evolution_SummaryComponent_set_owner (PortableServer_Servant servant,
					   Evolution_Summary summary,
					   CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummaryComponent *component;
	ExecutiveSummaryComponentPrivate *priv;
	ExecutiveSummaryClient *client;
	Evolution_Summary summary_duplicate;

	bonobo_object = bonobo_object_from_servant (servant);
	component = EXECUTIVE_SUMMARY_COMPONENT (bonobo_object);
	priv = component->private;

	/* Create a summary client */
	client = gtk_type_new (executive_summary_client_get_type ());

	summary_duplicate = CORBA_Object_duplicate (summary, ev);
	executive_summary_client_construct (client, summary_duplicate);

	priv->owner_client = client;
}

static void
impl_Evolution_SummaryComponent_unset_owner (PortableServer_Servant servant,
					     CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummaryComponent *component;
	ExecutiveSummaryComponentPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	component = EXECUTIVE_SUMMARY_COMPONENT (bonobo_object);
	priv = component->private;

	if (priv->owner_client == NULL)
		return;

	bonobo_object_unref (BONOBO_OBJECT (priv->owner_client));
	priv->owner_client = NULL;
}

static Bonobo_Control
impl_Evolution_SummaryComponent_create_bonobo_view (PortableServer_Servant servant,
						    CORBA_char **title,
						    CORBA_char **icon,
						    CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummaryComponent *component;
	ExecutiveSummaryComponentPrivate *priv;
	BonoboControl *control;
	char *initial_title, *initial_icon;
	
	bonobo_object = bonobo_object_from_servant (servant);
	component = EXECUTIVE_SUMMARY_COMPONENT (bonobo_object);
	priv = component->private;
	
	control = (* priv->create_bonobo_view) (component, &initial_title,
						&initial_icon,
						priv->closure);
	
	*title = CORBA_string_dup (initial_title ? initial_title:"");
	*icon = CORBA_string_dup (initial_icon ? initial_icon:"");
	g_free (initial_title);
	g_free (initial_icon);

	return bonobo_object_corba_objref (BONOBO_OBJECT (control));
}

static CORBA_char *
impl_Evolution_SummaryComponent_create_html_view (PortableServer_Servant servant,
						  CORBA_char **title,
						  CORBA_char **icon,
						  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummaryComponent *component;
	ExecutiveSummaryComponentPrivate *priv;
	CORBA_char *ret_str;
	char *ret_html;
	char *initial_title, *initial_icon;

	bonobo_object = bonobo_object_from_servant (servant);
	component = EXECUTIVE_SUMMARY_COMPONENT (bonobo_object);
	priv = component->private;

	ret_html = (* priv->create_html_view) (component, &initial_title,
					       &initial_icon,
					       priv->closure);

	*title = CORBA_string_dup (initial_title ? initial_title:"");
	*icon = CORBA_string_dup (initial_icon ? initial_icon:"");
	g_free (initial_title);
	g_free (initial_icon);

	ret_str = CORBA_string_dup (ret_html ? ret_html:"");
	g_free (ret_html);
	return ret_str;
}
	
static void
impl_Evolution_SummaryComponent_configure (PortableServer_Servant servant,
					   CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummaryComponent *component;
	ExecutiveSummaryComponentPrivate *priv;
	
	bonobo_object = bonobo_object_from_servant (servant);
	component = EXECUTIVE_SUMMARY_COMPONENT (bonobo_object);
	priv = component->private;
	
	(* priv->configure) (component, priv->closure);
}

static void
executive_summary_component_destroy (GtkObject *object)
{
	ExecutiveSummaryComponent *component;
	ExecutiveSummaryComponentPrivate *priv;
	CORBA_Environment ev;
	
	component = EXECUTIVE_SUMMARY_COMPONENT (object);
	priv = component->private;
	
	if (priv == NULL)
		return;
	
	CORBA_exception_init (&ev);
	
	if (priv->owner_client != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (priv->owner_client));
		priv->owner_client = NULL;
	}

	CORBA_exception_free (&ev);
	
	g_free (priv);
	component->private = NULL;
	
	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* Init */
static void
corba_class_init (void)
{
	POA_Evolution_SummaryComponent__vepv *vepv;
	POA_Evolution_SummaryComponent__epv *epv;
	PortableServer_ServantBase__epv *base_epv;
	
	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private = NULL;
	base_epv->finalize = NULL;
	base_epv->default_POA = NULL;
	
	epv = g_new0 (POA_Evolution_SummaryComponent__epv, 1);
	epv->supports = impl_Evolution_SummaryComponent_supports;
	epv->set_owner = impl_Evolution_SummaryComponent_set_owner;
	epv->unset_owner = impl_Evolution_SummaryComponent_unset_owner;
	epv->create_bonobo_view = impl_Evolution_SummaryComponent_create_bonobo_view;
	epv->create_html_view = impl_Evolution_SummaryComponent_create_html_view;
	epv->configure = impl_Evolution_SummaryComponent_configure;
	
	vepv = &SummaryComponent_vepv;
	vepv->_base_epv = base_epv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->Evolution_SummaryComponent_epv = epv;
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
	
	priv = g_new0 (ExecutiveSummaryComponentPrivate, 1);
	
	priv->create_bonobo_view = NULL;
	priv->create_html_view = NULL;
	priv->configure = NULL;
	
	priv->owner_client = NULL;
	priv->closure = NULL;
	
	component->private = priv;
}

void
executive_summary_component_construct (ExecutiveSummaryComponent *component,
				       Evolution_SummaryComponent corba_object,
				       EvolutionServicesCreateBonoboViewFn create_bonobo,
				       EvolutionServicesCreateHtmlViewFn create_html,
				       EvolutionServicesConfigureFn configure,
				       void *closure)
{
	ExecutiveSummaryComponentPrivate *priv;
	
	g_return_if_fail (component != NULL);
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);
	
	bonobo_object_construct (BONOBO_OBJECT (component), corba_object);
	
	priv = component->private;
	
	priv->create_bonobo_view = create_bonobo;
	priv->create_html_view = create_html;
	priv->configure = configure;
	
	priv->closure = closure;
}

BonoboObject *
executive_summary_component_new (EvolutionServicesCreateBonoboViewFn create_bonobo,
				 EvolutionServicesCreateHtmlViewFn create_html,
				 EvolutionServicesConfigureFn configure,
				 void *closure)
{
	ExecutiveSummaryComponent *component;
	POA_Evolution_SummaryComponent *servant;
	Evolution_SummaryComponent corba_object;
	
	servant = create_servant ();
	if (servant == NULL)
		return NULL;
	
	component = gtk_type_new (executive_summary_component_get_type ());
	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (component),
						       servant);
	
	executive_summary_component_construct (component, corba_object,
					       create_bonobo, create_html, 
					       configure, closure);
	
	return BONOBO_OBJECT (component);
}

E_MAKE_TYPE (executive_summary_component, "ExecutiveSummaryComponent", 
	     ExecutiveSummaryComponent, executive_summary_component_class_init,
	     executive_summary_component_init, PARENT_TYPE);

void
executive_summary_component_set_title (ExecutiveSummaryComponent *component,
				       const char *title)
{
	ExecutiveSummaryComponentPrivate *priv;

	g_return_if_fail (component != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT (component));

	priv = component->private;

	if (priv->owner_client == NULL) {
		g_warning ("Component not owned!");
		return;
	}

	executive_summary_client_set_title (priv->owner_client, component,
					    title);
}

void
executive_summary_component_flash (ExecutiveSummaryComponent *component)
{
	ExecutiveSummaryComponentPrivate *priv;

	g_return_if_fail (component != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT (component));

	priv = component->private;

	if (priv->owner_client == NULL) {
		g_warning ("Component not owned!");
		return;
	}

	executive_summary_client_flash (priv->owner_client, component);
}

void
executive_summary_component_update (ExecutiveSummaryComponent *component,
				    char *html)
{
	ExecutiveSummaryComponentPrivate *priv;

	g_return_if_fail (component != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT (component));

	priv = component->private;

	if (priv->owner_client == NULL) {
		g_warning ("Component not ownded!");
		return;
	}

	executive_summary_client_update (priv->owner_client, component, html);
}
