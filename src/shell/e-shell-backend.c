/*
 * e-shell-backend.c
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
 * Authors:
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 */

/**
 * SECTION: e-shell-backend
 * @short_description: dynamically loaded capabilities
 * @include: shell/e-shell-backend.h
 **/

#include "evolution-config.h"

#include "e-shell-backend.h"

#include <errno.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"

#include "e-shell.h"
#include "e-shell-view.h"

struct _EShellBackendPrivate {

	/* We keep a reference to corresponding EShellView subclass
	 * since it keeps a reference back to us.  This ensures the
	 * subclass is not finalized before we are, otherwise it
	 * would leak its EShellBackend reference. */
	EShellViewClass *shell_view_class;

	/* This tracks what the backend is busy doing. */
	GQueue *activities;

	gchar *config_dir;
	gchar *data_dir;
	gchar *prefer_new_item;

	/* This is set to delay shutdown. */
	gulong notify_busy_handler_id;

	guint started : 1;
};

enum {
	PROP_0,
	PROP_BUSY,
	PROP_PREFER_NEW_ITEM
};

enum {
	ACTIVITY_ADDED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (EShellBackend, e_shell_backend, E_TYPE_EXTENSION)

static void
shell_backend_debug_list_activities (EShellBackend *shell_backend)
{
	EShellBackendClass *class;
	GList *head, *link;
	guint n_activities;

	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
	g_return_if_fail (class != NULL);

	n_activities = g_queue_get_length (shell_backend->priv->activities);

	if (n_activities == 0)
		return;

	g_debug (
		"%u active '%s' %s:",
		n_activities, class->name,
		(n_activities == 1) ? "activity" : "activities");

	head = g_queue_peek_head_link (shell_backend->priv->activities);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EActivity *activity = E_ACTIVITY (link->data);
		gchar *description;
		const gchar *was;

		description = e_activity_describe (activity);
		was = e_activity_get_last_known_text (activity);

		if (description != NULL)
			g_debug ("* %s", description);
		else if (was != NULL)
			g_debug ("* (was \"%s\")", was);
		else
			g_debug ("* (no description)");

		g_free (description);
	}
}

static void
shell_backend_activity_finalized_cb (EShellBackend *shell_backend,
                                     EActivity *finalized_activity)
{
	g_queue_remove (shell_backend->priv->activities, finalized_activity);

	/* Only emit "notify::busy" when switching from busy to idle. */
	if (g_queue_is_empty (shell_backend->priv->activities))
		g_object_notify (G_OBJECT (shell_backend), "busy");

	g_object_unref (shell_backend);
}

static void
shell_backend_notify_busy_cb (EShellBackend *shell_backend,
                              GParamSpec *pspec,
                              EActivity *activity)
{
	shell_backend_debug_list_activities (shell_backend);

	if (!e_shell_backend_is_busy (shell_backend)) {
		/* Disconnecting this signal handler will unreference the
		 * EActivity and allow the shell to proceed with shutdown. */
		g_signal_handler_disconnect (
			shell_backend,
			shell_backend->priv->notify_busy_handler_id);
		shell_backend->priv->notify_busy_handler_id = 0;
	}
}

static void
shell_backend_prepare_for_quit_cb (EShell *shell,
                                   EActivity *activity,
                                   EShellBackend *shell_backend)
{
	shell_backend_debug_list_activities (shell_backend);

	if (e_shell_backend_is_busy (shell_backend)) {
		gulong handler_id;

		/* Referencing the EActivity delays shutdown; the
		 * reference count acts like a counting semaphore. */
		handler_id = g_signal_connect_data (
			shell_backend, "notify::busy",
			G_CALLBACK (shell_backend_notify_busy_cb),
			g_object_ref (activity),
			(GClosureNotify) g_object_unref, 0);
		shell_backend->priv->notify_busy_handler_id = handler_id;
	}
}

static GObject *
shell_backend_constructor (GType type,
                           guint n_construct_properties,
                           GObjectConstructParam *construct_properties)
{
	EShellBackend *shell_backend;
	EShellBackendClass *class;
	EShellViewClass *shell_view_class;
	EShell *shell;
	GObject *object;

	/* Chain up to parent's construct() method. */
	object = G_OBJECT_CLASS (e_shell_backend_parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);

	/* Install a reference to ourselves in the
	 * corresponding EShellViewClass structure. */
	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
	g_return_val_if_fail (class != NULL, object);

	shell_view_class = g_type_class_ref (class->shell_view_type);
	shell_view_class->shell_backend = g_object_ref (shell_backend);
	shell_backend->priv->shell_view_class = shell_view_class;

	g_signal_connect (
		shell, "prepare-for-quit",
		G_CALLBACK (shell_backend_prepare_for_quit_cb),
		shell_backend);

	return object;
}

