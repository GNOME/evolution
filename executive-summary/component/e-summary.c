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
#include <gtkhtml/gtkhtml-stream.h>
#include <gal/util/e-util.h>

#include <evolution-services/executive-summary.h>
#include <evolution-services/executive-summary-component-client.h>
#include <evolution-services/executive-summary-component-view.h>

#include "e-summary.h"
#include "e-summary-util.h"
#include "e-summary-url.h"

#define PARENT_TYPE (gtk_vbox_get_type ())

static GtkObjectClass *e_summary_parent_class;

struct _ESummaryPrivate {
	Evolution_Shell shell;

	GtkWidget *html_scroller;
	GtkWidget *html;

	GHashTable *id_to_view;
	GHashTable *summary_to_window;
	GList *window_list;

	guint idle;

	GtkHTMLStream *stream;
};

typedef struct _ESummaryWindow {
	ExecutiveSummary *summary;
	ExecutiveSummaryComponentView *view;
} ESummaryWindow;

static gboolean on_object_requested (GtkHTML *html,
				     GtkHTMLEmbedded *eb,
				     ESummary *summary);
static void e_summary_window_free (ESummaryWindow *window,
				   ESummaryPrivate *priv);
		
/* GtkObject methods */

static void
s2w_foreach (gpointer *key,
	     gpointer *value,
	     ESummaryPrivate *priv)
{
	e_summary_window_free ((ESummaryWindow *) value, priv);
	g_free (value);
}

