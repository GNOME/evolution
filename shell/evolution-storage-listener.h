/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-storage-listener.h
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "Evolution.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_STORAGE_LISTENER			(evolution_storage_listener_get_type ())
#define EVOLUTION_STORAGE_LISTENER(obj)			(GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_STORAGE_LISTENER, EvolutionStorageListener))
#define EVOLUTION_STORAGE_LISTENER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_STORAGE_LISTENER, EvolutionStorageListenerClass))
#define EVOLUTION_IS_STORAGE_LISTENER(obj)			(GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_STORAGE_LISTENER))
#define EVOLUTION_IS_STORAGE_LISTENER_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_STORAGE_LISTENER))


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
	void (* destroyed)	(EvolutionStorageListener *storage_listener);
	void (* new_folder)	(EvolutionStorageListener *storage_listener,
				 const char *path,
				 const Evolution_Folder *folder);
	void (* removed_folder)	(EvolutionStorageListener *storage_listener,
				 const char *path);
};


struct _EvolutionStorageListenerServant {
	POA_Evolution_StorageListener servant_placeholder;
	EvolutionStorageListener *gtk_object;
};
typedef struct _EvolutionStorageListenerServant EvolutionStorageListenerServant;


GtkType                   evolution_storage_listener_get_type   (void);
void                      evolution_storage_listener_construct  (EvolutionStorageListener  *listener,
								 Evolution_StorageListener  corba_objref);
EvolutionStorageListener *evolution_storage_listener_new        (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_STORAGE_LISTENER_H__ */
