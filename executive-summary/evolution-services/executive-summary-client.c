/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* executive-summary-client.c
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
#include "executive-summary-client.h"
#include "executive-summary-component.h"

#define PARENT_TYPE BONOBO_OBJECT_CLIENT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _ExecutiveSummaryClientPrivate {
	int dummy;
};

static void
executive_summary_client_destroy (GtkObject *object)
{
	ExecutiveSummaryClient *client;
	ExecutiveSummaryClientPrivate *priv;
	
	client = EXECUTIVE_SUMMARY_CLIENT (object);
	priv = client->private;
	
	if (priv == NULL)
		return;
	
	g_free (priv);
	client->private = NULL;
	
	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
executive_summary_client_init (ExecutiveSummaryClient *client)
{
	ExecutiveSummaryClientPrivate *priv;
	
	priv = g_new0 (ExecutiveSummaryClientPrivate, 1);
	client->private = priv;
}

static void
executive_summary_client_class_init (ExecutiveSummaryClientClass *client)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (client);
	parent_class = gtk_type_class (PARENT_TYPE);
	
	object_class->destroy = executive_summary_client_destroy;
}

void
executive_summary_client_construct (ExecutiveSummaryClient *client,
				    CORBA_Object corba_object)
{
	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_CLIENT (client));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);
	
	bonobo_object_client_construct (BONOBO_OBJECT_CLIENT (client), corba_object);
}

E_MAKE_TYPE (executive_summary_client, "ExecutiveSummaryClient",
	     ExecutiveSummaryClient, executive_summary_client_class_init,
	     executive_summary_client_init, PARENT_TYPE);

void
executive_summary_client_set_title (ExecutiveSummaryClient *client,
				    ExecutiveSummaryComponent *component,
				    const char *title)
{
	Evolution_Summary summary;
	Evolution_SummaryComponent corba_object;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	summary = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	corba_object = bonobo_object_corba_objref (BONOBO_OBJECT (component));

	Evolution_Summary_set_title (summary, corba_object, title, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error setting title to %s:%s", title, CORBA_exception_id (&ev));
	}

	CORBA_exception_free (&ev);
}

void
executive_summary_client_flash (ExecutiveSummaryClient *client,
				ExecutiveSummaryComponent *component)
{
	Evolution_Summary summary;
	Evolution_SummaryComponent corba_object;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	summary = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	corba_object = bonobo_object_corba_objref (BONOBO_OBJECT (component));

	Evolution_Summary_flash (summary, corba_object, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error flashing");
	}

	CORBA_exception_free (&ev);
}

void
executive_summary_client_update (ExecutiveSummaryClient *client,
				 ExecutiveSummaryComponent *component,
				 char *html)
{
	Evolution_Summary summary;
	Evolution_SummaryComponent corba_object;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	summary = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	corba_object = bonobo_object_corba_objref (BONOBO_OBJECT (component));

	Evolution_Summary_update_html_component (summary, corba_object, 
						 html, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error updating the component");
	}

	CORBA_exception_free (&ev);
}

		
