/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary.c
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

#include <gnome.h>
#include <bonobo.h>

#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <gtkhtml/gtkhtml-stream.h>
#include <gtkhtml/htmlengine.h>
#include <gtkhtml/htmlselection.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <libgnomevfs/gnome-vfs.h>

#include "e-summary.h"
#include "e-summary-factory.h"
#include "e-summary-util.h"
#include "e-summary-url.h"

#include <evolution-services/executive-summary-component.h>
#include <evolution-services/executive-summary-component-factory-client.h>
#include <evolution-services/executive-summary-html-view.h>

#define PARENT_TYPE (gtk_vbox_get_type ())

#define STORAGE_TYPE "fs"
#define IID_FILE "oaf.id"
#define DATA_FILE "data"

/* From component-factory.c */
extern char *evolution_dir;

static GtkObjectClass *e_summary_parent_class;

struct _ESummaryPrivate {
	GNOME_Evolution_Shell shell;
	GNOME_Evolution_ShellView shell_view_interface;

	GtkWidget *html_scroller;
	GtkWidget *html;

	guint idle;

	GtkHTMLStream *stream;
	gboolean grabbed;

	GList *window_list;

	char *header;
	int header_len;
	char *footer;
	int footer_len;
};

static gboolean on_object_requested (GtkHTML *html,
				     GtkHTMLEmbedded *eb,
				     ESummary *summary);
static void e_summary_save_state (ESummary *esummary,
				  const char *path);
static void e_summary_load_state (ESummary *esummary,
				  const char *path);

/* GtkObject methods */

static void
e_summary_destroy (GtkObject *object)
{
	ESummary *esummary = E_SUMMARY (object);
	ESummaryPrivate *priv;
	GList *l;
	char *prefix;

	priv = esummary->private;
	if (priv == NULL)
		return;

	prefix = g_concat_dir_and_file (evolution_dir, "config/");
	e_summary_save_state (esummary, prefix);
	g_free (prefix);

	e_summary_prefs_free (esummary->prefs);
	for (l = priv->window_list; l; l = l->next)
		e_summary_window_free (l->data);
	g_list_free (priv->window_list);

	g_free (priv->header);
	g_free (priv->footer);
	g_free (esummary->private);
	esummary->private = NULL;

	e_summary_parent_class->destroy (object);
}

static void
e_summary_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = e_summary_destroy;
	
	e_summary_parent_class = gtk_type_class (PARENT_TYPE);
}

static void
e_summary_start_load (ESummary *esummary)
{
	ESummaryPrivate *priv;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

	priv = esummary->private;

	priv->stream = gtk_html_begin (GTK_HTML (priv->html));

	/* HTML hacks */
	/* Hack to stop page returning to the top */
	GTK_HTML (priv->html)->engine->newPage = FALSE;
	/* Hack to make the border width of the page 0 */
	GTK_HTML (priv->html)->engine->leftBorder = 0;
	GTK_HTML (priv->html)->engine->rightBorder = 0;
	GTK_HTML (priv->html)->engine->topBorder = 0;
	GTK_HTML (priv->html)->engine->bottomBorder = 0;
}

static void
load_default_header (ESummary *esummary)
{
	ESummaryPrivate *priv;
	char *def = "<html><body bgcolor=\"#ffffff\">"
		"<table width=\"100%\"><tr><td align=\"right\">"
		"<img src=\"ccsplash.png\"></td></tr></table>"
		"<table><tr><td><a href=\"exec://bug-buddy\"><img src=\"file://gnome-spider.png\" width=\"24\" height=\"24\" border=\"0\">"
		"</a></td><td><a href=\"exec://bug-buddy\">Submit a bug report"
		"</a></td></tr></table><hr>";

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

	priv = esummary->private;

	g_return_if_fail (priv->stream != NULL);

	gtk_html_write (GTK_HTML (priv->html), priv->stream, def, strlen (def));
}
static void
load_default_footer (ESummary *esummary)
{
	ESummaryPrivate *priv;
	char *footer = "<hr><p align=\"right\">All Executive Summary comments to <a href=\"mailto:iain@helixcode.com\">Iain Holmes (iain@helixcode.com)</a></p></body></html>";

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

	priv = esummary->private;
	gtk_html_write (GTK_HTML (priv->html), priv->stream, 
			footer, strlen (footer));
}

static void
e_summary_end_load (ESummary *esummary)
{
	ESummaryPrivate *priv;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

	priv = esummary->private;
	gtk_html_end (GTK_HTML (priv->html), priv->stream, GTK_HTML_STREAM_OK);

	priv->stream = NULL;
}

