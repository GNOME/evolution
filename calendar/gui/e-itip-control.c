/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-itip-control.c
 *
 * Authors:
 *    Jesse Pavel <jpavel@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <config.h>
#include <gnome.h>
#include <bonobo.h>
#include <glade/glade.h>
#include <icaltypes.h>
#include <ical.h>

#include "e-itip-control.h"

/*
 * Bonobo::PersistStream
 *
 * These two functions implement the Bonobo::PersistStream load and
 * save methods which allow data to be loaded into and out of the
 * BonoboObject.
 */

typedef struct _EItipControlPrivate EItipControlPrivate;

struct _EItipControlPrivate {
	GladeXML *xml;
	GtkWidget *main_frame;
	GtkWidget *text_box;

	icalcomponent *main_comp;
};


static void
control_destroy_cb (GtkObject *object,
		    gpointer data)
{
	EItipControlPrivate *priv = data;

	gtk_object_unref (GTK_OBJECT (priv->xml));
	g_free (priv);
}
	

static char *
stream_read (Bonobo_Stream stream)
{
	Bonobo_Stream_iobuf *buffer;
	CORBA_Environment    ev;
	gchar *data = NULL;
	gint length = 0;

	CORBA_exception_init (&ev);
	do {
#define READ_CHUNK_SIZE 65536
		Bonobo_Stream_read (stream, READ_CHUNK_SIZE,
				    &buffer, &ev);

		if (ev._major != CORBA_NO_EXCEPTION) {
			CORBA_exception_free (&ev);
			return NULL;
		}

		if (buffer->_length <= 0)
			break;

		data = g_realloc (data,
				  length + buffer->_length);

		memcpy (data + length,
			buffer->_buffer, buffer->_length);

		length += buffer->_length;

		CORBA_free (buffer);
	} while (1);

	CORBA_free (buffer);
	CORBA_exception_free (&ev);

	if (data == NULL)
	  data = g_strdup("");

	return data;
} /* stream_read */

/*
 * This function implements the Bonobo::PersistStream:load method.
 */
static void
pstream_load (BonoboPersistStream *ps, const Bonobo_Stream stream,
	      Bonobo_Persist_ContentType type, void *data,
	      CORBA_Environment *ev)
{
	EItipControlPrivate *priv = data;
	gchar *vcalendar;
	gint pos, length;

	if (type && g_strcasecmp (type, "text/calendar") != 0 &&	    
	    g_strcasecmp (type, "text/x-calendar") != 0) {	    
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	if ((vcalendar = stream_read (stream)) == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_FileNotFound, NULL);
		return;
	}

	/* Do something with the data, here. */
	pos = 0;
	length = strlen (vcalendar);

	if (length > 0) 
		gtk_editable_delete_text (GTK_EDITABLE (priv->text_box), 0, length);

	gtk_editable_insert_text (GTK_EDITABLE (priv->text_box),
				  vcalendar,
				  length,
				  &pos);


	g_free (vcalendar);

} /* pstream_load */

/*
 * This function implements the Bonobo::PersistStream:save method.
 */
static void
pstream_save (BonoboPersistStream *ps, const Bonobo_Stream stream,
	      Bonobo_Persist_ContentType type, void *data,
	      CORBA_Environment *ev)
{
	EItipControlPrivate *priv = data;
	gchar                *vcalendar;
	int                  length;

	if (type && g_strcasecmp (type, "text/calendar") != 0 &&	    
	    g_strcasecmp (type, "text/x-calendar") != 0) {	    
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	/* Put something into vcalendar here. */
	length = gtk_text_get_length (GTK_TEXT (priv->text_box));
	vcalendar = gtk_editable_get_chars (GTK_EDITABLE (priv->text_box), 0, -1);

	bonobo_stream_client_write (stream, vcalendar, length, ev);
	g_free (vcalendar);
} /* pstream_save */

static CORBA_long
pstream_get_max_size (BonoboPersistStream *ps, void *data,
		      CORBA_Environment *ev)
{
	EItipControlPrivate *priv = data;
	gint length;
  
	length = gtk_text_get_length (GTK_TEXT (priv->text_box));

	return length;
}

static Bonobo_Persist_ContentTypeList *
pstream_get_content_types (BonoboPersistStream *ps, void *closure,
			   CORBA_Environment *ev)
{
	return bonobo_persist_generate_content_types (2, "text/calendar", "text/x-calendar");
}

static BonoboObject *
e_itip_control_factory (BonoboGenericFactory *Factory, void *closure)
{
	BonoboControl      *control;
	BonoboPersistStream *stream;
	EItipControlPrivate *priv;

	priv = g_new0 (EItipControlPrivate, 1);

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/" "e-itip-control.glade", "main_frame");

	/* Create the control. */
	priv->main_frame = glade_xml_get_widget (priv->xml, "main_frame");
	priv->text_box = glade_xml_get_widget (priv->xml, "text_box");
	gtk_text_set_editable (GTK_TEXT (priv->text_box), FALSE);

	gtk_signal_connect (GTK_OBJECT (priv->main_frame), "destroy",
			    GTK_SIGNAL_FUNC (control_destroy_cb), priv);

	gtk_widget_show (priv->text_box);
	gtk_widget_show (priv->main_frame);

	control = bonobo_control_new (priv->main_frame);

	stream = bonobo_persist_stream_new (pstream_load, pstream_save,
					    pstream_get_max_size,
					    pstream_get_content_types,
					    priv);

	if (stream == NULL) {
		bonobo_object_unref (BONOBO_OBJECT (control));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (control),
				    BONOBO_OBJECT (stream));

	return BONOBO_OBJECT (control);
}

void
e_itip_control_factory_init (void)
{
	static BonoboGenericFactory *factory = NULL;

	if (factory != NULL)
		return;

	factory =
		bonobo_generic_factory_new (
		        "OAFIID:control-factory:e_itipview:10441fcf-9a4f-4bf9-a026-d50b5462d45a",
			e_itip_control_factory, NULL);

	if (factory == NULL)
		g_error ("I could not register an iTip control factory.");
}

