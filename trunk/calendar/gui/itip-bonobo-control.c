/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Control for displaying iTIP mail messages
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Jesse Pavel <jpavel@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtkobject.h>
#include <gtk/gtkwidget.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-persist-stream.h>
#include <bonobo/bonobo-stream-client.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-exception.h>
#include <libical/ical.h>

#include "e-itip-control.h"
#include "itip-bonobo-control.h"

extern gchar *evolution_dir;

enum E_ITIP_BONOBO_ARGS {
	FROM_ADDRESS_ARG_ID,
	VIEW_ONLY_ARG_ID
};

/*
 * Bonobo::PersistStream
 *
 * These two functions implement the Bonobo::PersistStream load and
 * save methods which allow data to be loaded into and out of the
 * BonoboObject.
 */

static char *
stream_read (Bonobo_Stream stream)
{
	Bonobo_Stream_iobuf *buffer;
	CORBA_Environment ev;
	gchar *data = NULL;
	gint length = 0;

	CORBA_exception_init (&ev);
	do {
#define READ_CHUNK_SIZE 65536
		Bonobo_Stream_read (stream, READ_CHUNK_SIZE,
				    &buffer, &ev);

		if (BONOBO_EX (&ev)) {
			CORBA_exception_free (&ev);
			return NULL;
		}

		if (buffer->_length <= 0)
			break;

		data = g_realloc (data, length + buffer->_length + 1);
		memcpy (data + length, buffer->_buffer, buffer->_length);
		length += buffer->_length;
		data[length] = '\0';
		
		CORBA_free (buffer);
#undef READ_CHUNK_SIZE
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
typedef struct {
	EItipControl *itip;
	char *text;
} idle_data;

static gboolean
set_data_idle_cb (gpointer data) 
{	
	idle_data *id = data;
	
	e_itip_control_set_data (id->itip, id->text);
	g_object_unref (id->itip);
	g_free (id->text);
	g_free (id);
	
	return FALSE;
}

static void
pstream_load (BonoboPersistStream *ps, const Bonobo_Stream stream,
	      Bonobo_Persist_ContentType type, void *data,
	      CORBA_Environment *ev)
{
	EItipControl *itip = data;
	idle_data *id;
	
	if (type && g_ascii_strcasecmp (type, "text/calendar") != 0 &&	    
	    g_ascii_strcasecmp (type, "text/x-calendar") != 0) {
		bonobo_exception_set (ev, ex_Bonobo_Persist_WrongDataType);
		return;
	}

	id = g_new0 (idle_data, 1);
	if ((id->text = stream_read (stream)) == NULL) {
		bonobo_exception_set (ev, ex_Bonobo_Persist_FileNotFound);
		g_free (id);
		return;
	}
	g_object_ref (itip);
	id->itip = itip;
	
	g_idle_add (set_data_idle_cb, id);
}
/*
 * This function implements the Bonobo::PersistStream:save method.
 */
static void
pstream_save (BonoboPersistStream *ps, const Bonobo_Stream stream,
	      Bonobo_Persist_ContentType type, void *data,
	      CORBA_Environment *ev)
{
	EItipControl *itip = data;
	gchar *text;
	gint len;

	if (type && g_ascii_strcasecmp (type, "text/calendar") != 0 &&	    
	    g_ascii_strcasecmp (type, "text/x-calendar") != 0) {	    
		bonobo_exception_set (ev, ex_Bonobo_Persist_WrongDataType);
		return;
	}

	text = e_itip_control_get_data (itip);
	len = e_itip_control_get_data_size (itip);

	bonobo_stream_client_write (stream, text, len, ev);
	g_free (text);
} /* pstream_save */

/* static CORBA_long */
/* pstream_get_max_size (BonoboPersistStream *ps, void *data, */
/* 		      CORBA_Environment *ev) */
/* { */
/* 	EItipControl *itip = data; */
/* 	gint len; */
	
/* 	len = e_itip_control_get_data_size (itip); */
	
/*   	if (len > 0) */
/* 		return len; */

/* 	return 0L; */
/* } */

static Bonobo_Persist_ContentTypeList *
pstream_get_content_types (BonoboPersistStream *ps, void *closure,
			   CORBA_Environment *ev)
{
	return bonobo_persist_generate_content_types (2, "text/calendar", "text/x-calendar");
}

static void
get_prop (BonoboPropertyBag *bag, 
	   BonoboArg *arg,
	   guint arg_id, 	      
	   CORBA_Environment *ev,
	   gpointer user_data)
{
	EItipControl *itip = user_data;

	switch (arg_id) {
	case FROM_ADDRESS_ARG_ID:
		BONOBO_ARG_SET_STRING (arg, e_itip_control_get_from_address (itip));
		break;
	case VIEW_ONLY_ARG_ID:
		BONOBO_ARG_SET_INT (arg, e_itip_control_get_view_only (itip));
		break;
	}
}

static void
set_prop ( BonoboPropertyBag *bag, 
	   const BonoboArg *arg,
	   guint arg_id, 
	   CORBA_Environment *ev,
	   gpointer user_data)
{
	EItipControl *itip = user_data;

	switch (arg_id) {
	case FROM_ADDRESS_ARG_ID:
		e_itip_control_set_from_address (itip, BONOBO_ARG_GET_STRING (arg));
		break;
	case VIEW_ONLY_ARG_ID:
		e_itip_control_set_view_only (itip, BONOBO_ARG_GET_INT (arg));
		break;
	}
}


BonoboControl *
itip_bonobo_control_new (void)
{
	BonoboControl *control;
	BonoboPropertyBag *prop_bag;
	BonoboPersistStream *stream;
	GtkWidget *itip;

	itip = e_itip_control_new ();
	gtk_widget_show (itip);
	control = bonobo_control_new (itip);
	
	/* create a property bag */
	prop_bag = bonobo_property_bag_new (get_prop, set_prop, itip);
	bonobo_property_bag_add (prop_bag, "from_address", FROM_ADDRESS_ARG_ID, BONOBO_ARG_STRING, NULL,
				 "from_address", 0 );
	bonobo_property_bag_add (prop_bag, "view_only", VIEW_ONLY_ARG_ID, BONOBO_ARG_INT, NULL,
				 "view_only", 0 );

	bonobo_control_set_properties (control, bonobo_object_corba_objref (BONOBO_OBJECT (prop_bag)), NULL);
	bonobo_object_unref (BONOBO_OBJECT (prop_bag));

	bonobo_control_set_automerge (control, TRUE);

	stream = bonobo_persist_stream_new (pstream_load, pstream_save,
					    pstream_get_content_types,
					    "OAFIID:GNOME_Evolution_Calendar_iTip_Control:" BASE_VERSION,
					    itip);

	if (stream == NULL) {
		bonobo_object_unref (BONOBO_OBJECT (control));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (control),
				     BONOBO_OBJECT (stream));

	return control;
}