static void
e_summary_init (ESummary *esummary)
{
	GdkColor bgcolour = {0, 0xdfff, 0xdfff, 0xffff};
	ESummaryPrivate *priv;

	esummary->prefs = NULL;
	esummary->tmp_prefs = NULL;
	esummary->private = g_new0 (ESummaryPrivate, 1);
	priv = esummary->private;

	priv->window_list = NULL;
	priv->idle = 0;

	/* HTML widget */
	priv->html_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->html_scroller),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	priv->html = gtk_html_new ();
	gtk_html_set_editable (GTK_HTML (priv->html), FALSE);
	gtk_html_set_default_background_color (GTK_HTML (priv->html), &bgcolour);
	gtk_signal_connect (GTK_OBJECT (priv->html), "url-requested",
			    GTK_SIGNAL_FUNC (e_summary_url_request), esummary);
  	gtk_signal_connect (GTK_OBJECT (priv->html), "object-requested", 
  			    GTK_SIGNAL_FUNC (on_object_requested), esummary); 
	gtk_signal_connect (GTK_OBJECT (priv->html), "link-clicked",
			    GTK_SIGNAL_FUNC (e_summary_url_click), esummary);
	gtk_signal_connect (GTK_OBJECT (priv->html), "on-url",
			    GTK_SIGNAL_FUNC (e_summary_url_over), esummary);

	gtk_container_add (GTK_CONTAINER (priv->html_scroller), priv->html);
	gtk_widget_show_all (priv->html_scroller);

	e_summary_queue_rebuild (esummary);

	/* Pack stuff */
	gtk_box_pack_start (GTK_BOX (esummary), priv->html_scroller, 
			    TRUE, TRUE, 0);
}

E_MAKE_TYPE (e_summary, "ESummary", ESummary, e_summary_class_init,
	     e_summary_init, PARENT_TYPE);

GtkWidget *
e_summary_new (const GNOME_Evolution_Shell shell)
{
	ESummary *esummary;
	ESummaryPrivate *priv;
	char *path;

	esummary = gtk_type_new (e_summary_get_type ());
	priv = esummary->private;

	priv->shell = shell;

	/* Restore services */
	path = g_concat_dir_and_file (evolution_dir, 
				      "config");
	e_summary_load_state (esummary, path);
	g_free (path);

	return GTK_WIDGET (esummary);
}

#if 0
static void
control_unrealize (GtkHTMLEmbedded *eb,
		   GtkWidget *widget)
{
	g_print ("Removing\n");
}

static void
being_unrealized (GtkWidget *widget,
		  GtkHTMLEmbedded *eb)
{
	g_warning ("Widget is being unrealized");
	gtk_container_remove (GTK_CONTAINER (eb), widget);
}

static void
being_realized (GtkWidget *widget,
		gpointer user_data)
{
	gdk_window_ref (widget->window);
}
#endif

static gboolean
on_object_requested (GtkHTML *html,
		     GtkHTMLEmbedded *eb,
		     ESummary *esummary)
{
#if 0
	static GtkWidget *widget = NULL;
	int id;

	if (sscanf (eb->classid, "cid:%d", &id) != 1) {
		g_warning ("Could not get the view id: eb->classid = %s",
			   eb->classid);
		return FALSE;
	}

	if (widget == NULL || !GTK_IS_WIDGET (widget)) {
		g_print ("Create new\n");
      		widget = executive_summary_component_view_get_widget (view);  
/*    		widget = gtk_button_new_with_label ("Hello?");  */
		gtk_signal_connect (GTK_OBJECT (widget), "realize",
				    GTK_SIGNAL_FUNC (being_realized), NULL);
		gtk_signal_connect (GTK_OBJECT (widget), "unrealize",
				    GTK_SIGNAL_FUNC (being_unrealized), eb);
		g_print ("New widget: %p\n", GTK_BIN (widget)->child);
	} else {
		g_print ("No new\n");
	}

	if (widget == NULL) {
		g_warning ("View %d has no GtkWidget.", id);
		return FALSE;
	}

	gtk_signal_connect (GTK_OBJECT (eb), "unrealize",
			    GTK_SIGNAL_FUNC (control_unrealize), widget);
	gtk_widget_show_all (widget);
	gtk_widget_ref (widget);

	if (widget->parent == NULL)
		gtk_container_add (GTK_CONTAINER (eb), widget);

#endif
	return TRUE;
}

/* Generates the window controls and works out
   if they should be disabled or not */
