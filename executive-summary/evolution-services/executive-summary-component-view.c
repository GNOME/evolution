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

#include <gal/util/e-util.h>

#include <evolution-services/executive-summary-component.h>
#include <evolution-services/executive-summary-component-view.h>

struct _ExecutiveSummaryComponentViewPrivate {
	ExecutiveSummaryComponent *component;

	BonoboControl *control;
	Bonobo_Control objref;

	char *html;
	
	char *title;
	char *icon;
	
	int id;
};

static GtkObjectClass *parent_class = NULL;
#define PARENT_TYPE (gtk_object_get_type ())

enum {
	CONFIGURE,
	LAST_SIGNAL
};

static gint32 view_signals[LAST_SIGNAL] = { 0 };

static void
executive_summary_component_view_destroy (GtkObject *object)
{
	ExecutiveSummaryComponentView *view;
	ExecutiveSummaryComponentViewPrivate *priv;

	view = EXECUTIVE_SUMMARY_COMPONENT_VIEW (object);
	priv = view->private;
	if (priv == NULL)
		return;

	if (priv->component)
		bonobo_object_unref (BONOBO_OBJECT (priv->component));

	if (priv->control)
		bonobo_object_unref (BONOBO_OBJECT (priv->control));

	g_free (priv->html);
	g_free (priv->title);
	g_free (priv->icon);

	g_free (priv);
	view->private = NULL;

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
executive_summary_component_view_class_init (ExecutiveSummaryComponentViewClass *view_class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (view_class);

	object_class->destroy = executive_summary_component_view_destroy;
	
	view_signals[CONFIGURE] = gtk_signal_new ("configure",
						  GTK_RUN_FIRST,
						  object_class->type,
						  GTK_SIGNAL_OFFSET (ExecutiveSummaryComponentViewClass, configure),
						  gtk_marshal_NONE__NONE,
						  GTK_TYPE_NONE, 0);
	gtk_object_class_add_signals (object_class, view_signals, LAST_SIGNAL);

	parent_class = gtk_type_class (PARENT_TYPE);
}

static void
executive_summary_component_view_init (ExecutiveSummaryComponentView *view)
{
	ExecutiveSummaryComponentViewPrivate *priv;

	priv = g_new (ExecutiveSummaryComponentViewPrivate, 1);
	view->private = priv;

	priv->control = NULL;
	priv->objref = NULL;
	priv->html = NULL;
	priv->title = NULL;
	priv->icon = NULL;
	priv->id = -1;
}

E_MAKE_TYPE (executive_summary_component_view, "ExecutiveSummaryComponentView",
	     ExecutiveSummaryComponentView, 
	     executive_summary_component_view_class_init,
	     executive_summary_component_view_init, PARENT_TYPE);

void
executive_summary_component_view_construct (ExecutiveSummaryComponentView *view,
					    ExecutiveSummaryComponent *component,
					    BonoboControl *control,
					    const char *html,
					    const char *title,
					    const char *icon)
{
	ExecutiveSummaryComponentViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view));
	g_return_if_fail (control != NULL || html != NULL);
	
	priv = view->private;

	if (component != NULL) {
		bonobo_object_ref (BONOBO_OBJECT (component));
		priv->component = component;
	} else {
		priv->component = NULL;
	}

	if (control != NULL) {
		bonobo_object_ref (BONOBO_OBJECT (control));
		priv->control = control;
	} else {
		priv->control = NULL;
	}

	if (html) {
		priv->html = g_strdup (html);
	} else {
		priv->html = NULL;
	}

	if (title) {
		priv->title = g_strdup (title);
	} else {
		priv->title = NULL;
	}

	if (icon) {
		priv->icon = g_strdup (icon);
	} else {
		priv->icon = NULL;
	}
}

ExecutiveSummaryComponentView *
executive_summary_component_view_new (ExecutiveSummaryComponent *component,
				      BonoboControl *control,
				      const char *html,
				      const char *title,
				      const char *icon)
{
	ExecutiveSummaryComponentView *view;

	g_return_val_if_fail (control != NULL || html != NULL, NULL);
	
	view = gtk_type_new (executive_summary_component_view_get_type ());
	executive_summary_component_view_construct (view, component, control,
						    html, title, icon);

	return view;
}