static void
shell_backend_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BUSY:
			g_value_set_boolean (
				value, e_shell_backend_is_busy (
				E_SHELL_BACKEND (object)));
			return;

		case PROP_PREFER_NEW_ITEM:
			g_value_set_string (
				value,
				e_shell_backend_get_prefer_new_item (
				E_SHELL_BACKEND (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_backend_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREFER_NEW_ITEM:
			e_shell_backend_set_prefer_new_item (
				E_SHELL_BACKEND (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_backend_dispose (GObject *object)
{
	EShellBackend *self = E_SHELL_BACKEND (object);

	g_clear_pointer (&self->priv->shell_view_class, g_type_class_unref);

	if (self->priv->notify_busy_handler_id > 0) {
		g_signal_handler_disconnect (
			object, self->priv->notify_busy_handler_id);
		self->priv->notify_busy_handler_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_shell_backend_parent_class)->dispose (object);
}

static void
shell_backend_finalize (GObject *object)
{
	EShellBackend *self = E_SHELL_BACKEND (object);

	g_warn_if_fail (g_queue_is_empty (self->priv->activities));
	g_queue_free (self->priv->activities);

	g_free (self->priv->config_dir);
	g_free (self->priv->data_dir);
	g_free (self->priv->prefer_new_item);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_shell_backend_parent_class)->finalize (object);
}

static const gchar *
shell_backend_get_config_dir (EShellBackend *shell_backend)
{
	EShellBackendClass *class;

	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
	g_return_val_if_fail (class != NULL, NULL);

	/* Determine the user config directory for this backend. */
	if (G_UNLIKELY (shell_backend->priv->config_dir == NULL)) {
		const gchar *user_config_dir;

		user_config_dir = e_get_user_config_dir ();
		shell_backend->priv->config_dir =
			g_build_filename (user_config_dir, class->name, NULL);
		g_mkdir_with_parents (shell_backend->priv->config_dir, 0700);
	}

	return shell_backend->priv->config_dir;
}

static const gchar *
shell_backend_get_data_dir (EShellBackend *shell_backend)
{
	EShellBackendClass *class;

	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
	g_return_val_if_fail (class != NULL, NULL);

	/* Determine the user data directory for this backend. */
	if (G_UNLIKELY (shell_backend->priv->data_dir == NULL)) {
		const gchar *user_data_dir;

		user_data_dir = e_get_user_data_dir ();
		shell_backend->priv->data_dir =
			g_build_filename (user_data_dir, class->name, NULL);
		g_mkdir_with_parents (shell_backend->priv->data_dir, 0700);
	}

	return shell_backend->priv->data_dir;
}

static void
e_shell_backend_class_init (EShellBackendClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = shell_backend_constructor;
	object_class->get_property = shell_backend_get_property;
	object_class->set_property = shell_backend_set_property;
	object_class->dispose = shell_backend_dispose;
	object_class->finalize = shell_backend_finalize;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SHELL;

	class->get_config_dir = shell_backend_get_config_dir;
	class->get_data_dir = shell_backend_get_data_dir;

	/**
	 * EShellBackend:busy
	 *
	 * Whether any activities are still in progress.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_BUSY,
		g_param_spec_boolean (
			"busy",
			"Busy",
			"Whether any activities are still in progress",
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellBackend:prefer-new-item
	 *
	 * Name of an item to prefer in New toolbar button; can be NULL
	 **/
	g_object_class_install_property (
		object_class,
		PROP_PREFER_NEW_ITEM,
		g_param_spec_string (
			"prefer-new-item",
			"Prefer New Item",
			"Name of an item to prefer in New toolbar button",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

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
e_shell_backend_init (EShellBackend *shell_backend)
{
	shell_backend->priv = e_shell_backend_get_instance_private (shell_backend);
	shell_backend->priv->activities = g_queue_new ();
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
	EShellBackendClass *a_klass, *b_klass;
	gint aa, bb;

	a_klass = E_SHELL_BACKEND_GET_CLASS (shell_backend_a);
	b_klass = E_SHELL_BACKEND_GET_CLASS (shell_backend_b);

	g_return_val_if_fail (a_klass != NULL, 0);
	g_return_val_if_fail (b_klass != NULL, 0);

	aa = a_klass->sort_order;
	bb = b_klass->sort_order;

	return (aa < bb) ? -1 : (aa > bb) ? 1 : 0;
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
	EShellBackendClass *class;

	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), NULL);

	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->get_config_dir != NULL, NULL);

	return class->get_config_dir (shell_backend);
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
	EShellBackendClass *class;

	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), NULL);

	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->get_data_dir != NULL, NULL);

	return class->get_data_dir (shell_backend);
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
	EExtensible *extensible;

	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), NULL);

	extensible = e_extension_get_extensible (E_EXTENSION (shell_backend));

	return E_SHELL (extensible);
}

/**
 * e_shell_backend_add_activity:
 * @shell_backend: an #EShellBackend
 * @activity: an #EActivity
 *
 * Emits an #EShellBackend::activity-added signal and tracks the @activity
 * until it is finalized.
 **/
