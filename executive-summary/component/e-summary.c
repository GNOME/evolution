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
#include <gtk/gtkvbox.h>
#include <gal/util/e-util.h>
#include <e-summary-subwindow.h>

#include <widgets/misc/e-title-bar.h>
#include <executive-summary.h>

#include "e-summary.h"

#define PARENT_TYPE (gtk_vbox_get_type ())

static GtkObjectClass *e_summary_parent_class;

struct _ESummaryPrivate {
	GtkWidget *html_scroller;
	GtkWidget *html;

	GHashTable *summary_to_window;
	GList *window_list;

	GtkHTMLStream *stream;
};

typedef enum {
	E_SUMMARY_WINDOW_BONOBO,
	E_SUMMARY_WINDOW_HTML
} ESummaryWindowType;

typedef struct _ESummaryWindow {
	ExecutiveSummary *summary;
	char *title;
	
	ESummaryWindowType type;
	
	char *html;
	GtkWidget *control;
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
	bonobo_object_unref (BONOBO_OBJECT (key));
	e_summary_window_free ((ESummaryWindow *) value, priv);
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

static char *
e_pixmap_file (const char *filename)
{
	char *ret;
	char *edir;

	/* Try the evolution images dir */
	edir = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution",
				      filename);

	if (g_file_exists (edir)) {
		ret = g_strdup (edir);
		g_free (edir);

		return ret;
	}

	/* Try the evolution button images dir */
	edir = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution/buttons",
				      filename);

	if (g_file_exists (edir)) {
		ret = g_strdup (edir);
		g_free (edir);
		
		return ret;
	}

	/* Fall back to the gnome_pixmap_file */
	return gnome_pixmap_file (filename);
}
	
static void
request_cb (GtkHTML *html,
	    const gchar *url,
	    GtkHTMLStream *stream)
{
	char *filename;
	FILE *handle;

	filename = e_pixmap_file (url);

	if (filename == NULL) {
		gtk_html_stream_close (stream, GTK_HTML_STREAM_ERROR);
		return;
	}

	handle = fopen (filename, "r");
	g_free (filename);

	if (handle == NULL) {
		gtk_html_stream_close (stream, GTK_HTML_STREAM_ERROR);
		return;
	}

	while (!feof (handle)) {
		char buffer[4096];
		int size;

		/* Clear buffer */
		memset (buffer, 0x00, 4096);

		size = fread (buffer, 1, 4096, handle);
		if (size != 4096) {
			/* Under run */
			if (feof (handle)) {
				gtk_html_stream_write (stream, buffer, size);
				gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);
				fclose (handle);
				return;
			} else {
				/* Error occurred */
				gtk_html_stream_close (stream, GTK_HTML_STREAM_ERROR);
				fclose (handle);
				return;
			}
		}
		gtk_html_stream_write (stream, buffer, 4096);
	}

	gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);
	fclose (handle);
}

static void
e_summary_start_load (ESummary *summary)
{
	ESummaryPrivate *priv;
	char *header = "<html><body>";

	priv = summary->private;
	priv->stream = gtk_html_begin (GTK_HTML (priv->html));
	gtk_html_write (GTK_HTML (priv->html), priv->stream,
			header, strlen (header));
}

static void
load_default (ESummary *summary)
{
	ESummaryPrivate *priv;
	char *def = "<table width=\"100%\"><tr><td align=\"right\"><img src=\"ccsplash.png\"></td></tr></table><hr>";

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
	char *footer = "<hr></body></html>";

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
	g_print ("priv: %p\n", priv);

	priv->window_list = NULL;
	/* HTML widget */
	priv->html_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->html_scroller),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	priv->html = gtk_html_new ();
	gtk_html_set_editable (GTK_HTML (priv->html), FALSE);
	gtk_signal_connect (GTK_OBJECT (priv->html), "url-requested",
			    GTK_SIGNAL_FUNC (request_cb), NULL);
  	gtk_signal_connect (GTK_OBJECT (priv->html), "object-requested", 
  			    GTK_SIGNAL_FUNC (on_object_requested), esummary); 
	
	gtk_html_set_default_background_color (GTK_HTML (priv->html), &bgcolour);
	gtk_container_add (GTK_CONTAINER (priv->html_scroller), priv->html);
	gtk_widget_show_all (priv->html_scroller);

	e_summary_rebuild_page (esummary);

	/* Pack stuff */
	gtk_box_pack_start (GTK_BOX (esummary), priv->html_scroller, 
			    TRUE, TRUE, 0);

	/* Init hashtable */
	priv->summary_to_window = g_hash_table_new (NULL, NULL);
}

E_MAKE_TYPE (e_summary, "ESummary", ESummary, e_summary_class_init,
	     e_summary_init, PARENT_TYPE);

