/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-storage-set-view.h
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

#ifndef _EVOLUTION_STORAGE_SET_VIEW_H_
#define _EVOLUTION_STORAGE_SET_VIEW_H_

#include <bonobo/bonobo-object.h>

#include "e-storage-set-view.h"

#include "Evolution.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_STORAGE_SET_VIEW			(evolution_storage_set_view_get_type ())
#define EVOLUTION_STORAGE_SET_VIEW(obj)			(GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_STORAGE_SET_VIEW, EvolutionStorageSetView))
#define EVOLUTION_STORAGE_SET_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_STORAGE_SET_VIEW, EvolutionStorageSetViewClass))
#define EVOLUTION_IS_STORAGE_SET_VIEW(obj)			(GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_STORAGE_SET_VIEW))
#define EVOLUTION_IS_STORAGE_SET_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_STORAGE_SET_VIEW))


typedef struct _EvolutionStorageSetView        EvolutionStorageSetView;
typedef struct _EvolutionStorageSetViewPrivate EvolutionStorageSetViewPrivate;
typedef struct _EvolutionStorageSetViewClass   EvolutionStorageSetViewClass;

struct _EvolutionStorageSetView {
	BonoboObject parent;

	EvolutionStorageSetViewPrivate *priv;
};

struct _EvolutionStorageSetViewClass {
	BonoboObjectClass parent_class;
};


GtkType                  evolution_storage_set_view_get_type   (void);
void                     evolution_storage_set_view_construct  (EvolutionStorageSetView  *storage_set_view,
								Evolution_StorageSetView  corba_object,
								EStorageSetView          *storage_set_view_widget);
EvolutionStorageSetView *evolution_storage_set_view_new        (EStorageSetView          *storage_set_view_widget);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _EVOLUTION_STORAGE_SET_VIEW_H_ */
