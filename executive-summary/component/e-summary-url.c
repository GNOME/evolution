/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-url.c
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

#include <liboaf/liboaf.h>

#include <libgnomevfs/gnome-vfs.h>
#include "e-summary.h"
#include "e-summary-url.h"
#include "e-summary-util.h"

#include "Composer.h"

typedef enum _ESummaryProtocol {
	PROTOCOL_NONE,
	PROTOCOL_HTTP,
	PROTOCOL_MAILTO,
	PROTOCOL_VIEW,
	PROTOCOL_EXEC,
	PROTOCOL_FILE,
	PROTOCOL_CLOSE,
	PROTOCOL_LEFT,
	PROTOCOL_RIGHT,
	PROTOCOL_UP,
	PROTOCOL_DOWN,
	PROTOCOL_CONFIGURE,
	PROTOCOL_OTHER
} ESummaryProtocol;

static char *descriptions[] = {
	N_("Open %s with the default GNOME application"),
	N_("Open %s with the default GNOME web browser"),
	N_("Send an email to %s"),
	N_("Change the view to %s"),
	N_("Run %s"),
	N_("Open %s with the default GNOME application"),
	N_("Close %s"),
	N_("Move %s to the left"),
	N_("Move %s to the right"),
	N_("Move %s into the previous row"),
	N_("Move %s into the next row"),
	N_("Configure %s"),
	N_("Open %s with the default GNOME application")
};

typedef struct _PropertyDialog {
	BonoboListener *listener;
	int listener_id;

	Bonobo_EventSource eventsource;
	GtkWidget *dialog;
} PropertyDialog;
#define COMPOSER_IID "OAFIID:GNOME_Evolution_Mail_Composer"

#if HAVECACHE
static ESummaryCache *image_cache = NULL;
#endif

gboolean e_summary_url_mail_compose (ESummary *esummary,
				     const char *url);
gboolean e_summary_url_exec (const char *exec);

struct _DownloadInfo {
	GtkHTMLStream *stream;
	char *uri;
	char *buffer;