void
e_shell_backend_add_activity (EShellBackend *shell_backend,
                              EActivity *activity)
{
	EActivityState state;

	g_return_if_fail (E_IS_SHELL_BACKEND (shell_backend));
	g_return_if_fail (E_IS_ACTIVITY (activity));

	state = e_activity_get_state (activity);

	/* Disregard cancelled or completed activities. */

	if (state == E_ACTIVITY_CANCELLED)
		return;

	if (state == E_ACTIVITY_COMPLETED)
		return;

	g_queue_push_tail (shell_backend->priv->activities, activity);

	/* Emit the "activity-added" signal before adding a weak reference
	 * to the EActivity because EShellTaskbar's signal handler also adds
	 * a weak reference to the EActivity, and we want its GWeakNotify
	 * to run before ours, since ours may destroy the EShellTaskbar
	 * during application shutdown. */
	g_signal_emit (shell_backend, signals[ACTIVITY_ADDED], 0, activity);

	/* We reference the backend on every activity to
	 * guarantee the backend outlives the activity. */
	g_object_weak_ref (
		G_OBJECT (activity), (GWeakNotify)
		shell_backend_activity_finalized_cb,
		g_object_ref (shell_backend));

	/* Only emit "notify::busy" when switching from idle to busy. */
	if (g_queue_get_length (shell_backend->priv->activities) == 1)
		g_object_notify (G_OBJECT (shell_backend), "busy");
}

/**
 * e_shell_backend_is_busy:
 * @shell_backend: an #EShellBackend
 *
 * Returns %TRUE if any activities passed to e_shell_backend_add_activity()
 * are still in progress, %FALSE if the @shell_backend is currently idle.
 *
 * Returns: %TRUE if activities are still in progress
 **/
gboolean
e_shell_backend_is_busy (EShellBackend *shell_backend)
{
	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), FALSE);

	return !g_queue_is_empty (shell_backend->priv->activities);
}

/**
 * e_shell_backend_get_prefer_new_item:
 * @shell_backend: an #EShellBackend
 *
 * Returns: Name of a preferred item in New toolbar button, %NULL or
 * an empty string for no preference.
 *
 * Since: 3.4
 **/
const gchar *
e_shell_backend_get_prefer_new_item (EShellBackend *shell_backend)
{
	g_return_val_if_fail (shell_backend != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), NULL);

	return shell_backend->priv->prefer_new_item;
}

/**
 * e_shell_backend_set_prefer_new_item:
 * @shell_backend: an #EShellBackend
 * @prefer_new_item: name of an item
 *
 * Sets name of a preferred item in New toolbar button. Use %NULL or
 * an empty string for no preference.
 *
 * Since: 3.4
 **/
void
e_shell_backend_set_prefer_new_item (EShellBackend *shell_backend,
                                     const gchar *prefer_new_item)
{
	g_return_if_fail (shell_backend != NULL);
	g_return_if_fail (E_IS_SHELL_BACKEND (shell_backend));

	if (g_strcmp0 (shell_backend->priv->prefer_new_item, prefer_new_item) == 0)
		return;

	g_free (shell_backend->priv->prefer_new_item);
	shell_backend->priv->prefer_new_item = g_strdup (prefer_new_item);

	g_object_notify (G_OBJECT (shell_backend), "prefer-new-item");
}

/**
 * e_shell_backend_cancel_all:
 * @shell_backend: an #EShellBackend
 *
 * Cancels all activities passed to e_shell_backend_add_activity() that
 * have not already been finalized.  Note that an #EActivity can only be
 * cancelled if it was given a #GCancellable object.
 *
 * Also, assuming all activities are cancellable, there may still be a
 * delay before e_shell_backend_is_busy() returns %FALSE, because some
 * activities may not be able to respond to the cancellation request
 * immediately.  Connect to the "notify::busy" signal if you need
 * notification of @shell_backend becoming idle.
 **/
void
e_shell_backend_cancel_all (EShellBackend *shell_backend)
{
	GList *list, *iter;

	g_return_if_fail (E_IS_SHELL_BACKEND (shell_backend));

	list = g_queue_peek_head_link (shell_backend->priv->activities);

	for (iter = list; iter != NULL; iter = g_list_next (iter))
		e_activity_cancel (E_ACTIVITY (iter->data));
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
	g_return_if_fail (class != NULL);

	if (class->start != NULL)
		class->start (shell_backend);

	shell_backend->priv->started = TRUE;
}

/**
 * e_shell_backend_is_started:
 * @shell_backend: an #EShellBackend
 *
 * Returns whether e_shell_backend_start() was called for @shell_backend.
 *
 * Returns: whether @shell_backend is started
 **/
gboolean
e_shell_backend_is_started (EShellBackend *shell_backend)
{
	g_return_val_if_fail (E_IS_SHELL_BACKEND (shell_backend), FALSE);

	return shell_backend->priv->started;
}

/**
 * e_shell_backend_migrate:
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
	g_return_val_if_fail (class != NULL, TRUE);

	if (class->migrate == NULL)
		return TRUE;

	return class->migrate (shell_backend, major, minor, micro, error);
}