void
executive_summary_component_view_set_title (ExecutiveSummaryComponentView *view,
					    const char *title)
{
	ExecutiveSummaryComponentViewPrivate *priv;
	ExecutiveSummaryComponent *component;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view));

	priv = view->private;
	if (priv->title)
		g_free (priv->title);
	priv->title = g_strdup (title);

	component = priv->component;
	if (component == NULL) {
		return;
	}

	executive_summary_component_set_title (component, view);
}

const char *
executive_summary_component_view_get_title (ExecutiveSummaryComponentView *view)
{
	ExecutiveSummaryComponentViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view), NULL);

	priv = view->private;

	return priv->title;
}

void
executive_summary_component_view_set_icon (ExecutiveSummaryComponentView *view,
					   const char *icon)
{
	ExecutiveSummaryComponentViewPrivate *priv;
	ExecutiveSummaryComponent *component;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view));

	priv = view->private;
	if (priv->icon)
		g_free (priv->icon);
	priv->icon = g_strdup (icon);

	component = priv->component;
	if (component == NULL) {
		return;
	}
	
	executive_summary_component_set_icon (component, view);
}

const char *
executive_summary_component_view_get_icon (ExecutiveSummaryComponentView *view)
{
	ExecutiveSummaryComponentViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view), NULL);

	priv = view->private;
	
	return priv->icon;
}

void
executive_summary_component_view_flash (ExecutiveSummaryComponentView *view)
{
	ExecutiveSummaryComponentViewPrivate *priv;
	ExecutiveSummaryComponent *component;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view));
	
	priv = view->private;
	component = priv->component;
	if (component == NULL) {
		g_warning ("Calling %s from the wrong side of the CORBA interface", __FUNCTION__);
		return;
	}

	executive_summary_component_flash (component, view);
}

void
executive_summary_component_view_set_html (ExecutiveSummaryComponentView *view,
					   const char *html)
{
	ExecutiveSummaryComponentViewPrivate *priv;
	ExecutiveSummaryComponent *component;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view));

	priv = view->private;
	if (priv->html)
		g_free (priv->html);

	priv->html = g_strdup (html);

	component = priv->component;
	if (component == NULL) {
		return;
	}

	executive_summary_component_update (component, view);
}

void
executive_summary_component_view_configure (ExecutiveSummaryComponentView *view)
{
	gtk_signal_emit (GTK_OBJECT (view), view_signals[CONFIGURE]);
}

const char *
executive_summary_component_view_get_html (ExecutiveSummaryComponentView *view)
{
	ExecutiveSummaryComponentViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view), NULL);

	priv = view->private;
	
	return priv->html;
}

BonoboObject *
executive_summary_component_view_get_control (ExecutiveSummaryComponentView *view)
{
	ExecutiveSummaryComponentViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view), NULL);

	priv = view->private;

	return (BonoboObject *)priv->control;
}

void
executive_summary_component_view_set_id (ExecutiveSummaryComponentView *view,
					 int id)
{
	ExecutiveSummaryComponentViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view));

	priv = view->private;

	priv->id = id;
}

int
executive_summary_component_view_get_id (ExecutiveSummaryComponentView *view)
{
	ExecutiveSummaryComponentViewPrivate *priv;

	g_return_val_if_fail (view != NULL, -1);
	g_return_val_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view), -1);

	priv = view->private;
	
	return priv->id;
}

void
executive_summary_component_view_set_objref (ExecutiveSummaryComponentView *view,
					     Bonobo_Control objref)
{
	ExecutiveSummaryComponentViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view));

	priv = view->private;

	if (priv->objref) {
		g_warning ("View already has an objref.");
		return;
	}

	priv->objref = objref;
}

GtkWidget *
executive_summary_component_view_get_widget (ExecutiveSummaryComponentView *view)
{
	ExecutiveSummaryComponentViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view), NULL);

	priv = view->private;
	if (priv->objref == NULL) {
		g_warning ("View has no objref.");
		return NULL;
	}

	return bonobo_widget_new_control_from_objref (priv->objref, NULL);
}
