/*
 * e-shell-backend.c
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

#include "e-shell-backend.h"

#include <errno.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"

#include "e-shell.h"
#include "e-shell-view.h"

#define E_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_BACKEND, EShellBackendPrivate))

struct _EShellBackendPrivate {

	gpointer shell;  /* weak pointer */

	/* We keep a reference to corresponding EShellView subclass
	 * since it keeps a reference back to us.  This ensures the
	 * subclass is not finalized before we are, otherwise it
	 * would leak its EShellBackend reference. */
	EShellViewClass *shell_view_class;

	gchar *config_dir;
	gchar *data_dir;

	guint started	: 1;
};

enum {
	PROP_0,
	PROP_SHELL
};

enum {
	ACTIVITY_ADDED,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

static void
shell_backend_set_shell (EShellBackend *shell_backend,
                         EShell *shell)
{
	g_return_if_fail (shell_backend->priv->shell == NULL);

	shell_backend->priv->shell = shell;

	g_object_add_weak_pointer (
		G_OBJECT (shell_backend),
		&shell_backend->priv->shell);
}

static void
shell_backend_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL:
			shell_backend_set_shell (
				E_SHELL_BACKEND (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_backend_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL:
			g_value_set_object (
				value, e_shell_backend_get_shell (
				E_SHELL_BACKEND (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_backend_dispose (GObject *object)
{
	EShellBackendPrivate *priv;

	priv = E_SHELL_BACKEND_GET_PRIVATE (object);

	if (priv->shell != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->shell), &priv->shell);
		priv->shell = NULL;
	}

	if (priv->shell_view_class != NULL) {
		g_type_class_unref (priv->shell_view_class);
		priv->shell_view_class = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_backend_finalize (GObject *object)
{
	EShellBackendPrivate *priv;

	priv = E_SHELL_BACKEND_GET_PRIVATE (object);

	g_free (priv->data_dir);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
shell_backend_class_init (EShellBackendClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellBackendPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_backend_set_property;
	object_class->get_property = shell_backend_get_property;
	object_class->dispose = shell_backend_dispose;
	object_class->finalize = shell_backend_finalize;

	/**
	 * EShellBackend:shell
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
	 * EShellBackend::activity-added
	 * @shell_backend: the #EShellBackend that emitted the signal
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
shell_backend_init (EShellBackend *shell_backend,
                    EShellBackendClass *class)
{
	EShellViewClass *shell_view_class;
	gchar *dirname;

	shell_backend->priv = E_SHELL_BACKEND_GET_PRIVATE (shell_backend);

	/* Install a reference to ourselves in the corresponding
	 * EShellViewClass structure, */
	shell_view_class = g_type_class_ref (class->shell_view_type);
	shell_view_class->shell_backend = g_object_ref (shell_backend);
	shell_backend->priv->shell_view_class = shell_view_class;

	/* Determine the user data directory for this backend. */
	shell_backend->priv->data_dir = g_build_filename (
		e_get_user_data_dir (), class->name, NULL);

	/* Determine the user configuration directory for this backend. */
	shell_backend->priv->config_dir = g_build_filename (
		shell_backend->priv->data_dir, "config", NULL);

	/* Create the user configuration directory for this backend,
	 * which should also create the user data directory. */
	dirname = shell_backend->priv->config_dir;
	if (g_mkdir_with_parents (dirname, 0777) != 0)
		g_critical (
			"Cannot create directory %s: %s",
			dirname, g_strerror (errno));
}

GType
e_shell_backend_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EShellBackendClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) shell_backend_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellBackend),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_backend_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EShellBackend", &type_info, 0);
	}

	return type;
}

/**
 * e_shell_backend_compare:
 * @shell_backend_a: an #EShellBackend
 * @shell_backend_b: an #EShellBackend
 *
 * Using the <structfield>sort_order</structfield> field from both backends'
 * #EShellBackendClass, compares @shell_backend_a with @shell_mobule_b and
 * returns -1, 0 or +1 if @shell_backend_a is found to be less than, equal
 * to or greater than @shell_backend_b, respectively.
 *
 * Returns: -1, 0 or +1, for a less than, equal to or greater than result
 **/
gint
e_shell_backend_compare (EShellBackend *shell_backend_a,
                         EShellBackend *shell_backend_b)
{
	gint a = E_SHELL_BACKEND_GET_CLASS (shell_backend_a)->sort_order;
	gint b = E_SHELL_BACKEND_GET_CLASS (shell_backend_b)->sort_order;

	return (a < b) ? -1 : (a > b);
}

/**
 * e_shell_backend_get_config_dir:
 * @shell_backend: an #EShellBackend
 *
 * Returns the absolute path to the configuration directory for
 * @shell_backend.  The string is owned by @shell_backend and should
 * not be modified or freed.
 *
 * Returns: the backend's configuration directory
 **/
const gchar *
e_shell_backend_get_config_dir (EShellBackend *shell_backend)
{
	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), NULL);
	g_return_val_if_fail (shell_backend->priv->config_dir != NULL, NULL);

	return shell_backend->priv->config_dir;
}

/**
 * e_shell_backend_get_data_dir:
 * @shell_backend: an #EShellBackend
 *
 * Returns the absolute path to the data directory for @shell_backend.
 * The string is owned by @shell_backend and should not be modified or
 * freed.
 *
 * Returns: the backend's data directory
 **/
const gchar *
e_shell_backend_get_data_dir (EShellBackend *shell_backend)
{
	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), NULL);
	g_return_val_if_fail (shell_backend->priv->data_dir != NULL, NULL);

	return shell_backend->priv->data_dir;
}

/**
 * e_shell_backend_get_shell:
 * @shell_backend: an #EShellBackend
 *
 * Returns the #EShell singleton.
 *
 * Returns: the #EShell
 **/
EShell *
e_shell_backend_get_shell (EShellBackend *shell_backend)
{
	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), NULL);

	return E_SHELL (shell_backend->priv->shell);
}

/**
 * e_shell_backend_add_activity:
 * @shell_backend: an #EShellBackend
 * @activity: an #EActivity
 *
 * Emits an #EShellBackend::activity-added signal.
 **/
void
e_shell_backend_add_activity (EShellBackend *shell_backend,
                              EActivity *activity)
{
	g_return_if_fail (E_IS_SHELL_BACKEND (shell_backend));
	g_return_if_fail (E_IS_ACTIVITY (activity));

	g_signal_emit (shell_backend, signals[ACTIVITY_ADDED], 0, activity);
}

/**
 * e_shell_backend_start:
 * @shell_backend: an #EShellBackend
 *
 * Tells the @shell_backend to begin loading data or running background
 * tasks which may consume significant resources.  This gets called in
 * reponse to the user switching to the corresponding #EShellView for
 * the first time.  The function is idempotent for each @shell_backend.
 **/
void
e_shell_backend_start (EShellBackend *shell_backend)
{
	EShellBackendClass *class;

	g_return_if_fail (E_IS_SHELL_BACKEND (shell_backend));

	if (shell_backend->priv->started)
		return;

	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);

	if (class->start != NULL)
		class->start (shell_backend);

	shell_backend->priv->started = TRUE;
}

/**
 * e_shell_migrate:
 * @shell_backend: an #EShellBackend
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
e_shell_backend_migrate (EShellBackend *shell_backend,
                        gint major,
                        gint minor,
                        gint micro,
                        GError **error)
{
	EShellBackendClass *class;

	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), TRUE);

	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);

	if (class->migrate == NULL)
		return TRUE;

	return class->migrate (shell_backend, major, minor, micro, error);
}
