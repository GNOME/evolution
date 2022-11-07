/*
 * evolution-startup-wizard.c
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
#include <libebackend/libebackend.h>

#include <shell/e-shell.h>
#include <shell/e-shell-window.h>

#include <mail/e-mail-backend.h>
#include <mail/e-mail-config-assistant.h>
#include <mail/e-mail-config-welcome-page.h>

#include "e-startup-assistant.h"
#include "e-mail-config-import-page.h"
#include "e-mail-config-import-progress-page.h"

/* Standard GObject macros */
#define E_TYPE_STARTUP_WIZARD \
	(e_startup_wizard_get_type ())
#define E_STARTUP_WIZARD(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_STARTUP_WIZARD, EStartupWizard))

typedef struct _EStartupWizard EStartupWizard;
typedef struct _EStartupWizardClass EStartupWizardClass;

struct _EStartupWizard {
	EExtension parent;

	gboolean proceeded;
};

struct _EStartupWizardClass {
	EExtensionClass parent_class;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_startup_wizard_get_type (void);

G_DEFINE_DYNAMIC_TYPE (EStartupWizard, e_startup_wizard, E_TYPE_EXTENSION)

static EShell *
startup_wizard_get_shell (EStartupWizard *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return E_SHELL (extensible);
}

static GtkWidget *
startup_wizard_new_assistant (EStartupWizard *extension)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;

	shell = startup_wizard_get_shell (extension);
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	/* Note: We subclass EMailConfigAssistant so we can distinguish
	 *       the first-time account assistant from the normal account
	 *       assistant.  The backup-restore module relies on this to
	 *       add a "Restore" page to the first-time assistant only. */
	return e_startup_assistant_new (session);
}

static gboolean
startup_wizard_have_mail_account (EStartupWizard *extension)
{
	EShell *shell;
	ESourceRegistry *registry;
	GList *list, *link;
	guint skip_sources = 0;
	const gchar *extension_name;
	gboolean have_account;

	shell = startup_wizard_get_shell (extension);

	registry = e_shell_get_registry (shell);
	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;

	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link; link = g_list_next (link)) {
		ESource *source = link->data;
		ESourceMailAccount *mail_account = e_source_get_extension (source, extension_name);

		/* Exclude the built-in, 'On This Computer' and 'Search Folders' sources. */
		if (e_source_mail_account_get_builtin (mail_account) ||
		    g_strcmp0 (e_source_get_uid (source), E_MAIL_SESSION_LOCAL_UID) == 0 ||
		    g_strcmp0 (e_source_get_uid (source), E_MAIL_SESSION_VFOLDER_UID) == 0)
			skip_sources++;
	}

	have_account = g_list_length (list) > skip_sources;

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	return have_account;
}

static gboolean
startup_wizard_run_idle_cb (gpointer user_data)
{
	EStartupWizard *extension = user_data;
	EShell *shell;
	GtkWidget *window;

	/* Accounts should now be loaded if there were any to load.
	 * Check, and proceed with the Evolution Setup Assistant. */

	if (startup_wizard_have_mail_account (extension))
		return FALSE;

	shell = startup_wizard_get_shell (extension);
	window = startup_wizard_new_assistant (extension);

	gtk_window_set_transient_for (GTK_WINDOW (window), e_shell_get_active_window (shell));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (window), TRUE);

	gtk_widget_show (window);

	return FALSE;
}

static void
startup_wizard_load_accounts_done (GMainLoop *loop,
                                   EActivity *activity,
                                   gboolean is_last_ref)
{
	/* All asynchronous account loading operations should
	 * be complete now, so we can terminate the main loop. */
	if (is_last_ref)
		g_main_loop_quit (loop);
}