static void
e_summary_destroy (GtkObject *object)
{
	ESummary *esummary = E_SUMMARY (object);
	ESummaryPrivate *priv;

	priv = esummary->private;
	if (priv == NULL)
		return;

	g_hash_table_foreach (priv->summary_to_window, 
			      s2w_foreach, priv);
	g_hash_table_destroy (priv->summary_to_window);
	g_hash_table_destroy (priv->id_to_view);

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
e_summary_start_load (ESummary *summary)
{
	ESummaryPrivate *priv;
	char *header = "<html><body bgcolor=\"#ffffff\">";

	priv = summary->private;

	priv->stream = gtk_html_begin (GTK_HTML (priv->html));

	/* Hack to stop page returning to the top */
	GTK_HTML (priv->html)->engine->newPage = FALSE;

	gtk_html_write (GTK_HTML (priv->html), priv->stream,
			header, strlen (header));
}

static void
load_default (ESummary *summary)
{
	ESummaryPrivate *priv;
	char *def = "<table width=\"100%\"><tr><td align=\"right\">"
		"<img src=\"ccsplash.png\"></td></tr></table>"
		"<table width=\"100%\"><tr><td><a href=\"exec://bug-buddy\"><img src=\"file://gnome-spider.png\">"
		"</a></td><td><a href=\"exec://bug-buddy\">Submit a bug report"
		"</a></td><td>All Executive Summary comments to <a href=\"mailto:iain@helixcode.com\">Iain Holmes</a></td></tr></table><hr>";

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	priv = summary->private;

	g_return_if_fail (priv->stream != NULL);

	gtk_html_write (GTK_HTML (priv->html), priv->stream, def, strlen (def));
}

static void
e_summary_end_load (ESummary *summary)
{
	ESummaryPrivate *priv;
	char *footer = "</body></html>";

	priv = summary->private;
	gtk_html_write (GTK_HTML (priv->html), priv->stream, 
			footer, strlen (footer));
	gtk_html_end (GTK_HTML (priv->html), priv->stream, GTK_HTML_STREAM_OK);

	priv->stream = NULL;
}

static void
e_summary_init (ESummary *esummary)
{
	GdkColor bgcolour = {0, 0xdfff, 0xdfff, 0xffff};
	ESummaryPrivate *priv;

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
	
	gtk_container_add (GTK_CONTAINER (priv->html_scroller), priv->html);
	gtk_widget_show_all (priv->html_scroller);

	e_summary_rebuild_page (esummary);

	/* Pack stuff */
	gtk_box_pack_start (GTK_BOX (esummary), priv->html_scroller, 
			    TRUE, TRUE, 0);

	/* Init hashtables */
	priv->summary_to_window = g_hash_table_new (NULL, NULL);
	priv->id_to_view = g_hash_table_new (NULL, NULL);
}

E_MAKE_TYPE (e_summary, "ESummary", ESummary, e_summary_class_init,
	     e_summary_init, PARENT_TYPE);

GtkWidget *
e_summary_new (const Evolution_Shell shell)
{
	ESummary *esummary;
	ESummaryPrivate *priv;

	esummary = gtk_type_new (e_summary_get_type ());
	priv = esummary->private;

	priv->shell = shell;

	return GTK_WIDGET (esummary);
}

static gboolean
on_object_requested (GtkHTML *html,
		     GtkHTMLEmbedded *eb,
		     ESummary *summary)
{
	ESummaryWindow *window;
	int type;

	if (sscanf (eb->classid, "cid:%d-%p", &type, &window) != 2) {
		g_warning ("Could not get the window reference\n");
		return FALSE;
	}

	switch (type) {
	case 1:
		g_assert_not_reached ();
		break;

	case 2:
		g_warning ("Bonobo services are not supported in this version.");
		break;

	default:
		g_assert_not_reached ();
	}

	return TRUE;
}

static void
e_summary_display_window (ESummary *esummary,
			  ESummaryWindow *window,
			  int col)
{
	ESummaryPrivate *priv;
	char *footer = "</td></tr></table>";
	char *title_html;
	char *colour[2] = {"e6e8e4", 
			   "edeeeb"};
	char *title_colour[2] = {"bac1b6", 
				 "cdd1c7"};
	const char *title, *icon, *html;

	priv = esummary->private;

	title = executive_summary_component_view_get_title (window->view);
	icon = executive_summary_component_view_get_icon (window->view);
	html = executive_summary_component_view_get_html (window->view);

	/** FIXME: Make this faster by caching it? */
	title_html = g_strdup_printf ("<table cellspacing=\"0\" "
				      "cellpadding=\"0\" border=\"0\" width=\"100%%\" height=\"100%%\">"
				      "<tr><td bgcolor=\"#%s\">"
				      "<table width=\"100%%\"><tr><td>"
				      "<img src=\"%s\"></td>"
				      "<td nowrap align=\"center\" width=\"100%%\">"
				      "<b>%s</b></td></tr></table></td></tr><tr>"
				      "<td bgcolor=\"#%s\" height=\"100%%\">", 
				      title_colour[col % 2], icon, title,
				      colour[col % 2]);
	
	gtk_html_write (GTK_HTML (priv->html), priv->stream, title_html,
			strlen (title_html));
	g_free (title_html);
	
	if (html != NULL && *html != '\0') {
		gtk_html_write (GTK_HTML (priv->html), priv->stream,
				html, strlen (html));
	} else {
		g_warning ("Bonobo executive summary components are not supported at this time.");
#if 0
		body_cid = g_strdup_printf ("<object classid=\"cid:2-%p\"></object>", window);
		gtk_html_write (GTK_HTML (priv->html), priv->stream,
				body_cid, strlen (body_cid));
#endif
	}

	gtk_html_write (GTK_HTML (priv->html), priv->stream,
			footer, strlen (footer));
}

int
e_summary_rebuild_page (ESummary *esummary)
{
	ESummaryPrivate *priv;
	GList *windows;
	char *service_table = "<table numcols=\"3\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\" height=\"100%\">";
	int loc;

	g_return_val_if_fail (esummary != NULL, FALSE);
	g_return_val_if_fail (IS_E_SUMMARY (esummary), FALSE);

	priv = esummary->private;

	gtk_layout_freeze (GTK_LAYOUT (priv->html));
	e_summary_start_load (esummary);
	load_default (esummary);

	/* Load the start of the services */
	gtk_html_write (GTK_HTML (priv->html), priv->stream, service_table,
			strlen (service_table));
	/* Load each of the services */
	loc = 0;
	for (windows = priv->window_list; windows; windows = windows->next) {
		ESummaryWindow *window;
		char *td = "<td height=\"100%\" width=\"33%\" valign=\"top\">";
		
		window = windows->data;

		if (loc % 3 == 0) {
			if (loc != 0) {
				gtk_html_write (GTK_HTML (priv->html),
						priv->stream, "</tr>", 5);
			}
			gtk_html_write (GTK_HTML (priv->html),
					priv->stream, "<tr height=\"100%\">", 18);
		}

		gtk_html_write (GTK_HTML (priv->html), priv->stream,
				td, strlen (td));

		e_summary_display_window (esummary, window, (loc % 3));

		gtk_html_write (GTK_HTML (priv->html), priv->stream, "</td>", 5);
		loc++;
	}
				
	gtk_html_write (GTK_HTML (priv->html), priv->stream, "</tr></table>",
			13);
	e_summary_end_load (esummary);
	gtk_layout_thaw (GTK_LAYOUT (priv->html));

	priv->idle = 0;
	return FALSE;
}

void
e_summary_add_service (ESummary *esummary,
		       ExecutiveSummary *summary,
		       ExecutiveSummaryComponentView *view)
{
	ESummaryWindow *window;
	ESummaryPrivate *priv;
	int id;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));
	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY (summary));
	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW (view));

	window = g_new0 (ESummaryWindow, 1);
	window->summary = summary;

	window->view = view;

	priv = esummary->private;
	priv->window_list = g_list_append (priv->window_list, window);
	g_hash_table_insert (priv->summary_to_window, summary, window);
	
	id = executive_summary_component_view_get_id (view);
	g_print ("--%d: %p\n", id, view);
	g_hash_table_insert (priv->id_to_view, GINT_TO_POINTER (id), view);
}

