/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-component-registry.c
 *
 * Copyright (C) 2000, 2001, 2002  Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-component-registry.h"

#include "e-shell-utils.h"
#include <e-util/e-icon-factory.h>

#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-exception.h>

#include <string.h>
#include <stdlib.h>


#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;


struct _EComponentRegistryPrivate {
	GSList *infos;

	int init:1;
};


/* EComponentInfo handling.  */

static EComponentInfo *
component_info_new (const char *id,
		    GNOME_Evolution_Component iface,
		    const char *alias,
		    const char *button_label,
		    const char *menu_label,
		    const char *menu_accelerator,
		    int sort_order,
		    GdkPixbuf *button_icon,
		    GdkPixbuf *menu_icon)
{
	EComponentInfo *info = g_new0 (EComponentInfo, 1);

	info->id = g_strdup (id);
	info->iface = bonobo_object_dup_ref(iface, NULL);
	info->alias = g_strdup (alias);
	info->button_label = g_strdup (button_label);
	info->menu_label = g_strdup (menu_label);
	info->menu_accelerator = g_strdup (menu_accelerator);
	info->sort_order = sort_order;

	info->button_icon = button_icon;
	if (info->button_icon)
		g_object_ref (info->button_icon);

	info->menu_icon = menu_icon;
	if (info->menu_icon)
		g_object_ref (info->menu_icon);

	return info;
}

static void
component_info_free (EComponentInfo *info)
{
	g_free (info->id);
	g_free (info->alias);
	g_free (info->button_label);
	g_free (info->menu_label);
	g_free (info->menu_accelerator);

	if (info->button_icon)
		g_object_unref (info->button_icon);

	if (info->menu_icon)
		g_object_unref (info->menu_icon);

	if (info->iface != NULL)
		bonobo_object_release_unref (info->iface, NULL);

	g_slist_foreach (info->uri_schemas, (GFunc) g_free, NULL);
	g_slist_free (info->uri_schemas);

	g_free (info);
}

static int
component_info_compare_func (EComponentInfo *a,
			     EComponentInfo *b)
{
	if (a->sort_order != b->sort_order)
		return a->sort_order - b->sort_order;

	return strcmp (a->button_label, b->button_label);
}


/* Utility methods.  */

static void
set_schemas (EComponentInfo *component_info,
	     Bonobo_ServerInfo *server_info)
{
	Bonobo_ActivationProperty *property = bonobo_server_info_prop_find (server_info, "evolution:uri_schemas");
	Bonobo_StringList *list;
	int i;

	if (property == NULL)
		return;

	if (property->v._d != Bonobo_ACTIVATION_P_STRINGV) {
		CORBA_free (property);
		return;
	}

	list = & property->v._u.value_stringv;

	for (i = 0; i < list->_length; i ++)
		component_info->uri_schemas = g_slist_prepend (component_info->uri_schemas, g_strdup (list->_buffer [i]));

	CORBA_free (property);
}

