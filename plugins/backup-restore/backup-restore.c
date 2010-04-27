/*
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include "mail/em-config.h"
#include "mail/em-account-editor.h"
#include "e-util/e-alert-dialog.h"
#include "e-util/e-util.h"
#include "e-util/e-dialog-utils.h"
#include "shell/e-shell-utils.h"
#include "shell/e-shell-window.h"

gboolean	e_plugin_ui_init		(GtkUIManager *ui_manager,
						 EShellWindow *shell_window);

GtkWidget * backup_restore_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
void backup_restore_commit (EPlugin *ep, EMConfigTargetAccount *target);
void backup_restore_abort (EPlugin *ep, EMConfigTargetAccount *target);

typedef enum _br_flags {
	BR_OK = 1<<0,
	BR_START = 1<<1
}br_flags;

gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
	return 0;
}

static void
backup (const gchar *filename, gboolean restart)
{
	if (restart)
		execl (EVOLUTION_TOOLSDIR "/evolution-backup", "evolution-backup", "--gui", "--backup", "--restart", filename, (gchar *)NULL);
	else
		execl (EVOLUTION_TOOLSDIR "/evolution-backup", "evolution-backup", "--gui", "--backup", filename, (gchar *)NULL);
}

static void
restore (const gchar *filename, gboolean restart)
{
	if (restart)
		execl (EVOLUTION_TOOLSDIR "/evolution-backup", "evolution-backup", "--gui", "--restore", "--restart", filename, (gchar *)NULL);
	else
		execl (EVOLUTION_TOOLSDIR "/evolution-backup", "evolution-backup", "--gui", "--restore", filename, (gchar *)NULL);
}

static gboolean
sanity_check (const gchar *filename)
{
	gchar *command;
	gint result;
	gchar *quotedfname, *toolfname;

	quotedfname = g_shell_quote(filename);
	toolfname = g_build_filename (EVOLUTION_TOOLSDIR, "evolution-backup", NULL);

	command =  g_strdup_printf("%s --check %s", toolfname, quotedfname);
	result = system (command);
	g_free (command);
	g_free (quotedfname);
	g_free (toolfname);

#ifdef HAVE_SYS_WAIT_H
	g_message ("Sanity check result %d:%d %d", WIFEXITED (result), WEXITSTATUS (result), result);

	return WIFEXITED (result) && (WEXITSTATUS (result) == 0);
#else
	return result;
#endif
}

static guint32
dialog_prompt_user(GtkWindow *parent, const gchar *string, const gchar *tag, ...)
{
	GtkWidget *mbox, *check = NULL;
	va_list ap;
	gint button;
	guint32 mask = 0;
	EAlert *alert = NULL;

	va_start(ap, tag);
	alert = e_alert_new_valist(tag, ap);
	va_end(ap);

	mbox = e_alert_dialog_new (parent, alert);
	g_object_unref (alert);

	check = gtk_check_button_new_with_mnemonic (string);
	/* We should hardcode this to true */
	gtk_toggle_button_set_active ((GtkToggleButton *)check, TRUE);
	gtk_container_set_border_width((GtkContainer *)check, 12);
	gtk_box_pack_start ((GtkBox *)gtk_dialog_get_content_area ((GtkDialog *) mbox), check, TRUE, TRUE, 0);
	gtk_widget_show (check);

	button = gtk_dialog_run ((GtkDialog *) mbox);

	if (button == GTK_RESPONSE_YES)
		mask |= BR_OK;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check)))
		mask |= BR_START;

	gtk_widget_destroy(mbox);

	return mask;
}

static void
set_local_only (GtkFileChooser *file_chooser)
{
	/* XXX Has to be a local file, since the backup utility
	 *     takes a filename argument, not a URI. */
	gtk_file_chooser_set_local_only (file_chooser, TRUE);
}

