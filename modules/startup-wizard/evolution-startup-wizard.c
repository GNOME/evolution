/*
 * evolution-startup-wizard.c
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
 */

#include <config.h>
#include <glib/gi18n-lib.h>

#include <shell/e-shell.h>
#include <e-util/e-account-utils.h>
#include <e-util/e-alert-dialog.h>
#include <e-util/e-extension.h>
#include <e-util/e-import.h>

#include <mail/em-account-editor.h>
#include <capplet/settings/mail-capplet-shell.h>
#include <calendar/gui/calendar-config.h>

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

	EConfig *config;

	EImport *import;
	EImportTarget *import_target;

	/* Currently active importer. */
	EImportImporter *import_importer;

	/* List of available importers. */
	GSList *import_importers;

	/* List node of the active importer. */
	GSList *import_iterator;

	gboolean import_cancelled;
	gint import_progress_page_num;
	GtkWidget *import_progress_bar;
	GtkWidget *import_cancelled_msg;
	GtkWidget *import_completed_msg;
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
startup_wizard_terminate (void) {
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

static void
startup_wizard_import_status (EImport *import,
                              const gchar *what,
                              gint percentage,
                              EStartupWizard *extension)
{
	GtkProgressBar *progress_bar;
	gfloat fraction;

	fraction = (gfloat) (percentage / 100.0);
	progress_bar = GTK_PROGRESS_BAR (extension->import_progress_bar);
	gtk_progress_bar_set_fraction (progress_bar, fraction);
	gtk_progress_bar_set_text (progress_bar, what);
}

static void
startup_wizard_import_complete (EImport *import,
                                EStartupWizard *extension)
{
	EConfig *config = extension->config;

	extension->import_importer = NULL;
	extension->import_iterator = g_slist_next (extension->import_iterator);
	e_config_target_changed (config, E_CONFIG_TARGET_CHANGED_STATE);
}

static gboolean
startup_wizard_check_progress (EConfig *config,
                               const gchar *page_id,
                               EStartupWizard *extension)
{
	if (extension->import_cancelled)
		goto cancelled;

	if (extension->import_iterator == NULL)
		goto completed;

	gtk_widget_hide (extension->import_cancelled_msg);
	gtk_widget_hide (extension->import_completed_msg);

	extension->import_importer = extension->import_iterator->data;
	startup_wizard_import_status (extension->import, "", 0, extension);

	e_import_import (
		extension->import,
		extension->import_target,
		extension->import_importer,
		(EImportStatusFunc) startup_wizard_import_status,
		(EImportCompleteFunc) startup_wizard_import_complete,
		extension);

	return FALSE;

cancelled:

	gtk_widget_show (extension->import_cancelled_msg);
	startup_wizard_import_status (extension->import, "", 0, extension);

	return TRUE;

completed:

	gtk_widget_show (extension->import_completed_msg);
	startup_wizard_import_status (extension->import, "", 100, extension);

	return TRUE;
}

static void
startup_wizard_config_abort (EConfig *config,
                             GSList *items,
                             EStartupWizard *extension)
{
	GtkAssistant *assistant;
	gint page_num;

	assistant = GTK_ASSISTANT (config->widget);
	page_num = gtk_assistant_get_current_page (assistant);

	/* If we're not on the import progress page, terminate. */
	if (page_num != extension->import_progress_page_num) {
		startup_wizard_terminate ();
		g_assert_not_reached ();
	}

	/* XXX Overloading the cancel button like this is a bit evil,
	 *     but if we're on the import progress page and the import
	 *     has already been cancelled, terminate. */
	if (extension->import_cancelled) {
		startup_wizard_terminate ();
		g_assert_not_reached ();
	}

	if (extension->import_importer) {
		e_import_cancel (
			extension->import,
			extension->import_target,
			extension->import_importer);
	} else {
		startup_wizard_terminate ();
		g_assert_not_reached ();
	}

	extension->import_cancelled = TRUE;
	e_config_target_changed (config, E_CONFIG_TARGET_CHANGED_STATE);

	/* Prevent EConfig from destroying the GtkAssistant. */
	g_signal_stop_emission_by_name (assistant, "cancel");
}

static void
startup_wizard_config_commit (EConfig *config,
                              GSList *items,
                              EStartupWizard *extension)
{
	EShell *shell;
	EShellSettings *shell_settings;
	gchar *location;

	shell = startup_wizard_get_shell (extension);
	shell_settings = e_shell_get_shell_settings (shell);

	/* Use System Timezone by default. */
	e_shell_settings_set_boolean (
		shell_settings, "cal-use-system-timezone", TRUE);
	location = e_cal_util_get_system_timezone_location ();
	e_shell_settings_set_string (
		shell_settings, "cal-timezone-string", location);
	g_free (location);

	gtk_main_quit ();
}

static void
startup_wizard_config_free (EConfig *config,
                            GSList *items,
                            EStartupWizard *extension)
{
	while (items != NULL) {
		EConfigItem *config_item = items->data;

		g_free (config_item->path);
		g_object_unref (config_item->user_data);
		g_slice_free (EConfigItem, config_item);

		items = g_slist_delete_link (items, items);
	}

	g_object_unref (extension);
}

static GtkWidget *
startup_wizard_importer_page (EConfig *config,
                              EConfigItem *item,
                              GtkAssistant *assistant,
                              GtkWidget *old,
                              EStartupWizard *extension)
{
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *page;
	GSList *list, *iter;
	const gchar *title;
	guint n_importers;
	gint row = 0;

	list = extension->import_importers;
	n_importers = g_slist_length (list);

	/* Skip this page if there's nothing to import. */
	if (n_importers == 0)
		return NULL;

	page = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);
	gtk_widget_show (page);

	container = page;

	widget = gtk_label_new (
		_("Please select the information "
		  "that you would like to import:"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_table_new (n_importers, 2, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (widget), 12);
	gtk_table_set_row_spacings (GTK_TABLE (widget), 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	for (iter = list; iter != NULL; iter = iter->next) {
		EImportImporter *importer = iter->data;
		gchar *text;

		widget = e_import_get_widget (
			extension->import,
			extension->import_target, importer);
		if (widget == NULL)
			continue;
		gtk_table_attach (
			GTK_TABLE (container), widget,
			1, 2, row, row + 1, GTK_FILL, 0, 0, 0);
		gtk_widget_show (widget);

		text = g_strdup_printf (_("From %s:"), importer->name);
		widget = gtk_label_new (text);
		gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
		gtk_table_attach (
			GTK_TABLE (container), widget,
			0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
		gtk_widget_show (widget);

		row++;
	}

	title = _("Importing Files");
	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_title (assistant, page, title);

	return page;
}

static GtkWidget *
startup_wizard_progress_page (EConfig *config,
                              EConfigItem *item,
                              GtkAssistant *assistant,
                              GtkWidget *old,
                              EStartupWizard *extension)
{
	GtkSizeGroup *size_group;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *page;
	const gchar *title;
	gint page_num;

	/* Skip this page if there's nothing to import. */
	if (extension->import_importers == NULL)
		return NULL;

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

	page = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);
	gtk_widget_show (page);

	container = page;

	/* Just a spacer. */
	widget = gtk_alignment_new (0.5, 0.0, 0.0, 0.0);
	gtk_size_group_add_widget (size_group, widget);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	widget = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	extension->import_progress_bar = widget;
	gtk_widget_show (widget);

	widget = gtk_vbox_new (FALSE, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_alignment_new (0.5, 0.0, 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	extension->import_cancelled_msg = widget;
	gtk_widget_show (widget);

	widget = gtk_alignment_new (0.5, 0.0, 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	extension->import_completed_msg = widget;
	gtk_widget_show (widget);

	container = extension->import_cancelled_msg;

	widget = gtk_hbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new_from_stock (
		GTK_STOCK_CANCEL, GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new (
		_("Import cancelled. Click \"Forward\" to continue."));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = extension->import_completed_msg;

	widget = gtk_hbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new_from_icon_name (
		"emblem-default", GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new (
		_("Import complete. Click \"Forward\" to continue."));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	title = _("Importing Files");
	page_num = gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_title (assistant, page, title);

	extension->import_progress_page_num = page_num;

	return page;
}

static GtkWidget *
startup_wizard_new_assistant (EStartupWizard *extension)
{
	EMAccountEditor *emae;
	EConfig *config;
	EConfigItem *config_item;
	GtkWidget *widget;
	GSList *items = NULL;

	emae = em_account_editor_new (
		NULL, EMAE_ASSISTANT,
		"org.gnome.evolution.mail.config.accountWizard");

	config = E_CONFIG (emae->config);
	extension->config = g_object_ref (config);

	/* Insert the importer page. */

	config_item = g_slice_new0 (EConfigItem);
	config_item->type = E_CONFIG_PAGE;
	config_item->path = g_strdup ("60.importers");
	config_item->factory =
		(EConfigItemFactoryFunc) startup_wizard_importer_page;
	config_item->user_data = g_object_ref (extension);
	items = g_slist_prepend (items, config_item);

	/* Insert the progress page. */

	config_item = g_slice_new0 (EConfigItem);
	config_item->type = E_CONFIG_PAGE_PROGRESS;
	config_item->path = g_strdup ("70.progress");
	config_item->factory =
		(EConfigItemFactoryFunc) startup_wizard_progress_page;
	config_item->user_data = g_object_ref (extension);
	items = g_slist_prepend (items, config_item);

	e_config_add_items (
		config, items,
		(EConfigItemsFunc) startup_wizard_config_commit,
		(EConfigItemsFunc) startup_wizard_config_abort,
		(EConfigItemsFunc) startup_wizard_config_free,
		g_object_ref (extension));

	e_config_add_page_check (
		config, "70.progress", (EConfigCheckFunc)
		startup_wizard_check_progress, extension);

	e_config_create_window (config, NULL, _("Evolution Setup Assistant"));

	/* Additional tweaks.  The window must be created at this point. */

	widget = e_config_page_get (config, "0.start");
	gtk_assistant_set_page_title (
		GTK_ASSISTANT (config->widget), widget, _("Welcome"));

	widget = em_account_editor_get_widget (emae, "start_page_label");
	gtk_label_set_text (
		GTK_LABEL (widget),
		_("Welcome to Evolution. The next few screens will "
		  "allow Evolution to connect to your email accounts, "
		  "and to import files from other applications. \n\n"
		  "Please click the \"Forward\" button to continue. "));

	/* Finalize the EMAccountEditor along with the GtkAssistant. */
	g_object_set_data_full (
		G_OBJECT (config->window), "AccountEditor",
		emae, (GDestroyNotify) g_object_unref);

	return config->window;
}

static GtkWidget *
startup_wizard_new_capplet (EStartupWizard *extension)
{
	GtkWidget *capplet;

	capplet = mail_capplet_shell_new (0, TRUE, TRUE);

	g_signal_connect (
		capplet, "destroy",
		G_CALLBACK (gtk_main_quit), NULL);

	return capplet;
}

static void
startup_wizard_run (EStartupWizard *extension)
{
	EShell *shell;
	GtkWidget *window;
	EAccountList *account_list;
	const gchar *startup_view;
	gboolean express_mode;

	shell = e_shell_get_default ();
	express_mode = e_shell_get_express_mode (shell);
	startup_view = e_shell_get_startup_view (shell);

	account_list = e_get_account_list ();
	if (e_list_length (E_LIST (account_list)) > 0)
		return;

	if (express_mode && g_strcmp0 (startup_view, "mail") != 0)
		return;

	if (express_mode)
		window = startup_wizard_new_capplet (extension);
	else
		window = startup_wizard_new_assistant (extension);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (startup_wizard_terminate), NULL);

	gtk_widget_show (window);

	gtk_main ();
}

static void
startup_wizard_dispose (GObject *object)
{
	EStartupWizard *extension;

	extension = E_STARTUP_WIZARD (object);

	if (extension->config != NULL) {
		g_object_unref (extension->config);
		extension->config = NULL;
	}

	if (extension->import != NULL) {
		e_import_target_free (
			extension->import,
			extension->import_target);
		g_object_unref (extension->import);
		extension->import_target = NULL;
		extension->import = NULL;
	}

	g_slist_free (extension->import_importers);
	extension->import_importers = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_startup_wizard_parent_class)->dispose (object);
}

static void
startup_wizard_constructed (GObject *object)
{
	EStartupWizard *extension;
	EShell *shell;

	extension = E_STARTUP_WIZARD (object);
	shell = startup_wizard_get_shell (extension);

	g_signal_connect_swapped (
		shell, "event::ready-to-start",
		G_CALLBACK (startup_wizard_run), extension);
}

static void
e_startup_wizard_class_init (EStartupWizardClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = startup_wizard_dispose;
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
	extension->import =
		e_import_new ("org.gnome.evolution.shell.importer");
	extension->import_target = (EImportTarget *)
		e_import_target_new_home (extension->import);
	extension->import_importers = e_import_get_importers (
		extension->import, extension->import_target);
	extension->import_iterator = extension->import_importers;
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_startup_wizard_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
