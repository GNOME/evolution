/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* executive-summary.c - Bonobo implementation of Summary.idl
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

static void executive_summary_destroy (GtkObject *object);
static void executive_summary_class_init (ExecutiveSummaryClass *es_class);
static void executive_summary_init (ExecutiveSummary *es);

#define PARENT_TYPE (bonobo_object_get_type ())

static BonoboObjectClass *parent_class;

struct _ExecutiveSummaryPrivate {
	EvolutionServicesSetTitleFn set_title;
	EvolutionServicesFlashFn flash;
	EvolutionServicesUpdateFn update;

	void *closure;
};

/* CORBA interface implementation */
static POA_Evolution_Summary__vepv Summary_vepv;

static POA_Evolution_Summary *
create_servant (void)
{
	POA_Evolution_Summary *servant;
	CORBA_Environment ev;

	servant = (POA_Evolution_Summary *)g_new0 (BonoboObjectServant, 1);
	servant->vepv = &Summary_vepv;

	CORBA_exception_init (&ev);

	POA_Evolution_Summary__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}

static void
impl_Evolution_Summary_set_title (PortableServer_Servant servant,
				  const Evolution_SummaryComponent component,
				  const CORBA_char *title,
				  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummary *summary;
	ExecutiveSummaryPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	summary = EXECUTIVE_SUMMARY (bonobo_object);
	priv = summary->private;

	(* priv->set_title) (summary, component, title, priv->closure);
}

static void
impl_Evolution_Summary_flash (PortableServer_Servant servant,
			      const Evolution_SummaryComponent component,
			      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummary *summary;
	ExecutiveSummaryPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	summary = EXECUTIVE_SUMMARY (bonobo_object);
	priv = summary->private;

	(* priv->flash) (summary, component, priv->closure);
}

static void
impl_Evolution_Summary_update_html_component (PortableServer_Servant servant,
					      const Evolution_SummaryComponent component,
					      CORBA_char *html,
					      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummary *summary;
	ExecutiveSummaryPrivate *priv;
	struct _queuedata *qd;

	bonobo_object = bonobo_object_from_servant (servant);
	summary = EXECUTIVE_SUMMARY (bonobo_object);
	priv = summary->private;

	(*priv->update) (summary, component, html, priv->closure);
}

/* GtkObject methods */
static void
executive_summary_destroy (GtkObject *object)
{
	ExecutiveSummary *es;
	ExecutiveSummaryPrivate *priv;
	
	es = EXECUTIVE_SUMMARY (object);
	priv = es->private;
	
	if (priv == NULL)
		return;
	
	g_free (priv);
	es->private = NULL;
}

/* Initialisation */

static void
corba_class_init (void)
{
	POA_Evolution_Summary__vepv *vepv;
	POA_Evolution_Summary__epv *epv;
	PortableServer_ServantBase__epv *base_epv;
	
	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private = NULL;
	base_epv->finalize = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_Evolution_Summary__epv, 1);
	epv->set_title = impl_Evolution_Summary_set_title;
	epv->flash = impl_Evolution_Summary_flash;
	epv->update_html_component = impl_Evolution_Summary_update_html_component;

	vepv = &Summary_vepv;
	vepv->_base_epv = base_epv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->Evolution_Summary_epv = epv;
}

static void
executive_summary_class_init (ExecutiveSummaryClass *es_class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) es_class;
	
	object_class->destroy = executive_summary_destroy;
	
	parent_class = gtk_type_class (PARENT_TYPE);
	corba_class_init ();
}

static void
executive_summary_init (ExecutiveSummary *es)
{
	ExecutiveSummaryPrivate *priv;
	
	priv = g_new0 (ExecutiveSummaryPrivate, 1);
	es->private = priv;

	priv->set_title = NULL;
	priv->flash = NULL;
	priv->closure = NULL;
}

E_MAKE_TYPE (executive_summary, "ExecutiveSummary", ExecutiveSummary,
	     executive_summary_class_init, executive_summary_init, PARENT_TYPE);

void
executive_summary_construct (ExecutiveSummary *es,
			     Evolution_Summary corba_object,
			     EvolutionServicesSetTitleFn set_title,
			     EvolutionServicesFlashFn flash,
			     EvolutionServicesUpdateFn update,
			     void *closure)
{
	ExecutiveSummaryPrivate *priv;
	
	bonobo_object_construct (BONOBO_OBJECT (es), corba_object);
	
	priv = es->private;
	priv->set_title = set_title;
	priv->flash = flash;
	priv->update = update;
	priv->closure = closure;
}

BonoboObject *
executive_summary_new (EvolutionServicesSetTitleFn set_title,
		       EvolutionServicesFlashFn flash,
		       EvolutionServicesUpdateFn update,
		       void *closure)
{
	POA_Evolution_Summary *servant;
	Evolution_Summary corba_object;
	ExecutiveSummary *es;

	servant = create_servant ();
	if (servant == NULL)
		return NULL;

	es = gtk_type_new (executive_summary_get_type ());
	
	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (es), 
						       servant);
	executive_summary_construct (es, corba_object, set_title, flash,
				     update, closure);
	
	return BONOBO_OBJECT (es);
}

  
