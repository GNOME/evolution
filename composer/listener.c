/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  This file is part of gnome-spell bonobo component

    Copyright (C) 2000 Helix Code, Inc.
    Authors:           Radek Doulik <rodo@helixcode.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <config.h>
#include <bonobo.h>

#include "listener.h"

static BonoboObjectClass *listener_parent_class;
static POA_HTMLEditor_Listener__vepv listener_vepv;

inline static HTMLEditorListener *
html_editor_listener_from_servant (PortableServer_Servant servant)
{
	return HTML_EDITOR_LISTENER (bonobo_object_from_servant (servant));
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
resolve_image_url (HTMLEditorListener *l, gchar *url)
{
	gchar *cid = NULL;

	if (!strncmp (url, "file:", 5)) {
		gchar *id;

		id = (gchar *) g_hash_table_lookup (l->composer->inline_images, url + 5);
		if (!id) {
			id = header_msgid_generate ();
			g_hash_table_insert (l->composer->inline_images, g_strdup (url + 5), id);
		}
		cid = g_strconcat ("cid:", id, NULL);
	}

	return cid;
}

static CORBA_any *
impl_event (PortableServer_Servant _servant,
       const CORBA_char * name, const CORBA_any * arg,
       CORBA_Environment * ev)
{
	HTMLEditorListener *l = html_editor_listener_from_servant (_servant);
	BonoboArg *data;
	CORBA_any *rv = NULL;

	/* printf ("impl_event\n"); */

	if (!strcmp (name, "command")) {
		/* FIXME check for insert-paragraph command */
		data = HTMLEditor_Engine_get_paragraph_data (l->composer->editor_engine, "orig", ev);
		if (ev->_major == CORBA_NO_EXCEPTION && data) {
			if (CORBA_TypeCode_equal (data->_type, TC_boolean, ev) && BONOBO_ARG_GET_BOOLEAN (data)) {
				HTMLEditor_Engine_command (l->composer->editor_engine, "style-normal", ev);
				HTMLEditor_Engine_command (l->composer->editor_engine, "indent-zero", ev);
				HTMLEditor_Engine_command (l->composer->editor_engine, "italic-off", ev);
			}
			BONOBO_ARG_SET_BOOLEAN (data, CORBA_FALSE);
			HTMLEditor_Engine_set_paragraph_data (l->composer->editor_engine, "orig", data, ev);
		}
	} else if (!strcmp (name, "image_url")) {
		gchar *url;

		if ((url = resolve_image_url (l, BONOBO_ARG_GET_STRING (arg)))) {
			rv = bonobo_arg_new (TC_string);
			BONOBO_ARG_SET_STRING (rv, url);
			printf ("new url: %s\n", url);
			g_free (url);
		}
	}

	return rv ? rv : get_any_null ();
}

POA_HTMLEditor_Listener__epv *
html_editor_listener_get_epv (void)
{
	POA_HTMLEditor_Listener__epv *epv;

	epv = g_new0 (POA_HTMLEditor_Listener__epv, 1);

	epv->event = impl_event;

	return epv;
}

static void
init_listener_corba_class (void)
{
	listener_vepv.Bonobo_Unknown_epv    = bonobo_object_get_epv ();
	listener_vepv.HTMLEditor_Listener_epv = html_editor_listener_get_epv ();
}

static void
listener_class_init (HTMLEditorListenerClass *klass)
{
	listener_parent_class = gtk_type_class (bonobo_object_get_type ());

	init_listener_corba_class ();
}

GtkType
html_editor_listener_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"HTMLEditorListener",
			sizeof (HTMLEditorListener),
			sizeof (HTMLEditorListenerClass),
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

HTMLEditorListener *
html_editor_listener_construct (HTMLEditorListener *listener, HTMLEditor_Listener corba_listener)
{
	g_return_val_if_fail (listener != NULL, NULL);
	g_return_val_if_fail (IS_HTML_EDITOR_LISTENER (listener), NULL);
	g_return_val_if_fail (corba_listener != CORBA_OBJECT_NIL, NULL);

	if (!bonobo_object_construct (BONOBO_OBJECT (listener), (CORBA_Object) corba_listener))
		return NULL;

	return listener;
}

static HTMLEditor_Listener
create_listener (BonoboObject *listener)
{
	POA_HTMLEditor_Listener *servant;
	CORBA_Environment ev;

	servant = (POA_HTMLEditor_Listener *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &listener_vepv;

	CORBA_exception_init (&ev);
	POA_HTMLEditor_Listener__init ((PortableServer_Servant) servant, &ev);
	ORBIT_OBJECT_KEY(servant->_private)->object = NULL;

	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);

	return (HTMLEditor_Listener) bonobo_object_activate_servant (listener, servant);
}

HTMLEditorListener *
html_editor_listener_new (EMsgComposer *composer)
{
	HTMLEditorListener *listener;
	HTMLEditor_Listener corba_listener;

	listener = gtk_type_new (HTML_EDITOR_LISTENER_TYPE);
	listener->composer = composer;

	corba_listener = create_listener (BONOBO_OBJECT (listener));

	if (corba_listener == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (listener));
		return NULL;
	}
	
	return html_editor_listener_construct (listener, corba_listener);
}
