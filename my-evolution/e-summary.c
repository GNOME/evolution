/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
 *
 * Author: Iain Holmes
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib.h>

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-url.h>

#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-stream.h>
#include <gtkhtml/htmlengine.h>
#include <gtkhtml/htmlselection.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>

#include <bonobo/bonobo-listener.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-ui-component.h>

#include <libgnome/gnome-url.h>

#include <libgnomeprint/gnome-print-job.h>

#include <libgnomeprintui/gnome-print-job-preview.h>
#include <libgnomeprintui/gnome-print-dialog.h>

#include <gtk/gtkdialog.h>

#include <cal-util/timeutil.h>

#include <gtk/gtkmain.h>
#include <gtk/gtkscrolledwindow.h>

#include <gconf/gconf-client.h>

#include <string.h>
#include <unistd.h>

#include "e-summary.h"
#include "e-summary-preferences.h"
#include "my-evolution-html.h"
#include "Mailer.h"

#include <Evolution.h>
#include "e-util/e-dialog-utils.h"

#include <time.h>

#define PARENT_TYPE (gtk_vbox_get_type ())

extern char *evolution_dir;

static GList *all_summaries = NULL;

static GtkObjectClass *e_summary_parent_class;

struct _ESummaryMailFolderInfo {
	char *name;

	int count;
	int unread;
};

struct _ESummaryPrivate {
	BonoboControl *control;

	GtkWidget *html_scroller;
	GtkWidget *html;

	GHashTable *protocol_hash;

	GList *connections;

	guint pending_reload_tag;

	guint tomorrow_timeout_id;

	gboolean frozen;

	int queued_draw_idle_id;
};

typedef struct _ProtocolListener {
	ESummaryProtocolListener listener;
	void *closure;
} ProtocolListener;

static GHashTable *images_cache = NULL;

static void
free_protocol (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);
}

static void
destroy (GtkObject *object)
{
	ESummary *summary;
	ESummaryPrivate *priv;

	summary = E_SUMMARY (object);
	priv = summary->priv;

	if (priv == NULL) {
		return;
	}

	all_summaries = g_list_remove (all_summaries, summary);
	
	if (priv->pending_reload_tag) {
		gtk_timeout_remove (priv->pending_reload_tag);
		priv->pending_reload_tag = 0;
	}

	if (priv->queued_draw_idle_id != 0) {
		g_source_remove (priv->queued_draw_idle_id);
		priv->queued_draw_idle_id = 0;
	}

	if (summary->mail) {
		e_summary_mail_free (summary);
	}
	if (summary->calendar) {
		e_summary_calendar_free (summary);
	}
	if (summary->rdf) {
		e_summary_rdf_free (summary);
	}
	if (summary->weather) {
		e_summary_weather_free (summary);
	}
	if (summary->tasks) {
		e_summary_tasks_free (summary);
	}

	if (priv->tomorrow_timeout_id != 0)
		g_source_remove (priv->tomorrow_timeout_id);

	if (priv->protocol_hash) {
		g_hash_table_foreach (priv->protocol_hash, free_protocol, NULL);
		g_hash_table_destroy (priv->protocol_hash);
	}

	g_free (priv);
	summary->priv = NULL;

	e_summary_parent_class->destroy (object);
}

