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
#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-unicode.h>

#include <bonobo/bonobo-listener.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-ui-component.h>

#include <libgnome/gnome-url.h>

#include <libgnomeprint/gnome-print-master.h>

#include <libgnomeprintui/gnome-print-master-preview.h>
#include <libgnomeprintui/gnome-print-dialog.h>

#include <gtk/gtkdialog.h>

#include <gui/alarm-notify/alarm.h>
#include <cal-util/timeutil.h>

#include <gtk/gtkmain.h>
#include <gtk/gtkscrolledwindow.h>

#include <string.h>
#include <unistd.h>

#include "e-util/e-config-listener.h"

#include "e-summary.h"
#include "e-summary-preferences.h"
#include "my-evolution-html.h"
#include "Mailer.h"

#include <Evolution.h>

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
	GNOME_Evolution_ShellView shell_view_interface;

	GtkWidget *html_scroller;
	GtkWidget *html;

	GHashTable *protocol_hash;

	GList *connections;

	guint pending_reload_tag;

	gpointer alarm;

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

	alarm_remove (priv->alarm);
	alarm_done ();

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
	edir = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution",
				      filename);

	if (g_file_exists (edir)) {
		ret = g_strdup (edir);
		g_free (edir);

		return ret;
	}
	g_free (edir);

	/* Try the evolution button images dir */
	edir = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution/buttons",
				      filename);

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

	e_summary_parent_class = gtk_type_class (PARENT_TYPE);
}

static void
alarm_fn (gpointer alarm_id,
	  time_t trigger,
	  gpointer data)
{
	ESummary *summary;
	time_t t, day_end;

	summary = data;
	t = time (NULL);
	day_end = time_day_end_with_zone (t, summary->tz);
	summary->priv->alarm = alarm_add (day_end, alarm_fn, summary, NULL);

	e_summary_reconfigure (summary);
}
	
#define DEFAULT_HTML "<html><head><title>Summary</title></head><body bgcolor=\"#ffffff\">%s</body></html>" 

static void
e_summary_init (ESummary *summary)
{
	EConfigListener *config_listener;
	ESummaryPrivate *priv;
	GdkColor bgcolor = {0, 0xffff, 0xffff, 0xffff};
	time_t t, day_end;
	char *def, *default_utf;

	summary->priv = g_new (ESummaryPrivate, 1);

	priv = summary->priv;

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
	gtk_html_set_default_background_color (GTK_HTML (priv->html), &bgcolor);
	def = g_strdup_printf (DEFAULT_HTML, _("Please wait..."));
	default_utf = e_utf8_from_locale_string (def);
	gtk_html_load_from_string (GTK_HTML (priv->html), default_utf, strlen (default_utf));
	g_free (def);
	g_free (default_utf);

	gtk_signal_connect (GTK_OBJECT (priv->html), "url-requested",
			    GTK_SIGNAL_FUNC (e_summary_url_requested), summary);
	gtk_signal_connect (GTK_OBJECT (priv->html), "link-clicked",
			    GTK_SIGNAL_FUNC (e_summary_url_clicked), summary);

	gtk_container_add (GTK_CONTAINER (priv->html_scroller), priv->html);
	gtk_widget_show_all (priv->html_scroller);
	gtk_box_pack_start (GTK_BOX (summary), priv->html_scroller,
			    TRUE, TRUE, 0);

	priv->protocol_hash = NULL;
	priv->connections = NULL;

	config_listener = e_config_listener_new ();

	summary->timezone = e_config_listener_get_string_with_default (config_listener,
								       "/Calendar/Display/Timezone", "UTC",
								       NULL);
	if (!summary->timezone || !summary->timezone[0]) {
		g_free (summary->timezone);
		summary->timezone = g_strdup ("UTC");
	}
	summary->tz = icaltimezone_get_builtin_timezone (summary->timezone);

	g_object_unref (config_listener);

	t = time (NULL);
	if (summary->tz == NULL) {
		day_end = time_day_end (t);
	} else {
		day_end = time_day_end_with_zone (t, summary->tz);
	}

	priv->alarm = alarm_add (day_end, alarm_fn, summary, NULL);

	priv->queued_draw_idle_id = 0;
}

E_MAKE_TYPE (e_summary, "ESummary", ESummary, e_summary_class_init,
	     e_summary_init, PARENT_TYPE);

GtkWidget *
e_summary_new (const GNOME_Evolution_Shell shell,
	       ESummaryPrefs *prefs)
{
	ESummary *summary;

	summary = gtk_type_new (e_summary_get_type ());
	summary->shell = shell;
	/* Just get a pointer to the global preferences */
	summary->preferences = prefs;
	
	e_summary_add_protocol_listener (summary, "evolution", e_summary_evolution_protocol_listener, summary);

	e_summary_mail_init (summary);
	e_summary_calendar_init (summary);
	e_summary_tasks_init (summary);
	e_summary_rdf_init (summary);
	e_summary_weather_init (summary);

/*  	e_summary_draw (summary); */

	all_summaries = g_list_prepend (all_summaries, summary);
	return GTK_WIDGET (summary);
}

static void
do_summary_print (ESummary *summary,
		  gboolean preview)
{
	GnomePrintContext *print_context;
	GnomePrintMaster *print_master;
	GtkWidget *gpd;
	GnomePrintConfig *config = NULL;

	if (! preview) {
		gpd = gnome_print_dialog_new (_("Print Summary"), GNOME_PRINT_DIALOG_COPIES);

		switch (gtk_dialog_run (GTK_DIALOG (gpd))) {
		case GNOME_PRINT_DIALOG_RESPONSE_PRINT:
			break;

		case GNOME_PRINT_DIALOG_RESPONSE_PREVIEW:
			preview = TRUE;
			break;

		default:
			gtk_widget_destroy (gpd);
			return;
		}

		config = gnome_print_dialog_get_config (GNOME_PRINT_DIALOG (gpd));
	}

	print_master = gnome_print_master_new_from_config (config);
	
	print_context = gnome_print_master_get_context (print_master);
	gtk_html_print (GTK_HTML (summary->priv->html), print_context);
	gnome_print_master_close (print_master);

	if (preview) {
		GtkWidget *preview;

		preview = gnome_print_master_preview_new (print_master, _("Print Preview"));
		gtk_widget_show (preview);
	} else {
		int result = gnome_print_master_print (print_master);

		if (result == -1) {
			e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
				  _("Printing of Summary failed"));
		}
	}

	gtk_object_unref (GTK_OBJECT (print_master));
}

void
e_summary_print (BonoboUIComponent *component,
		 gpointer userdata,
		 const char *cname)
{
	ESummary *summary = userdata;

	do_summary_print (summary, FALSE);
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

void
e_summary_change_current_view (ESummary *summary,
			       const char *uri)
{
	GNOME_Evolution_ShellView svi;
	CORBA_Environment ev;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	svi = summary->shell_view_interface;
	if (svi == NULL) {
		return;
	}

	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellView_changeCurrentView (svi, uri, &ev);
	CORBA_exception_free (&ev);
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

	svi = summary->shell_view_interface;
	if (svi == NULL) {
		return;
	}

	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellView_setMessage (svi, message ? message : "", busy, &ev);
	CORBA_exception_free (&ev);
}

void
e_summary_unset_message (ESummary *summary)
{
	GNOME_Evolution_ShellView svi;
	CORBA_Environment ev;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	svi = summary->shell_view_interface;
	if (svi == NULL) {
		return;
	}

	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellView_unsetMessage (svi, &ev);
	CORBA_exception_free (&ev);
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
