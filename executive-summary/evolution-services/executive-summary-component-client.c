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

#include <bonobo.h>
#include <gnome.h>
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
	Evolution_SummaryComponent component;
	Evolution_Summary corba_object;
	CORBA_Environment ev;

	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client));
	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY (summary));

	CORBA_exception_init (&ev);
	component = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	corba_object = bonobo_object_corba_objref (BONOBO_OBJECT (summary));

	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);
	Evolution_SummaryComponent_set_owner (component, corba_object, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error setting owner");
	}

	CORBA_exception_free (&ev);
	return;
}

void
executive_summary_component_client_unset_owner (ExecutiveSummaryComponentClient *client)
{
	Evolution_SummaryComponent component;
	CORBA_Environment ev;

	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client));

	CORBA_exception_init (&ev);
	component = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	
	Evolution_SummaryComponent_unset_owner (component, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error unsetting owner");
	}

	CORBA_exception_free (&ev);
	return;
}

void
executive_summary_component_client_supports (ExecutiveSummaryComponentClient *client,
					     gboolean *bonobo,
					     gboolean *html)
{
	Evolution_SummaryComponent component;
	CORBA_Environment ev;

	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client));

	CORBA_exception_init (&ev);
	component = bonobo_object_corba_objref (BONOBO_OBJECT (client));

	Evolution_SummaryComponent_supports (component, bonobo, html, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error checking supports");
	}

	CORBA_exception_free (&ev);
	return;
}

Bonobo_Control
executive_summary_component_client_create_bonobo_view (ExecutiveSummaryComponentClient *client,
						       char **title)
{
	Bonobo_Control control;
	Evolution_SummaryComponent component;
	CORBA_Environment ev;

	g_return_val_if_fail (client != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client),
			      CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);
	component = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	control = Evolution_SummaryComponent_create_bonobo_view (component, title, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error creating view");
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	
	return control;
}

char *
executive_summary_component_client_create_html_view (ExecutiveSummaryComponentClient *client,
						     char **title)
{
	CORBA_char *ret_html;
	Evolution_SummaryComponent component;
	CORBA_Environment ev;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client),
			      NULL);

	CORBA_exception_init (&ev);
	component = bonobo_object_corba_objref (BONOBO_OBJECT (client));

	g_print ("Here\n");
	ret_html = Evolution_SummaryComponent_create_html_view (component, title, &ev);
	g_print ("Not here - %s\n", ret_html);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error creating HTML view");
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return (char *)g_strdup (ret_html);
}

void
executive_summary_component_client_configure (ExecutiveSummaryComponentClient *client)
{
	Evolution_SummaryComponent component;
	CORBA_Environment ev;

	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client));

	CORBA_exception_init (&ev);
	component = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	Evolution_SummaryComponent_configure (component, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error configuring service");
		bonobo_object_unref (BONOBO_OBJECT (client));
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);
	
	return;
}

E_MAKE_TYPE (executive_summary_component_client, 
	     "ExecutiveSummaryComponentClient", 
	     ExecutiveSummaryComponentClient,
	     executive_summary_component_client_class_init,
	     executive_summary_component_client_init, PARENT_TYPE)