static char *
make_control_html (ESummaryWindow *window,
		   int row, 
		   int col,
		   int numwindows)
{
	char *html, *tmp;
	int id = GPOINTER_TO_INT (window);
	gboolean config;

	config = TRUE;

	if (window->propertycontrol == CORBA_OBJECT_NIL)
		config = FALSE;

	html = g_strdup_printf ("<table><tr><td><a href=\"close://%d\">"
				"<img src=\"service-close.png\" border=\"0\">"
				"</a></td></tr><tr><td>", id);

	tmp = html;
	if (!config) {
		html = g_strdup_printf ("%s<img src=\"service-configure.png\">"
					"</td></tr></table>", tmp);
	} else {
		html = g_strdup_printf ("%s<a href=\"configure://%d\">"
					"<img src=\"service-configure.png\" border=\"0\">"
					"</a></td></tr></table>", tmp, id);
	}
	g_free (tmp);

	return html;
}

static void
e_summary_display_window_title (ESummary *esummary,
				ESummaryWindow *window,
				int row,
				int col,
				int numwindows)
{
	ESummaryPrivate *priv;
	char *title_html;
	char *control_html;
	char *title_colour[2] = {"bac1b6",
				 "cdd1c7"};

	priv = esummary->private;

	control_html = make_control_html (window, row, col, numwindows);
	title_html = g_strdup_printf ("<td bgcolor=\"#%s\">"
				      "<table width=\"100%%\" height=\"100%%\"><tr><td>"
				      "<img src=\"%s\" height=\"48\"></td>"
				      "<td nowrap align=\"center\" width=\"100%%\">"
				      "<b>%s</b></td><td>%s</td></tr></table></td>",
				      title_colour[col % 2], window->icon, 
				      window->title, control_html);
	g_free (control_html);
	
	gtk_html_write (GTK_HTML (priv->html), priv->stream, title_html,
			strlen (title_html));
	g_free (title_html);
}
	
static void
e_summary_display_window (ESummary *esummary,
			  ESummaryWindow *window,
			  int row,
			  int col,
			  int numwindows)
{
	ESummaryPrivate *priv;
	char *footer = "</td>";
	char *header;
	char *colour[2] = {"e6e8e4", 
			   "edeeeb"};

	priv = esummary->private;

	header = g_strdup_printf ("<td bgcolor=\"%s\" valign=\"top\">", colour[col % 2]);
	gtk_html_write (GTK_HTML (priv->html), priv->stream, header, strlen (header));
	g_free (header);

	if (window->html != CORBA_OBJECT_NIL) {
		char *html = NULL;
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		html = GNOME_Evolution_Summary_HTMLView_getHtml (window->html,
								 &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			CORBA_exception_free (&ev);
			g_warning ("Cannot get HTML.");
			if (html)
				CORBA_free (html);
		} else {
			CORBA_exception_free (&ev);
			
			gtk_html_write (GTK_HTML (priv->html), priv->stream,
					html, strlen (html));
			CORBA_free (html);
		}
	} else {
#if 0
		char *body_cid;

		body_cid = g_strdup_printf ("<object classid=\"cid:%d\"></object>", id);
		gtk_html_write (GTK_HTML (priv->html), priv->stream,
				body_cid, strlen (body_cid));
		g_free (body_cid);
#endif
	}

	gtk_html_write (GTK_HTML (priv->html), priv->stream,
			footer, strlen (footer));
}

static int
e_summary_rebuild_page (ESummary *esummary)
{
	ESummaryPrivate *priv;
	GList *windows;
	char *service_table = "<table numcols=\"%d\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\" height=\"100%%\" width=\"100%%\">";
	char *tmp;
	int numwindows, numrows;
	int i, j, k;
	int columns;

	g_return_val_if_fail (esummary != NULL, FALSE);
	g_return_val_if_fail (IS_E_SUMMARY (esummary), FALSE);

	priv = esummary->private;

	if (priv->idle == 0) {
		g_warning ("esummary->private->idle == 0! This means that "
			   "e_summary_rebuild_page was called by itself and "
			   "not queued. You should use e_summary_queue_rebuild "
			   "instead.");
		return FALSE;
	}

	/* If there is a selection, don't redraw the page so that the selection
	   isn't cleared */
	if (GTK_HTML (priv->html)->in_selection == TRUE ||
	    html_engine_is_selection_active (GTK_HTML (priv->html)->engine) == TRUE)
		return FALSE;

	gtk_layout_freeze (GTK_LAYOUT (priv->html));
	e_summary_start_load (esummary);
	
	if (priv->header == NULL || *priv->header == '\0') {
		load_default_header (esummary);
	} else {
		gtk_html_write (GTK_HTML (priv->html), priv->stream,
				priv->header, priv->header_len);
	}

	/* Load the start of the services */
	tmp = g_strdup_printf (service_table, esummary->prefs->columns);
	gtk_html_write (GTK_HTML (priv->html), priv->stream, tmp, strlen (tmp));
	g_free (tmp);
	/* Load each of the services */
	numwindows = g_list_length (priv->window_list);

	columns = esummary->prefs->columns;

	windows = priv->window_list;
	if (numwindows % columns == 0)
		numrows = numwindows / columns;
	else
		numrows = numwindows / columns + 1;

	for (i = 0; i < numrows; i++) {
		GList *window = windows;
		
		/* Do the same row twice: 
		   Once for the title, once for the contents */
		for (j = 0; j < 2; j++) {
			int limit;
			
			gtk_html_write (GTK_HTML (priv->html), priv->stream,
					"<tr>", 4);
				/* For each window on row i */
			limit = MIN (columns, (numwindows - (i * columns)));
			for (k = 0; k < limit; k++) {
				if (window == NULL)
					break;

				if (j == 0) {
					e_summary_display_window_title (esummary,
									window->data,
									k, k, 
									numwindows);
				} else {
					e_summary_display_window (esummary,
								  window->data,
								  k, k,
								  numwindows);
				}
				
				if (window != NULL)
					window = window->next;
				
				if (window == NULL)
					break;
			}
			
			gtk_html_write (GTK_HTML (priv->html), priv->stream,
					"</tr>", 5);
			if (j == 0)
				window = windows;
			else {
				if (window)
					windows = window;
				else 
					break;
			}
		}
	}
	gtk_html_write (GTK_HTML (priv->html), priv->stream, "</tr></table>", 13);
	
	if (priv->footer == NULL || *priv->footer == '\0') {
		load_default_footer (esummary);
	} else {
		gtk_html_write (GTK_HTML (priv->html), priv->stream,
				priv->footer, priv->footer_len);
	}
	
	e_summary_end_load (esummary);
	gtk_layout_thaw (GTK_LAYOUT (priv->html));

	priv->idle = 0;
	return FALSE;
}

