/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-module.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-shell-module.h"

#include <gmodule.h>
#include <glib/gi18n.h>

/* This is the symbol we look for when loading a module. */
#define INIT_SYMBOL	"e_shell_module_init"

#define E_SHELL_MODULE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_MODULE, EShellModulePrivate))

struct _EShellModulePrivate {

	/* Set during module initialization.  This must come
	 * first in the struct so the registry can read it. */
	EShellModuleInfo info;

	GModule *module;
	gchar *filename;

	/* Initializes the loaded module. */
	void (*init) (GTypeModule *module);
};

enum {
	PROP_0,
	PROP_FILENAME
};

static gpointer parent_class;

static void
shell_module_set_filename (EShellModule *shell_module,
                           const gchar *filename)
{
	g_free (shell_module->priv->filename);
	shell_module->priv->filename = g_strdup (filename);
}

static void
shell_module_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILENAME:
			shell_module_set_filename (
				E_SHELL_MODULE (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_module_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILENAME:
			g_value_set_string (
				value, e_shell_module_get_filename (
				E_SHELL_MODULE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_module_finalize (GObject *object)
{
	EShellModulePrivate *priv;

	priv = E_SHELL_MODULE_GET_PRIVATE (object);

	g_free (priv->filename);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
shell_module_load (GTypeModule *type_module)
{
	EShellModulePrivate *priv;
	gpointer symbol;

	priv = E_SHELL_MODULE_GET_PRIVATE (type_module);

	g_return_val_if_fail (priv->filename != NULL, FALSE);
	priv->module = g_module_open (priv->filename, 0);

	if (priv->module == NULL)
		goto fail;

	if (!g_module_symbol (priv->module, INIT_SYMBOL, &symbol))
		goto fail;

	priv->init = symbol;
	priv->init (type_module);

	return TRUE;

fail:
	g_warning ("%s", g_module_error ());

	if (priv->module != NULL)
		g_module_close (priv->module);

	return FALSE;
}

static void
shell_module_unload (GTypeModule *type_module)
{
	EShellModulePrivate *priv;

	priv = E_SHELL_MODULE_GET_PRIVATE (type_module);

	g_module_close (priv->module);
	priv->module = NULL;

	priv->init = NULL;
}

static void
shell_module_class_init (EShellModuleClass *class)
{
	GObjectClass *object_class;
	GTypeModuleClass *type_module_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellModulePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_module_set_property;
	object_class->get_property = shell_module_get_property;
	object_class->finalize = shell_module_finalize;

	type_module_class = G_TYPE_MODULE_CLASS (class);
	type_module_class->load = shell_module_load;
	type_module_class->unload = shell_module_unload;

	g_object_class_install_property (
		object_class,
		PROP_FILENAME,
		g_param_spec_string (
			"filename",
			_("Filename"),
			_("The filename of the module"),
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
shell_module_init (EShellModule *shell_module)
{
	shell_module->priv = E_SHELL_MODULE_GET_PRIVATE (shell_module);
}

GType
e_shell_module_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EShellModuleClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) shell_module_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellModule),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_module_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_TYPE_MODULE, "EShellModule", &type_info, 0);
	}

	return type;
}

EShellModule *
e_shell_module_new (const gchar *filename)
{
	g_return_val_if_fail (filename != NULL, NULL);

	return g_object_new (E_TYPE_SHELL_MODULE, "filename", filename, NULL);
}

gint
e_shell_module_compare (EShellModule *shell_module_a,
                        EShellModule *shell_module_b)
{
	gint a = shell_module_a->priv->info.sort_order;
	gint b = shell_module_b->priv->info.sort_order;

	return (a < b) ? -1 : (a > b);
}

const gchar *
e_shell_module_get_filename (EShellModule *shell_module)
{
	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), NULL);

	return shell_module->priv->filename;
}

GType
e_shell_module_get_view_type (EShellModule *shell_module)
{
	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), 0);

	return shell_module->priv->info.shell_view_type;
}

void
e_shell_module_window_created (EShellModule *shell_module,
                               EShellWindow *shell_window)
{
	g_return_if_fail (E_IS_SHELL_MODULE (shell_module));
	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	if (shell_module->priv->info.window_created != NULL)
		shell_module->priv->info.window_created (shell_window);
}

void
e_shell_module_set_info (EShellModule *shell_module,
                         const EShellModuleInfo *info)
{
	g_return_if_fail (E_IS_SHELL_MODULE (shell_module));
	g_return_if_fail (info != NULL);

	shell_module->priv->info.sort_order = info->sort_order;
	shell_module->priv->info.aliases = g_intern_string (info->aliases);
	shell_module->priv->info.schemas = g_intern_string (info->schemas);
	shell_module->priv->info.shell_view_type = info->shell_view_type;
}
