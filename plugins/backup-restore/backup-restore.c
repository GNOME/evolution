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
#include <libgnomeui/gnome-druid.h>
#include <libgnomeui/gnome-druid-page-standard.h>
#include "shell/es-menu.h"
#include "mail/em-config.h"
#include "mail/em-account-editor.h"
#include "e-util/e-error.h"
#include "e-util/e-util.h"
#include "e-util/e-dialog-utils.h"

void org_gnome_backup_restore_backup (EPlugin *ep, ESMenuTargetShell *target);
void org_gnome_backup_restore_restore (EPlugin *ep, ESMenuTargetShell *target);
GtkWidget * backup_restore_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
void backup_restore_commit (EPlugin *ep, EMConfigTargetAccount *target);
void backup_restore_abort (EPlugin *ep, EMConfigTargetAccount *target);

typedef enum _br_flags {
	BR_OK = 1<<0,
	BR_START = 1<<1
}br_flags;

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
	gchar *quotedfname;

	quotedfname = g_shell_quote(filename);

	command = g_strdup_printf ("%s/evolution-backup --check %s", EVOLUTION_TOOLSDIR, quotedfname);
	result = system (command);
	g_free (command);
	g_free (quotedfname);

#ifdef HAVE_SYS_WAIT_H
	g_message ("Sanity check result %d:%d %d", WIFEXITED (result), WEXITSTATUS (result), result);

	return WIFEXITED (result) && (WEXITSTATUS (result) == 0);
#else
	return result;
#endif
}

static guint32
dialog_prompt_user(GtkWindow *parent, const gchar *string, const gchar *tag, const gchar *arg0, ...)
{
	GtkWidget *mbox, *check = NULL;
	va_list ap;
	gint button;
	guint32 mask = 0;

	va_start(ap, arg0);
	mbox = e_error_newv(parent, tag, arg0, ap);
	va_end(ap);

	check = gtk_check_button_new_with_mnemonic (string);
	/* We should hardcode this to true */
	gtk_toggle_button_set_active ((GtkToggleButton *)check, TRUE);
	gtk_container_set_border_width((GtkContainer *)check, 12);
	gtk_box_pack_start ((GtkBox *)((GtkDialog *) mbox)->vbox, check, TRUE, TRUE, 0);
	gtk_widget_show (check);

	button = gtk_dialog_run ((GtkDialog *) mbox);

	if (button == GTK_RESPONSE_YES)
		mask |= BR_OK;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check)))
		mask |= BR_START;

	gtk_widget_destroy(mbox);

	return mask;
}

static gboolean
epbr_perform_pre_backup_checks (gchar * dir)
{
#ifdef G_OS_WIN32
	return TRUE;
#else
	return (g_access (dir, W_OK) == 0);
#endif
}

void
org_gnome_backup_restore_backup (EPlugin *ep, ESMenuTargetShell *target)
{
	GtkWidget *dlg;
	GtkWidget *vbox;
	gint response;

	dlg = e_file_get_save_filesel(target->target.widget, _("Select name of the Evolution backup file"), NULL, GTK_FILE_CHOOSER_ACTION_SAVE);

/*	dlg = gtk_file_chooser_dialog_new (_("Select name of the Evolution backup file"), GTK_WINDOW (target->target.widget),  */
/*					   GTK_FILE_CHOOSER_ACTION_SAVE,  */
/*					   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,  */
/*					   GTK_STOCK_SAVE, GTK_RESPONSE_OK, NULL); */

	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dlg), "evolution-backup.tar.gz");

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	response = gtk_dialog_run (GTK_DIALOG (dlg));
	if (response == GTK_RESPONSE_OK) {
		gchar *filename;
		guint32 mask;
		gchar *uri = NULL;
		gchar *dir;

		uri = gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER (dlg));
		e_file_update_save_path(uri, TRUE);

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
		dir = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dlg));
		gtk_widget_destroy (dlg);

		if (epbr_perform_pre_backup_checks (dir)) {

			mask = dialog_prompt_user (GTK_WINDOW (target->target.widget), _("_Restart Evolution after backup"), "org.gnome.backup-restore:backup-confirm", NULL);
			if (mask & BR_OK)
				backup (filename, (mask & BR_START) ? TRUE: FALSE);
		} else {
			e_error_run (NULL, "org.gnome.backup-restore:insufficient-permissions", NULL);
		}

		g_free (filename);
		g_free (dir);

		return;
	}

	gtk_widget_destroy (dlg);
}

void
org_gnome_backup_restore_restore (EPlugin *ep, ESMenuTargetShell *target)
{
	GtkWidget *dlg;
	GtkWidget *vbox;
	gint response;

	dlg = e_file_get_save_filesel(target->target.widget, _("Select name of the Evolution backup file to restore"), NULL, GTK_FILE_CHOOSER_ACTION_OPEN);

/*	dlg = gtk_file_chooser_dialog_new (_("Select Evolution backup file to restore"), GTK_WINDOW (target->target.widget),  */
/*					   GTK_FILE_CHOOSER_ACTION_OPEN,  */
/*					   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,  */
/*					   GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL); */

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	response = gtk_dialog_run (GTK_DIALOG (dlg));
	if (response == GTK_RESPONSE_OK) {
		gchar *filename;
		gchar *uri = NULL;

		uri = gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER (dlg));
		e_file_update_save_path(uri, TRUE);

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
		gtk_widget_destroy (dlg);

		if (sanity_check (filename)) {
			guint32 mask;

			mask = dialog_prompt_user (GTK_WINDOW (target->target.widget), _("_Restart Evolution after restore"), "org.gnome.backup-restore:restore-confirm", NULL);
			if (mask & BR_OK)
				restore (filename, mask & BR_START);
		} else {
			e_error_run (GTK_WINDOW (target->target.widget), "org.gnome.backup-restore:invalid-backup", NULL);
		}

		g_free (filename);

		return;
	}

	gtk_widget_destroy (dlg);
}

