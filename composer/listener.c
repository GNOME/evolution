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

#include <bonobo/bonobo-arg.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-stream-client.h>

#include "listener.h"

static BonoboObjectClass *listener_parent_class;
static POA_GNOME_GtkHTML_Editor_Listener__vepv listener_vepv;

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

static gchar *
resolve_image_url (EditorListener *l, gchar *url)
{
	CamelMimePart *part;
	const char *cid;

	part = g_hash_table_lookup (l->composer->inline_images_by_url, url);
	if (!part && !strncmp (url, "file:", 5)) {
		part = e_msg_composer_add_inline_image_from_file (l->composer,
								  url + 5);
	}
	if (!part && !strncmp (url, "cid:", 4)) {
		part = g_hash_table_lookup (l->composer->inline_images, url);
	}
	if (!part)
		return NULL;

	l->composer->current_images  = g_list_prepend (l->composer->current_images, part);

	cid = camel_mime_part_get_content_id (part);
	if (!cid)
		return NULL;

	return g_strconcat ("cid:", cid, NULL);
}

static void
reply_indent (EditorListener *l, CORBA_Environment * ev)
{
	if (!GNOME_GtkHTML_Editor_Engine_isParagraphEmpty (l->composer->editor_engine, ev)) {
		if (GNOME_GtkHTML_Editor_Engine_isPreviousParagraphEmpty (l->composer->editor_engine, ev))
			GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "cursor-backward", ev);
		else {
			GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "text-default-color", ev);
			GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "italic-off", ev);
			GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "insert-paragraph", ev);
			return;
		}
	}

	GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "style-normal", ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "indent-zero", ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "text-default-color", ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "italic-off", ev);
}

static void
clear_signature (GNOME_GtkHTML_Editor_Engine e, CORBA_Environment * ev)
{
	if (GNOME_GtkHTML_Editor_Engine_isParagraphEmpty (e, ev))
		GNOME_GtkHTML_Editor_Engine_setParagraphData (e, "signature", "0", ev);
	else if (GNOME_GtkHTML_Editor_Engine_isPreviousParagraphEmpty (e, ev)
		 && GNOME_GtkHTML_Editor_Engine_runCommand (e, "cursor-backward", ev)) {
		GNOME_GtkHTML_Editor_Engine_setParagraphData (e, "signature", "0", ev);
		GNOME_GtkHTML_Editor_Engine_runCommand (e, "cursor-forward", ev);
	}
	GNOME_GtkHTML_Editor_Engine_runCommand (e, "text-default-color", ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (e, "italic-off", ev);
}

static void
insert_paragraph_before (EditorListener *l, CORBA_Environment * ev)
{
	if (!l->composer->in_signature_insert) {
		CORBA_char *orig, *signature;
		gboolean changed = FALSE;
		/* FIXME check for insert-paragraph command */

		orig = GNOME_GtkHTML_Editor_Engine_getParagraphData (l->composer->editor_engine, "orig", ev);
		if (ev->_major == CORBA_NO_EXCEPTION) {
			if (orig && *orig == '1') {
				GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "text-default-color", ev);
				GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "italic-off", ev);
				changed = TRUE;
			}
		}
		if (!changed) {
			signature = GNOME_GtkHTML_Editor_Engine_getParagraphData (l->composer->editor_engine, "signature", ev);
			if (ev->_major == CORBA_NO_EXCEPTION) {
				if (signature && *signature == '1') {
					GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "text-default-color",
										ev);
					GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "italic-off", ev);
				}
			}
		}
	}
}

