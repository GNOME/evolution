/*
 * evolution-backup-restore.c
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

#include <unistd.h>
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

#include <shell/e-shell-utils.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>

#include <mail/e-mail-config-assistant.h>

#include "e-mail-config-restore-page.h"
#include "e-mail-config-restore-ready-page.h"

#ifdef G_OS_WIN32
#ifdef localtime_r
#undef localtime_r
#endif
/* The localtime() in Microsoft's C library *is* thread-safe */
#define localtime_r(timep, result) \
	(localtime (timep) ? memcpy ( \
	(result), localtime (timep), sizeof (*(result))) : 0)
#endif

typedef EExtension EvolutionBackupRestoreAssistant;
typedef EExtensionClass EvolutionBackupRestoreAssistantClass;

typedef EExtension EvolutionBackupRestoreMenuItems;
typedef EExtensionClass EvolutionBackupRestoreMenuItemsClass;

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType evolution_backup_restore_assistant_get_type (void);
GType evolution_backup_restore_menu_items_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EvolutionBackupRestoreAssistant,
	evolution_backup_restore_assistant,
	E_TYPE_EXTENSION)

G_DEFINE_DYNAMIC_TYPE (
	EvolutionBackupRestoreMenuItems,
	evolution_backup_restore_menu_items,
	E_TYPE_EXTENSION)

enum {
	BR_OK = 1 << 0,
	BR_START = 1 << 1
};

static void
backup (const gchar *filename,
        gboolean restart)
{
	if (restart)
		execl (
			EVOLUTION_TOOLSDIR "/evolution-backup",
			"evolution-backup",
			"--gui",
			"--backup",
			"--restart",
			filename,
			NULL);
	else
		execl (
			EVOLUTION_TOOLSDIR "/evolution-backup",
			"evolution-backup",
			"--gui",
			"--backup",
			filename,
			NULL);
}

static void
restore (const gchar *filename,
         gboolean restart)
{
	if (restart)
		execl (
			EVOLUTION_TOOLSDIR "/evolution-backup",
			"evolution-backup",
			"--gui",
			"--restore",
			"--restart",
			filename,
			NULL);
	else
		execl (
			EVOLUTION_TOOLSDIR "/evolution-backup",
			"evolution-backup",
			"--gui",
			"--restore",
			filename,
			NULL);
}

static guint32
dialog_prompt_user (GtkWindow *parent,
                    const gchar *string,
                    const gchar *tag,
                    ...)
{
	GtkWidget *dialog;
	GtkWidget *check = NULL;
	GtkWidget *container;
	va_list ap;
	gint button;
	guint32 mask = 0;
	EAlert *alert = NULL;

	va_start (ap, tag);
	alert = e_alert_new_valist (tag, ap);
	va_end (ap);

	dialog = e_alert_dialog_new (parent, alert);
	g_object_unref (alert);

	container = e_alert_dialog_get_content_area (E_ALERT_DIALOG (dialog));

	check = gtk_check_button_new_with_mnemonic (string);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);
	gtk_box_pack_start (GTK_BOX (container), check, FALSE, FALSE, 0);
	gtk_widget_show (check);

	button = gtk_dialog_run (GTK_DIALOG (dialog));

	if (button == GTK_RESPONSE_YES)
		mask |= BR_OK;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)))
		mask |= BR_START;

	gtk_widget_destroy (dialog);

	return mask;
}

static void
file_chooser_filter_changed_cb (GtkFileChooser *file_chooser,
				GParamSpec *param,
				gpointer user_data)
{
	GtkFileFilter *filter;
	const gchar *filter_name, *extension = NULL;
	gchar *current_name;

	g_return_if_fail (GTK_IS_FILE_CHOOSER (file_chooser));

	filter = gtk_file_chooser_get_filter (file_chooser);
	if (!filter)
		return;

	filter_name = gtk_file_filter_get_name (filter);
	if (g_strcmp0 (filter_name, "*.tar.xz") == 0) {
		extension = ".tar.xz";
	} else if (g_strcmp0 (filter_name, "*.tar.gz") == 0) {
		extension = ".tar.gz";
	}

	if (!extension)
		return;

	current_name = gtk_file_chooser_get_current_name (file_chooser);
	if (!current_name)
		return;

	if (!g_str_has_suffix (current_name, extension) &&
	    (g_str_has_suffix (current_name, ".tar.xz") ||
	     g_str_has_suffix (current_name, ".tar.gz"))) {
		gint extension_len, current_len;

		extension_len = strlen (extension);
		current_len = strlen (current_name);

		current_name[current_len - 2] = extension[extension_len - 2];

		gtk_file_chooser_set_current_name (file_chooser, current_name);
	}

	g_free (current_name);
}

