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

#include "e-summary.h"
#include "e-summary-factory.h"
#include "e-summary-util.h"
#include "e-summary-url.h"

#define PARENT_TYPE (gtk_vbox_get_type ())

/* From component-factory.c */
extern char *evolution_dir;

static GtkObjectClass *e_summary_parent_class;

struct _ESummaryPrivate {
	GNOME_Evolution_Shell shell;
	GNOME_Evolution_ShellView shell_view_interface;

	GtkWidget *html_scroller;
	GtkWidget *html;

	GHashTable *id_to_view;
	GHashTable *view_to_window;
	GHashTable *summary_to_window;
	GList *window_list;

	guint idle;

	GtkHTMLStream *stream;
	gboolean grabbed;
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
s2w_foreach (gpointer *key,
	     gpointer *value,
	     ESummary *esummary)
{
	e_summary_window_free ((ESummaryWindow *) value, esummary);
	g_free (value);
}

static void
e_summary_destroy (GtkObject *object)
{
	ESummary *esummary = E_SUMMARY (object);
	ESummaryPrivate *priv;
	char *prefix;

	priv = esummary->private;
	if (priv == NULL)
		return;

	prefix = g_concat_dir_and_file (evolution_dir, "config/Executive-Summary");
	e_summary_save_state (esummary, prefix);
	g_free (prefix);

	g_hash_table_foreach (priv->summary_to_window, 
			      (GHFunc) s2w_foreach, esummary);
	g_hash_table_destroy (priv->summary_to_window);
	g_hash_table_destroy (priv->id_to_view);
	g_hash_table_destroy (priv->view_to_window);

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
		"<table><tr><td><a href=\"exec://bug-buddy\"><img src=\"file://gnome-spider.png\" width=\"24\" height=\"24\" border=\"0\">"
		"</a></td><td><a href=\"exec://bug-buddy\">Submit a bug report"
		"</a></td></tr></table><hr>";

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
	char *footer = "<hr><p align=\"right\">All Executive Summary comments to <a href=\"mailto:iain@helixcode.com\">Iain Holmes (iain@helixcode.com)</a></p></body></html>";

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
	gtk_signal_connect (GTK_OBJECT (priv->html), "on_url",
			    GTK_SIGNAL_FUNC (e_summary_url_over), esummary);

	gtk_container_add (GTK_CONTAINER (priv->html_scroller), priv->html);
	gtk_widget_show_all (priv->html_scroller);

	e_summary_rebuild_page (esummary);

	/* Pack stuff */
	gtk_box_pack_start (GTK_BOX (esummary), priv->html_scroller, 
			    TRUE, TRUE, 0);

	/* Init hashtables */
	priv->summary_to_window = g_hash_table_new (NULL, NULL);
	priv->id_to_view = g_hash_table_new (NULL, NULL);
	priv->view_to_window = g_hash_table_new (NULL, NULL);
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
				      "config/Executive-Summary");
	e_summary_load_state (esummary, path);
	g_free (path);

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
			  int row,
			  int col)
{
	ESummaryPrivate *priv;
	char *footer = "</td></tr></table>";
	char *title_html;
	char *control_html;
	char *colour[2] = {"e6e8e4", 
			   "edeeeb"};
	char *title_colour[2] = {"bac1b6", 
				 "cdd1c7"};
	const char *title, *icon, *html;
	int id;

	priv = esummary->private;

	title = executive_summary_component_view_get_title (window->view);
	icon = executive_summary_component_view_get_icon (window->view);
	html = executive_summary_component_view_get_html (window->view);
	id = executive_summary_component_view_get_id (window->view);

	/** FIXME: Make this faster by caching it? */

	control_html = g_strdup_printf 
		("<table width=\"32\" height=\"48\"><tr><td>"
		 "<a href=\"close://%d\"><img border=\"0\" src=\"service-close.png\"></a></td>"
		 "<td><a href=\"configure://%d\"><img border=\"0\" src=\"service-configure.png\"></a></td></tr>"
		 "<tr><td><a href=\"left://%d\"><img border=\"0\" src=\"service-left.png\"></a></td>"
		 "<td><a href=\"right://%d\"><img border=\"0\" src=\"service-right.png\"></a></td></tr>"
		 "<tr><td><a href=\"down://%d\"><img border=\"0\" src=\"service-down.png\"></a></td>"
		 "<td><a href=\"up://%d\"><img border=\"0\" src=\"service-up.png\"></a></td></tr></table>", id, id, id, id, id, id);
	
	title_html = g_strdup_printf ("<table cellspacing=\"0\" "
				      "cellpadding=\"0\" border=\"0\" width=\"100%%\" height=\"100%%\">"
				      "<tr><td bgcolor=\"#%s\">"
				      "<table width=\"100%%\"><tr><td>"
				      "<img src=\"%s\"></td>"
				      "<td nowrap align=\"center\" width=\"100%%\">"
				      "<b>%s</b></td><td>%s</td></tr></table></td></tr><tr>"
				      "<td bgcolor=\"#%s\" height=\"100%%\">",
				      title_colour[col % 2], icon, title,
				      control_html, colour[col % 2]);
	g_free (control_html);
	
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

	/* If there is a selection, don't redraw the page so that the selection
	   isn't cleared */
	if (GTK_HTML (priv->html)->in_selection == TRUE ||
	    html_engine_is_selection_active (GTK_HTML (priv->html)->engine) == TRUE)
		return TRUE;

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

		e_summary_display_window (esummary, window, 
					  (loc / 3), (loc % 3));

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
		       ExecutiveSummaryComponentView *view,
		       const char *iid)     
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
	window->iid = g_strdup (iid);
	window->view = view;

	priv = esummary->private;
	priv->window_list = g_list_append (priv->window_list, window);
	g_hash_table_insert (priv->summary_to_window, summary, window);
	
	id = executive_summary_component_view_get_id (view);
	g_hash_table_insert (priv->id_to_view, GINT_TO_POINTER (id), view);
	g_hash_table_insert (priv->view_to_window, view, window);
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

