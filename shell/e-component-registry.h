/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-component-registry.h
 *
 * Copyright (C) 2000, 2003  Ximian, Inc.
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


#include "Evolution.h"

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define E_TYPE_COMPONENT_REGISTRY		(e_component_registry_get_type ())
#define E_COMPONENT_REGISTRY(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_COMPONENT_REGISTRY, EComponentRegistry))
#define E_COMPONENT_REGISTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_COMPONENT_REGISTRY, EComponentRegistryClass))
#define E_IS_COMPONENT_REGISTRY(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_COMPONENT_REGISTRY))
#define E_IS_COMPONENT_REGISTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_COMPONENT_REGISTRY))


typedef struct _EComponentRegistry        EComponentRegistry;
typedef struct _EComponentRegistryPrivate EComponentRegistryPrivate;
typedef struct _EComponentRegistryClass   EComponentRegistryClass;

struct _EComponentRegistry {
	GObject parent;

	EComponentRegistryPrivate *priv;
};

struct _EComponentRegistryClass {
	GObjectClass parent_class;
};

enum _EComponentRegistryField {
	ECR_FIELD_ID,
	ECR_FIELD_ALIAS,
	ECR_FIELD_SCHEMA,
};

struct _EComponentInfo {
	char *id;

	char *alias;

	/* NULL if not activated.  */
	GNOME_Evolution_Component iface;

	char *button_label;
	GdkPixbuf *button_icon;
	char *menu_label;
	char *menu_accelerator;
	GdkPixbuf *menu_icon;

	int sort_order;

	/* List of URI schemas that this component supports.  */
	GSList *uri_schemas;	/* <char *> */
};
typedef struct _EComponentInfo EComponentInfo;


GType               e_component_registry_get_type  (void);
EComponentRegistry *e_component_registry_new       (void);

GSList         *e_component_registry_peek_list  (EComponentRegistry *registry);
EComponentInfo *e_component_registry_peek_info  (EComponentRegistry *registry,
						 enum _EComponentRegistryField type,
						 const char *key);

GNOME_Evolution_Component  e_component_registry_activate  (EComponentRegistry *registry,
							   const char         *id,
							   CORBA_Environment  *ev);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_COMPONENT_REGISTRY_H__ */
