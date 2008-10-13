/*
 * e-shell-module.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-shell-module.h"

#include <errno.h>
#include <gmodule.h>
#include <glib/gi18n.h>
#include <e-util/e-util.h>

#include <e-shell.h>

/* This is the symbol we look for when loading a module. */
#define INIT_SYMBOL	"e_shell_module_init"

#define E_SHELL_MODULE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_MODULE, EShellModulePrivate))

struct _EShellModulePrivate {

	/* Set during module initialization.  This must
	 * come first in the struct so EShell can read it. */
	EShellModuleInfo info;

	GModule *module;
	gchar *filename;

	EShell *shell;
	gchar *config_dir;
	gchar *data_dir;

	GType shell_view_type;

	/* Initializes the loaded type module. */
	void (*init) (GTypeModule *type_module);
};

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_SHELL
};

enum {
	ACTIVITY_ADDED,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

static void
shell_module_set_filename (EShellModule *shell_module,
                           const gchar *filename)
{
	g_return_if_fail (shell_module->priv->filename == NULL);
	shell_module->priv->filename = g_strdup (filename);
}

static void
shell_module_set_shell (EShellModule *shell_module,
                        EShell *shell)
{
	g_return_if_fail (shell_module->priv->shell == NULL);
	shell_module->priv->shell = g_object_ref (shell);
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

		case PROP_SHELL:
			shell_module_set_shell (
				E_SHELL_MODULE (object),
				g_value_get_object (value));
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

		case PROP_SHELL:
			g_value_set_object (
				value, e_shell_module_get_shell (
				E_SHELL_MODULE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_module_dispose (GObject *object)
{
	EShellModulePrivate *priv;

	priv = E_SHELL_MODULE_GET_PRIVATE (object);

	if (priv->shell != NULL) {
		g_object_unref (priv->shell);
		priv->shell = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_module_finalize (GObject *object)
{
	EShellModulePrivate *priv;

	priv = E_SHELL_MODULE_GET_PRIVATE (object);

	g_free (priv->filename);
	g_free (priv->data_dir);

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
	object_class->dispose = shell_module_dispose;
	object_class->finalize = shell_module_finalize;

	type_module_class = G_TYPE_MODULE_CLASS (class);
	type_module_class->load = shell_module_load;
	type_module_class->unload = shell_module_unload;

	/**
	 * EShellModule:filename
	 *
	 * The filename of the module.
	 **/
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

	/**
	 * EShellModule:shell
	 *
	 * The #EShell singleton.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SHELL,
		g_param_spec_object (
			"shell",
			_("Shell"),
			_("The EShell singleton"),
			E_TYPE_SHELL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	/**
	 * EShellModule::activity-added
	 * @shell_module: the #EShellModule that emitted the signal
	 * @activity: an #EActivity
	 *
	 * Broadcasts a newly added activity.
	 **/
	signals[ACTIVITY_ADDED] = g_signal_new (
		"activity-added",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_ACTIVITY);
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

/**
 * e_shell_module_new:
 * @shell: an #EShell
 * @filename: the name of the file containing the shell module
 *
 * Loads @filename as a #GTypeModule and tries to invoke a module
 * function named <function>e_shell_module_init</function>, passing
 * the newly loaded #GTypeModule as an argument.  The shell module is
 * responsible for defining such a function to perform the appropriate
 * initialization steps.
 *
 * Returns: a new #EShellModule
 **/
EShellModule *
e_shell_module_new (EShell *shell,
                    const gchar *filename)
{
	g_return_val_if_fail (filename != NULL, NULL);

	return g_object_new (
		E_TYPE_SHELL_MODULE, "shell", shell,
		"filename", filename, NULL);
}

/**
 * e_shell_module_compare:
 * @shell_module_a: an #EShellModule
 * @shell_module_b: an #EShellModule
 *
 * Using the <structfield>sort_order</structfield> field from both modules'
 * #EShellModuleInfo, compares @shell_module_a with @shell_mobule_b and
 * returns -1, 0 or +1 if @shell_module_a is found to be less than, equal
 * to or greater than @shell_module_b, respectively.
 *
 * Returns: -1, 0 or +1, for a less than, equal to or greater than result
 **/
gint
e_shell_module_compare (EShellModule *shell_module_a,
                        EShellModule *shell_module_b)
{
	gint a = shell_module_a->priv->info.sort_order;
	gint b = shell_module_b->priv->info.sort_order;

	return (a < b) ? -1 : (a > b);
}

/**
 * e_shell_module_get_config_dir:
 * @shell_module: an #EShellModule
 *
 * Returns the absolute path to the configuration directory for
 * @shell_module.  The string is owned by @shell_module and should
 * not be modified or freed.
 *
 * Returns: the module's configuration directory
 **/
const gchar *
e_shell_module_get_config_dir (EShellModule *shell_module)
{
	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), NULL);
	g_return_val_if_fail (shell_module->priv->config_dir != NULL, NULL);

	return shell_module->priv->config_dir;
}

/**
 * e_shell_module_get_data_dir:
 * @shell_module: an #EShellModule
 *
 * Returns the absolute path to the data directory for @shell_module.
 * The string is owned by @shell_module and should not be modified or
 * freed.
 *
 * Returns: the module's data directory
 **/
const gchar *
e_shell_module_get_data_dir (EShellModule *shell_module)
{
	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), NULL);
	g_return_val_if_fail (shell_module->priv->data_dir != NULL, NULL);

	return shell_module->priv->data_dir;
}

/**
 * e_shell_module_get_filename:
 * @shell_module: an #EShellModule
 *
 * Returns the name of the file from which @shell_module was loaded.
 * The string is owned by @shell_module and should not be modified or
 * freed.
 *
 * Returns: the module's file name
 **/
const gchar *
e_shell_module_get_filename (EShellModule *shell_module)
{
	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), NULL);

	return shell_module->priv->filename;
}

/**
 * e_shell_module_get_shell:
 * @shell_module: an #EShellModule
 *
 * Returns the #EShell that was passed to e_shell_module_new().
 *
 * Returns: the #EShell
 **/
EShell *
e_shell_module_get_shell (EShellModule *shell_module)
{
	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), NULL);

	return shell_module->priv->shell;
}