static void
set_local_only (GtkFileChooserNative *file_chooser_native,
		gpointer user_data)
{
	GtkFileChooser *file_chooser = GTK_FILE_CHOOSER (file_chooser_native);

	/* XXX Has to be a local file, since the backup utility
	 *     takes a filename argument, not a URI. */
	gtk_file_chooser_set_local_only (file_chooser, TRUE);

	/* Not NULL when saving file, contains the suggested file name. */
	if (user_data) {
		GSList *filters;

		filters = gtk_file_chooser_list_filters (file_chooser);

		if (g_slist_length (filters) > 2) {
			if (g_str_has_suffix (user_data, ".xz"))
				gtk_file_chooser_set_filter (file_chooser, filters->data);
			else
				gtk_file_chooser_set_filter (file_chooser, filters->next->data);
		}

		g_slist_free (filters);

		g_signal_connect (file_chooser, "notify::filter",
			G_CALLBACK (file_chooser_filter_changed_cb), NULL);
	}
}

static gchar *
suggest_file_name (const gchar *extension)
{
	time_t t;
	struct tm tm;

	t = time (NULL);
	localtime_r (&t, &tm);

	return g_strdup_printf (
		"evolution-backup-%04d%02d%02d.tar%s",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		extension);
}

static gboolean
is_xz_available (void)
{
	gchar *path;

	path = g_find_program_in_path ("xz");

	g_free (path);

	return path != NULL;
}

static void
action_settings_backup_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EShellView *shell_view = user_data;
	gpointer parent_window;
	GFile *file;
	GFile *parent;
	GFileInfo *file_info;
	const gchar *attribute;
	GError *error = NULL;
	gchar *suggest, *tmp;
	gboolean has_xz;
	GSettings *settings;

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	tmp = g_settings_get_string (settings, "backup-restore-extension");

	has_xz = is_xz_available ();
	suggest = suggest_file_name ((has_xz && g_strcmp0 (tmp, ".xz") == 0) ? ".xz" : ".gz");

	g_free (tmp);

	parent_window = e_shell_view_get_shell_window (shell_view);

	if (!evolution_backup_restore_check_prog_exists ("tar", &error)) {
		e_alert_run_dialog_for_args (
			parent_window,
			"org.gnome.backup-restore:cannot-backup",
			error ? error->message : "Unknown error", NULL);
		g_clear_error (&error);
		g_object_unref (settings);
		return;
	}

	file = e_shell_run_save_dialog (
		e_shell_window_get_shell (e_shell_view_get_shell_window (shell_view)),
		_("Select name of the Evolution backup file"),
		suggest, has_xz ? "*.tar.xz;*.tar.gz" : "*.tar.gz",
		set_local_only, has_xz ? suggest : NULL);

	g_free (suggest);

	if (file == NULL) {
		g_object_unref (settings);
		return;
	}

	tmp = g_file_get_path (file);

	if (tmp && g_str_has_suffix (tmp, ".xz"))
		g_settings_set_string (settings, "backup-restore-extension", ".xz");
	else if (tmp && g_str_has_suffix (tmp, ".gz"))
		g_settings_set_string (settings, "backup-restore-extension", ".gz");

	g_object_unref (settings);

	if ((tmp && g_str_has_suffix (tmp, ".xz") &&
	    !evolution_backup_restore_check_prog_exists ("xz", &error)) ||
	    (tmp && !g_str_has_suffix (tmp, ".xz") &&
	    !evolution_backup_restore_check_prog_exists ("gzip", &error))) {
		e_alert_run_dialog_for_args (
			parent_window,
			"org.gnome.backup-restore:cannot-backup",
			error ? error->message : "Unknown error", NULL);
		g_clear_error (&error);
		g_object_unref (file);
		g_free (tmp);
		return;
	}

	g_free (tmp);

	/* Make sure the parent directory can be written to. */

	parent = g_file_get_parent (file);
	attribute = G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE;

	/* XXX The query operation blocks the main loop but we
	 *     know it's a local file, so let it slide for now. */
	file_info = g_file_query_info (
		parent, attribute, G_FILE_QUERY_INFO_NONE, NULL, &error);

	g_object_unref (parent);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		g_object_unref (file);
		return;
	}

	if (g_file_info_get_attribute_boolean (file_info, attribute)) {
		guint32 mask;
		gchar *path;

		mask = dialog_prompt_user (
			parent_window,
			_("_Restart Evolution after backup"),
			"org.gnome.backup-restore:backup-confirm", NULL);
		if (mask & BR_OK) {
			path = g_file_get_path (file);
			backup (path, (mask & BR_START) ? TRUE: FALSE);
			g_free (path);
		}
	} else {
		e_alert_run_dialog_for_args (
			parent_window,
			"org.gnome.backup-restore:insufficient-permissions",
			NULL);
	}

	g_object_unref (file_info);
	g_object_unref (file);
}