	gboolean error;
};
typedef struct _DownloadInfo DownloadInfo;

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
		g_warning ("Read error");
		info->error = TRUE;
		gnome_vfs_async_close (handle, close_callback, info);
	}
	
	if (bytes_read == 0) {
		info->error = FALSE;
		gnome_vfs_async_close (handle, close_callback, info);
	} else {
		gtk_html_stream_write (info->stream, buffer, bytes_read);
		gnome_vfs_async_read (handle, buffer, 4095, read_callback,
				      info);
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

void
e_summary_url_request (GtkHTML *html,
		       const gchar *url,
		       GtkHTMLStream *stream)
{
	char *filename;
	GnomeVFSAsyncHandle *handle;
	DownloadInfo *info;

	g_print ("url: %s\n", url);
	if (strncasecmp (url, "file:", 5) == 0) {
		url += 5;
		filename = e_pixmap_file (url);
	} else if (strchr (url, ':') >= strchr (url, '/')) {
		filename = e_pixmap_file (url);
	} else
		filename = g_strdup (url);

	if (filename == NULL) {
		gtk_html_stream_close (stream, GTK_HTML_STREAM_ERROR);
		return;
	}

	g_print ("Filename: %s\n", filename);

	info = g_new (DownloadInfo, 1);
	info->stream = stream;
	info->uri = filename;
	info->error = FALSE;

	gnome_vfs_async_open (&handle, filename, GNOME_VFS_OPEN_READ,
			      (GnomeVFSAsyncOpenCallback) open_callback, info);
}

static char *
parse_uri (const char *uri,
	   ESummaryProtocol protocol,
	   ESummary *esummary)
{
	char *parsed;
	char *p;
	int address;
	ESummaryWindow *window;

	switch (protocol) {
		
	case PROTOCOL_HTTP:
		/* "http://" == 7 */
		parsed = g_strdup (uri + 7);
		break;

	case PROTOCOL_EXEC:
		/* "exec://" == 7 */
		parsed = g_strdup (uri + 7);
		break;

	case PROTOCOL_VIEW:
		/* "view://" == 7 */
		parsed = g_strdup (uri + 7);
		break;

	case PROTOCOL_MAILTO:
		/* Fun. Mailto's might be "mailto:" or "mailto://" */
		if (strstr (uri, "mailto://") == NULL) {
			parsed = (char *) (uri + 7);
		} else {
			parsed = (char *) (uri + 9);
		}

		/* Now strip anything after a question mark, 
		   as it is a parameter (that we ignore for the time being) */
		if ( (p = strchr (parsed, '?')) != NULL) {
			parsed = g_strndup (parsed, p - parsed);
		} else {
			parsed = g_strdup (parsed);
		}

		break;

	case PROTOCOL_CLOSE:
		address = atoi (uri + 8);
		window = (ESummaryWindow *) GINT_TO_POINTER (address);
		parsed = g_strdup (window->title);
		break;

	case PROTOCOL_LEFT:
		address = atoi (uri + 7);
		window = (ESummaryWindow *) GINT_TO_POINTER (address);
		parsed = g_strdup (window->title);
		break;

	case PROTOCOL_RIGHT:
		address = atoi (uri + 8);
		window = (ESummaryWindow *) GINT_TO_POINTER (address);
		parsed = g_strdup (window->title);
		break;

	case PROTOCOL_UP:
		address = atoi (uri + 5);
		window = (ESummaryWindow *) GINT_TO_POINTER (address);
		parsed = g_strdup (window->title);
		break;

	case PROTOCOL_DOWN:
		address = atoi (uri + 7);
		window = (ESummaryWindow *) GINT_TO_POINTER (address);
		parsed = g_strdup (window->title);
		break;

	case PROTOCOL_CONFIGURE:
		address = atoi (uri + 12);
		window = (ESummaryWindow *) GINT_TO_POINTER (address);
		parsed = g_strdup (window->title);
		break;

	case PROTOCOL_NONE:
	case PROTOCOL_OTHER:
	default:
		/* Just return the uneditted uri. */
		parsed = g_strdup (uri);
		break;
	}

	return parsed;
}

static ESummaryProtocol
get_protocol (const char *url)
{
	char *lowerurl;
	ESummaryProtocol protocol = PROTOCOL_OTHER;
	
	lowerurl = g_strdup (url);
	g_strdown (lowerurl);
	
	/* Check for no :/ */
	if (strstr (lowerurl, "://") == NULL) {
		
		/* Annoying alternative for mailto URLs */
		if (strncmp (lowerurl, "mailto:", 6) != 0) {
			g_free (lowerurl);
			return PROTOCOL_NONE;
		} else {
			g_free (lowerurl);
			return PROTOCOL_MAILTO;
		}
	}
	
	switch (lowerurl[0]) {
	case 'c':
		switch (lowerurl[1]) {
		case 'l':
			if (strncmp (lowerurl + 2, "ose", 3) == 0)
				protocol = PROTOCOL_CLOSE;
			break;
		case 'o':
			if (strncmp (lowerurl + 2, "nfigure", 7) == 0)
				protocol = PROTOCOL_CONFIGURE;
			break;
		}

	case 'd':
		if (strncmp (lowerurl + 1, "own", 3) == 0)
			protocol = PROTOCOL_DOWN;
		break;

	case 'e':
		if (strncmp (lowerurl + 1, "xec", 3) == 0)
			protocol = PROTOCOL_EXEC;
		break;
		
	case 'f':
		if (strncmp (lowerurl + 1, "ile", 3) == 0)
			protocol = PROTOCOL_FILE;
		break;

	case 'h':
		if (strncmp (lowerurl + 1, "ttp", 3) == 0)
			protocol = PROTOCOL_HTTP;
		break;
		
	case 'l':
		if (strncmp (lowerurl + 1, "eft", 3) == 0)
			protocol = PROTOCOL_LEFT;
		break;

	case 'm':
		if (strncmp (lowerurl + 1, "ailto", 5) == 0)
			protocol = PROTOCOL_MAILTO;
		break;
		
	case 'r':
		if (strncmp (lowerurl + 1, "ight", 4) == 0)
			protocol = PROTOCOL_RIGHT;
		break;

	case 'u':
		if (lowerurl[1] == 'p')
			protocol = PROTOCOL_UP;
		break;
		
	case 'v':
		if (strncmp (lowerurl + 1, "iew", 3) == 0)
			protocol = PROTOCOL_VIEW;
		break;
		
	default:
		break;
	}
	
	g_free (lowerurl);
	
	return protocol;
}

static void
property_apply (GnomePropertyBox *propertybox,
		gint page_num,
		Bonobo_PropertyControl control)
{
	CORBA_Environment ev;

	g_print ("page_num: %d\n", page_num);

	CORBA_exception_init (&ev);
	Bonobo_PropertyControl_notifyAction (control, page_num, Bonobo_PropertyControl_APPLY, &ev);
	CORBA_exception_free (&ev);
}

static void
property_help (GnomePropertyBox *propertybox,
	       gint page_num,
	       Bonobo_PropertyControl control)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	Bonobo_PropertyControl_notifyAction (control, page_num, Bonobo_PropertyControl_HELP, &ev);
	CORBA_exception_free (&ev);
}

