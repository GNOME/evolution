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

enum {
	UPDATE,
	SET_TITLE,
	SET_ICON,
	FLASH,
	LAST_SIGNAL
};

static guint32 summary_signals [LAST_SIGNAL] = { 0 };
static BonoboObjectClass *parent_class;

struct _ExecutiveSummaryPrivate {
	int dummy;
};

/* CORBA interface implementation */
static POA_GNOME_Evolution_Summary_ViewFrame__vepv Summary_vepv;

static POA_GNOME_Evolution_Summary_ViewFrame *
create_servant (void)
{
	POA_GNOME_Evolution_Summary_ViewFrame *servant;
	CORBA_Environment ev;

	servant = (POA_GNOME_Evolution_Summary_ViewFrame *)g_new0 (BonoboObjectServant, 1);
	servant->vepv = &Summary_vepv;

	CORBA_exception_init (&ev);

	POA_GNOME_Evolution_Summary_ViewFrame__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}

static void
impl_GNOME_Evolution_Summary_ViewFrame_setTitle (PortableServer_Servant servant,
				  CORBA_long id,
				  const CORBA_char *title,
				  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummary *summary;

	bonobo_object = bonobo_object_from_servant (servant);
	summary = EXECUTIVE_SUMMARY (bonobo_object);

	gtk_signal_emit (GTK_OBJECT (summary), summary_signals[SET_TITLE], 
			 id, title);
}

static void
impl_GNOME_Evolution_Summary_ViewFrame_setIcon (PortableServer_Servant servant,
				 CORBA_long id,
				 const CORBA_char *title,
				 CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummary *summary;

	bonobo_object = bonobo_object_from_servant (servant);
	summary = EXECUTIVE_SUMMARY (bonobo_object);

	gtk_signal_emit (GTK_OBJECT (summary), summary_signals[SET_ICON],
			 id, title);
}

static void
impl_GNOME_Evolution_Summary_ViewFrame_flash (PortableServer_Servant servant,
					      CORBA_long id,
					      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummary *summary;

	bonobo_object = bonobo_object_from_servant (servant);
	summary = EXECUTIVE_SUMMARY (bonobo_object);

	gtk_signal_emit (GTK_OBJECT (summary), summary_signals[FLASH], id);
}

static void
impl_GNOME_Evolution_Summary_ViewFrame_updateComponent (PortableServer_Servant servant,
							const CORBA_long id,
							const CORBA_char *html,
							CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummary *summary;

	bonobo_object = bonobo_object_from_servant (servant);
	summary = EXECUTIVE_SUMMARY (bonobo_object);

	gtk_signal_emit (GTK_OBJECT (summary), summary_signals[UPDATE],
			 id, html);
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
	POA_GNOME_Evolution_Summary_ViewFrame__vepv *vepv;
	POA_GNOME_Evolution_Summary_ViewFrame__epv *epv;
	PortableServer_ServantBase__epv *base_epv;
	
	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private = NULL;
	base_epv->finalize = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_GNOME_Evolution_Summary_ViewFrame__epv, 1);
	epv->setTitle        = impl_GNOME_Evolution_Summary_ViewFrame_setTitle;
	epv->setIcon         = impl_GNOME_Evolution_Summary_ViewFrame_setIcon;
	epv->flash           = impl_GNOME_Evolution_Summary_ViewFrame_flash;
	epv->updateComponent = impl_GNOME_Evolution_Summary_ViewFrame_updateComponent;

	vepv = &Summary_vepv;
	vepv->_base_epv = base_epv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_Summary_ViewFrame_epv = epv;
}

static void
executive_summary_class_init (ExecutiveSummaryClass *es_class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) es_class;
	
	object_class->destroy = executive_summary_destroy;
	
	parent_class = gtk_type_class (PARENT_TYPE);

	summary_signals[UPDATE] = gtk_signal_new ("update",
						  GTK_RUN_LAST,
						  object_class->type,
						  GTK_SIGNAL_OFFSET (ExecutiveSummaryClass, update),
						  gtk_marshal_NONE__POINTER_POINTER,
						  GTK_TYPE_NONE, 2,
						  GTK_TYPE_POINTER,
						  GTK_TYPE_POINTER);
	summary_signals[SET_TITLE] = gtk_signal_new ("set_title",
						     GTK_RUN_LAST,
						     object_class->type,
						     GTK_SIGNAL_OFFSET (ExecutiveSummaryClass, set_title),
						     gtk_marshal_NONE__POINTER_POINTER,
						     GTK_TYPE_NONE, 2,
						     GTK_TYPE_POINTER,
						     GTK_TYPE_POINTER);
	summary_signals[SET_ICON] = gtk_signal_new ("set_icon",
						    GTK_RUN_LAST,
						    object_class->type,
						    GTK_SIGNAL_OFFSET (ExecutiveSummaryClass, set_icon),
						    gtk_marshal_NONE__POINTER_POINTER,
						    GTK_TYPE_NONE, 2,
						    GTK_TYPE_POINTER,
						    GTK_TYPE_POINTER);
	summary_signals[FLASH] = gtk_signal_new ("flash",
						 GTK_RUN_LAST,
						 object_class->type,
						 GTK_SIGNAL_OFFSET (ExecutiveSummaryClass, flash),
						 gtk_marshal_NONE__POINTER,
						 GTK_TYPE_NONE, 1,
						 GTK_TYPE_POINTER);
	gtk_object_class_add_signals (object_class, summary_signals, LAST_SIGNAL);

	corba_class_init ();
}

static void
executive_summary_init (ExecutiveSummary *es)
{
	ExecutiveSummaryPrivate *priv;

	priv = g_new (ExecutiveSummaryPrivate, 1);
	es->private = priv;
}

E_MAKE_TYPE (executive_summary, "ExecutiveSummary", ExecutiveSummary,
	     executive_summary_class_init, executive_summary_init, PARENT_TYPE);

void
executive_summary_construct (ExecutiveSummary *es,
			     GNOME_Evolution_Summary_ViewFrame corba_object)
{
	bonobo_object_construct (BONOBO_OBJECT (es), corba_object);
}

BonoboObject *
executive_summary_new (void)
{
	POA_GNOME_Evolution_Summary_ViewFrame *servant;
	GNOME_Evolution_Summary_ViewFrame corba_object;
	ExecutiveSummary *es;

	servant = create_servant ();
	if (servant == NULL)
		return NULL;

	es = gtk_type_new (executive_summary_get_type ());
	
	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (es), 
						       servant);
	executive_summary_construct (es, corba_object);
	
	return BONOBO_OBJECT (es);
}  
