/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-local-storage.h
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

#ifndef __EVOLUTION_LOCAL_STORAGE_H__
#define __EVOLUTION_LOCAL_STORAGE_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "evolution-storage.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_LOCAL_STORAGE			(evolution_local_storage_get_type ())
#define EVOLUTION_LOCAL_STORAGE(obj)			(GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_LOCAL_STORAGE, EvolutionLocalStorage))
#define EVOLUTION_LOCAL_STORAGE_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_LOCAL_STORAGE, EvolutionLocalStorageClass))
#define EVOLUTION_IS_LOCAL_STORAGE(obj)			(GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_LOCAL_STORAGE))
#define EVOLUTION_IS_LOCAL_STORAGE_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_LOCAL_STORAGE))


typedef struct _EvolutionLocalStorage        EvolutionLocalStorage;
typedef struct _EvolutionLocalStoragePrivate EvolutionLocalStoragePrivate;
typedef struct _EvolutionLocalStorageClass   EvolutionLocalStorageClass;

struct _EvolutionLocalStorage {
	EvolutionStorage parent;

	EvolutionLocalStoragePrivate *priv;
};

struct _EvolutionLocalStorageClass {
	EvolutionStorageClass parent_class;

	void (* set_display_name) (EvolutionLocalStorage *local_storage,
				   const char *path,
				   const char *display_name);
};


POA_Evolution_LocalStorage__epv *evolution_local_storage_get_epv    (void);

GtkType                          evolution_local_storage_get_type   (void);
void                             evolution_local_storage_construct  (EvolutionLocalStorage  *local_storage,
								     Evolution_LocalStorage  corba_object,
								     const char             *name);
EvolutionLocalStorage           *evolution_local_storage_new        (const char             *name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_LOCAL_STORAGE_H__ */