/* This is the function that should be called instead of 
   e_summary_queue_rebuild. This prevents multiple 
   rebuilds happening together. */
void
e_summary_queue_rebuild (ESummary *esummary)
{
	ESummaryPrivate *priv;

	priv = esummary->private;
	if (priv->idle != 0)
		return;

	priv->idle = g_idle_add ((GSourceFunc) e_summary_rebuild_page, esummary);
}

static void
listener_event (BonoboListener *listener,
		char *event_name,
		BonoboArg *event_data,
		CORBA_Environment *ev,
		gpointer user_data)
{
	ESummaryWindow *window = (ESummaryWindow *) user_data;

	if (strcmp (event_name, "Bonobo/Property:change:window_title") == 0) {
		if (window->title != NULL)
			g_free (window->title);

		window->title = g_strdup (BONOBO_ARG_GET_STRING (event_data));
		e_summary_queue_rebuild (window->esummary);
	} else if (strcmp (event_name, "Bonobo/Property:change:window_icon") == 0) {
		if (window->icon != NULL)
			g_free (window->icon);

		window->icon = g_strdup (BONOBO_ARG_GET_STRING (event_data));
		e_summary_queue_rebuild (window->esummary);
	} else if (strcmp (event_name, EXECUTIVE_SUMMARY_HTML_VIEW_HTML_CHANGED) == 0) {
		e_summary_queue_rebuild (window->esummary);
	}

	return;
}
		