static void
insert_paragraph_after (EditorListener *l, CORBA_Environment * ev)
{
	if (!l->composer->in_signature_insert) {
		CORBA_char *orig, *signature;
		/* FIXME check for insert-paragraph command */
		GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "text-default-color", ev);
		GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "italic-off", ev);

		orig = GNOME_GtkHTML_Editor_Engine_getParagraphData (l->composer->editor_engine, "orig", ev);
		if (ev->_major == CORBA_NO_EXCEPTION) {
			if (orig && *orig == '1')
				reply_indent (l, ev);
			GNOME_GtkHTML_Editor_Engine_setParagraphData (l->composer->editor_engine, "orig", "0", ev);
		}
		signature = GNOME_GtkHTML_Editor_Engine_getParagraphData (l->composer->editor_engine, "signature", ev);
		if (ev->_major == CORBA_NO_EXCEPTION) {
			if (signature && *signature == '1')
				clear_signature (l->composer->editor_engine, ev);
		}
	}
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

		if ((url = resolve_image_url (l, BONOBO_ARG_GET_STRING (arg)))) {
			rv = bonobo_arg_new (TC_string);
			BONOBO_ARG_SET_STRING (rv, url);
			/* printf ("new url: %s\n", url); */
			g_free (url);
		}
	} else if (!strcmp (name, "delete")) {
		CORBA_char *orig;
		
		if (GNOME_GtkHTML_Editor_Engine_isParagraphEmpty (l->composer->editor_engine, ev)) {
			orig = GNOME_GtkHTML_Editor_Engine_getParagraphData (l->composer->editor_engine, "orig", ev);
			if (ev->_major == CORBA_NO_EXCEPTION) {
				if (orig && *orig == '1') {
					GNOME_GtkHTML_Editor_Engine_setParagraphData (l->composer->editor_engine, "orig", "0", ev);

					GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "indent-zero", ev);
					GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "style-normal", ev);
					GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "text-default-color", ev);
					GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "italic-off", ev);
					GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "insert-paragraph", ev);
					GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "delete-back", ev);
				}
			}
		}
	} else if (!strcmp (name, "url_requested")) {
		GNOME_GtkHTML_Editor_URLRequestEvent *e;
		CamelMimePart *part;
		GByteArray *ba;
		CamelStream *cstream;
		CamelDataWrapper *wrapper;

		e = (GNOME_GtkHTML_Editor_URLRequestEvent *)arg->_value;

		if (!e->url || e->stream == CORBA_OBJECT_NIL)
			return get_any_null ();

		part = g_hash_table_lookup (l->composer->inline_images_by_url, e->url);
		if (!part)
			part = g_hash_table_lookup (l->composer->inline_images, e->url);
		if (!part)
			return get_any_null ();

		/* Write the data to a CamelStreamMem... */
		ba = g_byte_array_new ();
		cstream = camel_stream_mem_new_with_byte_array (ba);
		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
		camel_data_wrapper_write_to_stream (wrapper, cstream);

		bonobo_stream_client_write (e->stream, ba->data, ba->len, ev);

		camel_object_unref (CAMEL_OBJECT (cstream));
	}

	return rv ? rv : get_any_null ();
}

POA_GNOME_GtkHTML_Editor_Listener__epv *
listener_get_epv (void)
{
	POA_GNOME_GtkHTML_Editor_Listener__epv *epv;

	epv = g_new0 (POA_GNOME_GtkHTML_Editor_Listener__epv, 1);

	epv->event = impl_event;

	return epv;
}

static void
init_listener_corba_class (void)
{
	listener_vepv.Bonobo_Unknown_epv            = bonobo_object_get_epv ();
	listener_vepv.GNOME_GtkHTML_Editor_Listener_epv = listener_get_epv ();
}

static void
listener_class_init (EditorListenerClass *klass)
{
	listener_parent_class = gtk_type_class (bonobo_object_get_type ());

	init_listener_corba_class ();
}

GtkType
listener_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"EditorListener",
			sizeof (EditorListener),
			sizeof (EditorListenerClass),
			(GtkClassInitFunc) listener_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_object_get_type (), &info);
	}

	return type;
}

EditorListener *
listener_construct (EditorListener *listener, GNOME_GtkHTML_Editor_Listener corba_listener)
{
	g_return_val_if_fail (listener != NULL, NULL);
	g_return_val_if_fail (IS_EDITOR_LISTENER (listener), NULL);
	g_return_val_if_fail (corba_listener != CORBA_OBJECT_NIL, NULL);

	if (!bonobo_object_construct (BONOBO_OBJECT (listener), (CORBA_Object) corba_listener))
		return NULL;

	return listener;
}

static GNOME_GtkHTML_Editor_Listener
create_listener (BonoboObject *listener)
{
	POA_GNOME_GtkHTML_Editor_Listener *servant;
	CORBA_Environment ev;

	servant = (POA_GNOME_GtkHTML_Editor_Listener *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &listener_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_GtkHTML_Editor_Listener__init ((PortableServer_Servant) servant, &ev);
	ORBIT_OBJECT_KEY(servant->_private)->object = NULL;

	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);

	return (GNOME_GtkHTML_Editor_Listener) bonobo_object_activate_servant (listener, servant);
}

EditorListener *
listener_new (EMsgComposer *composer)
{
	EditorListener *listener;
	GNOME_GtkHTML_Editor_Listener corba_listener;

	listener = gtk_type_new (EDITOR_LISTENER_TYPE);
	listener->composer = composer;

	corba_listener = create_listener (BONOBO_OBJECT (listener));

	if (corba_listener == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (listener));
		return NULL;
	}
	
	return listener_construct (listener, corba_listener);
}
