
/*--------------------------------*-C-*---------------------------------*
 *
 * Author :
 *  Matt Loper <matt@helixcode.com>
 *
 *  Copyright 2000, Helix Code, Inc. (http://www.helixcode.com) .
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 *
 *----------------------------------------------------------------------*/

#include <config.h>
#include "camel-formatter.h"

static GtkObjectClass *parent_class=NULL;
static void _finalize (GtkObject *object);

struct _CamelFormatterPrivate {
	CamelMimeMessage *msg;
};

static void
write_field_to_stream (gchar* description, gchar* value, CamelStream *stream)
{
	gchar *s;
	g_assert (description && value);
	
	s = g_strdup_printf ("<b>%s: %s</b><br>\n",
			     description, value);
	camel_stream_write_string (stream, s);
	g_free (s);
}

static void
write_recipients_to_stream (const gchar *recipient_type,
			    GList *recipients,
			    CamelStream* stream_out)
{
	gchar *s;
	g_assert (recipient_type && stream_out);
	
	s = g_strdup_printf ("<b>%s: <br>\n", recipient_type);
	camel_stream_write_string (stream_out, s);
	g_free (s);

 	while (recipients) {
		camel_stream_write_string (stream_out, recipients->data);
		recipients = recipients->next;
		if (recipients)
			camel_stream_write_string (stream_out, "; ");
	}
	camel_stream_write_string (stream_out, "</b><br>\n");	
}

void
set_mime_message (CamelFormatter* cfm, CamelMimeMessage* msg)
{
	g_assert(cfm && msg);
	cfm->priv->msg = msg;
}



/**
 * camel_formatter_to_html: write data content in a byte stream
 * @data_wrapper: the camel formatter object
 * @stream_out byte stream where data will be written 
 * 
 * This method must be overriden by subclasses
 * Data must be written in the bytes stream
 * in a architecture independant fashion. 
 * If data is a standard data (for example an jpg image)
 * it must be serialized in the strea exactly as it 
 * would be saved on disk. A simple dump of the stream in
 * a file should be sufficient for the data to be 
 * re-read by a foreign application.  
 * 
 **/
void
camel_formatter_make_html (CamelFormatter *mhs,
			   CamelStream* stream_out)
{
	gchar *s = NULL;
	GList *recipients = NULL;
	CamelMimeMessage *mime_message;

	g_assert (mhs && stream_out);
	g_assert (mhs->priv->msg);

	mime_message = mhs->priv->msg;
	
	camel_stream_write_string (stream_out, "Content type: text/html\n");

	if ((s = (gchar*)camel_mime_message_get_subject (mime_message))) {
		write_field_to_stream ("Subject: ", s, stream_out);
	}

	if ((s = (gchar*)camel_mime_message_get_from (mime_message))) {
		write_field_to_stream ("From: ", s, stream_out);
	}

	if ((s = (gchar*)camel_mime_message_get_received_date (mime_message))) {
		write_field_to_stream ("Received Date: ", s, stream_out);
	}

	if ((s = (gchar*)camel_mime_message_get_sent_date (mime_message))) {
		write_field_to_stream ("Sent Date: ", s, stream_out);
	}		

	/* Fill out the "To:" recipients line */
	recipients = camel_mime_message_get_recipients (
		mime_message, CAMEL_RECIPIENT_TYPE_TO);
	if (recipients)
		write_recipients_to_stream ("To:", recipients, stream_out);

	/* Fill out the "CC:" recipients line */
	recipients = camel_mime_message_get_recipients (
		mime_message, CAMEL_RECIPIENT_TYPE_CC);
	if (recipients)
		write_recipients_to_stream ("CC:", recipients, stream_out);	
	/* Fill out the "BCC:" recipients line */
	recipients = camel_mime_message_get_recipients (
		mime_message, CAMEL_RECIPIENT_TYPE_BCC);	
	if (recipients)
		write_recipients_to_stream ("BCC:", recipients, stream_out);
}

static void
camel_formatter_class_init (CamelFormatterClass *camel_formatter_class)
{
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_formatter_class);

	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method overload */
	gtk_object_class->finalize = _finalize;
}

static void
camel_formatter_init (gpointer object, gpointer klass) 
{
	CamelFormatter* cmf = CAMEL_FORMATTER (object);
	cmf->priv->msg = NULL;
}


GtkType
camel_formatter_get_type (void)
{
	static GtkType camel_formatter_type = 0;
	
	if (!camel_formatter_type)	{
		GtkTypeInfo camel_formatter_info =	
		{
			"CamelFormatter",
			sizeof (CamelFormatter),
			sizeof (CamelFormatterClass),
			(GtkClassInitFunc) camel_formatter_class_init,
			(GtkObjectInitFunc) camel_formatter_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_formatter_type = gtk_type_unique (
			gtk_object_get_type (),
			&camel_formatter_info);
	}
	
	return camel_formatter_type;
}


static void           
_finalize (GtkObject *object)
{
	CamelFormatter *mhs = CAMEL_FORMATTER (object);

	g_free (mhs->priv);
	
	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}


