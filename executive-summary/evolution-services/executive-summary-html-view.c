/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* executive-summary-html-view.c - Bonobo implementation of 
 *                                 HtmlView.idl
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
#include <bonobo/bonobo-event-source.h>
#include <gnome.h>
#include <gal/util/e-util.h>

#include "Executive-Summary.h"
#include "executive-summary-html-view.h"

static void executive_summary_html_view_destroy (GtkObject *object);
static void executive_summary_html_view_init (ExecutiveSummaryHtmlView *component);
static void executive_summary_html_view_class_init (ExecutiveSummaryHtmlViewClass *klass);

#define PARENT_TYPE (bonobo_object_get_type ())

static BonoboObjectClass *parent_class;

struct _ExecutiveSummaryHtmlViewPrivate {
	BonoboEventSource *event_source;

	char *html;
};

/* CORBA interface */
static POA_GNOME_Evolution_Summary_HTMLView__vepv HTMLView_vepv;

static POA_GNOME_Evolution_Summary_HTMLView *
create_servant (void)
{
	POA_GNOME_Evolution_Summary_HTMLView *servant;
	CORBA_Environment ev;

	servant = (POA_GNOME_Evolution_Summary_HTMLView *)g_new0 (BonoboObjectServant, 1);
	servant->vepv = &HTMLView_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_Evolution_Summary_HTMLView__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}

static CORBA_char *
impl_GNOME_Evolution_Summary_HTMLView_getHtml (PortableServer_Servant servant,
					       CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ExecutiveSummaryHtmlView *view;
	ExecutiveSummaryHtmlViewPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	view = EXECUTIVE_SUMMARY_HTML_VIEW (bonobo_object);
	priv = view->private;

	return CORBA_string_dup (priv->html? priv->html: "");
}

/* GtkObject methods */
static void
executive_summary_html_view_destroy (GtkObject *object)
{
	ExecutiveSummaryHtmlView *view;
	ExecutiveSummaryHtmlViewPrivate *priv;

	view = EXECUTIVE_SUMMARY_HTML_VIEW (object);
	priv = view->private;

	if (priv == NULL)
		return;

	g_free (priv->html);
	g_free (priv);

	view->private = NULL;

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
corba_class_init (void)
{
	POA_GNOME_Evolution_Summary_HTMLView__vepv *vepv;
	POA_GNOME_Evolution_Summary_HTMLView__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private = NULL;
	base_epv->finalize = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_GNOME_Evolution_Summary_HTMLView__epv, 1);
	epv->getHtml = impl_GNOME_Evolution_Summary_HTMLView_getHtml;

	vepv = &HTMLView_vepv;
	vepv->_base_epv = base_epv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_Summary_HTMLView_epv = epv;
}

static void
executive_summary_html_view_class_init (ExecutiveSummaryHtmlViewClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = executive_summary_html_view_destroy;

	parent_class = gtk_type_class (PARENT_TYPE);

	corba_class_init ();
}

static void
executive_summary_html_view_init (ExecutiveSummaryHtmlView *view)
{
	ExecutiveSummaryHtmlViewPrivate *priv;

	priv = g_new (ExecutiveSummaryHtmlViewPrivate, 1);
	priv->html = NULL;
	priv->event_source = NULL;

	view->private = priv;
}

E_MAKE_TYPE (executive_summary_html_view, "ExecutiveSummaryHtmlView",
	     ExecutiveSummaryHtmlView, executive_summary_html_view_class_init,
	     executive_summary_html_view_init, PARENT_TYPE);

static void
executive_summary_html_view_construct (ExecutiveSummaryHtmlView *view,
				       GNOME_Evolution_Summary_HTMLView corba_object)
{
	ExecutiveSummaryHtmlViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);

	priv = view->private;
	
	priv->event_source = bonobo_event_source_new ();
	g_warning ("new event source: %p", priv->event_source);
	bonobo_object_add_interface (BONOBO_OBJECT (view), 
				     BONOBO_OBJECT (priv->event_source));

	bonobo_object_construct (BONOBO_OBJECT (view), corba_object);
}

/*** Public API ***/
/**
 * executive_summary_html_view_new:
 *
 * Creates a new BonoboObject that implements 
 * the HTMLView.idl interface.
 *
 * Returns: A BonoboObject.
 */
BonoboObject *
executive_summary_html_view_new (void)
{
	ExecutiveSummaryHtmlView *view;
	POA_GNOME_Evolution_Summary_HTMLView *servant;
	GNOME_Evolution_Summary_HTMLView corba_object;

	servant = create_servant ();
	if (servant == NULL)
		return NULL;

	view = gtk_type_new (executive_summary_html_view_get_type ());
	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (view),
						       servant);
	executive_summary_html_view_construct (view, corba_object);

	return BONOBO_OBJECT (view);
}

/**
 * executive_summary_html_view_set_html:
 * @view: The ExecutiveSummaryHtmlView to operate on,
 * @html: The HTML as a string.
 *
 * Sets the HTML string in @view to @html. @html is copied into @view,
 * so after this call you are free to do what you want with @html.
 */
void
executive_summary_html_view_set_html (ExecutiveSummaryHtmlView *view,
				      const char *html)
{
	ExecutiveSummaryHtmlViewPrivate *priv;
	CORBA_any any;
	CORBA_short s;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_HTML_VIEW (view));

	priv = view->private;
	if (priv->html)
		g_free (priv->html);

	if (html != NULL)
		priv->html = g_strdup (html);
	else
		priv->html = NULL;

	/* Notify any listeners */
	s = 0;

	any._type = (CORBA_TypeCode) TC_short;
	any._value = &s;

	g_warning ("Notifying event source: %p", priv->event_source);
	bonobo_event_source_notify_listeners (BONOBO_EVENT_SOURCE (priv->event_source),
					      "html_changed", &any, NULL);
}
/**
 * executive_summary_html_view_get_html:
 * @view: The ExecutiveSummaryHtmlView to operate on.
 *
 * Retrieves the HTML stored in @view. This return value is not duplicated
 * before returning, so you should not free it. Instead, if you want to free
 * the HTML stored in @view, you should use 
 * executive_summary_html_view_set_html (view, NULL);.
 *
 * Returns: A pointer to the HTML stored in @view.
 */
const char *
executive_summary_html_view_get_html (ExecutiveSummaryHtmlView *view)
{
	ExecutiveSummaryHtmlViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (IS_EXECUTIVE_SUMMARY_HTML_VIEW (view), NULL);

	priv = view->private;
	return priv->html;
}
	