static void
check_toggled (GtkToggleButton *button, GnomeDruid *druid)
{
	 GtkWidget *box = g_object_get_data ((GObject *)button, "box");
	gboolean state =  gtk_toggle_button_get_active ((GtkToggleButton *)button);
	gchar *prevfile = g_object_get_data ((GObject *)druid, "restore-file");

	gtk_widget_set_sensitive (box, state);
	gnome_druid_set_show_finish (druid, state);
	if (state && !prevfile)
		gnome_druid_set_buttons_sensitive (druid, TRUE, FALSE, TRUE, TRUE);
	else
		gnome_druid_set_buttons_sensitive (druid, TRUE, TRUE, TRUE, TRUE);

	g_object_set_data ((GObject *)druid, "restore", GINT_TO_POINTER (state?1:0));

}

static void
restore_wizard (GnomeDruidPage *druidpage, GnomeDruid *druid, gpointer user_data)
{
	gboolean state = GPOINTER_TO_INT(g_object_get_data((GObject *)druid, "restore")) ? TRUE:FALSE;
	gchar *file = g_object_get_data ((GObject *)druid, "restore-file");

	if (state) {
		if (!file ||!sanity_check (file)) {
			 e_error_run ((GtkWindow *)druid, "org.gnome.backup-restore:invalid-backup", NULL);
		} else
			restore (file, TRUE);

	}
}

static void
file_changed (GtkFileChooser *chooser, GnomeDruid *druid)
{
	gchar *file = NULL, *prevfile=NULL;
	gchar *uri = NULL;

	uri = gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER (chooser));
	e_file_update_save_path(uri, TRUE);

	file = gtk_file_chooser_get_filename (chooser);
	prevfile = g_object_get_data ((GObject *)druid, "restore-file");
	g_object_set_data ((GObject *)druid, "restore-file", file);
	g_free (prevfile);
	if (file) {
		gnome_druid_set_buttons_sensitive (druid, TRUE, TRUE, TRUE, TRUE);
	} else
		gnome_druid_set_buttons_sensitive (druid, TRUE, FALSE, TRUE, TRUE);

}
GtkWidget *
backup_restore_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data)
{
	GtkWidget *page;
	GtkWidget *box, *hbox, *label, *cbox, *button;

	page = gnome_druid_page_standard_new_with_vals (_("Restore from backup"), NULL, NULL);
	hbox = gtk_hbox_new (FALSE, 6);
	label = gtk_label_new (_("You can restore Evolution from your backup. It can restore all the Mails, Calendars, Tasks, Memos, Contacts. It also restores all your personal settings, mail filters etc."));
	gtk_label_set_line_wrap ((GtkLabel *)label, TRUE);
	gtk_label_set_single_line_mode ((GtkLabel *)label, FALSE);
	gtk_box_pack_start ((GtkBox *)hbox, label, FALSE, FALSE, 6);
	box = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start ((GtkBox *)box, hbox, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	cbox = gtk_check_button_new_with_mnemonic (_("_Restore Evolution from the backup file"));
	g_signal_connect (cbox, "toggled", G_CALLBACK (check_toggled), hook_data->parent);
	gtk_box_pack_start ((GtkBox *)hbox, cbox, FALSE, FALSE, 6);
	gtk_box_pack_start ((GtkBox *)box, hbox, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	g_object_set_data ((GObject *)cbox, "box", hbox);
	label = gtk_label_new (_("Please select an Evolution Archive to restore:"));
	gtk_box_pack_start ((GtkBox *)hbox, label, FALSE, FALSE, 12);

	button = gtk_file_chooser_button_new (_("Choose a file to restore"), GTK_FILE_CHOOSER_ACTION_OPEN);
	g_signal_connect (button, "selection-changed", G_CALLBACK (file_changed), hook_data->parent);
	gtk_file_chooser_button_set_width_chars ((GtkFileChooserButton *)button, 20);
	gtk_box_pack_start ((GtkBox *)hbox, button, FALSE, FALSE, 0);
	gtk_box_pack_start ((GtkBox *)box, hbox, FALSE, FALSE, 0);
	gtk_widget_set_sensitive (hbox, FALSE);

	gtk_container_add ((GtkContainer *) GNOME_DRUID_PAGE_STANDARD (page)->vbox, box);
	gtk_widget_show_all (box);
	gnome_druid_append_page (GNOME_DRUID (hook_data->parent), GNOME_DRUID_PAGE (page));
	g_object_set_data ((GObject *)hook_data->parent, "restore", GINT_TO_POINTER (FALSE));
	g_signal_connect (page, "finish", G_CALLBACK (restore_wizard), NULL);
	return GTK_WIDGET (page);
}
void
backup_restore_commit (EPlugin *ep, EMConfigTargetAccount *target)
{
	/* Nothing really */
	printf("commit\n");
}

void
backup_restore_abort (EPlugin *ep, EMConfigTargetAccount *target)
{
	/* Nothing really */
}

