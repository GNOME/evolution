/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-component-registry.h
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

#ifndef __E_COMPONENT_REGISTRY_H__
#define __E_COMPONENT_REGISTRY_H__

#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_COMPONENT_REGISTRY             (e_component_registry_get_type ())
#define E_COMPONENT_REGISTRY(obj)             (GTK_CHECK_CAST ((obj), E_TYPE_COMPONENT_REGISTRY, EComponentRegistry))
#define E_COMPONENT_REGISTRY_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_COMPONENT_REGISTRY, EComponentRegistryClass))
#define E_IS_COMPONENT_REGISTRY(obj)          (GTK_CHECK_TYPE ((obj), E_TYPE_COMPONENT_REGISTRY))
#define E_IS_COMPONENT_REGISTRY_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_COMPONENT_REGISTRY))


typedef struct _EComponentRegistry        EComponentRegistry;
typedef struct _EComponentRegistryPrivate EComponentRegistryPrivate;
typedef struct _EComponentRegistryClass   EComponentRegistryClass;

#include "e-shell.h"
#include "evolution-shell-component-client.h"

struct _EComponentRegistry {
	GtkObject parent;

	EComponentRegistryPrivate *priv;
};

struct _EComponentRegistryClass {
	GtkObjectClass parent_class;
};


GtkType                        e_component_registry_get_type             (void);
void                           e_component_registry_construct            (EComponentRegistry *component_registry,
									  EShell             *shell);
EComponentRegistry            *e_component_registry_new                  (EShell             *shell);

gboolean                       e_component_registry_register_component   (EComponentRegistry *component_registry,
									  const char         *id,
									  CORBA_Environment  *ev);

GList                         *e_component_registry_get_id_list          (EComponentRegistry *component_registry);

EvolutionShellComponentClient *e_component_registry_get_component_by_id  (EComponentRegistry *component_registry,
									  const char         *id);

EvolutionShellComponentClient *e_component_registry_restart_component  (EComponentRegistry *component_registry,
									const char         *id,
									CORBA_Environment  *ev);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_COMPONENT_REGISTRY_H__ */
