/*
 * e-summary.c: ESummary object.
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
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

#include <bonobo/bonobo-listener.h>
#include <libgnome/gnome-paper.h>
#include <libgnome/gnome-url.h>

#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print-master-preview.h>

#include "e-summary.h"
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
	char *buffer;

	gboolean error;
} DownloadInfo;

struct _ESummaryPrivate {
	GNOME_Evolution_Shell shell;
	GNOME_Evolution_ShellView shell_view_interface;

	GtkWidget *html_scroller;
	GtkWidget *html;

	GHashTable *protocol_hash;
};

typedef struct _ProtocolListener {
	ESummaryProtocolListener listener;
	void *closure;
} ProtocolListener;


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

	g_free (priv);
	summary->priv = NULL;

	e_summary_parent_class->destroy (object);
}

void
e_summary_draw (ESummary *summary)
{
	GString *string;
	char *html;
	char date[256];
	time_t t;

	if (summary->mail == NULL || summary->calendar == NULL
	    || summary->rdf == NULL || summary->weather == NULL) {
		return;
	}

	string = g_string_new (HTML_1);
	t = time (NULL);
	strftime (date, 255, "%A, %d %B %Y", localtime (&t));

	html = g_strdup_printf (HTML_2, date);
	g_string_append (string, html);
	g_free (html);
	g_string_append (string, HTML_3);

	/* Weather and RDF stuff here */
	html = e_summary_weather_get_html (summary);
	g_string_append (string, html);

	html = e_summary_rdf_get_html (summary);
	g_string_append (string, html);

	g_string_append (string, HTML_4);

	html = (char *) e_summary_mail_get_html (summary);

	g_string_append (string, html);

	html = (char *) e_summary_calendar_get_html (summary);
	g_string_append (string, html);

	g_string_append (string, HTML_5);
	gtk_html_load_from_string (GTK_HTML (summary->priv->html), string->str,
				   strlen (string->str));
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
	return gnome_pixmap_file (filename);
}

static void
close_callback (GnomeVFSAsyncHandle *handle,
		GnomeVFSResult result,
		gpointer data)
{
	DownloadInfo *info = data;

	if (info->error) {
		gtk_html_stream_close (info->stream, GTK_HTML_STREAM_ERROR);
	} else {
		gtk_html_stream_close (info->stream, GTK_HTML_STREAM_OK);
	}

	g_free (info->uri);
	g_free (info->buffer);
	g_free (info);
}

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
		info->error = TRUE;
		gnome_vfs_async_close (handle, close_callback, info);
	}

	if (bytes_read == 0) {
		info->error = FALSE;
		gnome_vfs_async_close (handle, close_callback, info);
	} else {
		gtk_html_stream_write (info->stream, buffer, bytes_read);
		gnome_vfs_async_read (handle, buffer, 4095, read_callback, info);
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

	info->buffer = g_new (char, 4096);
	gnome_vfs_async_read (handle, info->buffer, 4095, read_callback, info);
}

static void
e_summary_url_clicked (GtkHTML *html,
		       const char *url,
		       ESummary *summary)
{
	char *protocol, *protocol_end;
	ProtocolListener *protocol_listener;

	g_print ("URL: %s\n", url);

	protocol_end = strchr (url, ':');
	if (protocol_end == NULL) {
		/* No url, let gnome work it out */
		gnome_url_show (url);
		return;
	}

	protocol = g_strndup (url, protocol_end - url);
	g_print ("Protocol: %s.\n", protocol);

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

	info = g_new (DownloadInfo, 1);
	info->stream = stream;
	info->uri = filename;
	info->error = FALSE;

	gnome_vfs_async_open (&handle, filename, GNOME_VFS_OPEN_READ,
			      (GnomeVFSAsyncOpenCallback) open_callback, info);
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

#define DEFAULT_HTML "<html><head><title>My Evolution</title></head><body bgcolor=\"#ffffff\">hello</body></html>" 

static void
e_summary_init (ESummary *summary)
{
	ESummaryPrivate *priv;
	GdkColor bgcolor = {0, 0xffff, 0xffff, 0xffff};
	summary->priv = g_new (ESummaryPrivate, 1);

	priv = summary->priv;

	priv->html_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->html_scroller),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	priv->html = gtk_html_new ();
	gtk_html_set_editable (GTK_HTML (priv->html), FALSE);
	gtk_html_set_default_content_type (GTK_HTML (priv->html),
					   "text/html; charset=utf-8");
	gtk_html_set_default_background_color (GTK_HTML (priv->html), &bgcolor);
	gtk_html_load_from_string (GTK_HTML (priv->html), DEFAULT_HTML,
				   strlen (DEFAULT_HTML));

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
	e_summary_rdf_init (summary);
	e_summary_weather_init (summary);

	e_summary_draw (summary);

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
		gpd = GNOME_PRINT_DIALOG (gnome_print_dialog_new (_("Print My Evolution"), GNOME_PRINT_DIALOG_COPIES));
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
				  _("Printing of My Evolution failed"));
		}
	}

	gtk_object_unref (GTK_OBJECT (print_master));
}

void
e_summary_print (GtkWidget *widget,
		 ESummary *summary)
{
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
		g_print ("Creating\n");
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