typedef struct _ValidateBackupFileData {
	EShellWindow *shell_window;
	gchar *path;
	gboolean is_valid;
} ValidateBackupFileData;

static void
validate_backup_file_data_free (gpointer ptr)
{
	ValidateBackupFileData *vbf = ptr;

	if (vbf) {
		if (vbf->is_valid) {
			guint32 mask;

			mask = dialog_prompt_user (
				vbf->shell_window ? GTK_WINDOW (vbf->shell_window) : NULL,
				_("Re_start Evolution after restore"),
				"org.gnome.backup-restore:restore-confirm", NULL);
			if (mask & BR_OK)
				restore (vbf->path, mask & BR_START);
		}

		g_clear_object (&vbf->shell_window);
		g_free (vbf->path);
		g_slice_free (ValidateBackupFileData, vbf);
	}
}

static void
validate_backup_file_thread (EAlertSinkThreadJobData *job_data,
			     gpointer user_data,
			     GCancellable *cancellable,
			     GError **error)
{
	ValidateBackupFileData *vbf = user_data;
	GError *local_error = NULL;

	g_return_if_fail (vbf != NULL);
	g_return_if_fail (vbf->path != NULL);

	vbf->is_valid = evolution_backup_restore_validate_backup_file (vbf->path, &local_error);

	if (local_error) {
		e_alert_sink_thread_job_set_alert_ident (job_data, "org.gnome.backup-restore:cannot-restore");
		e_alert_sink_thread_job_set_alert_arg_0 (job_data, local_error->message);
		g_propagate_error (error, local_error);
	} else {
		/* The error text doesn't matter here, it will not be shown to the user */
		if (!vbf->is_valid)
			g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed");
	}
}

static void
action_settings_restore_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EShellView *shell_view = user_data;
	EActivity *activity;
	GFile *file;
	gchar *path, *description;
	ValidateBackupFileData *vbf;

	file = e_shell_run_open_dialog (
		e_shell_window_get_shell (e_shell_view_get_shell_window (shell_view)),
		_("Select name of the Evolution backup file to restore"),
		set_local_only, NULL);

	if (file == NULL)
		return;

	path = g_file_get_path (file);

	description = g_strdup_printf (_("Checking content of backup file “%s”, please wait…"), path);

	vbf = g_slice_new0 (ValidateBackupFileData);
	vbf->shell_window = e_shell_view_get_shell_window (shell_view);
	if (vbf->shell_window)
		g_object_ref (vbf->shell_window);
	vbf->path = g_strdup (path);

	activity = e_shell_view_submit_thread_job (shell_view, description, "org.gnome.backup-restore:invalid-backup", path,
		validate_backup_file_thread, vbf, validate_backup_file_data_free);
	if (activity)
		e_activity_set_cancellable (activity, NULL);

	g_clear_object (&activity);
	g_object_unref (file);
	g_free (description);
	g_free (path);
}

static gboolean
evolution_backup_restore_filename_to_visible (GBinding *binding,
                                              const GValue *source_value,
                                              GValue *target_value,
                                              gpointer unused)
{
	const gchar *filename;
	gboolean visible;

	filename = g_value_get_string (source_value);
	visible = (filename != NULL && *filename != '\0');
	g_value_set_boolean (target_value, visible);

	return TRUE;
}