static void
startup_wizard_load_accounts (EStartupWizard *extension)
{
	EShell *shell;
	EActivity *activity;
	GMainContext *context;
	GMainLoop *loop;
	GSource *source;

	/* This works similar to the offline and shutdown procedure in
	 * EShell.  We broadcast a "load-accounts" EShell event with an
	 * EActivity.  The EActivity has a toggle reference which we use
	 * as a counting semaphore.  If another module needs to handle
	 * the event asynchronously, it should reference the EActivity
	 * until its async operation completes, then drop the reference.
	 * Once the signal handlers finish and only the toggle reference
	 * remains, we then proceed with the Evolution Setup Assistant. */

	shell = startup_wizard_get_shell (extension);

	/* Start a temporary main loop so asynchronous account loading
	 * operations can signal completion from an idle callback.  We push
	 * our own GMainContext as the thread-default so we don't trigger
	 * other GSources that have already been attached to the current
	 * thread-default context, such as the idle callback in main.c. */
	context = g_main_context_new ();
	loop = g_main_loop_new (context, TRUE);
	g_main_context_push_thread_default (context);

	activity = e_activity_new ();
	e_activity_set_text (activity, _("Loading accounts…"));

	/* Drop our normal (non-toggle) EActivity reference from an
	 * idle callback.  If nothing else references the EActivity
	 * then it will be a very short-lived main loop. */
	source = g_idle_source_new ();
	g_source_set_callback (
		source, (GSourceFunc) gtk_false,
		activity, (GDestroyNotify) g_object_unref);
	g_source_attach (source, context);
	g_source_unref (source);

	/* Add a toggle reference to the EActivity which,
	 * when triggered, will terminate the main loop. */
	g_object_add_toggle_ref (
		G_OBJECT (activity), (GToggleNotify)
		startup_wizard_load_accounts_done, loop);

	/* Broadcast the "load-accounts" event. */
	e_shell_event (shell, "load-accounts", activity);

	/* And now we wait... */
	g_main_loop_run (loop);

	/* Increment the reference count so we can safely emit
	 * a signal without triggering the toggle reference. */
	g_object_ref (activity);

	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

	g_object_remove_toggle_ref (
		G_OBJECT (activity), (GToggleNotify)
		startup_wizard_load_accounts_done, loop);

	/* Finalize the activity. */
	g_object_unref (activity);

	/* Finalize the main loop. */
	g_main_loop_unref (loop);

	/* Pop our GMainContext off the thread-default stack. */
	g_main_context_pop_thread_default (context);
	g_main_context_unref (context);
}

static void
startup_wizard_notify_active_view_cb (EShellWindow *shell_window,
				      GParamSpec *param,
				      EStartupWizard *extension)
{
	if (extension->proceeded) {
		g_signal_handlers_disconnect_by_data (shell_window, extension);
		return;
	}

	if (g_strcmp0 ("mail", e_shell_window_get_active_view (shell_window)) == 0) {
		g_signal_handlers_disconnect_by_data (shell_window, extension);
		g_signal_handlers_disconnect_by_data (startup_wizard_get_shell (extension), extension);

		extension->proceeded = TRUE;

		if (gtk_widget_get_realized (GTK_WIDGET (shell_window)))
			startup_wizard_run_idle_cb (extension);
		else
			g_idle_add (startup_wizard_run_idle_cb, extension);
	}
}

static void
startup_wizard_window_added_cb (EStartupWizard *extension,
				GtkWindow *window,
				EShell *shell)
{
	if (extension->proceeded) {
		g_signal_handlers_disconnect_by_data (shell, extension);
		return;
	}

	if (E_IS_SHELL_WINDOW (window)) {
		EShellWindow *shell_window = E_SHELL_WINDOW (window);

		if (g_strcmp0 ("mail", e_shell_window_get_active_view (shell_window)) == 0) {
			startup_wizard_notify_active_view_cb (shell_window, NULL, extension);
		} else {
			g_signal_connect (window, "notify::active-view",
				G_CALLBACK (startup_wizard_notify_active_view_cb), extension);
		}
	}
}

static void
startup_wizard_constructed (GObject *object)
{
	EShell *shell;
	EStartupWizard *extension;
	GSettings *settings;

	extension = E_STARTUP_WIZARD (object);
	shell = startup_wizard_get_shell (extension);

	g_signal_connect_swapped (
		shell, "event::ready-to-start",
		G_CALLBACK (startup_wizard_load_accounts), extension);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	extension->proceeded = !g_settings_get_boolean (settings, "show-startup-wizard");
	g_object_unref (settings);

	if (!extension->proceeded) {
		g_signal_connect_swapped (
			shell, "window-added",
			G_CALLBACK (startup_wizard_window_added_cb), extension);
	}

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_startup_wizard_parent_class)->constructed (object);
}

static void
e_startup_wizard_class_init (EStartupWizardClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = startup_wizard_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SHELL;
}

static void
e_startup_wizard_class_finalize (EStartupWizardClass *class)
{
}

static void
e_startup_wizard_init (EStartupWizard *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_startup_wizard_register_type (type_module);
	e_startup_assistant_type_register (type_module);
	e_mail_config_import_page_type_register (type_module);
	e_mail_config_import_progress_page_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

