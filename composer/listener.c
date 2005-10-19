/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of gnome-spell bonobo component

    Copyright (C) 2000 Ximian, Inc.
    Authors:           Radek Doulik <rodo@ximian.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.
 
    You should have received a copy of the GNU General Public
    License along with this program; if not, write to the
    Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <bonobo/bonobo-arg.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-stream-client.h>
#include <camel/camel-stream-mem.h>

#include "listener.h"

static BonoboObjectClass *listener_parent_class;

inline static EditorListener *
listener_from_servant (PortableServer_Servant servant)
{
	return EDITOR_LISTENER (bonobo_object_from_servant (servant));
}

static CORBA_any *
get_any_null ()
{
	CORBA_any *rv;

	rv = CORBA_any__alloc ();
	rv->_type = TC_null;

	return rv;
}

static void
insert_paragraph_before (EditorListener *l, CORBA_Environment * ev)
{
	e_msg_composer_insert_paragraph_before (l->composer);
}

static void
insert_paragraph_after (EditorListener *l, CORBA_Environment * ev)
{
	e_msg_composer_insert_paragraph_after (l->composer);
}

static CORBA_any *
impl_event (PortableServer_Servant _servant,
	    const CORBA_char * name, const CORBA_any * arg,
	    CORBA_Environment * ev)
{
	EditorListener *l = listener_from_servant (_servant);
	CORBA_any  *rv = NULL;
	gchar *command;

	if (!strcmp (name, "command_before")) {
		command = BONOBO_ARG_GET_STRING (arg);
		if (!strcmp (command, "insert-paragraph")) {
			insert_paragraph_before (l, ev);
		}
	} else if (!strcmp (name, "command_after")) {
		command = BONOBO_ARG_GET_STRING (arg);
		if (!strcmp (command, "insert-paragraph")) {
			insert_paragraph_after (l, ev);
		}
	} else if (!strcmp (name, "image_url")) {
		gchar *url;
		
		if ((url = e_msg_composer_resolve_image_url (l->composer, BONOBO_ARG_GET_STRING (arg)))) {
			rv = bonobo_arg_new (BONOBO_ARG_STRING);
			BONOBO_ARG_SET_STRING (rv, url);
			/* printf ("new url: %s\n", url); */
			g_free (url);
		}
	} else if (!strcmp (name, "delete")) {
		e_msg_composer_delete (l->composer);
		
	} else if (!strcmp (name, "url_requested")) {
		GNOME_GtkHTML_Editor_URLRequestEvent *e = arg->_value;
		CamelMimePart *part;
		GByteArray *ba;
		CamelStream *cstream;
		CamelDataWrapper *wrapper;
	
		if (!e->url || e->stream == CORBA_OBJECT_NIL)
			return get_any_null ();

		part = e_msg_composer_url_requested (l->composer, e->url);
		
		if (!part)
			return get_any_null ();
		
		/* Write the data to a CamelStreamMem... */
		ba = g_byte_array_new ();
		cstream = camel_stream_mem_new_with_byte_array (ba);
		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
		camel_data_wrapper_decode_to_stream (wrapper, cstream);
		bonobo_stream_client_write (e->stream, ba->data, ba->len, ev);

		camel_object_unref (cstream);
			
	}

	return rv ? rv : get_any_null ();
}

static void
listener_class_init (EditorListenerClass *klass)
{
	POA_GNOME_GtkHTML_Editor_Listener__epv *epv;

	listener_parent_class = g_type_class_ref(bonobo_object_get_type ());

	epv = &klass->epv;
	epv->event = impl_event;
}

static void
listener_init(EditorListener *object)
{
}

BONOBO_TYPE_FUNC_FULL(EditorListener, GNOME_GtkHTML_Editor_Listener, BONOBO_TYPE_OBJECT, listener);

EditorListener *
listener_new (EMsgComposer *composer)
{
	EditorListener *listener;

	listener = g_object_new (EDITOR_LISTENER_TYPE, NULL);
	listener->composer = composer;
	
	return listener;
}
