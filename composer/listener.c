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
			CORBA_free (orig);
		}
		if (!changed) {
			signature = GNOME_GtkHTML_Editor_Engine_getParagraphData (l->composer->editor_engine, "signature", ev);
			if (ev->_major == CORBA_NO_EXCEPTION) {
				if (signature && *signature == '1') {
					GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "text-default-color",
										ev);
					GNOME_GtkHTML_Editor_Engine_runCommand (l->composer->editor_engine, "italic-off", ev);
				}
				CORBA_free (signature);
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
			CORBA_free (orig);
		}
		signature = GNOME_GtkHTML_Editor_Engine_getParagraphData (l->composer->editor_engine, "signature", ev);
		if (ev->_major == CORBA_NO_EXCEPTION) {
			if (signature && *signature == '1')
				clear_signature (l->composer->editor_engine, ev);
			CORBA_free (signature);
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
			rv = bonobo_arg_new (BONOBO_ARG_STRING);
			BONOBO_ARG_SET_STRING (rv, url);
			/* printf ("new url: %s\n", url); */
			g_free (url);
		}
	} else if (!strcmp (name, "delete")) {
		if (GNOME_GtkHTML_Editor_Engine_isParagraphEmpty (l->composer->editor_engine, ev)) {
			CORBA_char *orig;
			CORBA_char *signature;
		
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
				CORBA_free (orig);
			}
			signature = GNOME_GtkHTML_Editor_Engine_getParagraphData (l->composer->editor_engine, "signature", ev);
			if (ev->_major == CORBA_NO_EXCEPTION) {
				if (signature && *signature == '1')
					GNOME_GtkHTML_Editor_Engine_setParagraphData (l->composer->editor_engine, "signature", "0", ev);
				CORBA_free (signature);
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