void
e_summary_window_free (ESummaryWindow *window,
		       ESummary *esummary)
{
	ESummaryPrivate *priv;

	g_return_if_fail (window != NULL);
	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

	priv = esummary->private;
	g_free (window->iid);

	priv->window_list = g_list_remove (priv->window_list, window);

	bonobo_object_unref (BONOBO_OBJECT (window->summary));
	gtk_object_unref (GTK_OBJECT (window->view));
}

/* Call this before e_summary_window_free, execpt when you are freeing
   the hash table */
void
e_summary_window_remove_from_ht (ESummaryWindow *window,
				 ESummary *esummary)
{
	ESummaryPrivate *priv;

	priv = esummary->private;
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
	
	priv->idle = g_idle_add ((GSourceFunc) e_summary_rebuild_page, esummary);
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

	return view;
}

void
e_summary_set_shell_view_interface (ESummary *summary,
				    GNOME_Evolution_ShellView svi)
{
	ESummaryPrivate *priv;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));
	g_return_if_fail (svi != CORBA_OBJECT_NIL);

	priv = summary->private;
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
e_summary_load_state (ESummary *esummary,
		      const char *path)
{
	char *fullpath;
	char **argv;
	int argc, i;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

	fullpath = g_strdup_printf ("=%s=/services/iids", path);
	gnome_config_get_vector (fullpath, &argc, &argv);

	for (i = 0; i < argc; i++) {
		e_summary_factory_embed_service_from_id (esummary, argv[i]);
	}
	
	g_free (argv);
	g_free (fullpath);
}

static void
e_summary_save_state (ESummary *esummary,
		      const char *path)
{
	ESummaryPrivate *priv;
	GList *windows;
	char *fullpath;
	char **argv;
	int argc, i;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

	fullpath = g_strdup_printf("=%s=/services/iids", path);
	priv = esummary->private;

	argc = g_list_length (priv->window_list);
	argv = g_new (char *, argc);

	for (windows = priv->window_list, i = 0; windows; 
	     windows = windows->next, i++) {
		ESummaryWindow *window;

		window = windows->data;
		g_print ("%s: IID: %s\n", path, window->iid);
		argv[i] = window->iid;
	}

	gnome_config_set_vector (fullpath, argc, (const char **)argv);

	gnome_config_sync ();
	gnome_config_drop_all ();

	g_free (fullpath);
	g_free (argv);
}

ESummaryWindow *
e_summary_window_from_view (ESummary *esummary,
			    ExecutiveSummaryComponentView *view)
{
	ESummaryPrivate *priv;

	priv = esummary->private;
	return g_hash_table_lookup (priv->view_to_window, view);
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