ESummaryWindow *
e_summary_add_service (ESummary *esummary,
		       GNOME_Evolution_Summary_Component component,
		       const char *iid)
{
	ESummaryWindow *window;
	ESummaryPrivate *priv;
	Bonobo_Unknown unknown = CORBA_OBJECT_NIL;
	Bonobo_Listener listener;
	CORBA_Environment ev;

	g_return_val_if_fail (esummary != NULL, NULL);
	g_return_val_if_fail (IS_E_SUMMARY (esummary), NULL);
	g_return_val_if_fail (component != CORBA_OBJECT_NIL, NULL);

	priv = esummary->private;

	window = g_new0 (ESummaryWindow, 1);
	window->component = component;
	window->iid = g_strdup (iid);
	window->esummary = esummary;

	/* See what interfaces our component supports */
	CORBA_exception_init (&ev);
	unknown = Bonobo_Unknown_queryInterface (component,
						 "IDL:Bonobo/Control:1.0", &ev);
	window->control = (Bonobo_Control) unknown;

	unknown = Bonobo_Unknown_queryInterface (component,
						 "IDL:GNOME/Evolution/Summary/HTMLView:1.0", 
						 &ev);
	window->html = (GNOME_Evolution_Summary_HTMLView) unknown;

	/* Check at least one of the above interfaces was supported */
	if (window->html == CORBA_OBJECT_NIL &&
	    window->control == CORBA_OBJECT_NIL) {
		CORBA_Environment ev2;
		g_warning ("This component does not support either" 
			   "Bonobo/Control:1.0 or GNOME/Evolution/Summary/HTMLView:1.0");

		CORBA_exception_init (&ev2);
		CORBA_Object_release (component, &ev2);
		CORBA_exception_free (&ev2);

		g_free (window);
		return NULL;
	}

	window->event_source = Bonobo_Unknown_queryInterface(window->component,
							     "IDL:Bonobo/EventSource:1.0", &ev);
	if (window->event_source == CORBA_OBJECT_NIL) {
		g_warning ("There is no Bonobo::EventSource interface");

		/* FIXME: Free whatever objects exist */
		g_free (window);
		return NULL;
	}

	window->listener = bonobo_listener_new (NULL, NULL);
	gtk_signal_connect (GTK_OBJECT (window->listener), "event_notify",
			    GTK_SIGNAL_FUNC (listener_event), window);
	listener = bonobo_object_corba_objref (BONOBO_OBJECT (window->listener));
	window->listener_id = Bonobo_EventSource_addListener (window->event_source, listener, &ev);

	unknown = Bonobo_Unknown_queryInterface (component,
						 "IDL:Bonobo/PropertyBag:1.0",
						 &ev);
	window->propertybag = (Bonobo_PropertyBag) unknown;

	unknown = Bonobo_Unknown_queryInterface (component,
						 "IDL:Bonobo/PersistStream:1.0",
						 &ev);
	window->persiststream = (Bonobo_PersistStream) unknown;
	
	unknown = Bonobo_Unknown_queryInterface (component,
						 "IDL:Bonobo/PropertyControl:1.0",
						 &ev);
	window->propertycontrol = (Bonobo_PropertyControl) unknown;

	/* Cache the title and icon */
	window->title = bonobo_property_bag_client_get_value_string (window->propertybag,
								     "window_title", 
								     NULL);
	window->icon = bonobo_property_bag_client_get_value_string (window->propertybag,
								    "window_icon", NULL);

	CORBA_exception_free (&ev);
	priv->window_list = g_list_append (priv->window_list, window);

	return window;
}


ESummaryWindow *
e_summary_embed_service_from_id (ESummary *esummary,
				 const char *obj_id)
{
	GNOME_Evolution_Summary_Component component;
	ExecutiveSummaryComponentFactoryClient *client;
	ESummaryWindow *window;
	
	client = executive_summary_component_factory_client_new (obj_id);
	
	component = executive_summary_component_factory_client_create_view (client);

	/* Don't need the client any more */
	bonobo_object_unref (BONOBO_OBJECT (client));

	window = e_summary_add_service (esummary, component, obj_id);
	e_summary_queue_rebuild (esummary);
	
	return window;
}

void
e_summary_window_free (ESummaryWindow *window)
{
	CORBA_Environment ev;

	g_return_if_fail (window != NULL);

	g_free (window->iid);
	g_free (window->icon);
	g_free (window->title);

	CORBA_exception_init (&ev);

	if (window->control != CORBA_OBJECT_NIL) {
		bonobo_object_release_unref (window->control, &ev);
	}

	if (window->event_source != CORBA_OBJECT_NIL) {
		Bonobo_EventSource_removeListener (window->event_source,
						   window->listener_id,
						   &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("CORBA ERROR: %s", CORBA_exception_id (&ev));
		}
    		bonobo_object_release_unref (window->event_source, &ev);
	}

	bonobo_object_release_unref (window->propertybag, &ev);
	bonobo_object_release_unref (window->persiststream, &ev);
	bonobo_object_release_unref (window->propertycontrol, &ev);
	bonobo_object_unref (BONOBO_OBJECT (window->listener));
	bonobo_object_release_unref (window->html, &ev);
	
	bonobo_object_release_unref (window->component, &ev);
	CORBA_exception_free (&ev);

	g_free (window);
}

void
e_summary_remove_window (ESummary *esummary,
			 ESummaryWindow *window)
{
	ESummaryPrivate *priv;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));
	g_return_if_fail (window != NULL);

	priv = esummary->private;
	priv->window_list = g_list_remove (priv->window_list, window);
	e_summary_window_free (window);
}
	
void
e_summary_set_shell_view_interface (ESummary *esummary,
				    GNOME_Evolution_ShellView svi)
{
	ESummaryPrivate *priv;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));
	g_return_if_fail (svi != CORBA_OBJECT_NIL);

	priv = esummary->private;
	priv->shell_view_interface = svi;
}

/* Wrappers for GNOME_Evolution_ShellView */
void
e_summary_set_message (ESummary *esummary,
		       const char *message,
		       gboolean busy)
{
	ESummaryPrivate *priv;
	GNOME_Evolution_ShellView svi;
	CORBA_Environment ev;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

	priv = esummary->private;

	svi = priv->shell_view_interface;
	if (svi == NULL)
		return;

	CORBA_exception_init (&ev);
	if (message != NULL)
		GNOME_Evolution_ShellView_setMessage (svi, message, busy, &ev);
	else 
		GNOME_Evolution_ShellView_setMessage (svi, "", busy, &ev);
	CORBA_exception_free (&ev);
}