static gboolean
draw_idle_cb (void *data)
{
	ESummary *summary;
	GString *string;
	GtkHTMLStream *stream;
	char *html;
	char date[256], *date_utf;
	time_t t;

	summary = E_SUMMARY (data);

	string = g_string_new (HTML_1);
	t = time (NULL);
	strftime (date, 255, _("%A, %B %e %Y"), localtime (&t));

	date_utf = e_utf8_from_locale_string (date);
	html = g_strdup_printf (HTML_2, date_utf);
	g_free (date_utf);
	g_string_append (string, html);
	g_free (html);
	g_string_append (string, HTML_3);

	/* Weather and RDF stuff here */
	html = e_summary_weather_get_html (summary);
	if (html != NULL) {
		g_string_append (string, html);
		g_free (html);
	}

	html = e_summary_rdf_get_html (summary);
	if (html != NULL) {
		g_string_append (string, html);
		g_free (html);
	}

	g_string_append (string, HTML_4);

	html = (char *) e_summary_mail_get_html (summary);
	if (html != NULL) {
		g_string_append (string, html);
	}

	html = (char *) e_summary_calendar_get_html (summary);
	if (html != NULL) {
		g_string_append (string, html);
	}
	
	html = (char *) e_summary_tasks_get_html (summary);
	if (html != NULL) {
		g_string_append (string, html);
	}

	g_string_append (string, HTML_5);

	stream = gtk_html_begin (GTK_HTML (summary->priv->html));
  	GTK_HTML (summary->priv->html)->engine->newPage = FALSE;
	gtk_html_write (GTK_HTML (summary->priv->html), stream, string->str, strlen (string->str));
	gtk_html_end (GTK_HTML (summary->priv->html), stream, GTK_HTML_STREAM_OK);

	g_string_free (string, TRUE);

	summary->priv->queued_draw_idle_id = 0;

	return FALSE;
}

void
e_summary_draw (ESummary *summary)
{
	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	if (summary->mail == NULL || summary->calendar == NULL
	    || summary->rdf == NULL || summary->weather == NULL 
	    || summary->tasks == NULL) {
		return;
	}

	if (summary->priv->queued_draw_idle_id != 0)
		return;

	summary->priv->queued_draw_idle_id = g_idle_add (draw_idle_cb, summary);
}

void
e_summary_redraw_all (void)
{
	GList *p;

	for (p = all_summaries; p; p = p->next) {
		e_summary_draw (E_SUMMARY (p->data));
	}
}

static char *
e_pixmap_file (const char *filename)
{
	char *ret;
	char *edir;

	if (g_file_exists (filename)) {
		ret = g_strdup (filename);

		return ret;
	}

	/* Try the evolution images dir */
	edir = g_concat_dir_and_file (EVOLUTION_IMAGESDIR, filename);

	if (g_file_exists (edir)) {
		ret = g_strdup (edir);
		g_free (edir);

		return ret;
	}
	g_free (edir);

	/* Try the evolution button images dir */
	edir = g_concat_dir_and_file (EVOLUTION_BUTTONSDIR, filename);

	if (g_file_exists (edir)) {
		ret = g_strdup (edir);
		g_free (edir);
		
		return ret;
	}
	g_free (edir);

	/* Fall back to the gnome_pixmap_file */
	ret = gnome_pixmap_file (filename);
	if (ret == NULL) {
		g_warning ("Could not find pixmap for %s", filename);
	}

	return ret;
}

struct _imgcache {
	char *buffer;
	int bufsize;
};

static void
e_summary_url_clicked (GtkHTML *html,
		       const char *url,
		       ESummary *summary)
{
	char *protocol, *protocol_end;
	ProtocolListener *protocol_listener;

	protocol_end = strchr (url, ':');
	if (protocol_end == NULL) {
		/* No url, let gnome work it out */
		gnome_url_show (url, NULL);
		return;
	}

	protocol = g_strndup (url, protocol_end - url);

	protocol_listener = g_hash_table_lookup (summary->priv->protocol_hash,
						 protocol);
	g_free (protocol);

	if (protocol_listener == NULL) {
		/* Again, let gnome work it out */
		gnome_url_show (url, NULL);
		return;
	}

	protocol_listener->listener (summary, url, protocol_listener->closure);
}

