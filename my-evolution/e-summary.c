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

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>

#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-stream.h>
#include <gtkhtml/htmlengine.h>
#include <gtkhtml/htmlselection.h>

#include <libgnomevfs/gnome-vfs.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-unicode.h>

#include <bonobo/bonobo-listener.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo-conf/bonobo-config-database.h>

#include <libgnome/gnome-paper.h>
#include <libgnome/gnome-url.h>

#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print-master-preview.h>

#include <gui/alarm-notify/alarm.h>
#include <cal-util/timeutil.h>

#include "e-summary.h"
#include "e-summary-preferences.h"
#include "my-evolution-html.h"
#include "Mail.h"

#include <Evolution.h>

#include <time.h>

#define PARENT_TYPE (gtk_vbox_get_type ())

extern char *evolution_dir;

static GtkObjectClass *e_summary_parent_class;

struct _ESummaryMailFolderInfo {
	char *name;

	int count;
	int unread;
};

typedef struct _DownloadInfo {
	GtkHTMLStream *stream;
	char *uri;
	char *buffer, *ptr;
	guint32 bufsize;
} DownloadInfo;

struct _ESummaryPrivate {
	GNOME_Evolution_Shell shell;
	GNOME_Evolution_ShellView shell_view_interface;

	GtkWidget *html_scroller;
	GtkWidget *html;

	GHashTable *protocol_hash;

	GList *connections;

	guint pending_reload_tag;

	gpointer alarm;

	gboolean frozen;
	gboolean redraw_pending;
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

	if (priv->pending_reload_tag) {
		gtk_timeout_remove (priv->pending_reload_tag);
		priv->pending_reload_tag = 0;
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

void
e_summary_draw (ESummary *summary)
{
	GString *string;
	GtkHTMLStream *stream;
	char *html;
	char date[256], *date_utf;
	time_t t;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	if (summary->mail == NULL || summary->calendar == NULL
	    || summary->rdf == NULL || summary->weather == NULL 
	    || summary->tasks == NULL) {
		return;
	}

	if (summary->priv->frozen == TRUE) {
		summary->priv->redraw_pending = TRUE;
		return;
	}

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
close_callback (GnomeVFSAsyncHandle *handle,
		GnomeVFSResult result,
		gpointer data)
{
	DownloadInfo *info = data;
	struct _imgcache *img;

	if (images_cache == NULL) {
		images_cache = g_hash_table_new (g_str_hash, g_str_equal);
	}

	img = g_new (struct _imgcache, 1);
	img->buffer = info->buffer;
	img->bufsize = info->bufsize;

	g_hash_table_insert (images_cache, info->uri, img);
	g_free (info);
}

/* The way this behaves is a workaround for ximian bug 10235: loading
 * the image into gtkhtml progressively will result in garbage being
 * drawn, so we wait until we've read the whole thing and then write
 * it all at once.
 */
static void
read_callback (GnomeVFSAsyncHandle *handle,
	       GnomeVFSResult result,
	       gpointer buffer,
	       GnomeVFSFileSize bytes_requested,
	       GnomeVFSFileSize bytes_read,
	       gpointer data)
{
	DownloadInfo *info = data;

	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		gtk_html_stream_close (info->stream, GTK_HTML_STREAM_ERROR);
		gnome_vfs_async_close (handle, close_callback, info);
	} else if (bytes_read == 0) {
		gtk_html_stream_write (info->stream, info->buffer, info->bufsize);
		gtk_html_stream_close (info->stream, GTK_HTML_STREAM_OK);
		gnome_vfs_async_close (handle, close_callback, info);
	} else {
		bytes_read += info->ptr - info->buffer;
		info->bufsize += 4096;
		info->buffer = g_realloc (info->buffer, info->bufsize);
		info->ptr = info->buffer + bytes_read;
		gnome_vfs_async_read (handle, info->ptr, 4095, read_callback, info);
	}
}

static void
open_callback (GnomeVFSAsyncHandle *handle,
	       GnomeVFSResult result,
	       DownloadInfo *info)
{
	if (result != GNOME_VFS_OK) {
		gtk_html_stream_close (info->stream, GTK_HTML_STREAM_ERROR);
		g_free (info->uri);
		g_free (info);
		return;
	}

	info->bufsize = 4096;
	info->buffer = info->ptr = g_new (char, info->bufsize);
	gnome_vfs_async_read (handle, info->buffer, 4095, read_callback, info);
}

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
		gnome_url_show (url);
		return;
	}

	protocol = g_strndup (url, protocol_end - url);

	protocol_listener = g_hash_table_lookup (summary->priv->protocol_hash,
						 protocol);
	g_free (protocol);

	if (protocol_listener == NULL) {
		/* Again, let gnome work it out */
		gnome_url_show (url);
		return;
	}

	protocol_listener->listener (summary, url, protocol_listener->closure);
}

static void
e_summary_url_requested (GtkHTML *html,
			 const char *url,
			 GtkHTMLStream *stream,
			 ESummary *summary)
{
	char *filename;
	GnomeVFSAsyncHandle *handle;
	DownloadInfo *info;
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
	}

	if (img == NULL) {
		info = g_new (DownloadInfo, 1);
		info->stream = stream;
		info->uri = filename;

		gnome_vfs_async_open (&handle, filename, GNOME_VFS_OPEN_READ,
				      (GnomeVFSAsyncOpenCallback) open_callback, info);
	} else {
		gtk_html_stream_write (stream, img->buffer, img->bufsize);
		gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);
	}
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
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	ESummaryPrivate *priv;
	GdkColor bgcolor = {0, 0xffff, 0xffff, 0xffff};
	time_t t, day_end;
	char *def, *default_utf;

	summary->priv = g_new (ESummaryPrivate, 1);

	priv = summary->priv;

	priv->frozen = FALSE;
	priv->redraw_pending = FALSE;
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
#if 0
	gtk_signal_connect (GTK_OBJECT (priv->html), "on-url",
			    GTK_SIGNAL_FUNC (e_summary_on_url), summary);
