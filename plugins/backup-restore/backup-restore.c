#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include "shell/es-menu.h"

void org_gnome_backup_restore_backup (EPlugin *ep, ESMenuTargetShell *target);
void org_gnome_backup_restore_restore (EPlugin *ep, ESMenuTargetShell *target);

static void
backup (const char *filename, gboolean restart)
{
	if (restart)
		execl (EVOLUTION_TOOLSDIR "/backup", "backup", "--backup", "--restart", filename, NULL);
	else
		execl (EVOLUTION_TOOLSDIR "/backup", "backup", "--backup", filename, NULL);
}

static void
restore (const char *filename, gboolean restart)
{
	if (restart)
		execl (EVOLUTION_TOOLSDIR "/backup", "backup", "--restore", "--restart", filename, NULL);
	else
		execl (EVOLUTION_TOOLSDIR "/backup", "backup", "--restore", filename, NULL);
}

static gboolean
sanity_check (const char *filename)
{
	char *command;
	int result;

	command = g_strdup_printf ("%s/backup --check %s", EVOLUTION_TOOLSDIR, filename);
	result = system (command);
	g_free (command);

	g_message ("Sanity check result %d:%d", WIFEXITED (result), WEXITSTATUS (result));
	
	return WIFEXITED (result) && (WEXITSTATUS (result) == 0);
}

void
org_gnome_backup_restore_backup (EPlugin *ep, ESMenuTargetShell *target)
{
	GtkWidget *dlg;
	GtkWidget *vbox, *check;
	int response;
	
	dlg = gtk_file_chooser_dialog_new (_("Select name of Evolution archive"), GTK_WINDOW (target->target.widget), 
					   GTK_FILE_CHOOSER_ACTION_SAVE, 
					   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
					   GTK_STOCK_SAVE, GTK_RESPONSE_OK, NULL);

	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dlg), "evolution-backup.tar.gz");
	
	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	
	check = gtk_check_button_new_with_mnemonic (_("_Restart Evolution after backup"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);
	gtk_widget_show (check);

	gtk_box_pack_start (GTK_BOX (vbox), check, FALSE, TRUE, 0);
	gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dlg), vbox);
	
	response = gtk_dialog_run (GTK_DIALOG (dlg));
	if (response == GTK_RESPONSE_OK) {
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));

		backup (filename, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)));

		g_free (filename);
	}
	
	gtk_widget_destroy (dlg);
}

void
org_gnome_backup_restore_restore (EPlugin *ep, ESMenuTargetShell *target)
{
	GtkWidget *dlg;
	GtkWidget *vbox, *check;
	int response;
	
	dlg = gtk_file_chooser_dialog_new (_("Select Evolution archive to restore"), GTK_WINDOW (target->target.widget), 
					   GTK_FILE_CHOOSER_ACTION_OPEN, 
					   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
					   GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);
	
	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	
	check = gtk_check_button_new_with_mnemonic (_("_Restart Evolution after restore"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);
	gtk_widget_show (check);

	gtk_box_pack_start (GTK_BOX (vbox), check, FALSE, TRUE, 0);
	gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dlg), vbox);

	response = gtk_dialog_run (GTK_DIALOG (dlg));
	if (response == GTK_RESPONSE_OK) {
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
		
		if (sanity_check (filename)) {
			restore (filename, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)));
		} else {
			g_message ("Invalid archive");
		}
		
		g_free (filename);
	}
	
	gtk_widget_destroy (dlg);
}