static void
property_event (BonoboListener *listener,
		char *event_name,
		CORBA_any *any,
		CORBA_Environment *ev,
		gpointer user_data)
{
	PropertyDialog *data = (PropertyDialog *) user_data;
	if (strcmp (event_name, BONOBO_PROPERTY_CONTROL_CHANGED) == 0) {
		gnome_property_box_changed (GNOME_PROPERTY_BOX (data->dialog));
		return;
	}
}

static void
dialog_destroyed (GtkObject *object,
		  PropertyDialog *dialog)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	Bonobo_EventSource_removeListener (dialog->eventsource,
					   dialog->listener_id, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Error: %s", CORBA_exception_id (&ev));
	}

	bonobo_object_unref (BONOBO_OBJECT (dialog->listener));
	CORBA_exception_free (&ev);
	g_free (dialog);
}

void
e_summary_url_click (GtkWidget *widget,
		     const char *url,
		     ESummary *esummary)
{
	ESummaryProtocol protocol;
	char *parsed;
	int address;
	ESummaryWindow *window;
	Bonobo_Control control;
	Bonobo_Listener corba_listener;
	GtkWidget *prefsbox, *control_widget;
	CORBA_Environment ev;
	PropertyDialog *data;
	int num_pages, i;

	protocol = get_protocol (url);

	parsed = parse_uri (url, protocol, esummary);

	switch (protocol) {
	case PROTOCOL_MAILTO:
		/* Open a composer window */
		e_summary_url_mail_compose (esummary, parsed);
		break;
		
	case PROTOCOL_VIEW:
		/* Change the EShellView's current uri */
		e_summary_change_current_view (esummary, parsed);
		break;
		
	case PROTOCOL_EXEC:
		/* Execute the rest of the url */
		e_summary_url_exec (parsed);
		break;

	case PROTOCOL_CLOSE:
		/* Close the window. */
		address = atoi (url + 8);
		window = (ESummaryWindow *) GINT_TO_POINTER (address);
		if (window->iid == NULL)
			break;

		e_summary_remove_window (esummary, window);
		e_summary_queue_rebuild (esummary);
		break;

	case PROTOCOL_CONFIGURE:
		/* Configure the window. . . */
		address = atoi (url + 12);
		window = (ESummaryWindow *) GINT_TO_POINTER (address);
		if (window->iid == NULL)
			break;
	
		data = g_new (PropertyDialog, 1);
		/* Create the property box */
		prefsbox = gnome_property_box_new ();
		data->dialog = prefsbox;

		CORBA_exception_init (&ev);
		data->eventsource = window->event_source;
		data->listener = bonobo_listener_new (property_event, data);
		corba_listener = bonobo_object_corba_objref (BONOBO_OBJECT (data->listener));
		data->listener_id = Bonobo_EventSource_addListener (data->eventsource,
								    corba_listener, &ev);

		gtk_signal_connect (GTK_OBJECT (prefsbox), "apply",
				    GTK_SIGNAL_FUNC (property_apply), 
				    window->propertycontrol);
		gtk_signal_connect (GTK_OBJECT (prefsbox), "help",
				    GTK_SIGNAL_FUNC (property_help),
				    window->propertycontrol);
		gtk_signal_connect (GTK_OBJECT (prefsbox), "destroy",
				    GTK_SIGNAL_FUNC (dialog_destroyed), data);

		num_pages = Bonobo_PropertyControl__get_pageCount (window->propertycontrol, &ev);
		for (i = 0; i < num_pages; i++) {
			control = Bonobo_PropertyControl_getControl (window->propertycontrol, i, &ev);
			if (ev._major != CORBA_NO_EXCEPTION) {
				g_warning ("Unable to get property control.");
				CORBA_exception_free (&ev);
				break;
			}
			control_widget = bonobo_widget_new_control_from_objref (control,
										CORBA_OBJECT_NIL);
			gnome_property_box_append_page (GNOME_PROPERTY_BOX (prefsbox),
							control_widget,
							gtk_label_new ("page"));
		}

		gtk_widget_show_all (prefsbox);

		break;

	case PROTOCOL_LEFT:
		/* Window left */
		address = atoi (url + 7);
		window = (ESummaryWindow *) GINT_TO_POINTER (address);
		if (window->iid == NULL)
			break;

		e_summary_window_move_left (esummary, window);
		e_summary_queue_rebuild (esummary);
		break;

	case PROTOCOL_RIGHT:
		address = atoi (url + 8);
		window = (ESummaryWindow *) GINT_TO_POINTER (address);
		if (window->iid == NULL)
			break;

		e_summary_window_move_right (esummary, window);
		e_summary_queue_rebuild (esummary);
		break;

	case PROTOCOL_UP:
		address = atoi (url + 5);
		window = (ESummaryWindow *) GINT_TO_POINTER (address);
		if (window->iid == NULL)
			break;

		e_summary_window_move_up (esummary, window);
		e_summary_queue_rebuild (esummary);
		break;

	case PROTOCOL_DOWN:
		address = atoi (url + 7);
		window = (ESummaryWindow *) GINT_TO_POINTER (address);
		if (window->iid == NULL)
			break;

		e_summary_window_move_down (esummary, window);
		e_summary_queue_rebuild (esummary);
		break;

	case PROTOCOL_NONE:
	case PROTOCOL_OTHER:
	case PROTOCOL_HTTP:
	case PROTOCOL_FILE:
	default:
		/* Let browser handle it */
		gnome_url_show (url);
		break;
		
	}      

	g_free (parsed);
}

