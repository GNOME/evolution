/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-local-storage-client.h
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

#ifndef __EVOLUTION_LOCAL_STORAGE_CLIENT_H__
#define __EVOLUTION_LOCAL_STORAGE_CLIENT_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_LOCAL_STORAGE_CLIENT			(evolution_local_storage_client_get_type ())
#define EVOLUTION_LOCAL_STORAGE_CLIENT(obj)			(GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_LOCAL_STORAGE_CLIENT, EvolutionLocalStorageClient))
#define EVOLUTION_LOCAL_STORAGE_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_LOCAL_STORAGE_CLIENT, EvolutionLocalStorageClientClass))
#define EVOLUTION_IS_LOCAL_STORAGE_CLIENT(obj)			(GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_LOCAL_STORAGE_CLIENT))
#define EVOLUTION_IS_LOCAL_STORAGE_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_LOCAL_STORAGE_CLIENT))


typedef struct _EvolutionLocalStorageClient        EvolutionLocalStorageClient;
typedef struct _EvolutionLocalStorageClientPrivate EvolutionLocalStorageClientPrivate;
typedef struct _EvolutionLocalStorageClientClass   EvolutionLocalStorageClientClass;

struct _EvolutionLocalStorageClient {
	GtkObject parent;

	EvolutionLocalStorageClientPrivate *priv;
};

struct _EvolutionLocalStorageClientClass {
	GtkObjectClass parent_class;

	/* Signals.  */
	void (* new_folder)	(EvolutionLocalStorageClient *local_storage_client);
	void (* removed_folder)	(EvolutionLocalStorageClient *local_storage_client);
};


GtkType                      evolution_local_storage_client_get_type  (void);
EvolutionLocalStorageClient *evolution_local_storage_client_new       (Evolution_Shell corba_shell);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_LOCAL_STORAGE_CLIENT_H__ */
