/*
 * e-plugin-lib.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-plugin-lib.h"

#ifdef G_OS_WIN32
#include <libedataserver/libedataserver.h> /* for e_util_replace_prefix() */
#include "e-util/e-util-private.h"
#endif

#include <string.h>

static gpointer parent_class;
static GType plugin_lib_type;

/* TODO:
 * We need some way to manage lifecycle.
 * We need some way to manage state.
 *
 * Maybe just the g module init method will do, or we could add
 * another which returns context.
 *
 * There is also the question of per-instance context, e.g. for config
 * pages.
 */

static gint
plugin_lib_loadmodule (EPlugin *plugin)
{
	EPluginLib *plugin_lib = E_PLUGIN_LIB (plugin);
	EPluginLibEnableFunc enable;
	gboolean found_symbol;

	if (plugin_lib->module != NULL)
		return 0;

	if (plugin_lib->location == NULL) {
		plugin->enabled = FALSE;
		g_warning ("Location not set in plugin '%s'", plugin->name);
		return -1;
	}

	plugin_lib->module = g_module_open (plugin_lib->location, 0);

	if (plugin_lib->module == NULL) {
		plugin->enabled = FALSE;
		g_warning (
			"can't load plugin '%s': %s",
			plugin_lib->location, g_module_error ());
		return -1;
	}

	/* If the plugin is disabled, we're done. */
	if (!plugin->enabled)
		return 0;

	found_symbol = g_module_symbol (
		plugin_lib->module,
		"e_plugin_lib_enable",
		(gpointer) &enable);

	if (found_symbol) {
		if (enable (plugin, TRUE) != 0) {
			plugin->enabled = FALSE;
			g_module_close (plugin_lib->module);
			plugin_lib->module = NULL;
			return -1;
		}
	}

	return 0;
}

static gpointer
plugin_lib_get_symbol (EPlugin *plugin,
                       const gchar *name)
{
	EPluginLib *plugin_lib = E_PLUGIN_LIB (plugin);
	gpointer symbol;

	if (plugin_lib_loadmodule (plugin) != 0)
		return NULL;

	if (!g_module_symbol (plugin_lib->module, name, &symbol))
		return NULL;

	return symbol;
}

static gpointer
plugin_lib_invoke (EPlugin *plugin,
                   const gchar *name,
                   gpointer data)
{
	EPluginLib *plugin_lib = E_PLUGIN_LIB (plugin);
	EPluginLibFunc func;

	if (!plugin->enabled) {
		g_warning (
			"Trying to invoke '%s' on disabled plugin '%s'",
			name, plugin->id);
		return NULL;
	}

	func = plugin_lib_get_symbol (plugin, name);

	if (func == NULL) {
		g_warning (
			"Cannot resolve symbol '%s' in plugin '%s' "
			"(not exported?)", name, plugin_lib->location);
		return NULL;
	}

	return func (plugin, data);
}

static gint
plugin_lib_construct (EPlugin *plugin,
                      xmlNodePtr root)
{
	EPluginLib *plugin_lib = E_PLUGIN_LIB (plugin);

	/* Set the location before chaining up, as some EPluginHooks
	 * will cause the module to load during hook construction. */

	plugin_lib->location = e_plugin_xml_prop (root, "location");

	if (plugin_lib->location == NULL) {
		g_warning ("Library plugin '%s' has no location", plugin->id);
		return -1;
	}
#ifdef G_OS_WIN32
	{
		gchar *mapped_location =
			e_util_replace_prefix (
				BUILDTIME_EVOLUTION_PLUGINDIR,
				EVOLUTION_PLUGINDIR,
				plugin_lib->location);
		g_free (plugin_lib->location);
		plugin_lib->location = mapped_location;
	}
#endif

	/* Chain up to parent's construct() method. */
	if (E_PLUGIN_CLASS (parent_class)->construct (plugin, root) == -1)
		return -1;

	/* If we're enabled, check for the load-on-startup property */
	if (plugin->enabled) {
		xmlChar *tmp;

		tmp = xmlGetProp (root, (const guchar *)"load-on-startup");
		if (tmp) {
			if (plugin_lib_loadmodule (plugin) != 0) {
				xmlFree (tmp);
				return -1;
			}
			xmlFree (tmp);
		}
	}

	return 0;
}

static GtkWidget *
plugin_lib_get_configure_widget (EPlugin *plugin)
{
	EPluginLibGetConfigureWidgetFunc get_configure_widget;

	get_configure_widget = plugin_lib_get_symbol (
		plugin, "e_plugin_lib_get_configure_widget");

	if (get_configure_widget != NULL)
		return get_configure_widget (plugin);

	return NULL;
}

static void
plugin_lib_enable (EPlugin *plugin,
                   gint state)
{
	EPluginLib *plugin_lib = E_PLUGIN_LIB (plugin);
	EPluginLibEnableFunc enable;

	E_PLUGIN_CLASS (parent_class)->enable (plugin, state);

	/* if we're disabling and it isn't loaded, nothing to do */
	if (!state && plugin_lib->module == NULL)
		return;

	enable = plugin_lib_get_symbol (plugin, "e_plugin_lib_enable");

	if (enable != NULL)
		enable (plugin, state);
}

static void
plugin_lib_finalize (GObject *object)
{
	EPluginLib *plugin_lib = E_PLUGIN_LIB (object);

	g_free (plugin_lib->location);

	if (plugin_lib->module)
		g_module_close (plugin_lib->module);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
plugin_lib_class_init (EPluginClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = plugin_lib_finalize;

	class->construct = plugin_lib_construct;
	class->invoke = plugin_lib_invoke;
	class->get_symbol = plugin_lib_get_symbol;
	class->enable = plugin_lib_enable;
	class->get_configure_widget = plugin_lib_get_configure_widget;
	class->type = "shlib";
}

GType
e_plugin_lib_get_type (void)
{
	return plugin_lib_type;
}

void
e_plugin_lib_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EPluginLibClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) plugin_lib_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EPluginLib),
		0,     /* n_preallocs */
		(GInstanceInitFunc) NULL,
		NULL   /* value_table */
	};

	plugin_lib_type = g_type_module_register_type (
		type_module, E_TYPE_PLUGIN,
		"EPluginLib", &type_info, 0);
}