static void
parse_mail_url (char *url,
		GList **cc,
		GList **bcc,
		char **subject)
{
	char **options;
	int i = 0;

	options = g_strsplit (url, "&", 0);
	while (options[i] != NULL) {
		char **params;

		params = g_strsplit (options[i], "=", 2);
		if (strcmp (params[0], "subject") == 0) {
			*subject = g_strdup (params[1]);
		} else if (strcmp (params[0], "cc") == 0) {
			*cc = g_list_prepend (*cc, g_strdup (params[1]));
		} else if (strcmp (params[1], "bcc") == 0) {
			*bcc = g_list_prepend (*bcc, g_strdup (params[1]));
		}

		g_strfreev (params);
		i++;
	}
	
	g_strfreev (options);
	/* Reverse the list so it's in the correct order */
	*cc = g_list_reverse (*cc);
	*bcc = g_list_reverse (*bcc);
}

static void
recipients_from_list (GNOME_Evolution_Composer_RecipientList *recipients,
		      GList *list)
{
	GList *t;
	int i;

	for (i = 0, t = list; t; i++, t = t->next) {
		GNOME_Evolution_Composer_Recipient *recipient;
		char *address = (char *)t->data;

		recipient = recipients->_buffer + i;
		recipient->name = CORBA_string_dup ("");
		recipient->address = CORBA_string_dup (address ? address : "");
	}
}

static void
free_list (GList *list)
{
	for (; list; list = list->next) {
		g_free (list->data);
	}
}