/**
 * e_shell_module_get_shell_view_type:
 * @shell_module: an #EShellModule
 *
 * Returns the #GType of the #EShellView subclass for @shell_module.
 *
 * Returns: the #GType of an #EShellView subclass
 **/
GType
e_shell_module_get_shell_view_type (EShellModule *shell_module)
{
	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), 0);

	return shell_module->priv->shell_view_type;
}

/**
 * e_shell_module_add_activity:
 * @shell_module: an #EShellModule
 * @activity: an #EActivity
 *
 * Emits an #EShellModule::activity-added signal.
 **/
void
e_shell_module_add_activity (EShellModule *shell_module,
                             EActivity *activity)
{
	g_return_if_fail (E_IS_SHELL_MODULE (shell_module));
	g_return_if_fail (E_IS_ACTIVITY (activity));

	g_signal_emit (shell_module, signals[ACTIVITY_ADDED], 0, activity);
}

/**
 * e_shell_module_is_busy:
 * @shell_module: an #EShellModule
 *
 * Returns %TRUE if @shell_module is busy and cannot be shutdown at
 * present.  Each module must define what "busy" means to them and
 * determine an appropriate response.
 *
 * XXX This function is likely to change or disappear.  I'm toying with
 *     the idea of just having it check whether there are any unfinished
 *     #EActivity<!-- -->'s left, so we have a consistent and easily
 *     testable definition of what "busy" means.
 *
 * Returns: %TRUE if the module is busy
 **/
gboolean
e_shell_module_is_busy (EShellModule *shell_module)
{
	EShellModuleInfo *module_info;

	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), FALSE);

	module_info = &shell_module->priv->info;

	if (module_info->is_busy != NULL)
		return module_info->is_busy (shell_module);

	return FALSE;
}