static void
action_settings_backup_cb (GtkAction *action,
                           EShellWindow *shell_window)
{
	GFile *file;
	GFile *parent;
	GFileInfo *file_info;
	const gchar *attribute;
	GError *error = NULL;

	file = e_shell_run_save_dialog (
		e_shell_window_get_shell (shell_window),
		_("Select name of the Evolution backup file"),
		"evolution-backup.tar.gz", "*.tar.gz", (GtkCallback)
		set_local_only, NULL);

	if (file == NULL)
		return;

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
		return;
	}

	if (g_file_info_get_attribute_boolean (file_info, attribute)) {
		guint32 mask;
		gchar *path;

		mask = dialog_prompt_user (
			GTK_WINDOW (shell_window),
			_("_Restart Evolution after backup"),
			"org.gnome.backup-restore:backup-confirm", NULL);
		if (mask & BR_OK) {
			path = g_file_get_path (file);
			backup (path, (mask & BR_START) ? TRUE: FALSE);
			g_free (path);
		}
	} else {
		e_alert_run_dialog_for_args (
			GTK_WINDOW (shell_window),
			"org.gnome.backup-restore:insufficient-permissions", NULL);
	}

	g_object_unref (file_info);
	g_object_unref (file);
}

static void
action_settings_restore_cb (GtkAction *action,
                            EShellWindow *shell_window)
{
	GFile *file;
	gchar *path;

	file = e_shell_run_open_dialog (
		e_shell_window_get_shell (shell_window),
		_("Select name of the Evolution backup file to restore"),
		(GtkCallback) set_local_only, NULL);

	if (file == NULL)
		return;

	path = g_file_get_path (file);

	if (sanity_check (path)) {
		guint32 mask;

		mask = dialog_prompt_user (
			GTK_WINDOW (shell_window),
			_("_Restart Evolution after restore"),
			"org.gnome.backup-restore:restore-confirm", NULL);
		if (mask & BR_OK)
			restore (path, mask & BR_START);
	} else {
		e_alert_run_dialog_for_args (
			GTK_WINDOW (shell_window),
			"org.gnome.backup-restore:invalid-backup", NULL);
	}

	g_object_unref (file);
	g_free (path);
}

static void
check_toggled (GtkToggleButton *button, GtkAssistant *assistant)
{
	GtkWidget *box = g_object_get_data ((GObject *)button, "box");
	gboolean state = gtk_toggle_button_get_active ((GtkToggleButton *)button);

	gtk_widget_set_sensitive (box, state);

	g_object_set_data ((GObject *)assistant, "restore", GINT_TO_POINTER (state?1:0));

	e_config_target_changed ((EConfig *) g_object_get_data ((GObject *)assistant, "restore-config"), E_CONFIG_TARGET_CHANGED_STATE);
}

static void
file_changed (GtkFileChooser *chooser, GtkAssistant *assistant)
{
	gchar *file = NULL, *prevfile = NULL;

	file = gtk_file_chooser_get_filename (chooser);
	prevfile = g_object_get_data ((GObject *)assistant, "restore-file");
	g_object_set_data ((GObject *)assistant, "restore-file", file);
	g_free (prevfile);

	e_config_target_changed ((EConfig *) g_object_get_data ((GObject *)assistant, "restore-config"), E_CONFIG_TARGET_CHANGED_STATE);
}

static gboolean
backup_restore_check (EConfig *ec, const gchar *pageid, gpointer data)
{
	GtkAssistant *assistant = data;
	gint do_restore;
	gchar *file;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_ASSISTANT (data), FALSE);

	do_restore = GPOINTER_TO_INT (g_object_get_data ((GObject *)assistant, "restore"));
	file = g_object_get_data ((GObject *)assistant, "restore-file");

	e_config_set_page_is_finish (ec, "0.startup_page.10.backup_restore", do_restore);

	return !do_restore || file;
}