static char *
e_read_file_with_length (const char *filename,
			 size_t *length)
{
	int fd;
	struct stat stat_buf;
	char *buf;
	size_t bytes_read, size;

	g_return_val_if_fail (filename != NULL, NULL);

	fd = open (filename, O_RDONLY);
	g_return_val_if_fail (fd != -1, NULL);

	fstat (fd, &stat_buf);
	size = stat_buf.st_size;
	buf = g_new (char, size + 1);

	bytes_read = 0;
	while (bytes_read < size) {
		ssize_t rc;

		rc = read (fd, buf + bytes_read, size - bytes_read);
		if (rc < 0) {
			if (errno != EINTR) {
				close (fd);
				g_free (buf);
				
				return NULL;
			}
		} else if (rc == 0) {
			break;
		} else {
			bytes_read += rc;
		}
	}

	buf[bytes_read] = '\0';

	if (length) {
		*length = bytes_read;
	}

	return buf;
}

static void
e_summary_url_requested (GtkHTML *html,
			 const char *url,
			 GtkHTMLStream *stream,
			 ESummary *summary)
{
	char *filename;
	struct _imgcache *img = NULL;

	if (strncasecmp (url, "file:", 5) == 0) {
		url += 5;
		filename = e_pixmap_file (url);
	} else if (strchr (url, ':') >= strchr (url, '/')) {
		filename = e_pixmap_file (url);
	} else {
		filename = g_strdup (url);
	}

	if (filename == NULL) {
		gtk_html_stream_close (stream, GTK_HTML_STREAM_ERROR);
		return;
	}

	if (images_cache != NULL) {
		img = g_hash_table_lookup (images_cache, filename);
	} else {
		images_cache = g_hash_table_new (g_str_hash, g_str_equal);
	}

	if (img == NULL) {
		size_t length;
		char *contents;

		contents = e_read_file_with_length (filename, &length);
		if (contents == NULL) {
			return;
		}

		img = g_new (struct _imgcache, 1);
		img->buffer = contents;
		img->bufsize = length;

		g_hash_table_insert (images_cache, g_strdup (filename), img);
	}

	gtk_html_stream_write (stream, img->buffer, img->bufsize);
	gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);
}

static void
e_summary_evolution_protocol_listener (ESummary *summary,
				       const char *uri,
				       void *closure)
{
	e_summary_change_current_view (summary, uri);
}

static void
e_summary_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = destroy;

	e_summary_parent_class = g_type_class_ref(PARENT_TYPE);
}

static gboolean tomorrow_timeout (gpointer data);

static void
reset_tomorrow_timeout (ESummary *summary)
{
	time_t now, day_end;

	now = time (NULL);
	if (summary->tz)
		day_end = time_day_end_with_zone (now, summary->tz);
	else
		day_end = time_day_end (now);

	/* (Yes, the number of milliseconds in a day is less than UINT_MAX) */
	summary->priv->tomorrow_timeout_id =
		g_timeout_add ((day_end - now) * 1000,
			       tomorrow_timeout, summary);
}

static gboolean
tomorrow_timeout (gpointer data)
{
	ESummary *summary = data;

	reset_tomorrow_timeout (summary);
	e_summary_reconfigure (summary);

	return FALSE;
}
	
#define DEFAULT_HTML "<html><head><title>Summary</title></head><body bgcolor=\"#ffffff\">%s</body></html>" 