#if 0
void
e_summary_add_html_service (ESummary *esummary,
			    ExecutiveSummary *summary,
			    ExecutiveSummaryComponentClient *client,
			    const char *html,
			    const char *title,
			    const char *icon)
{
	ESummaryWindow *window;
	ESummaryPrivate *priv;

	window = g_new0 (ESummaryWindow, 1);
	window->type = E_SUMMARY_WINDOW_HTML;
	window->html = g_strdup (html);
	window->title = g_strdup (title);
	window->icon = icon ? g_strdup (icon) : NULL;
	window->client = client;

	window->summary = summary;
	priv = esummary->private;
	priv->window_list = g_list_append (priv->window_list, window);

	g_hash_table_insert (priv->summary_to_window, summary, window);
}

void
e_summary_add_bonobo_service (ESummary *esummary,
			      ExecutiveSummary *summary,
			      ExecutiveSummaryComponentClient *client,
			      GtkWidget *control,
			      const char *title,
			      const char *icon)
{
	ESummaryWindow *window;
	ESummaryPrivate *priv;
	
	window = g_new0 (ESummaryWindow, 1);
	window->type = E_SUMMARY_WINDOW_BONOBO;
	window->control = control;

	window->client = client;

	window->title = g_strdup (title);
	window->summary = summary;
	window->icon = icon ? g_strdup (icon): NULL;

	priv = esummary->private;
	priv->window_list = g_list_append (priv->window_list, window);

	g_hash_table_insert (priv->summary_to_window, summary, window);
}
#endif

static void
e_summary_window_free (ESummaryWindow *window,
		       ESummaryPrivate *priv)
{
	g_print ("%s\n", __FUNCTION__);
	priv->window_list = g_list_remove (priv->window_list, window);

	bonobo_object_unref (BONOBO_OBJECT (window->summary));
	gtk_object_unref (GTK_OBJECT (window->view));
}

/* Call this before e_summary_window_free, execpt when you are freeing
   the hash table */
static void
e_summary_window_remove_from_ht (ESummaryWindow *window,
				 ESummaryPrivate *priv)
{
	g_hash_table_remove (priv->summary_to_window, window->summary);
}

void
e_summary_update_window (ESummary *esummary,
			 ExecutiveSummary *summary,
			 const char *html)
{
	ESummaryPrivate *priv;
	
	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));
	g_return_if_fail (summary != NULL);
	
	priv = esummary->private;
	
	if (priv->idle != 0)
		return;
	
	priv->idle = g_idle_add (e_summary_rebuild_page, esummary);
}

ExecutiveSummaryComponentView *
e_summary_view_from_id (ESummary *esummary,
			int id)
{
	ESummaryPrivate *priv;
	ExecutiveSummaryComponentView *view;

	g_return_val_if_fail (esummary != NULL, NULL);
	g_return_val_if_fail (IS_E_SUMMARY (esummary), NULL);
	g_return_val_if_fail (id > 0, NULL);

	priv = esummary->private;
	view = g_hash_table_lookup (priv->id_to_view, GINT_TO_POINTER (id));

	g_print ("%d: %p\n", id, view);
	return view;
}
