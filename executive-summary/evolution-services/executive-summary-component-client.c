/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* executive-summary-component-client.c
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

#include <gal/util/e-util.h>

#include <liboaf/liboaf.h>

#include <Executive-Summary.h>
#include "executive-summary-component-client.h"
#include "executive-summary.h"

#define PARENT_TYPE BONOBO_OBJECT_CLIENT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _ExecutiveSummaryComponentClientPrivate {
	int dummy;
};

static void
executive_summary_component_client_destroy (GtkObject *object)
{
	ExecutiveSummaryComponentClient *client;
	ExecutiveSummaryComponentClientPrivate *priv;
	
	client = EXECUTIVE_SUMMARY_COMPONENT_CLIENT (object);
	priv = client->private;
	
	if (priv == NULL)
		return;
	
	g_free (priv);
	client->private = NULL;
	
	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
executive_summary_component_client_init (ExecutiveSummaryComponentClient *client)
{
	ExecutiveSummaryComponentClientPrivate *priv;
	
	priv = g_new0 (ExecutiveSummaryComponentClientPrivate, 1);
	client->private = priv;
}

static void
executive_summary_component_client_class_init (ExecutiveSummaryComponentClientClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = executive_summary_component_client_destroy;
	
	parent_class = gtk_type_class (PARENT_TYPE);
}

void
executive_summary_component_client_construct (ExecutiveSummaryComponentClient *client,
					      CORBA_Object corba_object)
{
	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);
	
	bonobo_object_client_construct (BONOBO_OBJECT_CLIENT (client), corba_object);
}

ExecutiveSummaryComponentClient*
executive_summary_component_client_new (const char *id)
{
	ExecutiveSummaryComponentClient *client;
	CORBA_Environment ev;
	CORBA_Object corba_object;
	
	g_return_val_if_fail (id != NULL, NULL);
	
	CORBA_exception_init (&ev);
	
	corba_object = oaf_activate_from_id ((char *)id, 0, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		g_warning ("Could not start %s\n", id);
		return NULL;
	}
	
	CORBA_exception_free (&ev);
	
	if (corba_object == CORBA_OBJECT_NIL) {
		g_warning ("Could not activate %s\n", id);
		return NULL;
	}
	
	client = gtk_type_new (executive_summary_component_client_get_type ());
	executive_summary_component_client_construct (client, corba_object);
	
	return client;
}

/* External API */
void
executive_summary_component_client_set_owner (ExecutiveSummaryComponentClient *client,
					      ExecutiveSummary *summary)
{
	GNOME_Evolution_Summary_Component component;
	GNOME_Evolution_Summary_ViewFrame corba_object;
	CORBA_Environment ev;

	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client));
	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY (summary));

	CORBA_exception_init (&ev);
	component = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	corba_object = bonobo_object_corba_objref (BONOBO_OBJECT (summary));

	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);
	GNOME_Evolution_Summary_Component_setOwner (component, corba_object, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error setting owner");
	}

	CORBA_exception_free (&ev);
	return;
}

void
executive_summary_component_client_unset_owner (ExecutiveSummaryComponentClient *client)
{
	GNOME_Evolution_Summary_Component component;
	CORBA_Environment ev;

	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client));

	CORBA_exception_init (&ev);
	component = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	
	GNOME_Evolution_Summary_Component_unsetOwner (component, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error unsetting owner");
	}

	CORBA_exception_free (&ev);
	return;
}

E_MAKE_TYPE (executive_summary_component_client, 
	     "ExecutiveSummaryComponentClient", 
	     ExecutiveSummaryComponentClient,
	     executive_summary_component_client_class_init,
	     executive_summary_component_client_init, PARENT_TYPE)
