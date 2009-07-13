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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-component-registry.h"

#include <glib/gi18n.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-exception.h>

#include <string.h>
#include <stdlib.h>

struct _EComponentRegistryPrivate {
	GSList *infos;

	guint init:1;
};

G_DEFINE_TYPE (EComponentRegistry, e_component_registry, G_TYPE_OBJECT)

/* EComponentInfo handling.  */

static EComponentInfo *
component_info_new (const gchar *id,
		    GNOME_Evolution_Component iface,
		    const gchar *alias,
		    const gchar *button_label,
		    const gchar *button_tooltips,
		    const gchar *menu_label,
		    const gchar *menu_accelerator,
                    const gchar *icon_name,
		    gint sort_order)
{
	EComponentInfo *info = g_new0 (EComponentInfo, 1);

	info->id = g_strdup (id);
	info->iface = bonobo_object_dup_ref(iface, NULL);
	info->alias = g_strdup (alias);
	info->button_label = g_strdup (button_label);
	info->button_tooltips = g_strdup (button_tooltips);
	info->menu_label = g_strdup (menu_label);
	info->menu_accelerator = g_strdup (menu_accelerator);
        info->icon_name = g_strdup (icon_name);
	info->sort_order = sort_order;

	return info;
}

static void
component_info_free (EComponentInfo *info)
{
	g_free (info->id);
	g_free (info->alias);
	g_free (info->button_label);
	g_free (info->button_tooltips);
	g_free (info->menu_label);
	g_free (info->menu_accelerator);

	if (info->iface != NULL)
		bonobo_object_release_unref (info->iface, NULL);

	g_slist_foreach (info->uri_schemas, (GFunc) g_free, NULL);
	g_slist_free (info->uri_schemas);

	g_free (info);
}

static gint
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
	gint i;

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
	const gchar * const *language_names;
	CORBA_Environment ev;
	GSList *languages = NULL;
	gchar *query;
	gint i;

	if (registry->priv->init)
		return;

	registry->priv->init = TRUE;

	CORBA_exception_init (&ev);
	query = g_strdup_printf ("repo_ids.has ('IDL:GNOME/Evolution/Component:%s')", BASE_VERSION);
	info_list = bonobo_activation_query (query, NULL, &ev);
	g_free (query);

	if (BONOBO_EX (&ev)) {
		gchar *ex_text = bonobo_exception_get_text (&ev);
		g_warning ("Cannot query for components: %s\n", ex_text);
		g_free (ex_text);
		CORBA_exception_free (&ev);
		return;
	}

	language_names = g_get_language_names ();
	while (*language_names != NULL)
		languages = g_slist_append (languages, (gpointer)(*language_names++));

	for (i = 0; i < info_list->_length; i++) {
		const gchar *id;
		const gchar *label;
		const gchar *menu_label;
		const gchar *menu_accelerator;
		const gchar *alias;
		const gchar *icon_name;
		const gchar *sort_order_string;
		const gchar *tooltips;
		EComponentInfo *info;
		gint sort_order;
		GNOME_Evolution_Component iface;

		id = info_list->_buffer[i].iid;
		iface = bonobo_activation_activate_from_id ((gchar *)id, 0, NULL, &ev);
		if (BONOBO_EX (&ev) || iface == CORBA_OBJECT_NIL) {
			gchar *ex_text = bonobo_exception_get_text (&ev);

			g_warning("Cannot activate '%s': %s\n", id, ex_text);
			g_free(ex_text);
			CORBA_exception_free(&ev);
			CORBA_exception_init(&ev);
			continue;
		}

		label = bonobo_server_info_prop_lookup (& info_list->_buffer[i], "evolution:button_label", languages);

		tooltips = bonobo_server_info_prop_lookup (& info_list->_buffer[i], "evolution:button_tooltips", languages);

		menu_label = bonobo_server_info_prop_lookup (& info_list->_buffer[i], "evolution:menu_label", languages);

		menu_accelerator = bonobo_server_info_prop_lookup (& info_list->_buffer[i], "evolution:menu_accelerator", languages);

		alias = bonobo_server_info_prop_lookup (& info_list->_buffer[i], "evolution:component_alias", NULL);

		icon_name = bonobo_server_info_prop_lookup (& info_list->_buffer[i], "evolution:button_icon", NULL);

		sort_order_string = bonobo_server_info_prop_lookup (& info_list->_buffer[i],
								    "evolution:button_sort_order", NULL);
		if (sort_order_string == NULL)
			sort_order = 0;
		else
			sort_order = atoi (sort_order_string);

		info = component_info_new (id, iface, alias, label, tooltips, menu_label,
					   menu_accelerator, icon_name, sort_order);
		set_schemas (info, & info_list->_buffer [i]);

		registry->priv->infos = g_slist_prepend (registry->priv->infos, info);

		bonobo_object_release_unref(iface, NULL);
	}
	g_slist_free(languages);

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

	(* G_OBJECT_CLASS (e_component_registry_parent_class)->finalize) (object);
}

static void
e_component_registry_class_init (EComponentRegistryClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = impl_finalize;
}

static void
e_component_registry_init (EComponentRegistry *registry)
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
				const gchar *key)
{
	GSList *p, *q;

	g_return_val_if_fail (E_IS_COMPONENT_REGISTRY (registry), NULL);

	query_components(registry);

	for (p = registry->priv->infos; p != NULL; p = p->next) {
		EComponentInfo *info = p->data;

		switch (field) {
		case ECR_FIELD_ID:
			if (info->id && (strcmp (info->id, key) == 0))
				return info;
			break;
		case ECR_FIELD_ALIAS:
			if (info->alias && (strcmp (info->alias, key) == 0))
				return info;
			break;
		case ECR_FIELD_SCHEMA:
			for (q = info->uri_schemas; q != NULL; q = q->next)
				if (strcmp((gchar *)q->data, key) == 0)
					return info;
			break;
		}
	}

	return NULL;
}

GNOME_Evolution_Component
e_component_registry_activate (EComponentRegistry *registry,
			       const gchar *id,
			       CORBA_Environment  *ev)
{
	EComponentInfo *info;

	g_return_val_if_fail (E_IS_COMPONENT_REGISTRY (registry), CORBA_OBJECT_NIL);

	info = e_component_registry_peek_info (registry, ECR_FIELD_ID, id);
	if (info == NULL) {
		g_warning ("%s - Unknown id \"%s\"", G_STRFUNC, id);
		return CORBA_OBJECT_NIL;
	}

	/* it isn't in the registry unless it is already activated */
	return bonobo_object_dup_ref (info->iface, NULL);
}