GtkWidget *
e_summary_new (const Evolution_Shell shell)
{
	ESummary *esummary;

	esummary = gtk_type_new (e_summary_get_type ());

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
		gtk_widget_show (window->control);

		gtk_widget_ref (GTK_WIDGET (window->control));
		if (window->control->parent != NULL) {
			gtk_container_remove (GTK_CONTAINER (window->control->parent), window->control);
		}
		gtk_container_add (GTK_CONTAINER (eb), window->control);

		gtk_widget_unref (GTK_WIDGET (window->control));
		break;

	default:
		g_assert_not_reached ();
	}

	return TRUE;
}

static void
e_summary_display_window (ESummary *esummary,
			  ESummaryWindow *window)
{
	ESummaryPrivate *priv;
	char *footer = "</td></tr></table>";
	char *title_cid, *body_cid;

	priv = esummary->private;

	title_cid = g_strdup_printf ("<table height=\"100%%\" width=\"100%%\"><tr><td bgcolor=\"#ff0000\" align=\"left\"><b>%s</b></td></tr><tr><td>", window->title);
	gtk_html_write (GTK_HTML (priv->html), priv->stream, title_cid,
			strlen (title_cid));
	g_free (title_cid);
	
	switch (window->type) {
	case E_SUMMARY_WINDOW_HTML:
		gtk_html_write (GTK_HTML (priv->html), priv->stream,
				window->html, strlen (window->html));
		break;

	case E_SUMMARY_WINDOW_BONOBO:
		body_cid = g_strdup_printf ("<object classid=\"cid:2-%p\"></object>", window);
		gtk_html_write (GTK_HTML (priv->html), priv->stream,
				body_cid, strlen (body_cid));
		break;

	default:
		break;
	}

	gtk_html_write (GTK_HTML (priv->html), priv->stream,
			footer, strlen (footer));
}

void 
e_summary_rebuild_page (ESummary *esummary)
{
	ESummaryPrivate *priv;
	GList *windows;
	char *service_table = "<table numcols=\"2\" width=\"100%\">";
	int loc;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));

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
		
		window = windows->data;

		if (loc % 2 == 0) {
			g_print ("new line:%d\n", loc);
			if (loc != 0) {
				gtk_html_write (GTK_HTML (priv->html),
						priv->stream, "</tr>", 5);
			}
			gtk_html_write (GTK_HTML (priv->html),
					priv->stream, "<tr>", 4);
		}

		gtk_html_write (GTK_HTML (priv->html), priv->stream,
				"<td>", 4);

		e_summary_display_window (esummary, window);

		gtk_html_write (GTK_HTML (priv->html), priv->stream, "</td>", 5);
		loc++;
	}
				
	gtk_html_write (GTK_HTML (priv->html), priv->stream, "</tr></table>",
			13);
	e_summary_end_load (esummary);
	gtk_layout_thaw (GTK_LAYOUT (priv->html));
}

void
e_summary_add_html_service (ESummary *esummary,
			    ExecutiveSummary *summary,
			    const char *html,
			    const char *title)
{
	ESummaryWindow *window;
	ESummaryPrivate *priv;

	window = g_new0 (ESummaryWindow, 1);
	window->type = E_SUMMARY_WINDOW_HTML;
	window->html = g_strdup (html);
	window->title = g_strdup (title);

	window->summary = summary;
	priv = esummary->private;
	priv->window_list = g_list_append (priv->window_list, window);

	g_hash_table_insert (priv->summary_to_window, summary, window);
}

void
e_summary_add_bonobo_service (ESummary *esummary,
			      ExecutiveSummary *summary,
			      GtkWidget *control,
			      const char *title)
{
	ESummaryWindow *window;
	ESummaryPrivate *priv;
	
	window = g_new0 (ESummaryWindow, 1);
	window->type = E_SUMMARY_WINDOW_BONOBO;
	window->control = control;

	window->title = g_strdup (title);
	window->summary = summary;
	
	priv = esummary->private;
	priv->window_list = g_list_append (priv->window_list, window);

	g_hash_table_insert (priv->summary_to_window, summary, window);
}

static void
e_summary_window_free (ESummaryWindow *window,
		       ESummaryPrivate *priv)
{
	g_free (window->title);
	if (window->type == E_SUMMARY_WINDOW_BONOBO)
		gtk_widget_unref (window->control);
	else
		g_free (window->html);

	priv->window_list = g_list_remove (priv->window_list, window);

	g_hash_table_remove (priv->summary_to_window, window->summary);
	g_free (window);
}

void
e_summary_update_window (ESummary *esummary,
			 ExecutiveSummary *summary,
			 const char *html)
{
	ESummaryWindow *window;
	ESummaryPrivate *priv;

	g_return_if_fail (esummary != NULL);
	g_return_if_fail (IS_E_SUMMARY (esummary));
	g_return_if_fail (summary != NULL);

	priv = esummary->private;
	window = g_hash_table_lookup (priv->summary_to_window, summary);

	g_return_if_fail (window != NULL);

	g_free (window->html);
	window->html = g_strdup (html);

	e_summary_rebuild_page (esummary);
}
     
