/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-storage-listener.h
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

#ifndef __EVOLUTION_STORAGE_LISTENER_H__
#define __EVOLUTION_STORAGE_LISTENER_H__

#include <gtk/gtkobject.h>
#include "Evolution.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_STORAGE_LISTENER			(evolution_storage_listener_get_type ())
#define EVOLUTION_STORAGE_LISTENER(obj)			(GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_STORAGE_LISTENER, EvolutionStorageListener))
#define EVOLUTION_STORAGE_LISTENER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_STORAGE_LISTENER, EvolutionStorageListenerClass))
#define EVOLUTION_IS_STORAGE_LISTENER(obj)		(GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_STORAGE_LISTENER))
#define EVOLUTION_IS_STORAGE_LISTENER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_STORAGE_LISTENER))


typedef struct _EvolutionStorageListener        EvolutionStorageListener;
typedef struct _EvolutionStorageListenerPrivate EvolutionStorageListenerPrivate;
typedef struct _EvolutionStorageListenerClass   EvolutionStorageListenerClass;

struct _EvolutionStorageListener {
	GtkObject parent;

	EvolutionStorageListenerPrivate *priv;
};

struct _EvolutionStorageListenerClass {
	GtkObjectClass parent_class;

	/* Signals.  */
	void (* destroyed)       (EvolutionStorageListener *storage_listener);
	void (* new_folder)      (EvolutionStorageListener *storage_listener,
				  const char *path,
				  const GNOME_Evolution_Folder *folder);
	void (* update_folder)   (EvolutionStorageListener *storage_listener,
				  const char *path,
				  int unread_count);
	void (* removed_folder)  (EvolutionStorageListener *storage_listener,
				  const char *path);
	void (* has_subfolders)  (EvolutionStorageListener *storage_listener,
				  const char *path,
				  const char *message);

	void (* shared_folder_discovery_result) (EvolutionStorageListener *storage_listener,
						 const char *user,
						 const char *folder_name,
						 const char *storage_path,
						 const char *physical_uri);
};


struct _EvolutionStorageListenerServant {
	POA_GNOME_Evolution_StorageListener servant_placeholder;
	EvolutionStorageListener *gtk_object;
};
typedef struct _EvolutionStorageListenerServant EvolutionStorageListenerServant;


GtkType                    evolution_storage_listener_get_type      (void);
void                       evolution_storage_listener_construct     (EvolutionStorageListener  *listener,
								     GNOME_Evolution_StorageListener  corba_objref);
EvolutionStorageListener  *evolution_storage_listener_new           (void);

GNOME_Evolution_StorageListener  evolution_storage_listener_corba_objref  (EvolutionStorageListener *listener);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_STORAGE_LISTENER_H__ */