void
e_summary_unset_message (ESummary *esummary)
{
	ESummaryPrivate *priv;
	GNOME_Evolution_ShellView svi;
	CORBA_Environment ev;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

	priv = esummary->private;

	svi = priv->shell_view_interface;
	if (svi == NULL)
		return;

	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellView_unsetMessage (svi, &ev);
	CORBA_exception_free (&ev);
}

void
e_summary_change_current_view (ESummary *esummary,
			       const char *uri)
{
	ESummaryPrivate *priv;
	GNOME_Evolution_ShellView svi;
	CORBA_Environment ev;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

	priv = esummary->private;

	svi = priv->shell_view_interface;
	if (svi == NULL)
		return;

	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellView_changeCurrentView (svi, uri, &ev);
	CORBA_exception_free (&ev);
}

void
e_summary_set_title (ESummary *esummary,
		     const char *title)
{
	ESummaryPrivate *priv;
	GNOME_Evolution_ShellView svi;
	CORBA_Environment ev;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

	priv = esummary->private;

	svi = priv->shell_view_interface;
	if (svi == NULL)
		return;

	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellView_setTitle (svi, title, &ev);
	CORBA_exception_free (&ev);
}

static void
e_summary_load_page (ESummary *esummary)
{
	ESummaryPrivate *priv;
	GnomeVFSHandle *handle = NULL;
	GnomeVFSResult result;
	GtkWidget *toplevel;
	GString *string;
	char *str, *comment;
	char *filename;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));
	
	priv = esummary->private;

	/* Pass NULL to reset the page to the default */
	if ((esummary->prefs->page) == NULL || 
	    *(esummary->prefs->page) == '\0') {
		filename = g_concat_dir_and_file (EVOLUTION_DATADIR, "/evolution/summary.html");
	} else {
		filename = g_strdup (esummary->prefs->page);
	}

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (esummary));
	string = g_string_new ("");
	result = gnome_vfs_open (&handle, filename, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		e_notice (GTK_WINDOW (toplevel), GNOME_MESSAGE_BOX_WARNING,
			  _("Cannot open the HTML file:\n%s"), filename);
		g_free (filename);
		return;
	}

	g_free (filename);
	while (1) {
		char buffer[4096];
		GnomeVFSFileSize size;

		memset (buffer, 0x00, 4096);
		result = gnome_vfs_read (handle, buffer, 4096, &size);
		if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
			e_notice (GTK_WINDOW (toplevel), GNOME_MESSAGE_BOX_WARNING, 
				  _("Error reading data:\n%s"),
				  gnome_vfs_result_to_string (result));
			gnome_vfs_close (handle);
			return;
		}
		if (size == 0)
			break; /* EOF */

		string = g_string_append (string, buffer);
	}

	gnome_vfs_close (handle);
	str = string->str;
	g_string_free (string, FALSE);

	comment = strstr (str, "<!-- EVOLUTION EXECUTIVE SUMMARY SERVICES DO NOT REMOVE -->");
	if (comment == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_WARNING, 
			  _("File does not have a place for the services.\n"));
		g_free (str);
		return;
	}

	if (priv->header != NULL)
		g_free (priv->header);
	priv->header = g_strndup (str, comment - str);
	priv->header_len = strlen (priv->header);

	if (priv->footer != NULL)
		g_free (priv->footer);
	priv->footer = g_strdup (comment);
	priv->footer_len = strlen (priv->footer);
	g_free (str);
}

static char *
load_component_id_stream_read (Bonobo_Stream stream,
			       CORBA_Environment *ev)
{
	Bonobo_Stream_iobuf *buffer;
	GString *str;
	char *ans;

	str = g_string_sized_new (256);
#define READ_CHUNK_SIZE 65536
	do {
		int i;
		Bonobo_Stream_read (stream, READ_CHUNK_SIZE, &buffer, ev);
		if (ev->_major != CORBA_NO_EXCEPTION)
			return NULL;

		/* FIXME: make better PLEASE!!!*/
		for (i = 0; i < buffer->_length; i++)
			g_string_append_c (str, buffer->_buffer[i]);

		if (buffer->_length <= 0)
			break;
		CORBA_free (buffer);
	} while (1);
#undef READ_CHUNK_SIZE
	CORBA_free (buffer);

	ans = str->str;
	g_string_free (str, FALSE);

	return ans;
}