static void
e_summary_init (ESummary *summary)
{
	GConfClient *gconf_client;
	ESummaryPrivate *priv;
	char *def;

	summary->priv = g_new (ESummaryPrivate, 1);

	priv = summary->priv;

	priv->control = NULL;
	
	priv->frozen = TRUE;
	priv->pending_reload_tag = 0;

	priv->html_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->html_scroller),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	priv->html = gtk_html_new ();
	gtk_html_set_editable (GTK_HTML (priv->html), FALSE);
	gtk_html_set_default_content_type (GTK_HTML (priv->html),
					   "text/html; charset=utf-8");

	def = g_strdup_printf (DEFAULT_HTML, _("Please wait..."));
	gtk_html_load_from_string (GTK_HTML (priv->html), def, strlen (def));
	g_free (def);

	g_signal_connect (priv->html, "url-requested", G_CALLBACK (e_summary_url_requested), summary);
	g_signal_connect (priv->html, "link-clicked", G_CALLBACK (e_summary_url_clicked), summary);

	gtk_container_add (GTK_CONTAINER (priv->html_scroller), priv->html);
	gtk_widget_show_all (priv->html_scroller);
	gtk_box_pack_start (GTK_BOX (summary), priv->html_scroller,
			    TRUE, TRUE, 0);

	priv->protocol_hash = NULL;
	priv->connections = NULL;

	gconf_client = gconf_client_get_default ();

	summary->timezone = gconf_client_get_string (gconf_client, "/apps/evolution/calendar/display/timezone", NULL);
	if (!summary->timezone || !summary->timezone[0]) {
		g_free (summary->timezone);
		summary->timezone = g_strdup ("UTC");
	}
	summary->tz = icaltimezone_get_builtin_timezone (summary->timezone);
	reset_tomorrow_timeout (summary);

	g_object_unref (gconf_client);

	priv->queued_draw_idle_id = 0;
}

E_MAKE_TYPE (e_summary, "ESummary", ESummary, e_summary_class_init,
	     e_summary_init, PARENT_TYPE);

GtkWidget *
e_summary_new (ESummaryPrefs *prefs)
{
	ESummary *summary;

	summary = gtk_type_new (e_summary_get_type ());

	/* Just get a pointer to the global preferences */
	summary->preferences = prefs;
	
	e_summary_add_protocol_listener (summary, "evolution", e_summary_evolution_protocol_listener, summary);

	e_summary_mail_init (summary);
	e_summary_calendar_init (summary);
	e_summary_tasks_init (summary);
	e_summary_rdf_init (summary);
	e_summary_weather_init (summary);

	all_summaries = g_list_prepend (all_summaries, summary);
	return GTK_WIDGET (summary);
}

BonoboControl *
e_summary_get_control (ESummary *summary)
{
	g_return_val_if_fail (summary != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (IS_E_SUMMARY (summary), CORBA_OBJECT_NIL);

	return summary->priv->control;
}

void 
e_summary_set_control (ESummary *summary, BonoboControl *control)
{
	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	if (summary->priv->control)
		g_object_remove_weak_pointer (G_OBJECT (summary->priv->control), (void **) &summary->priv->control);
	
	summary->priv->control = control;

	if (summary->priv->control)
		g_object_add_weak_pointer (G_OBJECT (summary->priv->control), (void **) &summary->priv->control);
}

static void
do_summary_print (ESummary *summary)
{
	GnomePrintContext *print_context;
	GnomePrintJob *print_master;
	GtkWidget *gpd;
	GnomePrintConfig *config = NULL;
	GtkWidget *preview_widget;
	gboolean preview = FALSE;

	gpd = gnome_print_dialog_new (NULL, _("Print Summary"), GNOME_PRINT_DIALOG_COPIES);

	switch (gtk_dialog_run (GTK_DIALOG (gpd))) {
	case GNOME_PRINT_DIALOG_RESPONSE_PRINT:
		preview = FALSE;
		break;

	case GNOME_PRINT_DIALOG_RESPONSE_PREVIEW:
		preview = TRUE;
		break;

	default:
		if (preview_widget != NULL)
			gtk_widget_destroy (preview_widget);
		gtk_widget_destroy (gpd);
		return;
	}

	config = gnome_print_dialog_get_config (GNOME_PRINT_DIALOG (gpd));

	print_master = gnome_print_job_new (config);
	
	print_context = gnome_print_job_get_context (print_master);
	gtk_html_print (GTK_HTML (summary->priv->html), print_context);
	gnome_print_job_close (print_master);

	gtk_widget_destroy (gpd);

	if (preview) {
		preview_widget = gnome_print_job_preview_new (print_master, _("Print Preview"));
		gtk_widget_show (preview_widget);
	} else {
		int result = gnome_print_job_print (print_master);

		if (result == -1)
			e_notice (gpd, GTK_MESSAGE_ERROR, _("Printing of Summary failed"));
	}

	g_object_unref (print_master);
}

void
e_summary_print (BonoboUIComponent *component,
		 gpointer userdata,
		 const char *cname)
{
	ESummary *summary = userdata;

	do_summary_print (summary);
}

void
e_summary_add_protocol_listener (ESummary *summary,
				 const char *protocol,
				 ESummaryProtocolListener listener,
				 void *closure)
{
	ProtocolListener *old;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));
	g_return_if_fail (protocol != NULL);
	g_return_if_fail (listener != NULL);

	if (summary->priv->protocol_hash == NULL) {
		summary->priv->protocol_hash = g_hash_table_new (g_str_hash,
								 g_str_equal);
		old = NULL;
	} else {
		old = g_hash_table_lookup (summary->priv->protocol_hash, protocol);
	}

	if (old != NULL) {
		return;
	}

	old = g_new (ProtocolListener, 1);
	old->listener = listener;
	old->closure = closure;

	g_hash_table_insert (summary->priv->protocol_hash, g_strdup (protocol), old);
}

