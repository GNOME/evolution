/*
 * e-contact-map.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2011 Dan Vratil <dvratil@redhat.com>
 *
 */

#ifndef E_CONTACT_MAP_H
#define E_CONTACT_MAP_H

#ifdef ENABLE_CONTACT_MAPS

#include <gtk/gtk.h>

#include <champlain/champlain.h>
#include <champlain-gtk/champlain-gtk.h>

#include <libebook/libebook.h>

/* Standard GObject macros */
#define E_TYPE_CONTACT_MAP \
	(e_contact_map_get_type ())
#define E_CONTACT_MAP(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONTACT_MAP, EContactMap))
#define E_CONTACT_MAP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONTACT_MAP, EContactMapClass))
#define E_IS_CONTACT_MAP(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONTACT_MAP))
#define E_IS_CONTACT_MAP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONTACT_MAP))
#define E_CONTACT_MAP_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONTACT_MAP, EContactMapClass))

G_BEGIN_DECLS

typedef struct _EContactMap EContactMap;
typedef struct _EContactMapClass EContactMapClass;
typedef struct _EContactMapPrivate EContactMapPrivate;

struct _EContactMap {
	GtkChamplainEmbed parent;
	EContactMapPrivate *priv;
};

struct _EContactMapClass {
	GtkWindowClass parent_class;

	/* Signals */
	void		(*contact_added)	(EContactMap *map,
						 ClutterActor *marker);
	void		(*contact_removed)	(EContactMap *map,
						 const gchar *name);
	void		(*geocoding_started)	(EContactMap *map,
						 ClutterActor *marker);
	void		(*geocoding_failed)	(EContactMap *map,
						 const gchar *name);
};

GType		e_contact_map_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_contact_map_new		(void);
void		e_contact_map_add_contact	(EContactMap *map,
						 EContact *contact);
void		e_contact_map_add_marker	(EContactMap *map,
						 const gchar *name,
						 const gchar *contact_uid,
						 EContactAddress *address,
						 EContactPhoto *photo);
void		e_contact_map_remove_contact	(EContactMap *map,
						 const gchar *name);
void		e_contact_map_zoom_on_marker	(EContactMap *map,
						 ClutterActor *marker);
ChamplainView *	e_contact_map_get_view		(EContactMap *map);

G_END_DECLS

#endif /* ENABLE_CONTACT_MAPS */

#endif /* E_CONTACT_MAP_H */
