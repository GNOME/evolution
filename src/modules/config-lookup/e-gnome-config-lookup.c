/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "mail/e-mail-autoconfig.h"
#include "e-util/e-util.h"

#include "e-gnome-config-lookup.h"

/* Standard GObject macros */
#define E_TYPE_GNOME_CONFIG_LOOKUP \
	(e_gnome_config_lookup_get_type ())
#define E_GNOME_CONFIG_LOOKUP(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_GNOME_CONFIG_LOOKUP, EGnomeConfigLookup))
#define E_GNOME_CONFIG_LOOKUP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_GNOME_CONFIG_LOOKUP, EGnomeConfigLookupClass))
#define E_IS_GNOME_CONFIG_LOOKUP(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_GNOME_CONFIG_LOOKUP))
#define E_IS_GNOME_CONFIG_LOOKUP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_GNOME_CONFIG_LOOKUP))
#define E_GNOME_CONFIG_LOOKUP_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_GNOME_CONFIG_LOOKUP, EGnomeConfigLookupClass))

typedef struct _EGnomeConfigLookup EGnomeConfigLookup;
typedef struct _EGnomeConfigLookupClass EGnomeConfigLookupClass;

struct _EGnomeConfigLookup {
	EExtension parent;
};

struct _EGnomeConfigLookupClass {
	EExtensionClass parent_class;
};

GType e_gnome_config_lookup_get_type (void) G_GNUC_CONST;

G_DEFINE_DYNAMIC_TYPE (EGnomeConfigLookup, e_gnome_config_lookup, E_TYPE_EXTENSION)

static void
gnome_config_lookup_thread (EConfigLookup *config_lookup,
			    const ENamedParameters *params,
			    gpointer user_data,
			    GCancellable *cancellable)
{
	EMailAutoconfig *mail_autoconfig;
	ESourceRegistry *registry;
	const gchar *email_address;

	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (params != NULL);

	registry = e_config_lookup_get_registry (config_lookup);
	email_address = e_named_parameters_get (params, E_CONFIG_LOOKUP_PARAM_EMAIL_ADDRESS);

	if (!email_address || !*email_address)
		return;

	mail_autoconfig = e_mail_autoconfig_new_sync (registry, email_address, cancellable, NULL);
	if (mail_autoconfig)
		e_mail_autoconfig_copy_results_to_config_lookup (mail_autoconfig, config_lookup);

	g_clear_object (&mail_autoconfig);
}

static void
gnome_config_lookup_run_cb (EConfigLookup *config_lookup,
			    const ENamedParameters *params,
			    EActivity *activity,
			    gpointer user_data)
{
	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (E_IS_GNOME_CONFIG_LOOKUP (user_data));
	g_return_if_fail (E_IS_ACTIVITY (activity));

	e_config_lookup_create_thread (config_lookup, params, activity,
		gnome_config_lookup_thread, NULL, NULL);
}

static void
gnome_config_lookup_constructed (GObject *object)
{
	EConfigLookup *config_lookup;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_gnome_config_lookup_parent_class)->constructed (object);

	config_lookup = E_CONFIG_LOOKUP (e_extension_get_extensible (E_EXTENSION (object)));

	g_signal_connect (config_lookup, "run",
		G_CALLBACK (gnome_config_lookup_run_cb), object);
}

static void
e_gnome_config_lookup_class_init (EGnomeConfigLookupClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = gnome_config_lookup_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CONFIG_LOOKUP;
}

static void
e_gnome_config_lookup_class_finalize (EGnomeConfigLookupClass *class)
{
}

static void
e_gnome_config_lookup_init (EGnomeConfigLookup *extension)
{
}

void
e_gnome_config_lookup_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_gnome_config_lookup_register_type (type_module);
}
