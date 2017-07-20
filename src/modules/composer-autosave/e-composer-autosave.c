/*
 * e-composer-autosave.c
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

#include "e-composer-autosave.h"

#include <composer/e-msg-composer.h>

#include "e-autosave-utils.h"

#define E_COMPOSER_AUTOSAVE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_COMPOSER_AUTOSAVE, EComposerAutosavePrivate))

#define AUTOSAVE_INTERVAL	60 /* seconds */

struct _EComposerAutosavePrivate {
	GCancellable *cancellable;
	guint timeout_id;

	/* Prevent error dialogs from piling up. */
	gboolean error_shown;
};

G_DEFINE_DYNAMIC_TYPE (
	EComposerAutosave,
	e_composer_autosave,
	E_TYPE_EXTENSION)

static void
composer_autosave_finished_cb (GObject *source_object,
                               GAsyncResult *result,
                               gpointer user_data)
{
	EMsgComposer *composer;
	EComposerAutosave *autosave;
	GFile *snapshot_file;
	GError *local_error = NULL;

	composer = E_MSG_COMPOSER (source_object);
	autosave = E_COMPOSER_AUTOSAVE (user_data);

	snapshot_file = e_composer_get_snapshot_file (composer);
	e_composer_save_snapshot_finish (composer, result, &local_error);

	/* Return silently if we were cancelled. */
	if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		g_error_free (local_error);

	else if (local_error != NULL) {
		gchar *basename;

		if (G_IS_FILE (snapshot_file))
			basename = g_file_get_basename (snapshot_file);
		else
			basename = g_strdup (" ");

		/* Only show one error dialog at a time. */
		if (!autosave->priv->error_shown) {
			autosave->priv->error_shown = TRUE;
			e_alert_run_dialog_for_args (
				GTK_WINDOW (composer),
				"mail-composer:no-autosave",
				basename, local_error->message, NULL);
			autosave->priv->error_shown = FALSE;
		} else
			g_warning ("%s: %s", basename, local_error->message);

		g_free (basename);
		g_error_free (local_error);
	}

	g_object_unref (autosave);
}

static gboolean
composer_autosave_timeout_cb (gpointer user_data)
{
	EComposerAutosave *autosave;
	EExtensible *extensible;

	autosave = E_COMPOSER_AUTOSAVE (user_data);
	extensible = e_extension_get_extensible (E_EXTENSION (autosave));

	/* Cancel the previous snapshot if it's still in
	 * progress and start a new snapshot operation. */
	g_cancellable_cancel (autosave->priv->cancellable);
	g_object_unref (autosave->priv->cancellable);
	autosave->priv->cancellable = g_cancellable_new ();

	e_composer_save_snapshot (
		E_MSG_COMPOSER (extensible),
		autosave->priv->cancellable,
		composer_autosave_finished_cb,
		g_object_ref (autosave));

	autosave->priv->timeout_id = 0;

	return FALSE;
}

static void
composer_autosave_changed_cb (EComposerAutosave *autosave)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (autosave));

	editor = e_msg_composer_get_editor (E_MSG_COMPOSER (extensible));
	cnt_editor = e_html_editor_get_content_editor (editor);

	if (autosave->priv->timeout_id == 0 && e_content_editor_get_changed (cnt_editor)) {
		autosave->priv->timeout_id = e_named_timeout_add_seconds (
			AUTOSAVE_INTERVAL,
			composer_autosave_timeout_cb, autosave);
	}
}

static void
composer_autosave_dispose (GObject *object)
{
	EComposerAutosavePrivate *priv;

	priv = E_COMPOSER_AUTOSAVE_GET_PRIVATE (object);

	/* Cancel any snapshots in progress. */
	g_cancellable_cancel (priv->cancellable);

	if (priv->timeout_id > 0) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	g_clear_object (&priv->cancellable);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_composer_autosave_parent_class)->dispose (object);
}

static void
composer_autosave_constructed (GObject *object)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	EExtensible *extensible;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_composer_autosave_parent_class)->constructed (object);

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	editor = e_msg_composer_get_editor (E_MSG_COMPOSER (extensible));
	cnt_editor = e_html_editor_get_content_editor (editor);

	/* Do not use e_signal_connect_notify_swapped() here,
	   this module relies on "false" change notifications. */
	g_signal_connect_swapped (
		cnt_editor, "notify::changed",
		G_CALLBACK (composer_autosave_changed_cb), object);

	g_signal_connect_swapped (
		cnt_editor, "content-changed",
		G_CALLBACK (composer_autosave_changed_cb), object);
}

static void
e_composer_autosave_class_init (EComposerAutosaveClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (EComposerAutosavePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = composer_autosave_dispose;
	object_class->constructed = composer_autosave_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MSG_COMPOSER;
}

static void
e_composer_autosave_class_finalize (EComposerAutosaveClass *class)
{
}

static void
e_composer_autosave_init (EComposerAutosave *autosave)
{
	autosave->priv = E_COMPOSER_AUTOSAVE_GET_PRIVATE (autosave);
	autosave->priv->cancellable = g_cancellable_new ();
}

void
e_composer_autosave_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_composer_autosave_register_type (type_module);
}