static char *
load_component_id (Bonobo_Storage corba_storage,
		   CORBA_Environment *ev)
{
	Bonobo_Stream corba_stream;
	char *iid;

	corba_stream = Bonobo_Storage_openStream (corba_storage, IID_FILE,
						  Bonobo_Storage_READ, ev);
	if (ev->_major != CORBA_NO_EXCEPTION)
		return NULL;

	if (corba_stream) {
		iid = load_component_id_stream_read (corba_stream, ev);
		Bonobo_Unknown_unref (corba_stream, ev);
		CORBA_Object_release (corba_stream, ev);
	} else {
		g_warning ("Cannot find `%s'", IID_FILE);
		return NULL;
	}

	return iid;
}

static void
load_component (ESummary *esummary,
		BonoboStorage *storage,
		int index)
{
	char *curdir;
	char *iid;
	Bonobo_Storage corba_subdir;
	Bonobo_Storage corba_storage;
	ESummaryWindow *window;
	CORBA_Environment ev;

	curdir = g_strdup_printf ("%08d", index);
	corba_storage = bonobo_object_corba_objref (BONOBO_OBJECT (storage));
	CORBA_exception_init (&ev);

	corba_subdir = Bonobo_Storage_openStorage (corba_storage, curdir,
						   Bonobo_Storage_READ, &ev);
	if (corba_subdir == CORBA_OBJECT_NIL) {
		g_free (curdir);
		return;
	}

	iid = load_component_id (corba_subdir, &ev);

	if (iid) {
		Bonobo_Stream corba_stream;

		window = e_summary_embed_service_from_id (esummary, iid);
		if (window) {
			if (window->persiststream) {
				corba_stream = Bonobo_Storage_openStream 
					(corba_subdir,
					 DATA_FILE,
					 Bonobo_Storage_READ |
					 Bonobo_Storage_CREATE, &ev);
				if (ev._major != CORBA_NO_EXCEPTION) {
					g_print ("Gah");
					return;
				}

				Bonobo_PersistStream_load (window->persiststream,
							   corba_stream, 
							   "", &ev);
				if (ev._major != CORBA_NO_EXCEPTION)
					g_warning ("Could not load `%s'", iid);

				bonobo_object_release_unref (corba_stream, &ev);
			}
		}

		g_free (iid);
	}

	bonobo_object_release_unref (corba_subdir, &ev);
	CORBA_exception_free (&ev);
	g_free (curdir);
}

void
e_summary_reconfigure (ESummary *esummary)
{
	e_summary_load_page (esummary);
	e_summary_queue_rebuild (esummary);
}

static void
e_summary_load_state (ESummary *esummary,
		      const char *path)
{
	char *fullpath;
	BonoboStorage *storage;
	Bonobo_Storage corba_storage;
	Bonobo_Storage_DirectoryList *list;
	CORBA_Environment ev;
	int i;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

	fullpath = g_strdup_printf ("%s/Executive-Summary", path);
	storage = bonobo_storage_open (STORAGE_TYPE, fullpath,
				       Bonobo_Storage_READ |
				       Bonobo_Storage_WRITE, 
				       0664);
	if (storage != NULL) {
		CORBA_exception_init (&ev);
		
		corba_storage = bonobo_object_corba_objref (BONOBO_OBJECT (storage));
		list = Bonobo_Storage_listContents (corba_storage, "/", 0, &ev);
		if (list) {
			for (i = 0; i < list->_length; i++)
				load_component (esummary, storage, i);
			
			CORBA_free (list);
		}

		bonobo_object_unref (BONOBO_OBJECT (storage));
		CORBA_exception_free (&ev);
	}

	g_free (fullpath);

	/* Load the preferences */
	if (esummary->prefs != NULL)
		e_summary_prefs_free (esummary->prefs);

	esummary->prefs = e_summary_prefs_load (path);
	e_summary_reconfigure (esummary);
}

static void
save_component (BonoboStorage *storage,
		ESummaryWindow *window,
		int index)
{
	char *curdir = g_strdup_printf ("%08d", index);
	Bonobo_Storage corba_storage;
	Bonobo_Storage corba_subdir;
	CORBA_Environment ev;

	corba_storage = bonobo_object_corba_objref (BONOBO_OBJECT (storage));
	CORBA_exception_init (&ev);

	corba_subdir = Bonobo_Storage_openStorage (corba_storage, curdir,
						   Bonobo_Storage_CREATE|
						   Bonobo_Storage_WRITE|
						   Bonobo_Storage_READ, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot create '%s'", curdir);
		g_free (curdir);
	} else {
		Bonobo_Stream corba_stream;

		g_free (curdir);
		corba_stream = Bonobo_Storage_openStream
			(corba_subdir, IID_FILE, 
			 Bonobo_Storage_CREATE|
			 Bonobo_Storage_READ|
			 Bonobo_Storage_WRITE, &ev);

		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("EEK: %s", CORBA_exception_id (&ev));
			if (corba_subdir != CORBA_OBJECT_NIL)
				bonobo_object_release_unref (corba_subdir, &ev);
			CORBA_exception_free (&ev);
			return;
		}

		bonobo_stream_client_write_string (corba_stream,
						   window->iid, TRUE, &ev);
		bonobo_object_release_unref (corba_stream, &ev);

		corba_stream = Bonobo_Storage_openStream (corba_subdir, DATA_FILE,
							  Bonobo_Storage_CREATE,
							  &ev);
		if (window->persiststream != CORBA_OBJECT_NIL) {
			Bonobo_PersistStream_save (window->persiststream,
						   corba_stream, "", &ev);
			if (ev._major != CORBA_NO_EXCEPTION) {
				g_warning ("Unable to save %s", window->iid);
			}
		}

		bonobo_object_release_unref (corba_stream, &ev);
	}

	if (corba_subdir != CORBA_OBJECT_NIL)
		bonobo_object_release_unref (corba_subdir, &ev);
	CORBA_exception_free (&ev);
}

