/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-storage-set-view-listener.h
 *
 * Copyright (C) 2000  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef _EVOLUTION_STORAGE_SET_VIEW_LISTENER_H_
#define _EVOLUTION_STORAGE_SET_VIEW_LISTENER_H_

#include <gtk/gtkobject.h>

#include "Evolution.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_STORAGE_SET_VIEW_LISTENER		(evolution_storage_set_view_listener_get_type ())
#define EVOLUTION_STORAGE_SET_VIEW_LISTENER(obj)		(GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_STORAGE_SET_VIEW_LISTENER, EvolutionStorageSetViewListener))
#define EVOLUTION_STORAGE_SET_VIEW_LISTENER_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_STORAGE_SET_VIEW_LISTENER, EvolutionStorageSetViewListenerClass))
#define EVOLUTION_IS_STORAGE_SET_VIEW_LISTENER(obj)		(GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_STORAGE_SET_VIEW_LISTENER))
#define EVOLUTION_IS_STORAGE_SET_VIEW_LISTENER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_STORAGE_SET_VIEW_LISTENER))


typedef struct _EvolutionStorageSetViewListener        EvolutionStorageSetViewListener;
typedef struct _EvolutionStorageSetViewListenerPrivate EvolutionStorageSetViewListenerPrivate;
typedef struct _EvolutionStorageSetViewListenerClass   EvolutionStorageSetViewListenerClass;

struct _EvolutionStorageSetViewListener {
	GtkObject parent;

	EvolutionStorageSetViewListenerPrivate *priv;
};

struct _EvolutionStorageSetViewListenerClass {
	GtkObjectClass parent_class;

	void (* folder_selected) (EvolutionStorageSetViewListener *listener,
				  const char *uri);
	void (* folder_toggled) (EvolutionStorageSetViewListener *listener,
				 const char *uri,
				 gboolean active);
};


struct _EvolutionStorageSetViewListenerServant {
	POA_GNOME_Evolution_StorageSetViewListener servant_placeholder;
	EvolutionStorageSetViewListener *gtk_object;
};
typedef struct _EvolutionStorageSetViewListenerServant EvolutionStorageSetViewListenerServant;


GtkType                          evolution_storage_set_view_listener_get_type      (void);
void                             evolution_storage_set_view_listener_construct     (EvolutionStorageSetViewListener  *listener,
										    GNOME_Evolution_StorageSetViewListener  corba_objref);
EvolutionStorageSetViewListener *evolution_storage_set_view_listener_new           (void);

GNOME_Evolution_StorageSetViewListener evolution_storage_set_view_listener_corba_objref  (EvolutionStorageSetViewListener *listener);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _EVOLUTION_STORAGE_SET_VIEW_LISTENER_H_ */
