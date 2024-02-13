/*
 * e-startup-assistant.c
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

#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include <mail/e-mail-config-welcome-page.h>

#include "e-mail-config-import-page.h"
#include "e-mail-config-import-progress-page.h"

#include "e-startup-assistant.h"

#define NEW_COLLECTION_ACCOUNT_URI "evolution://new-collection-account"

struct _EStartupAssistantPrivate {
	EActivity *import_activity;
	EMailConfigImportPage *import_page;
	EMailConfigImportProgressPage *progress_page;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EStartupAssistant, e_startup_assistant, E_TYPE_MAIL_CONFIG_ASSISTANT, 0,
	G_ADD_PRIVATE_DYNAMIC (EStartupAssistant))

static gboolean
activate_collection_account_link_cb (GtkLabel *label,
				     const gchar *uri,
				     gpointer user_data)
{
	EStartupAssistant *assistant = user_data;
	EMailSession *session;
	GtkWindow *window;

	if (g_strcmp0 (uri, NEW_COLLECTION_ACCOUNT_URI) != 0)
		return FALSE;

	session = e_mail_config_assistant_get_session (E_MAIL_CONFIG_ASSISTANT (assistant));

	window = e_collection_account_wizard_new_window (
		gtk_window_get_transient_for (GTK_WINDOW (assistant)),
		e_mail_session_get_registry (session));

	gtk_widget_destroy (GTK_WIDGET (assistant));

	gtk_window_present (window);

	return TRUE;
}

static void
startup_assistant_import_done (GObject *source_object,
                               GAsyncResult *result,
                               gpointer user_data)
{
	EMailConfigImportPage *page;
	EStartupAssistant *assistant;
	EActivity *activity;
	GError *error = NULL;

	page = E_MAIL_CONFIG_IMPORT_PAGE (source_object);
	assistant = E_STARTUP_ASSISTANT (user_data);
	activity = assistant->priv->import_activity;

	e_mail_config_import_page_import_finish (page, result, &error);

	if (e_activity_handle_cancellation (activity, error)) {
		g_error_free (error);

	} else {
		/* XXX The current EImport API does not allow importers to
		 *     report errors.  Once we have a better importing API
		 *     we'll have to figure out how to show import errors,
		 *     but for now just emit a runtime warning. */
		if (error != NULL) {
			g_warning ("%s: %s", G_STRFUNC, error->message);
			g_error_free (error);
		}

		e_activity_set_percent (activity, 100.0);
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	g_object_unref (assistant);
}

static void
startup_assistant_dispose (GObject *object)
{
	EStartupAssistant *self = E_STARTUP_ASSISTANT (object);

	g_clear_object (&self->priv->import_activity);
	g_clear_object (&self->priv->import_page);
	g_clear_object (&self->priv->progress_page);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_startup_assistant_parent_class)->dispose (object);
}