static void
e_summary_save_state (ESummary *esummary,
		      const char *path)
{
	ESummaryPrivate *priv;
	BonoboStorage *storage;
	Bonobo_Storage corba_storage;
	CORBA_Environment ev;
	GList *windows;
	char *fullpath;
	int i;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

	priv = esummary->private;

	fullpath = g_strdup_printf("%s/Executive-Summary", path);
	g_print ("fullpath: %s\n", fullpath);

	/* FIXME: Use RC's rmdir function */
	e_summary_rm_dir (fullpath);

	storage = bonobo_storage_open (STORAGE_TYPE, fullpath,
				       Bonobo_Storage_READ |
				       Bonobo_Storage_WRITE |
				       Bonobo_Storage_CREATE, 0660);
	g_return_if_fail (storage);

	CORBA_exception_init (&ev);
	corba_storage = bonobo_object_corba_objref (BONOBO_OBJECT (storage));
	
	i = 0;
	for (windows = priv->window_list; windows; windows = windows->next) {
		save_component (storage, windows->data, i);
		g_print ("IID: %s\n", ((ESummaryWindow *)windows->data)->iid);
		i++;
	}

	Bonobo_Storage_commit (corba_storage, &ev);
	CORBA_exception_free (&ev);
	bonobo_object_unref (BONOBO_OBJECT (storage));

	e_summary_prefs_save (esummary->prefs, path);
	g_free (fullpath);
}

void
e_summary_window_move_left (ESummary *esummary,
			    ESummaryWindow *window)
{
	ESummaryPrivate *priv;
	GList *win_item, *grandparent;
	int position;

	priv = esummary->private;

	/* Need to cache this location */
	win_item = g_list_find (priv->window_list, window);
	
	/* Find the item 2 previous. */
	if (win_item->prev == NULL)
		return; /* Item was first, can't be moved left */

	grandparent = win_item->prev->prev;

	/* Remove it from the list */
	priv->window_list = g_list_remove_link (priv->window_list, win_item);

	/* Insert it after the grandparent */
	position = g_list_position (priv->window_list, grandparent);
	priv->window_list = g_list_insert (priv->window_list, win_item->data,
					   position + 1);
	g_list_free_1 (win_item);
}

void
e_summary_window_move_right (ESummary *esummary,
			     ESummaryWindow *window)
{
	ESummaryPrivate *priv;
	GList *win_item, *child;
	int position;

	priv = esummary->private;

	win_item = g_list_find (priv->window_list, window);

	if (win_item->next == NULL)
		return;

	child = win_item->next;
	
	priv->window_list = g_list_remove_link (priv->window_list, win_item);
	
	position = g_list_position (priv->window_list, child);
	priv->window_list = g_list_insert (priv->window_list, win_item->data,
					   position + 1);
	g_list_free_1 (win_item);
}

void
e_summary_window_move_up (ESummary *esummary,
			  ESummaryWindow *window)
{
	ESummaryPrivate *priv;
	GList *win_item;
	int position;

	priv = esummary->private;

	win_item = g_list_find (priv->window_list, window);
	
	position = g_list_position (priv->window_list, win_item);
	priv->window_list = g_list_remove_link (priv->window_list, win_item);

	priv->window_list = g_list_insert (priv->window_list, win_item->data,
					   position - 3);
	g_list_free_1 (win_item);
}

void
e_summary_window_move_down (ESummary *esummary,
			    ESummaryWindow *window)
{
	ESummaryPrivate *priv;
	GList *win_item;
	int position;

	priv = esummary->private;

	win_item = g_list_find (priv->window_list, window);

	position = g_list_position (priv->window_list, win_item);
	priv->window_list = g_list_remove_link (priv->window_list, win_item);

	priv->window_list = g_list_insert (priv->window_list, win_item->data,
					   position + 3);
}
