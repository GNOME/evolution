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
#include "executive-summary-component-view.h"
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

#if 0
void
executive_summary_component_client_supports (ExecutiveSummaryComponentClient *client,
					     gboolean *bonobo,
					     gboolean *html)
{
	GNOME_Evolution_Summary_Component component;
	CORBA_Environment ev;

	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client));

	CORBA_exception_init (&ev);
	component = bonobo_object_corba_objref (BONOBO_OBJECT (client));

	GNOME_Evolution_Summary_Component_supports (component, bonobo, html, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error checking supports");
	}

	CORBA_exception_free (&ev);
	return;
}
#endif

ExecutiveSummaryComponentView *
executive_summary_component_client_create_view (ExecutiveSummaryComponentClient *client,
						int id)
{
	ExecutiveSummaryComponentView *view;
	GNOME_Evolution_Summary_Component component;
	char *html, *title, *icon;
	Bonobo_Control control;
	BonoboControl *bc;
	int ret_id;
	CORBA_Environment ev;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client),
			      NULL);
	
	CORBA_exception_init (&ev);
	if (client)
		component = bonobo_object_corba_objref (BONOBO_OBJECT (client));

	/* Get all the details about the view */
	g_print ("In %s\n", __FUNCTION__);
	ret_id = GNOME_Evolution_Summary_Component_createView (component, id, &control, 
							       &html, &title, &icon, &ev);
	g_print ("Out %s\n", __FUNCTION__);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error creating view");
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	/* Create a local copy of the remote view */
	if (control != CORBA_OBJECT_NIL) {
		bc = BONOBO_CONTROL (bonobo_widget_new_control_from_objref (control, NULL));
	} else {
		bc = NULL;
	}

	view = executive_summary_component_view_new (NULL, bc, html, title,
						     icon);
	executive_summary_component_view_set_id (view, ret_id);
	
	return view;
}

#if 0
char *
executive_summary_component_client_create_html_view (ExecutiveSummaryComponentClient *client,
						     char **title,
						     char **icon)
{
	CORBA_char *ret_html;
	GNOME_Evolution_Summary_Component component;
	CORBA_Environment ev;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client),
			      NULL);

	CORBA_exception_init (&ev);
	component = bonobo_object_corba_objref (BONOBO_OBJECT (client));

	ret_html = GNOME_Evolution_Summary_Component_create_html_view (component, title, icon, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error creating HTML view");
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return (char *)g_strdup (ret_html);
}
#endif

void
executive_summary_component_client_configure (ExecutiveSummaryComponentClient *client)
{
	GNOME_Evolution_Summary_Component component;
	CORBA_Environment ev;

	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client));

	CORBA_exception_init (&ev);
	component = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	GNOME_Evolution_Summary_Component_configure (component, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error configuring service");
		bonobo_object_unref (BONOBO_OBJECT (client));
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);
	
	return;
}

void
executive_summary_component_client_destroy_view (ExecutiveSummaryComponentClient *client,
						 ExecutiveSummaryComponentView *view) 
{
	int id;
	GNOME_Evolution_Summary_Component component;
	CORBA_Environment ev;

	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT (client));
	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view));

	id = executive_summary_component_view_get_id (view);

	CORBA_exception_init (&ev);
	component = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	GNOME_Evolution_Summary_Component_destroyView (component, id, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error destroying view #%d", id);
	}

	CORBA_exception_free (&ev);

	return;
}

E_MAKE_TYPE (executive_summary_component_client, 
	     "ExecutiveSummaryComponentClient", 
	     ExecutiveSummaryComponentClient,
	     executive_summary_component_client_class_init,
	     executive_summary_component_client_init, PARENT_TYPE)