GtkWidget *
backup_restore_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data)
{
	GtkWidget *page, *hbox, *label, *cbox, *button;
	GtkAssistant *assistant = GTK_ASSISTANT (hook_data->parent);

	page = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);

	hbox = gtk_hbox_new (FALSE, 6);
	label = gtk_label_new (_("You can restore Evolution from your backup. It can restore all the Mails, Calendars, Tasks, Memos, Contacts. It also restores all your personal settings, mail filters etc."));
	gtk_label_set_line_wrap ((GtkLabel *)label, TRUE);
	gtk_label_set_single_line_mode ((GtkLabel *)label, FALSE);
	gtk_box_pack_start ((GtkBox *)hbox, label, FALSE, FALSE, 6);
	gtk_box_pack_start ((GtkBox *)page, hbox, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	cbox = gtk_check_button_new_with_mnemonic (_("_Restore Evolution from the backup file"));
	g_signal_connect (cbox, "toggled", G_CALLBACK (check_toggled), assistant);
	gtk_box_pack_start ((GtkBox *)hbox, cbox, FALSE, FALSE, 6);
	gtk_box_pack_start ((GtkBox *)page, hbox, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	g_object_set_data ((GObject *)cbox, "box", hbox);
	label = gtk_label_new (_("Please select an Evolution Archive to restore:"));
	gtk_box_pack_start ((GtkBox *)hbox, label, FALSE, FALSE, 12);

	button = gtk_file_chooser_button_new (_("Choose a file to restore"), GTK_FILE_CHOOSER_ACTION_OPEN);
	g_signal_connect (button, "selection-changed", G_CALLBACK (file_changed), assistant);
	gtk_file_chooser_button_set_width_chars ((GtkFileChooserButton *)button, 20);
	gtk_box_pack_start ((GtkBox *)hbox, button, FALSE, FALSE, 0);
	gtk_box_pack_start ((GtkBox *)page, hbox, FALSE, FALSE, 0);
	gtk_widget_set_sensitive (hbox, FALSE);

	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_title (assistant, page, _("Restore from backup"));
	gtk_widget_show_all (page);

	g_object_set_data ((GObject *)assistant, "restore", GINT_TO_POINTER (FALSE));
	g_object_set_data ((GObject *)assistant, "restore-config", hook_data->config);

	e_config_add_page_check (hook_data->config, "0.startup_page.10.backup_restore", backup_restore_check, assistant);

	return GTK_WIDGET (page);
}
void
backup_restore_commit (EPlugin *ep, EMConfigTargetAccount *target)
{
	GtkWidget *assistant = target->target.config->widget;
	gboolean state = GPOINTER_TO_INT (g_object_get_data ((GObject *)assistant, "restore")) ? TRUE : FALSE;
	gchar *file = g_object_get_data ((GObject *)assistant, "restore-file");

	if (state) {
		if (!file || !sanity_check (file)) {
			e_alert_run_dialog_for_args ((GtkWindow *)assistant,
						     "org.gnome.backup-restore:invalid-backup",
						     NULL);
		} else {
			restore (file, TRUE);
		}
	}
}

void
backup_restore_abort (EPlugin *ep, EMConfigTargetAccount *target)
{
	/* Nothing really */
}

static GtkActionEntry entries[] = {

	{ "settings-backup",
	  NULL,
	  N_("_Backup Evolution Settings..."),
	  NULL,
	  N_("Backup Evolution data and settings to an archive file"),
	  G_CALLBACK (action_settings_backup_cb) },

	{ "settings-restore",
	  NULL,
	  N_("R_estore Evolution Settings..."),
	  NULL,
	  N_("Restore Evolution data and settings from an archive file"),
	  G_CALLBACK (action_settings_restore_cb) }
};

gboolean
e_plugin_ui_init (GtkUIManager *ui_manager,
                  EShellWindow *shell_window)
{
	GtkActionGroup *action_group;

	action_group = e_shell_window_get_action_group (shell_window, "shell");

	/* Add actions to the "shell" action group. */
	gtk_action_group_add_actions (
		action_group, entries,
		G_N_ELEMENTS (entries), shell_window);

	return TRUE;
}
