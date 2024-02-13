/*
 * e-composer-registry.c
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
 */

#include "evolution-config.h"

#include <glib/gstdio.h>
#include <libebackend/libebackend.h>

#include <shell/e-shell.h>
#include <shell/e-shell-window.h>
#include <composer/e-msg-composer.h>

#include "e-autosave-utils.h"

#include "e-composer-registry.h"

struct _EComposerRegistryPrivate {
	GQueue composers;
	gboolean orphans_restored;
	gulong map_event_handler_id;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EComposerRegistry, e_composer_registry, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (EComposerRegistry))

static void
composer_registry_recovered_cb (GObject *source_object,
                                GAsyncResult *result,
                                gpointer user_data)
{
	EMsgComposer *composer;
	EComposerRegistry *registry;
	GError *local_error = NULL;

	registry = E_COMPOSER_REGISTRY (user_data);

	composer = e_composer_load_snapshot_finish (
		E_SHELL (source_object), result, &local_error);

	if (local_error != NULL) {
		/* FIXME Show an alert dialog here explaining
		 *       why we could not recover the message.
		 *       Will need a new error XML entry. */
		g_warn_if_fail (composer == NULL);
		g_warning ("%s: %s", G_STRFUNC, local_error->message);
		g_error_free (local_error);
		goto exit;
	}

	gtk_widget_show (GTK_WIDGET (composer));

	g_object_unref (composer);

exit:
	g_object_unref (registry);
}

static void
composer_registry_restore_orphans (EComposerRegistry *registry,
                                   GtkWindow *parent)
{
	EExtensible *extensible;
	GList *orphans;
	gint response;
	GError *local_error = NULL;

	extensible = e_extension_get_extensible (E_EXTENSION (registry));

	/* Look for orphaned auto-save files. */
	orphans = e_composer_find_orphans (
		&registry->priv->composers, &local_error);
	if (orphans == NULL) {
		if (local_error != NULL) {
			g_warning ("%s", local_error->message);
			g_error_free (local_error);
		}
		return;
	}

	/* Ask if the user wants to recover the orphaned files. */
	response = e_alert_run_dialog_for_args (
		parent, "mail-composer:recover-autosave", NULL);

	/* Based on the user's reponse, recover or delete them. */
	while (orphans != NULL) {
		GFile *file = orphans->data;

		if (response == GTK_RESPONSE_YES)
			e_composer_load_snapshot (
				E_SHELL (extensible),
				file, NULL,
				composer_registry_recovered_cb,
				g_object_ref (registry));
		else
			g_file_delete (file, NULL, NULL);

		g_object_unref (file);

		orphans = g_list_delete_link (orphans, orphans);
	}
}

static gboolean
composer_registry_map_event_cb (GtkWindow *parent,
                                GdkEvent *event,
                                EComposerRegistry *registry)
{
	composer_registry_restore_orphans (registry, parent);
	registry->priv->orphans_restored = TRUE;

	/* This is a one-time-only signal handler.
	 * Disconnect from subsequent map events. */
	g_signal_handler_disconnect (
		parent, registry->priv->map_event_handler_id);
	registry->priv->map_event_handler_id = 0;

	return FALSE;
}

static void
composer_registry_notify_cb (EComposerRegistry *registry,
                             GObject *where_the_object_was)
{
	/* Remove the finalized composer from the registry. */
	g_queue_remove (&registry->priv->composers, where_the_object_was);

	g_object_unref (registry);
}

static void
composer_registry_window_added_cb (GtkApplication *application,
                                   GtkWindow *window,
                                   EComposerRegistry *registry)
{
	/* Offer to restore any orphaned auto-save files from the
	 * previous session once the first EShellWindow is mapped. */
	if (E_IS_SHELL_WINDOW (window) && !registry->priv->orphans_restored) {
		gulong handler_id;

		handler_id = g_signal_connect (
			window, "map-event",
			G_CALLBACK (composer_registry_map_event_cb),
			registry);
		registry->priv->map_event_handler_id = handler_id;

	/* Track the new composer window. */
	} else if (E_IS_MSG_COMPOSER (window)) {
		g_queue_push_tail (&registry->priv->composers, window);
		g_object_weak_ref (
			G_OBJECT (window), (GWeakNotify)
			composer_registry_notify_cb,
			g_object_ref (registry));
	}
}

static void
composer_registry_finalize (GObject *object)
{
	EComposerRegistry *self = E_COMPOSER_REGISTRY (object);

	/* All composers should have been finalized by now. */
	g_warn_if_fail (g_queue_is_empty (&self->priv->composers));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_composer_registry_parent_class)->finalize (object);
}

static void
composer_registry_constructed (GObject *object)
{
	EExtensible *extensible;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_composer_registry_parent_class)->constructed (object);

	extensible = e_extension_get_extensible (E_EXTENSION (object));

	/* Listen for new watched windows. */
	g_signal_connect (
		extensible, "window-added",
		G_CALLBACK (composer_registry_window_added_cb),
		object);
}

static void
e_composer_registry_class_init (EComposerRegistryClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = composer_registry_finalize;
	object_class->constructed = composer_registry_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SHELL;
}

static void
e_composer_registry_class_finalize (EComposerRegistryClass *class)
{
}

static void
e_composer_registry_init (EComposerRegistry *registry)
{
	registry->priv = e_composer_registry_get_instance_private (registry);
}

void
e_composer_registry_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_composer_registry_register_type (type_module);
}