gboolean
e_summary_url_mail_compose (ESummary *esummary,
			    const char *url)
{
	CORBA_Object composer;
	CORBA_Environment ev;
	char *full_address, *address, *proto, *q;
	GNOME_Evolution_Composer_RecipientList *to, *cc, *bcc;
	GNOME_Evolution_Composer_Recipient *recipient;
	CORBA_char *subject;
	GList *gcc = NULL, *gbcc = NULL;
	char *gsubject = NULL;
	
	CORBA_exception_init (&ev);
	
	/* FIXME: Query for IIDs? */
	composer = oaf_activate_from_id ((char *)COMPOSER_IID, 0, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		g_warning ("Unable to start composer component!");
		return FALSE;
	}

	if ( (proto = strstr (url, "://")) != NULL){
		full_address = proto + 3;
	} else {
		if (strncmp (url, "mailto:", 7) == 0)
			full_address = (char *) (url + 7);
		else
			full_address = (char *) url;
	}

	q = strchr (full_address, '?');
	if (q != NULL) {
		address = g_strndup (full_address, q - full_address);
		parse_mail_url (q + 1, &gcc, &gbcc, &gsubject);
	} else {
		address = g_strdup (full_address);
	}

	to = GNOME_Evolution_Composer_RecipientList__alloc ();
	to->_length = 1;
	to->_maximum = 1;
	to->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (to->_maximum);
	
	recipient = to->_buffer;
	recipient->name = CORBA_string_dup ("");
	recipient->address = CORBA_string_dup (address?address:"");
	g_free (address);

	/* FIXME: Get these out of the URL */
	cc = GNOME_Evolution_Composer_RecipientList__alloc ();
	cc->_length = g_list_length (gcc);
	cc->_maximum = cc->_length;
	cc->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (cc->_maximum);
	
	recipients_from_list (cc, gcc);
	free_list (gcc);
	g_list_free (gcc);

	bcc = GNOME_Evolution_Composer_RecipientList__alloc ();
	bcc->_length = g_list_length (gbcc);
	bcc->_maximum = bcc->_length;
	bcc->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (bcc->_maximum);
	
	recipients_from_list (bcc, gbcc);
	free_list (gbcc);
	g_list_free (gbcc);

	subject = CORBA_string_dup (gsubject ? gsubject : "");
	g_free (gsubject);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Composer_setHeaders (composer, to, cc, bcc, subject, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		CORBA_free (to);
		CORBA_free (cc);
		CORBA_free (bcc);
		CORBA_free (subject);

		g_warning ("%s(%d): Error setting headers", __FUNCTION__, __LINE__);
		return FALSE;
	}
	
	CORBA_free (to);
	CORBA_free (cc);
	CORBA_free (bcc);
	CORBA_free (subject);

	CORBA_exception_init (&ev);  
	GNOME_Evolution_Composer_show (composer, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		g_warning ("%s(%d): Error showing composer", __FUNCTION__, __LINE__);
		return FALSE;
	}
	
	CORBA_exception_free (&ev);
	
	/* FIXME: Free the composer? */
	
	return TRUE;
}

gboolean
e_summary_url_exec (const char *exec)
{
	gchar **exec_array;
	int argc;
	
	exec_array = g_strsplit (exec, " ", 0);
	
	argc = 0;
	while (exec_array[argc] != NULL) {
		argc++;
	}
	
	gnome_execute_async (NULL, argc, exec_array);
	
	g_strfreev (exec_array);
	return TRUE;
}

static char *
e_summary_url_describe (const char *uri, 
			ESummary *esummary)
{
	ESummaryProtocol protocol;
	char *contents, *description;

	protocol = get_protocol (uri);
	contents = parse_uri (uri, protocol, esummary);

	description = g_strdup_printf (_(descriptions[protocol]), contents);
	g_free (contents);

	return description;
}

void
e_summary_url_over (GtkHTML *html,
		    const char *uri,
		    ESummary *esummary)
{
	char *description;

	if (uri != NULL) {
		description = e_summary_url_describe (uri, esummary);
		e_summary_set_message (esummary, description, FALSE);
		g_free (description);
	} else {
		e_summary_unset_message (esummary);
	}
}

/* Cache stuff */
#if HAVECACHE
void
e_summary_url_init_cache (void)
{
	if (image_cache != NULL)
		return;

	image_cache = e_summary_cache_new ();
}

void
e_summary_url_cache_destroy (void)
{
	if (image_cache == NULL)
		return;

	gtk_object_unref (GTK_OBJECT (image_cache));

	image_cache = NULL;
}
#endif
