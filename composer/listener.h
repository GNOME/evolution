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

#ifndef LISTENER_H_
#define LISTENER_H_

#include <libgnome/gnome-defs.h>
#include <bonobo/bonobo-object.h>
#include "Editor.h"
#include "e-msg-composer.h"

BEGIN_GNOME_DECLS

#define EDITOR_LISTENER_TYPE        (listener_get_type ())
#define EDITOR_LISTENER(o)          (GTK_CHECK_CAST ((o), EDITOR_LISTENER_TYPE, EditorListener))
#define EDITOR_LISTENER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), EDITOR_LISTENER_TYPE, EditorListenerClass))
#define IS_EDITOR_LISTENER(o)       (GTK_CHECK_TYPE ((o), EDITOR_LISTENER_TYPE))
#define IS_EDITOR_LISTENER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), EDITOR_LISTENER_TYPE))

typedef struct {
	BonoboObject parent;
	EMsgComposer *composer;
} EditorListener;

typedef struct {
	BonoboObjectClass parent_class;
} EditorListenerClass;

GtkType                                 listener_get_type   (void);
EditorListener                         *listener_construct  (EditorListener                *listener,
									GNOME_GtkHTML_Editor_Listener  corba_listener);
EditorListener                         *listener_new        (EMsgComposer                  *composer);
POA_GNOME_GtkHTML_Editor_Listener__epv *listener_get_epv    (void);

END_GNOME_DECLS

#endif /* LISTENER_H_ */
