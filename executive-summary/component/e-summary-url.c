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
	PROTOCOL_OTHER
} ESummaryProtocol;

#define COMPOSER_IID "OAFIID:evolution-composer:evolution-mail:cd8618ea-53e1-4b9e-88cf-ec578bdb903b"

void
e_summary_url_request (GtkHTML *html,
		       const gchar *url,
		       GtkHTMLStream *stream)
{
	char *filename;
	GnomeVFSHandle *handle = NULL;
	GnomeVFSResult result;

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
	
	result = gnome_vfs_open (&handle, filename, GNOME_VFS_OPEN_READ);
	
	if (result != GNOME_VFS_OK) {
		g_warning ("%s: %s", filename, 
			   gnome_vfs_result_to_string (result));
		g_free (filename);
		gtk_html_stream_close (stream, GTK_HTML_STREAM_ERROR);
		return;
	}

	g_free (filename);
	while (1) {
		char buffer[4096];
		GnomeVFSFileSize size;

		/* Clear buffer */
		memset (buffer, 0x00, 4096);

		result = gnome_vfs_read (handle, buffer, 4096, &size);
		if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
			g_warning ("Error reading data: %s", 
				   gnome_vfs_result_to_string (result));
			gnome_vfs_close (handle);
			gtk_html_stream_close (stream, GTK_HTML_STREAM_ERROR);
		}

		if (size == 0)
			break; /* EOF */

		gtk_html_stream_write (stream, buffer, size);
	}
	
	gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);
	gnome_vfs_close (handle);
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
	case 'h':
		if (strncmp (lowerurl + 1, "ttp", 3) == 0)
			protocol = PROTOCOL_HTTP;
		break;
		
	case 'm':
		if (strncmp (lowerurl + 1, "ailto", 5) == 0)
			protocol = PROTOCOL_MAILTO;
		break;
		
	case 'v':
		if (strncmp (lowerurl + 1, "iew", 3) == 0)
			protocol = PROTOCOL_VIEW;
		break;
		
	case 'e':
		if (strncmp (lowerurl + 1, "xec", 3) == 0)
			protocol = PROTOCOL_EXEC;
		break;
		
	case 'f':
		if (strncmp (lowerurl + 1, "ile", 3) == 0)
			protocol = PROTOCOL_FILE;
		break;
		
	default:
		break;
	}
	
	g_free (lowerurl);
	
	return protocol;
}

void
e_summary_url_click (GtkWidget *widget,
		     const char *url,
		     ESummary *esummary)
{
	ESummaryProtocol protocol;
	g_print ("URL: %s\n", url);
	
	protocol = get_protocol (url);
	
	switch (protocol) {
	case PROTOCOL_MAILTO:
		/* Open a composer window */
		e_summary_url_mail_compose (esummary, url); 
		break;
		
	case PROTOCOL_VIEW:
		/* Change the EShellView's current uri */
		break;
		
	case PROTOCOL_EXEC:
		/* Execute the rest of the url */
		e_summary_url_exec (url + 7);
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
}

gboolean
e_summary_url_mail_compose (ESummary *esummary,
			    const char *url)
{
	CORBA_Object composer;
	CORBA_Environment ev;
	Evolution_Composer_RecipientList *to, *cc, *bcc;
	Evolution_Composer_Recipient *recipient;
	char *address, *proto;
	CORBA_char *subject;
	
	CORBA_exception_init (&ev);
	
	/* FIXME: Query for IIDs? */
	composer = oaf_activate_from_id ((char *)COMPOSER_IID, 0, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		g_warning ("Unable to start composer component!");
		return FALSE;
	}
	
	if ( (proto = strstr (url, "://")) != NULL){
		address = proto + 3;
	} else {
		address = url + 7;
	}
	
	to = Evolution_Composer_RecipientList__alloc ();
	to->_length = 1;
	to->_maximum = 1;
	to->_buffer = CORBA_sequence_Evolution_Composer_Recipient_allocbuf (to->_maximum);
	
	recipient = to->_buffer;
	recipient->name = CORBA_string_dup ("");
	recipient->address = CORBA_string_dup (address?address:"");
	
	/* FIXME: Get these out of the URL */
	cc = Evolution_Composer_RecipientList__alloc ();
	cc->_length = 0;
	cc->_maximum = 0;
	cc->_buffer = CORBA_sequence_Evolution_Composer_Recipient_allocbuf (cc->_maximum);
	
	bcc = Evolution_Composer_RecipientList__alloc ();
	bcc->_length = 0;
	bcc->_maximum = 0;
	bcc->_buffer = CORBA_sequence_Evolution_Composer_Recipient_allocbuf (bcc->_maximum);
	
	subject = CORBA_string_dup ("");
	
	CORBA_exception_init (&ev);
	Evolution_Composer_set_headers (composer, to, cc, bcc, subject, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		CORBA_free (to);
		g_warning ("%s(%d): Error setting headers", __FUNCTION__, __LINE__);
		return FALSE;
	}
	
	CORBA_free (to);
	
	CORBA_exception_init (&ev);  
	Evolution_Composer_show (composer, &ev);
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
}
