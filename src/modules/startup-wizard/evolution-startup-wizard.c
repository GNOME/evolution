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

G_GNUC_NORETURN static void
startup_wizard_terminate (void)
{
	gtk_main_quit ();
	_exit (0);
}

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
	ESource *source;
	ESourceRegistry *registry;
	GList *list, *link;
	const gchar *extension_name;
	const gchar *uid;
	gboolean have_account;

	shell = startup_wizard_get_shell (extension);

	registry = e_shell_get_registry (shell);
	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;

	list = e_source_registry_list_sources (registry, extension_name);

	/* Exclude the built-in 'On This Computer' source. */
	uid = E_MAIL_SESSION_LOCAL_UID;
	source = e_source_registry_ref_source (registry, uid);
	link = g_list_find (list, source);
	if (link != NULL) {
		/* We have two references to the ESource,
		 * one from e_source_registry_list_sources()
		 * and one from e_source_registry_ref_source().
		 * Drop them both. */
		g_object_unref (source);
		g_object_unref (source);
		list = g_list_delete_link (list, link);
	}

	/* Exclude the built-in 'Search Folders' source. */
	uid = E_MAIL_SESSION_VFOLDER_UID;
	source = e_source_registry_ref_source (registry, uid);
	link = g_list_find (list, source);
	if (link != NULL) {
		/* We have two references to the ESource,
		 * one from e_source_registry_list_sources()
		 * and one from e_source_registry_ref_source().
		 * Drop them both. */
		g_object_unref (source);
		g_object_unref (source);
		list = g_list_delete_link (list, link);
	}

	have_account = (list != NULL);

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	return have_account;
}

static void
startup_wizard_weak_ref_cb (gpointer data,
                            GObject *where_the_object_was)
{
	gtk_main_quit ();
}

static void
startup_wizard_run (EStartupWizard *extension)
{
	GtkWidget *window = NULL;

	/* Accounts should now be loaded if there were any to load.
	 * Check, and proceed with the Evolution Setup Assistant. */

	if (startup_wizard_have_mail_account (extension))
		return;

	if (window == NULL) {
		window = startup_wizard_new_assistant (extension);
		g_signal_connect (
			window, "cancel",
			G_CALLBACK (startup_wizard_terminate), NULL);
	}

	g_object_weak_ref (
		G_OBJECT (window),
		startup_wizard_weak_ref_cb, NULL);

	gtk_widget_show (window);

	gtk_main ();
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
	e_activity_set_text (activity, _("Loading accounts..."));

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

	/* Proceed with the Evolution Setup Assistant. */
	startup_wizard_run (extension);
}

static void
startup_wizard_constructed (GObject *object)
{
	EShell *shell;
	EStartupWizard *extension;

	extension = E_STARTUP_WIZARD (object);
	shell = startup_wizard_get_shell (extension);

	g_signal_connect_swapped (
		shell, "event::ready-to-start",
		G_CALLBACK (startup_wizard_load_accounts), extension);

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