static void
startup_assistant_constructed (GObject *object)
{
	EStartupAssistant *assistant;
	EMailConfigPage *page;
	gint n_pages, ii;

	assistant = E_STARTUP_ASSISTANT (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_startup_assistant_parent_class)->constructed (object);

	/* Note: We exclude this page if there is no application data
	 *       to import, but we don't know that until we create it. */
	page = e_mail_config_import_page_new ();
	if (e_mail_config_import_page_get_n_importers (
			E_MAIL_CONFIG_IMPORT_PAGE (page)) == 0) {
		g_object_unref (g_object_ref_sink (page));
	} else {
		e_mail_config_assistant_add_page (
			E_MAIL_CONFIG_ASSISTANT (assistant), page);
		assistant->priv->import_page = E_MAIL_CONFIG_IMPORT_PAGE (g_object_ref (page));

		/* Obviously we only need an import progress page if
		 * there's a chance we may be importing something. */
		page = e_mail_config_import_progress_page_new (
			assistant->priv->import_activity);
		e_mail_config_assistant_add_page (
			E_MAIL_CONFIG_ASSISTANT (assistant), page);
	}

	/* Additional tweaks. */

	n_pages = gtk_assistant_get_n_pages (GTK_ASSISTANT (assistant));
	for (ii = 0; ii < n_pages; ii++) {
		GtkWidget *nth_page, *checkbox, *label;
		GtkBox *main_box;
		GSettings *settings;
		gchar *text, *linkified;

		nth_page = gtk_assistant_get_nth_page (
			GTK_ASSISTANT (assistant), ii);

		if (!E_IS_MAIL_CONFIG_WELCOME_PAGE (nth_page))
			continue;

		gtk_assistant_set_page_title (
			GTK_ASSISTANT (assistant), nth_page, _("Welcome"));

		e_mail_config_welcome_page_set_text (
			E_MAIL_CONFIG_WELCOME_PAGE (nth_page),
			_("Welcome to Evolution.\n\nThe next few screens will "
			"allow Evolution to connect to your email accounts, "
			"and to import files from other applications."));

		main_box = e_mail_config_welcome_page_get_main_box (E_MAIL_CONFIG_WELCOME_PAGE (nth_page));
		settings = e_util_ref_settings ("org.gnome.evolution.mail");

		checkbox = gtk_check_button_new_with_mnemonic (_("Do not _show this wizard again"));
		gtk_widget_show (checkbox);

		g_settings_bind (settings, "show-startup-wizard",
			checkbox, "active",
			G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);

		gtk_box_pack_end (main_box, checkbox, FALSE, FALSE, 4);

		g_object_unref (settings);

		linkified = g_markup_printf_escaped ("<a href=\"" NEW_COLLECTION_ACCOUNT_URI "\">%s</a>",
			/* Translators: This is part of "Alternatively, you can %s (email, contacts and calendaring) instead." sentence from the same translation context. */
			C_("wizard-ca-note", "create a collection account"));
		/* Translators: The '%s' is replaced with "create a collection account" from the same translation context. */
		text = g_strdup_printf (C_("wizard-ca-note", "Alternatively, you can %s (email, contacts and calendaring) instead."), linkified);
		g_free (linkified);

		label = gtk_label_new (text);
		g_object_set (G_OBJECT (label),
			"hexpand", TRUE,
			"halign", GTK_ALIGN_START,
			"use-markup", TRUE,
			"visible", TRUE,
			"wrap", TRUE,
			"wrap-mode", PANGO_WRAP_WORD_CHAR,
			"xalign", 0.0,
			NULL);

		gtk_box_pack_end (main_box, label, FALSE, FALSE, 4);

		g_signal_connect (label, "activate-link",
			G_CALLBACK (activate_collection_account_link_cb), assistant);

		g_free (text);

		break;
	}
}

static void
startup_assistant_prepare (GtkAssistant *assistant,
                           GtkWidget *page)
{
	EStartupAssistant *self = E_STARTUP_ASSISTANT (assistant);

	/* Chain up to parent's prepare() method. */
	GTK_ASSISTANT_CLASS (e_startup_assistant_parent_class)->prepare (assistant, page);

	if (E_IS_MAIL_CONFIG_IMPORT_PROGRESS_PAGE (page)) {
		EActivity *activity;

		activity = self->priv->import_activity;
		e_activity_set_state (activity, E_ACTIVITY_RUNNING);

		e_mail_config_import_page_import (
			self->priv->import_page, activity,
			startup_assistant_import_done,
			g_object_ref (assistant));
	}
}

static void
e_startup_assistant_class_init (EStartupAssistantClass *class)
{
	GObjectClass *object_class;
	GtkAssistantClass *assistant_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = startup_assistant_dispose;
	object_class->constructed = startup_assistant_constructed;

	assistant_class = GTK_ASSISTANT_CLASS (class);
	assistant_class->prepare = startup_assistant_prepare;
}

static void
e_startup_assistant_class_finalize (EStartupAssistantClass *class)
{
}

static void
e_startup_assistant_init (EStartupAssistant *assistant)
{
	EActivity *activity;
	GCancellable *cancellable;

	assistant->priv = e_startup_assistant_get_instance_private (assistant);

	cancellable = g_cancellable_new ();

	activity = e_activity_new ();
	e_activity_set_cancellable (activity, cancellable);
	e_activity_set_state (activity, E_ACTIVITY_WAITING);
	assistant->priv->import_activity = activity;

	g_object_unref (cancellable);
}

void
e_startup_assistant_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_startup_assistant_register_type (type_module);
}

GtkWidget *
e_startup_assistant_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		E_TYPE_STARTUP_ASSISTANT,
		"session", session, NULL);
}