static GNOME_Evolution_ShellView
retrieve_shell_view_interface (BonoboControl *control)
{
	Bonobo_ControlFrame control_frame;
	GNOME_Evolution_ShellView shell_view_interface;
	CORBA_Environment ev;
	
	control_frame = bonobo_control_get_control_frame (control, NULL);
	
	if (control_frame == NULL)
		return CORBA_OBJECT_NIL;
	
	CORBA_exception_init (&ev);
	shell_view_interface = Bonobo_Unknown_queryInterface (control_frame,
							      "IDL:GNOME/Evolution/ShellView:1.0",
							      &ev);

	if (BONOBO_EX (&ev))
		shell_view_interface = CORBA_OBJECT_NIL;
	
	CORBA_exception_free (&ev);
	
	return shell_view_interface;
}

void
e_summary_change_current_view (ESummary *summary,
			       const char *uri)
{
	GNOME_Evolution_ShellView svi;
	CORBA_Environment ev;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	svi = retrieve_shell_view_interface (summary->priv->control);
	if (svi == CORBA_OBJECT_NIL)
		return;

	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellView_changeCurrentView (svi, uri, &ev);
	CORBA_exception_free (&ev);

	bonobo_object_release_unref (svi, NULL);
}

void
e_summary_set_message (ESummary *summary,
		       const char *message,
		       gboolean busy)
{
	GNOME_Evolution_ShellView svi;
	CORBA_Environment ev;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	svi = retrieve_shell_view_interface (summary->priv->control);
	if (svi == CORBA_OBJECT_NIL)
		return;

	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellView_setMessage (svi, message ? message : "", busy, &ev);
	CORBA_exception_free (&ev);

	bonobo_object_release_unref (svi, NULL);
}

void
e_summary_unset_message (ESummary *summary)
{
	GNOME_Evolution_ShellView svi;
	CORBA_Environment ev;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	svi = retrieve_shell_view_interface (summary->priv->control);
	if (svi == CORBA_OBJECT_NIL)
		return;

	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellView_unsetMessage (svi, &ev);
	CORBA_exception_free (&ev);

	bonobo_object_release_unref (svi, NULL);
}

void
e_summary_reconfigure (ESummary *summary)
{
	if (summary->rdf != NULL) {
		e_summary_rdf_reconfigure (summary);
	}

	if (summary->weather != NULL) {
		e_summary_weather_reconfigure (summary);
	}

	if (summary->calendar != NULL) {
		e_summary_calendar_reconfigure (summary);
	}

	if (summary->tasks != NULL) {
		e_summary_tasks_reconfigure (summary);
	}

	e_summary_draw (summary);
}