static void
query_components (EComponentRegistry *registry)
{
	Bonobo_ServerInfoList *info_list;
	CORBA_Environment ev;
	GSList *language_list;
	const GList *l;
	char *query;
	int i;

	if (registry->priv->init)
		return;

	registry->priv->init = TRUE;

	CORBA_exception_init (&ev);
	query = g_strdup_printf ("repo_ids.has ('IDL:GNOME/Evolution/Component:%s')", BASE_VERSION);
	info_list = bonobo_activation_query (query, NULL, &ev);
	g_free (query);

	if (BONOBO_EX (&ev)) {
		char *ex_text = bonobo_exception_get_text (&ev);
		g_warning ("Cannot query for components: %s\n", ex_text);
		g_free (ex_text);
		CORBA_exception_free (&ev);
		return;
	}

	l = gnome_i18n_get_language_list("LC_MESSAGES");
	for (language_list=NULL;l;l=l->next)
		language_list = g_slist_append(language_list, l->data);

	for (i = 0; i < info_list->_length; i++) {
		const char *id;
		const char *label;
		const char *menu_label;
		const char *menu_accelerator;
		const char *alias;
		const char *icon_name;
		const char *sort_order_string;
		GdkPixbuf *icon = NULL, *menuicon = NULL;
		EComponentInfo *info;
		int sort_order;
		GNOME_Evolution_Component iface;

		id = info_list->_buffer[i].iid;
		iface = bonobo_activation_activate_from_id (id, 0, NULL, &ev);
		if (BONOBO_EX (&ev) || iface == CORBA_OBJECT_NIL) {
			char *ex_text = bonobo_exception_get_text (&ev);

			g_warning("Cannot activate '%s': %s\n", id, ex_text);
			g_free(ex_text);
			CORBA_exception_free(&ev);
			CORBA_exception_init(&ev);
			continue;
		}

		label = bonobo_server_info_prop_lookup (& info_list->_buffer[i], "evolution:button_label", language_list);
		if (label == NULL)
			label = g_strdup (_("Unknown"));

		menu_label = bonobo_server_info_prop_lookup (& info_list->_buffer[i], "evolution:menu_label", language_list);
		if (menu_label == NULL)
			menu_label = g_strdup (_("Unknown"));

		menu_accelerator = bonobo_server_info_prop_lookup (& info_list->_buffer[i], "evolution:menu_accelerator", language_list);

		alias = bonobo_server_info_prop_lookup (& info_list->_buffer[i], "evolution:component_alias", NULL);

		icon_name = bonobo_server_info_prop_lookup (& info_list->_buffer[i], "evolution:button_icon", NULL);
		if (icon_name) {
			icon = e_icon_factory_get_icon (icon_name, E_ICON_SIZE_LARGE_TOOLBAR);
			menuicon = e_icon_factory_get_icon (icon_name, E_ICON_SIZE_MENU);
		}

		sort_order_string = bonobo_server_info_prop_lookup (& info_list->_buffer[i],
								    "evolution:button_sort_order", NULL);
		if (sort_order_string == NULL)
			sort_order = 0;
		else
			sort_order = atoi (sort_order_string);

		info = component_info_new (id, iface, alias, label, menu_label,
					   menu_accelerator, sort_order, icon, menuicon);
		set_schemas (info, & info_list->_buffer [i]);

		registry->priv->infos = g_slist_prepend (registry->priv->infos, info);

		if (icon != NULL)
			g_object_unref (icon);
		bonobo_object_release_unref(iface, NULL);
	}
	g_slist_free(language_list);

	CORBA_free (info_list);
	CORBA_exception_free (&ev);

	registry->priv->infos = g_slist_sort (registry->priv->infos,
					      (GCompareFunc) component_info_compare_func);
}


/* GObject methods.  */

static void
impl_finalize (GObject *object)
{
	EComponentRegistry *component_registry;
	EComponentRegistryPrivate *priv;

	component_registry = E_COMPONENT_REGISTRY (object);
	priv = component_registry->priv;

	g_slist_foreach (priv->infos, (GFunc) component_info_free, NULL);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
class_init (EComponentRegistryClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_ref(PARENT_TYPE);
}


static void
init (EComponentRegistry *registry)
{
	registry->priv = g_new0 (EComponentRegistryPrivate, 1);
}


EComponentRegistry *
e_component_registry_new (void)
{
	return g_object_new (e_component_registry_get_type (), NULL);
}


GSList *
e_component_registry_peek_list (EComponentRegistry *registry)
{
	g_return_val_if_fail (E_IS_COMPONENT_REGISTRY (registry), NULL);

	query_components(registry);

	return registry->priv->infos;
}


EComponentInfo *
e_component_registry_peek_info (EComponentRegistry *registry,
				enum _EComponentRegistryField field,
				const char *key)
{
	GSList *p, *q;

	g_return_val_if_fail (E_IS_COMPONENT_REGISTRY (registry), NULL);

	query_components(registry);

	for (p = registry->priv->infos; p != NULL; p = p->next) {
		EComponentInfo *info = p->data;

		switch (field) {
		case ECR_FIELD_ID:
			if (strcmp (info->id, key) == 0)
				return info;
			break;
		case ECR_FIELD_ALIAS:
			if (strcmp (info->alias, key) == 0)
				return info;
			break;
		case ECR_FIELD_SCHEMA:
			for (q = info->uri_schemas; q != NULL; q = q->next)
				if (strcmp((char *)q->data, key) == 0)
					return info;
			break;
		}
	}

	return NULL;
}

GNOME_Evolution_Component
e_component_registry_activate (EComponentRegistry *registry,
			       const char *id,
			       CORBA_Environment  *ev)
{
	EComponentInfo *info;

	g_return_val_if_fail (E_IS_COMPONENT_REGISTRY (registry), CORBA_OBJECT_NIL);

	info = e_component_registry_peek_info (registry, ECR_FIELD_ID, id);
	if (info == NULL) {
		g_warning (G_GNUC_FUNCTION " - Unknown id \"%s\"", id);
		return CORBA_OBJECT_NIL;
	}

	/* it isn't in the registry unless it is already activated */
	return bonobo_object_dup_ref (info->iface, NULL);
}


E_MAKE_TYPE (e_component_registry, "EComponentRegistry", EComponentRegistry,
	     class_init, init, PARENT_TYPE)
