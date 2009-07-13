/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_COMPONENT_REGISTRY_H__
#define __E_COMPONENT_REGISTRY_H__

#include "Evolution.h"

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

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
	ECR_FIELD_SCHEMA
};

struct _EComponentInfo {
	gchar *id;

	gchar *alias;

	/* NULL if not activated.  */
	GNOME_Evolution_Component iface;

	gchar *button_label;
	gchar *button_tooltips;
	gchar *menu_label;
	gchar *menu_accelerator;
	gchar *icon_name;

	gint sort_order;

	/* List of URI schemas that this component supports.  */
	GSList *uri_schemas;	/* <char *> */
};
typedef struct _EComponentInfo EComponentInfo;

GType               e_component_registry_get_type  (void);
EComponentRegistry *e_component_registry_new       (void);

GSList         *e_component_registry_peek_list  (EComponentRegistry *registry);
EComponentInfo *e_component_registry_peek_info  (EComponentRegistry *registry,
						 enum _EComponentRegistryField type,
						 const gchar *key);

GNOME_Evolution_Component  e_component_registry_activate  (EComponentRegistry *registry,
							   const gchar         *id,
							   CORBA_Environment  *ev);

G_END_DECLS

#endif /* __E_COMPONENT_REGISTRY_H__ */
