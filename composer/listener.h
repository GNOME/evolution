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
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef LISTENER_H_
#define LISTENER_H_

#include <bonobo/bonobo-object.h>
#include "Editor.h"
#include "e-msg-composer.h"

#define EDITOR_LISTENER_TYPE        (listener_get_type ())
#define EDITOR_LISTENER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), EDITOR_LISTENER_TYPE, EditorListener))
#define EDITOR_LISTENER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), EDITOR_LISTENER_TYPE, EditorListenerClass))
#define IS_EDITOR_LISTENER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EDITOR_LISTENER_TYPE))
#define IS_EDITOR_LISTENER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EDITOR_LISTENER_TYPE))

typedef struct {
	BonoboObject parent;
	EMsgComposer *composer;
} EditorListener;

typedef struct {
	BonoboObjectClass parent_class;

	POA_GNOME_GtkHTML_Editor_Listener__epv epv;
} EditorListenerClass;

GtkType                                 listener_get_type   (void);
EditorListener                         *listener_construct  (EditorListener                *listener,
									GNOME_GtkHTML_Editor_Listener  corba_listener);
EditorListener                         *listener_new        (EMsgComposer                  *composer);

#endif /* LISTENER_H_ */
