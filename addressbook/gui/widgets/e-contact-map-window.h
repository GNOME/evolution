/*
 * e-contact-map-window.h
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

#ifndef E_CONTACT_MAP_WINDOW_H
#define E_CONTACT_MAP_WINDOW_H

#ifdef ENABLE_CONTACT_MAPS

#include <gtk/gtk.h>

#include <libebook/libebook.h>

#include "e-contact-map.h"

/* Standard GObject macros */
#define E_TYPE_CONTACT_MAP_WINDOW \
	(e_contact_map_window_get_type ())
#define E_CONTACT_MAP_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONTACT_MAP_WINDOW, EContactMapWindow))
#define E_CONTACT_MAP_WINDOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONTACT_MAP_WINDOW, EContactMapWindowClass))
#define E_IS_CONTACT_MAP_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONTACT_MAP_WINDOW))
#define E_IS_CONTACT_MAP_WINDOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONTACT_MAP_WINDOW))
#define E_CONTACT_MAP_WINDOW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONTACT_MAP_WINDOW, EContactMapWindowClass))

G_BEGIN_DECLS

typedef struct _EContactMapWindow EContactMapWindow;
typedef struct _EContactMapWindowClass EContactMapWindowClass;
typedef struct _EContactMapWindowPrivate EContactMapWindowPrivate;

struct _EContactMapWindow {
	GtkWindow parent;
	EContactMapWindowPrivate *priv;
};

struct _EContactMapWindowClass {
	GtkWindowClass parent_class;

	void		(*show_contact_editor)	(EContactMapWindow *window,
						 const gchar *contact_uid);
};

GType		e_contact_map_window_get_type	(void) G_GNUC_CONST;
EContactMapWindow *
		e_contact_map_window_new	(void);
EContactMap *	e_contact_map_window_get_map	(EContactMapWindow *window);
void		e_contact_map_window_load_addressbook
						(EContactMapWindow *window,
						 EBookClient *book);

G_END_DECLS

#endif /* ENABLE_CONTACT_MAPS */

#endif /* E_CONTACT_MAP_WINDOW_H */
