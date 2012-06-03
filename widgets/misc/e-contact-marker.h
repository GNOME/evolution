/*
 * e-contact-marker.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Copyright (C) 2008 Pierre-Luc Beaudoin <pierre-luc@pierlux.com>
 * Copyright (C) 2011 Jiri Techet <techet@gmail.com>
 * Copyright (C) 2011 Dan Vratil <dvratil@redhat.com>
 *
 */

#ifndef E_CONTACT_MARKER_H
#define E_CONTACT_MARKER_H

#ifdef WITH_CONTACT_MAPS

#include <libebook/libebook.h>

#include <champlain/champlain.h>

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define E_TYPE_CONTACT_MARKER e_contact_marker_get_type ()

#define E_CONTACT_MARKER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CONTACT_MARKER, EContactMarker))

#define E_CONTACT_MARKER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CONTACT_MARKER, EContactMarkerClass))

#define E_IS_CONTACT_MARKER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CONTACT_MARKER))

#define E_IS_CONTACT_MARKER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_CONTACT_MARKER))

#define E_CONTACT_MARKER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_CONTACT_MARKER, EContactMarkerClass))

typedef struct _EContactMarkerPrivate EContactMarkerPrivate;

typedef struct _EContactMarker EContactMarker;
typedef struct _EContactMarkerClass EContactMarkerClass;

struct _EContactMarker
{
	ChamplainLabel parent;
	EContactMarkerPrivate *priv;
};

struct _EContactMarkerClass
{
	ChamplainLabelClass parent_class;

	void (*double_clicked)	(ClutterActor *actor);
};

GType e_contact_marker_get_type		(void);

ClutterActor * e_contact_marker_new		(const gchar *name,
						 const gchar *contact_uid,
						 EContactPhoto *photo);

const gchar * e_contact_marker_get_contact_uid	(EContactMarker *marker);

G_END_DECLS

#endif /* WITH_CONTACT_MAPS */

#endif