void
e_summary_reconfigure_all (void)
{
	GList *p;

	/* This is here, because it only needs to be done once for all summaries */
	e_summary_mail_reconfigure ();

	for (p = all_summaries; p; p = p->next) {
		e_summary_reconfigure (E_SUMMARY (p->data));
	}
}

static gint
e_summary_reload_timeout (gpointer closure)
{
	ESummary *summary = closure;

	if (summary->rdf != NULL)
		e_summary_rdf_update (summary);

	if (summary->weather != NULL)
		e_summary_weather_update (summary);

	if (summary->calendar != NULL)
		e_summary_calendar_reconfigure (summary);

	if (summary->tasks != NULL)
		e_summary_tasks_reconfigure (summary);

	summary->priv->pending_reload_tag = 0;

	return FALSE;
}

void
e_summary_reload (BonoboUIComponent *component,
		  gpointer userdata,
		  const char *cname)
{
	ESummary *summary = userdata;

	/*
	  This is an evil hack to work around a bug in gnome-vfs:
	  gnome-vfs seems to not properly lock partially-constructed
	  objects, so if you gnome_vfs_async_open and then immediately
	  gnome_vfs_async_cancel, it is possible to start to destroy
	  an object before it is totally constructed.  Hilarity ensures.

	  This is an evil and stupid hack, but it slows down our reload
	  requests enough the gnome-vfs should be able to keep up.  And
	  given that these are not instantaneous operations to begin 
	  with, the users should be none the wiser. -JT
	*/

	if (summary->priv->pending_reload_tag) {
		gtk_timeout_remove (summary->priv->pending_reload_tag);
	}

	summary->priv->pending_reload_tag =
		gtk_timeout_add (80, e_summary_reload_timeout, summary);
}

int 
e_summary_count_connections (ESummary *summary)
{
	GList *p;
	int count = 0;

	g_return_val_if_fail (IS_E_SUMMARY (summary), 0);

	for (p = summary->priv->connections; p; p = p->next) {
		ESummaryConnection *c;

		c = p->data;
		count += c->count (summary, c->closure);
	}

	return count;
}

GList *
e_summary_add_connections (ESummary *summary)
{
	GList *p;
	GList *connections = NULL;

	g_return_val_if_fail (IS_E_SUMMARY (summary), NULL);

	for (p = summary->priv->connections; p; p = p->next) {
		ESummaryConnection *c;
		GList *r;

		c = p->data;
		r = c->add (summary, c->closure);

		connections = g_list_concat (connections, r);
	}

	return connections;
}

void
e_summary_set_online (ESummary *summary,
		      GNOME_Evolution_OfflineProgressListener progress,
		      gboolean online,
		      ESummaryOnlineCallback callback,
		      void *closure)
{
	GList *p;

	g_return_if_fail (IS_E_SUMMARY (summary));

	for (p = summary->priv->connections; p; p = p->next) {
		ESummaryConnection *c;

		c = p->data;
		c->callback = callback;
		c->callback_closure = closure;

		c->set_online (summary, progress, online, c->closure);

		if (callback != NULL)
			callback (summary, closure);
	}
}

void
e_summary_add_online_connection (ESummary *summary,
				 ESummaryConnection *connection)
{
	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));
	g_return_if_fail (connection != NULL);

	summary->priv->connections = g_list_prepend (summary->priv->connections,
						     connection);
}

void
e_summary_remove_online_connection (ESummary *summary,
				    ESummaryConnection *connection)
{
	GList *p;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));
	g_return_if_fail (connection != NULL);

	p = g_list_find (summary->priv->connections, connection);
	g_return_if_fail (p != NULL);

	summary->priv->connections = g_list_remove_link (summary->priv->connections, p);
	g_list_free (p);
}
