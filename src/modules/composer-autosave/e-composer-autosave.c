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

#define AUTOSAVE_INTERVAL	60 /* seconds */

struct _EComposerAutosavePrivate {
	GCancellable *cancellable;
	guint timeout_id;

	GFile *malfunction_snapshot_file;
	gboolean editor_is_malfunction;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EComposerAutosave, e_composer_autosave, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (EComposerAutosave))

static void composer_autosave_changed_cb (EComposerAutosave *autosave);

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
		EHTMLEditor *editor;
		gchar *basename;

		if (G_IS_FILE (snapshot_file))
			basename = g_file_get_basename (snapshot_file);
		else
			basename = g_strdup (" ");

		editor = e_msg_composer_get_editor (composer);

		if (editor) {
			e_alert_submit (
				E_ALERT_SINK (editor),
				"mail-composer:no-autosave",
				basename, local_error->message, NULL);
		} else
			g_warning ("%s: %s", basename, local_error->message);

		g_free (basename);
		g_error_free (local_error);

		/* Re-schedule on error, maybe it'll work a bit later */
		composer_autosave_changed_cb (autosave);
	}

	g_object_unref (autosave);
}

static gboolean
composer_autosave_timeout_cb (gpointer user_data)
{
	EComposerAutosave *autosave;
	EExtensible *extensible;
	EMsgComposer *composer;

	autosave = E_COMPOSER_AUTOSAVE (user_data);

	if (autosave->priv->editor_is_malfunction) {
		autosave->priv->timeout_id = 0;
		return FALSE;
	}

	extensible = e_extension_get_extensible (E_EXTENSION (autosave));
	composer = E_MSG_COMPOSER (extensible);

	/* Do not do anything when it's busy */
	if (e_msg_composer_is_soft_busy (composer))
		return TRUE;

	/* Cancel the previous snapshot if it's still in
	 * progress and start a new snapshot operation. */
	g_cancellable_cancel (autosave->priv->cancellable);
	g_object_unref (autosave->priv->cancellable);
	autosave->priv->cancellable = g_cancellable_new ();

	autosave->priv->timeout_id = 0;

	e_composer_save_snapshot (
		composer,
		autosave->priv->cancellable,
		composer_autosave_finished_cb,
		g_object_ref (autosave));

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

	if (autosave->priv->timeout_id == 0 &&
	    !autosave->priv->editor_is_malfunction &&
	    e_content_editor_get_changed (cnt_editor)) {
		autosave->priv->timeout_id = e_named_timeout_add_seconds (
			AUTOSAVE_INTERVAL,
			composer_autosave_timeout_cb, autosave);
	}
}

static void
composer_autosave_editor_is_malfunction_cb (EComposerAutosave *autosave)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (autosave));

	editor = e_msg_composer_get_editor (E_MSG_COMPOSER (extensible));
	cnt_editor = e_html_editor_get_content_editor (editor);

	g_clear_object (&autosave->priv->malfunction_snapshot_file);
	autosave->priv->editor_is_malfunction = e_content_editor_is_malfunction (cnt_editor);

	if (autosave->priv->editor_is_malfunction) {
		e_composer_prevent_snapshot_file_delete (E_MSG_COMPOSER (extensible));
		autosave->priv->malfunction_snapshot_file = e_composer_get_snapshot_file (E_MSG_COMPOSER (extensible));
		if (autosave->priv->malfunction_snapshot_file)
			g_object_ref (autosave->priv->malfunction_snapshot_file);
	} else {
		e_composer_allow_snapshot_file_delete (E_MSG_COMPOSER (extensible));
		composer_autosave_changed_cb (autosave);
	}
}

static void
composer_autosave_recovered_cb (GObject *source_object,
				GAsyncResult *result,
				gpointer user_data)
{
	EMsgComposer *composer;
	GError *local_error = NULL;

	composer = e_composer_load_snapshot_finish (E_SHELL (source_object), result, &local_error);

	if (local_error != NULL) {
		/* FIXME Show an alert dialog here explaining
		 *       why we could not recover the message.
		 *       Will need a new error XML entry. */
		g_warn_if_fail (composer == NULL);
		g_warning ("%s: %s", G_STRFUNC, local_error->message);
		g_error_free (local_error);
	} else {
		gtk_widget_show (GTK_WIDGET (composer));
		g_object_unref (composer);
	}
}

static void
composer_autosave_msg_composer_before_destroy_cb (EMsgComposer *composer,
						  gpointer user_data)
{
	EComposerAutosave *autosave = user_data;

	g_return_if_fail (autosave != NULL);

	/* Cancel any snapshots in progress, composer is going to destroy its content. */
	g_cancellable_cancel (autosave->priv->cancellable);

	if (autosave->priv->malfunction_snapshot_file) {
		if (e_alert_run_dialog_for_args (GTK_WINDOW (composer), "mail-composer:recover-autosave-malfunction", NULL) == GTK_RESPONSE_YES) {
			e_composer_load_snapshot (
				e_msg_composer_get_shell (composer), autosave->priv->malfunction_snapshot_file, NULL,
				composer_autosave_recovered_cb, NULL);
		} else {
			g_file_delete (autosave->priv->malfunction_snapshot_file, NULL, NULL);
		}
	}
}

static void
composer_autosave_dispose (GObject *object)
{
	EComposerAutosave *self = E_COMPOSER_AUTOSAVE (object);

	/* Cancel any snapshots in progress. */
	g_cancellable_cancel (self->priv->cancellable);

	if (self->priv->timeout_id > 0) {
		g_source_remove (self->priv->timeout_id);
		self->priv->timeout_id = 0;
	}

	g_clear_object (&self->priv->cancellable);
	g_clear_object (&self->priv->malfunction_snapshot_file);

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

	g_signal_connect (
		extensible, "before-destroy",
		G_CALLBACK (composer_autosave_msg_composer_before_destroy_cb), object);

	e_signal_connect_notify_swapped (
		cnt_editor, "notify::is-malfunction",
		G_CALLBACK (composer_autosave_editor_is_malfunction_cb), object);

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
	autosave->priv = e_composer_autosave_get_instance_private (autosave);
	autosave->priv->cancellable = g_cancellable_new ();
	autosave->priv->malfunction_snapshot_file = NULL;
	autosave->priv->editor_is_malfunction = FALSE;
}

void
e_composer_autosave_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_composer_autosave_register_type (type_module);
}