static void
evolution_backup_restore_prepare_cb (GtkAssistant *assistant,
                                     GtkWidget *page,
                                     EMailConfigRestorePage *restore_page)
{
	const gchar *filename;

	/* If we've landed on the EMailConfigRestoreReadyPage, that
	 * means the user has chosen a valid backup file to restore
	 * so start the "evolution-backup" tool immediately. */

	filename = e_mail_config_restore_page_get_filename (restore_page);

	if (E_IS_MAIL_CONFIG_RESTORE_READY_PAGE (page))
		restore (filename, TRUE);
}

static void
evolution_backup_restore_assistant_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	EMailConfigAssistant *assistant;
	const gchar *type_name;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (evolution_backup_restore_assistant_parent_class)->constructed (object);

	assistant = E_MAIL_CONFIG_ASSISTANT (extensible);

	/* XXX We only want to add the EMailConfigRestorePage to an
	 *     EStartupAssistant instance, not a normal EMailConfigAssistant.
	 *     But EStartupAssistant is defined in the "startup-wizard" module
	 *     and we can't access its GType without knowing its type name, so
	 *     just hard-code the type name. */
	type_name = G_OBJECT_TYPE_NAME (assistant);
	if (g_strcmp0 (type_name, "EStartupAssistant") == 0) {
		EMailConfigPage *restore_page;
		EMailConfigPage *ready_page;

		restore_page = e_mail_config_restore_page_new ();
		e_mail_config_assistant_add_page (assistant, restore_page);

		ready_page = e_mail_config_restore_ready_page_new ();
		e_mail_config_assistant_add_page (assistant, ready_page);

		e_binding_bind_property_full (
			restore_page, "filename",
			ready_page, "visible",
			G_BINDING_SYNC_CREATE,
			evolution_backup_restore_filename_to_visible,
			NULL,
			NULL, (GDestroyNotify) NULL);

		g_signal_connect (
			assistant, "prepare",
			G_CALLBACK (evolution_backup_restore_prepare_cb),
			restore_page);
	}
}

static void
evolution_backup_restore_assistant_class_init (EExtensionClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = evolution_backup_restore_assistant_constructed;

	class->extensible_type = E_TYPE_MAIL_CONFIG_ASSISTANT;
}

static void
evolution_backup_restore_assistant_class_finalize (EExtensionClass *class)
{
}

static void
evolution_backup_restore_assistant_init (EExtension *extension)
{
}

static void
evolution_backup_restore_menu_items_constructed (GObject *object)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<submenu action='file-menu'>"
		      "<placeholder id='file-actions'>"
			"<item action='settings-backup'/>"
			"<item action='settings-restore'/>"
		      "</placeholder>"
		    "</submenu>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entries[] = {

		{ "settings-backup",
		  NULL,
		  N_("_Back up Evolution Data…"),
		  NULL,
		  N_("Back up Evolution data and settings to an archive file"),
		  action_settings_backup_cb, NULL, NULL },

		{ "settings-restore",
		  NULL,
		  N_("R_estore Evolution Data…"),
		  NULL,
		  N_("Restore Evolution data and settings from an archive file"),
		  action_settings_restore_cb, NULL, NULL }
	};

	EExtensible *extensible;
	EShellView *shell_view;
	EUIManager *ui_manager;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (evolution_backup_restore_menu_items_parent_class)->constructed (object);

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	shell_view = E_SHELL_VIEW (extensible);
	ui_manager = e_shell_view_get_ui_manager (shell_view);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "backup-restore", NULL,
		entries, G_N_ELEMENTS (entries), shell_view, eui);
}

static void
evolution_backup_restore_menu_items_class_init (EExtensionClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = evolution_backup_restore_menu_items_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SHELL_VIEW;
}

static void
evolution_backup_restore_menu_items_class_finalize (EExtensionClass *class)
{
}

static void
evolution_backup_restore_menu_items_init (EExtension *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	evolution_backup_restore_assistant_register_type (type_module);
	evolution_backup_restore_menu_items_register_type (type_module);

	e_mail_config_restore_page_type_register (type_module);
	e_mail_config_restore_ready_page_type_register (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