#endif

	gtk_container_add (GTK_CONTAINER (priv->html_scroller), priv->html);
	gtk_widget_show_all (priv->html_scroller);
	gtk_box_pack_start (GTK_BOX (summary), priv->html_scroller,
			    TRUE, TRUE, 0);

	priv->protocol_hash = NULL;
	priv->connections = NULL;

	summary->prefs_window = NULL;
	e_summary_preferences_init (summary);

	CORBA_exception_init (&ev);
	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&ev);
		g_warning ("Error getting Wombat. Using defaults");
		return;
	}

	summary->timezone = bonobo_config_get_string_with_default (db,
		"/Calendar/Display/Timezone", "UTC", NULL);
	if (!summary->timezone || !summary->timezone[0]) {
		g_free (summary->timezone);
		summary->timezone = g_strdup ("UTC");
	}
	summary->tz = icaltimezone_get_builtin_timezone (summary->timezone);

	bonobo_object_release_unref (db, NULL);
	CORBA_exception_free (&ev);

	t = time (NULL);
	if (summary->tz == NULL) {
		day_end = time_day_end (t);
	} else {
		day_end = time_day_end_with_zone (t, summary->tz);
	}

	priv->alarm = alarm_add (day_end, alarm_fn, summary, NULL);
}

E_MAKE_TYPE (e_summary, "ESummary", ESummary, e_summary_class_init,
	     e_summary_init, PARENT_TYPE);

GtkWidget *
e_summary_new (const GNOME_Evolution_Shell shell)
{
	ESummary *summary;

	summary = gtk_type_new (e_summary_get_type ());
	summary->priv->shell = shell;

	e_summary_add_protocol_listener (summary, "evolution", e_summary_evolution_protocol_listener, summary);

	e_summary_mail_init (summary, shell);
	e_summary_calendar_init (summary);
	e_summary_tasks_init (summary);
	e_summary_rdf_init (summary);
	e_summary_weather_init (summary);

/*  	e_summary_draw (summary); */

	return GTK_WIDGET (summary);
}

static void
do_summary_print (ESummary *summary,
		  gboolean preview)
{
	GnomePrintContext *print_context;
	GnomePrintMaster *print_master;
	GnomePrintDialog *gpd;
	GnomePrinter *printer = NULL;
	int copies = 1;
	int collate = FALSE;

	if (!preview) {
		gpd = GNOME_PRINT_DIALOG (gnome_print_dialog_new (_("Print Summary"), GNOME_PRINT_DIALOG_COPIES));
		gnome_dialog_set_default (GNOME_DIALOG (gpd), GNOME_PRINT_PRINT);

		switch (gnome_dialog_run (GNOME_DIALOG (gpd))) {
		case GNOME_PRINT_PRINT:
			break;

		case GNOME_PRINT_PREVIEW:
			preview = TRUE;
			break;

		case -1:
			return;

		default:
			gnome_dialog_close (GNOME_DIALOG (gpd));
			return;
		}

		gnome_print_dialog_get_copies (gpd, &copies, &collate);
		printer = gnome_print_dialog_get_printer (gpd);
		gnome_dialog_close (GNOME_DIALOG (gpd));
	}

	print_master = gnome_print_master_new ();
	
	if (printer) {
		gnome_print_master_set_printer (print_master, printer);
	}
	gnome_print_master_set_copies (print_master, copies, collate);
	print_context = gnome_print_master_get_context (print_master);
	gtk_html_print (GTK_HTML (summary->priv->html), print_context);
	gnome_print_master_close (print_master);

	if (preview) {
		gboolean landscape = FALSE;
		GnomePrintMasterPreview *preview;

		preview = gnome_print_master_preview_new_with_orientation (
			print_master, _("Print Preview"), landscape);
		gtk_widget_show (GTK_WIDGET (preview));
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
	if (summary->mail != NULL) {
		e_summary_mail_reconfigure (summary);
	}

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
}

static gint
e_summary_reload_timeout (gpointer closure)
{
	ESummary *summary = closure;

	if (summary->rdf != NULL) {
		e_summary_rdf_update (summary);
	}

	if (summary->weather != NULL) {
		e_summary_weather_update (summary);
	}

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

	g_print ("Count: %d", count);
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
		g_print ("Setting %s\n", online ? "online" : "offline");

		if (callback != NULL) {
			callback (summary, closure);
		}
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

void
e_summary_freeze (ESummary *summary)
{
	g_return_if_fail (IS_E_SUMMARY (summary));
	g_return_if_fail (summary->priv != NULL);

	if (summary->priv->frozen == TRUE) {
		return;
	}

	summary->priv->frozen = TRUE;
}

void
e_summary_thaw (ESummary *summary)
{
	g_return_if_fail (IS_E_SUMMARY (summary));
	g_return_if_fail (summary->priv != NULL);

	if (summary->priv->frozen == FALSE) {
		return;
	}

	summary->priv->frozen = FALSE;
	if (summary->priv->redraw_pending) {
		e_summary_draw (summary);
	}
}
