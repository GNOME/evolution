/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-uri-schema-registry.h
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * published by the Free Software Foundation; either version 2 of the
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_URI_SCHEMA_REGISTRY_H_
#define _E_URI_SCHEMA_REGISTRY_H_

#include "evolution-shell-component-client.h"

#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_URI_SCHEMA_REGISTRY			(e_uri_schema_registry_get_type ())
#define E_URI_SCHEMA_REGISTRY(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_URI_SCHEMA_REGISTRY, EUriSchemaRegistry))
#define E_URI_SCHEMA_REGISTRY_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_URI_SCHEMA_REGISTRY, EUriSchemaRegistryClass))
#define E_IS_URI_SCHEMA_REGISTRY(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_URI_SCHEMA_REGISTRY))
#define E_IS_URI_SCHEMA_REGISTRY_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_URI_SCHEMA_REGISTRY))


typedef struct _EUriSchemaRegistry        EUriSchemaRegistry;
typedef struct _EUriSchemaRegistryPrivate EUriSchemaRegistryPrivate;
typedef struct _EUriSchemaRegistryClass   EUriSchemaRegistryClass;

struct _EUriSchemaRegistry {
	GtkObject parent;

	EUriSchemaRegistryPrivate *priv;
};

struct _EUriSchemaRegistryClass {
	GtkObjectClass parent_class;
};


GtkType             e_uri_schema_registry_get_type  (void);
EUriSchemaRegistry *e_uri_schema_registry_new       (void);

void                           e_uri_schema_registry_set_handler_for_schema  (EUriSchemaRegistry            *registry,
									      const char                    *schema,
									      EvolutionShellComponentClient *shell_component);
EvolutionShellComponentClient *e_uri_schema_registry_get_handler_for_schema  (EUriSchemaRegistry            *registry,
									      const char                    *schema);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_URI_SCHEMA_REGISTRY_H_ */