/**
 * e_shell_module_shutdown:
 * @shell_module: an #EShellModule
 *
 * Alerts @shell_module to begin shutdown procedures.  If the module is
 * busy and cannot immediately shut down, the function returns %FALSE.
 * A %TRUE response implies @shell_module has successfully shut down.
 *
 * XXX This function is likely to change or disappear.  I'm toying with
 *     the idea of just having it check whether there are any unfinished
 *     #EActivity<!-- -->'s left, so we have a consistent and easily
 *     testable definition of what "busy" means.
 *
 * Returns: %TRUE if the module has shut down, %FALSE if the module is
 *          busy and cannot immediately shut down
 */
gboolean
e_shell_module_shutdown (EShellModule *shell_module)
{
	EShellModuleInfo *module_info;

	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), TRUE);

	module_info = &shell_module->priv->info;

	if (module_info->shutdown != NULL)
		return module_info->shutdown (shell_module);

	return TRUE;
}

/**
 * e_shell_migrate:
 * @shell_module: an #EShellModule
 * @major: major part of version to migrate from
 * @minor: minor part of version to migrate from
 * @micro: micro part of version to migrate from
 * @error: return location for a #GError, or %NULL
 *
 * Attempts to migrate data and settings from version %major.%minor.%micro.
 * Returns %TRUE if the migration was successful or if no action was
 * necessary.  Returns %FALSE and sets %error if the migration failed.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
e_shell_module_migrate (EShellModule *shell_module,
                        gint major,
                        gint minor,
                        gint micro,
                        GError **error)
{
	EShellModuleInfo *module_info;

	g_return_val_if_fail (E_IS_SHELL_MODULE (shell_module), TRUE);

	module_info = &shell_module->priv->info;

	if (module_info->migrate != NULL)
		return module_info->migrate (
			shell_module, major, minor, micro, error);

	return TRUE;
}

/**
 * e_shell_module_set_info:
 * @shell_module: an #EShellModule
 * @info: an #EShellModuleInfo
 * @shell_view_type: the #GType of a #EShellView subclass
 *
 * Registers basic configuration information about @shell_module that
 * the #EShell can use for processing command-line arguments.
 *
 * Configuration information should be registered from
 * @shell_module<!-- -->'s <function>e_shell_module_init</function>
 * initialization function.  See e_shell_module_new() for more information.
 **/
void
e_shell_module_set_info (EShellModule *shell_module,
                         const EShellModuleInfo *info,
                         GType shell_view_type)
{
	GTypeModule *type_module;
	EShellModuleInfo *module_info;
	const gchar *pathname;

	g_return_if_fail (E_IS_SHELL_MODULE (shell_module));
	g_return_if_fail (info != NULL);

	type_module = G_TYPE_MODULE (shell_module);
	module_info = &shell_module->priv->info;

	/* A module name is required. */
	g_return_if_fail (info->name != NULL);
	module_info->name = g_intern_string (info->name);
	g_type_module_set_name (type_module, module_info->name);

	module_info->aliases = g_intern_string (info->aliases);
	module_info->schemes = g_intern_string (info->schemes);
	module_info->sort_order = info->sort_order;

	module_info->is_busy = info->is_busy;
	module_info->shutdown = info->shutdown;
	module_info->migrate = info->migrate;

	/* Determine the user data directory for this module. */
	g_free (shell_module->priv->data_dir);
	shell_module->priv->data_dir = g_build_filename (
		e_get_user_data_dir (), module_info->name, NULL);

	/* Determine the user configuration directory for this module. */
	g_free (shell_module->priv->config_dir);
	shell_module->priv->config_dir = g_build_filename (
		shell_module->priv->data_dir, "config", NULL);

	/* Create the user configuration directory for this module,
	 * which should also create the user data directory. */
	pathname = shell_module->priv->config_dir;
	if (g_mkdir_with_parents (pathname, 0777) != 0)
		g_critical (
			"Cannot create directory %s: %s",
			pathname, g_strerror (errno));

	shell_module->priv->shell_view_type = shell_view_type;
}
