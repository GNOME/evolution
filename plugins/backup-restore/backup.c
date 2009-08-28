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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "e-util/e-util.h"

#define EVOUSERDATADIR_MAGIC "#EVO_USERDATADIR#"

#define EVOLUTION "evolution"
#define EVOLUTION_DIR "$HOME/.evolution/"
#define EVOLUTION_DIR_BACKUP "$HOME/.evolution-old/"
#define GCONF_DUMP_FILE "backup-restore-gconf.xml"
#define GCONF_DUMP_PATH EVOLUTION_DIR GCONF_DUMP_FILE
#define GCONF_DIR "/apps/evolution"
#define ARCHIVE_NAME "evolution-backup.tar.gz"

static gboolean backup_op = FALSE;
static gchar *bk_file = NULL;
static gboolean restore_op = FALSE;
static gchar *res_file = NULL;
static gboolean check_op = FALSE;
static gchar *chk_file = NULL;
static gboolean restart_arg = FALSE;
static gboolean gui_arg = FALSE;
static gchar **opt_remaining = NULL;
static gint result = 0;
static GtkWidget *progress_dialog;
static GtkWidget *pbar;
static gchar *txt = NULL;
static gboolean complete = FALSE;

static GOptionEntry options[] = {
	{ "backup", '\0', 0, G_OPTION_ARG_NONE, &backup_op,
	  N_("Backup Evolution directory"), NULL },
	{ "restore", '\0', 0, G_OPTION_ARG_NONE, &restore_op,
	  N_("Restore Evolution directory"), NULL },
	{ "check", '\0', 0, G_OPTION_ARG_NONE, &check_op,
	  N_("Check Evolution Backup"), NULL },
	{ "restart", '\0', 0, G_OPTION_ARG_NONE, &restart_arg,
	  N_("Restart Evolution"), NULL },
	{ "gui", '\0', 0, G_OPTION_ARG_NONE, &gui_arg,
	  N_("With Graphical User Interface"), NULL },
	{ G_OPTION_REMAINING, '\0', 0,
	  G_OPTION_ARG_STRING_ARRAY, &opt_remaining },
	{ NULL }
};

#define d(x)

#define print_and_run(x) G_STMT_START { g_message ("%s", x); system (x); } G_STMT_END
#define CANCEL(x) if (x) return;

static gboolean check (const gchar *filename);

static GString *
replace_string (const gchar *text, const gchar *find, const gchar *replace)
{
	const gchar *p, *next;
	GString *str;
	gint find_len;

	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (find != NULL, NULL);
	g_return_val_if_fail (*find, NULL);

	find_len = strlen (find);
	str = g_string_new ("");

	p = text;
	while (next = strstr (p, find), next) {
		if (p + 1 < next)
			g_string_append_len (str, p, next - p);

		if (replace && *replace)
			g_string_append (str, replace);

		p = next + find_len;
	}

	g_string_append (str, p);

	return str;
}

static void
replace_in_file (const gchar *filename, const gchar *find, const gchar *replace)
{
	gchar *content = NULL;
	GError *error = NULL;
	GString *filenamestr = NULL;

	g_return_if_fail (filename != NULL);
	g_return_if_fail (find != NULL);
	g_return_if_fail (*find);
	g_return_if_fail (replace != NULL);

	if (strstr (filename, "$HOME")) {
		filenamestr = replace_string (filename, "$HOME", g_get_home_dir ());

		if (!filenamestr) {
			g_warning ("%s: Replace string on $HOME failed!", G_STRFUNC);
			return;
		}

		filename = filenamestr->str;
	}

	if (g_file_get_contents (filename, &content, NULL, &error)) {
		GString *str = replace_string (content, find, replace);

		if (str) {
			if (!g_file_set_contents (filename, str->str, -1, &error) && error) {
				g_warning ("%s: cannot write file content, error: %s", G_STRFUNC, error->message);
				g_error_free (error);
			}

			g_string_free (str, TRUE);
		} else {
			g_warning ("%s: Replace of '%s' to '%s' failed!", G_STRFUNC, find, replace);
		}

		g_free (content);
	} else if (error) {
		g_warning ("%s: Cannot read file content, error: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	if (filenamestr)
		g_string_free (filenamestr, TRUE);
}

static void
run_cmd (const gchar *cmd)
{
	if (!cmd)
		return;

	if (strstr (cmd, "$HOME") != NULL) {
		/* read the doc for g_get_home_dir to know why replacing it here */
		GString *str = replace_string (cmd, "$HOME", g_get_home_dir ());

		if (str) {
			print_and_run (str->str);
			g_string_free (str, TRUE);
		}
	} else
		print_and_run (cmd);
}

static void
backup (const gchar *filename)
{
	gchar *command;
	gchar *quotedfname;

	g_return_if_fail (filename && *filename);
	quotedfname = g_shell_quote(filename);

	CANCEL (complete);
	txt = _("Shutting down Evolution");
	/* FIXME Will the versioned setting always work? */
	run_cmd (EVOLUTION " --force-shutdown");

	run_cmd ("rm $HOME/.evolution/.running");

	CANCEL (complete);
	txt = _("Backing Evolution accounts and settings");
	run_cmd ("gconftool-2 --dump " GCONF_DIR " > " GCONF_DUMP_PATH);

	replace_in_file (GCONF_DUMP_PATH, e_get_user_data_dir (), EVOUSERDATADIR_MAGIC);

	CANCEL (complete);
	txt = _("Backing Evolution data (Mails, Contacts, Calendar, Tasks, Memos)");

	/* FIXME stay on this file system ,other options?" */
	/* FIXME compression type?" */
	/* FIXME date/time stamp?" */
	/* FIXME backup location?" */
	command = g_strdup_printf ("cd $HOME && tar chf - .evolution .camel_certs | gzip > %s", quotedfname);
	run_cmd (command);
	g_free (command);
	g_free (quotedfname);

	txt = _("Backup complete");

	if (restart_arg) {

		CANCEL (complete);
		txt = _("Restarting Evolution");
		complete=TRUE;

		run_cmd (EVOLUTION);
	}

}

static void
restore (const gchar *filename)
{
	gchar *command;
	gchar *quotedfname;

	g_return_if_fail (filename && *filename);

	if (!check (filename)) {
		g_message ("Cannot restore from an incorrect archive '%s'.", filename);

		if (restart_arg) {
			CANCEL (complete);
			txt = _("Restarting Evolution");
			complete=TRUE;
			run_cmd (EVOLUTION);
		}

		return;
	}

	quotedfname = g_shell_quote(filename);

	/* FIXME Will the versioned setting always work? */
	CANCEL (complete);
	txt = _("Shutting down Evolution");
	run_cmd (EVOLUTION " --force-shutdown");

	CANCEL (complete);
	txt = _("Backup current Evolution data");
	run_cmd ("mv " EVOLUTION_DIR " " EVOLUTION_DIR_BACKUP);
	run_cmd ("mv $HOME/.camel_certs ~/.camel_certs_old");

	CANCEL (complete);
	txt = _("Extracting files from backup");
	command = g_strdup_printf ("cd $HOME && gzip -cd %s| tar xf -", quotedfname);
	run_cmd (command);
	g_free (command);
	g_free (quotedfname);

	CANCEL (complete);
	txt = _("Loading Evolution settings");

	replace_in_file (GCONF_DUMP_PATH, EVOUSERDATADIR_MAGIC, e_get_user_data_dir ());

	run_cmd ("gconftool-2 --load " GCONF_DUMP_PATH);

	CANCEL (complete);
	txt = _("Removing temporary backup files");
	run_cmd ("rm -rf " GCONF_DUMP_PATH);
	run_cmd ("rm -rf " EVOLUTION_DIR_BACKUP);
	run_cmd ("rm -rf $HOME/.camel_certs_old");
	run_cmd ("rm $HOME/.evolution/.running");

	CANCEL (complete);
	txt = _("Ensuring local sources");

	if (restart_arg) {
		CANCEL (complete);
		txt = _("Restarting Evolution");
		complete=TRUE;
		run_cmd (EVOLUTION);
	}

}

static gboolean
check (const gchar *filename)
{
	gchar *command;
	gchar *quotedfname;

	g_return_val_if_fail (filename && *filename, FALSE);
	quotedfname = g_shell_quote(filename);

	command = g_strdup_printf ("tar ztf %s 1>/dev/null", quotedfname);
	result = system (command);
	g_free (command);

	g_message ("First result %d", result);
	if (result) {
		g_free (quotedfname);
		return FALSE;
	}

	command = g_strdup_printf ("tar ztf %s | grep -e \"^\\.evolution/$\"", quotedfname);
	result = system (command);
	g_free (command);

	g_message ("Second result %d", result);
	if (result) {
		g_free (quotedfname);
		return FALSE;
	}

	command = g_strdup_printf ("tar ztf %s | grep -e \"^\\.evolution/%s$\"", quotedfname, GCONF_DUMP_FILE);
	result = system (command);
	g_free (command);
	g_free (quotedfname);

	g_message ("Third result %d", result);

	return result == 0;
}

static gboolean
pbar_update (gpointer data)
{
	if (!complete) {
		gtk_progress_bar_pulse ((GtkProgressBar *)pbar);
		gtk_progress_bar_set_text ((GtkProgressBar *)pbar, txt);
		return TRUE;
	}

	gtk_main_quit ();
	return FALSE;
}

static gpointer
thread_start (gpointer data)
{
	if (backup_op)
		backup (bk_file);
	else if (restore_op)
		restore (res_file);
	else if (check_op)
		check (chk_file);

	complete = TRUE;

	return GINT_TO_POINTER(result);
}

static gboolean
idle_cb(gpointer data)
{
	GThread *t;

	if (gui_arg) {
		/* Show progress dialog */
		gtk_progress_bar_pulse ((GtkProgressBar *)pbar);
		g_timeout_add (50, pbar_update, NULL);
	}

	t = g_thread_create (thread_start, NULL, FALSE, NULL);

	return FALSE;
}

static void
dlg_response (GtkWidget *dlg, gint response, gpointer data)
{
	/* We will cancel only backup/restore operations and not the check operation */
	complete = TRUE;

	/* If the response is not of delete_event then destroy the event */
	if (response != GTK_RESPONSE_NONE)
		gtk_widget_destroy (dlg);

	/* We will kill just the tar operation. Rest of the them will be just a second of microseconds.*/
	run_cmd ("pkill tar");

	if (bk_file && backup_op && response == GTK_RESPONSE_REJECT) {
		/* backup was canceled, delete the backup file as it is not needed now */
		gchar *cmd, *filename;

		g_message ("Backup canceled, removing partial backup file.");

		filename = g_shell_quote (bk_file);
		cmd = g_strconcat ("rm ", filename, NULL);

		run_cmd (cmd);

		g_free (cmd);
		g_free (filename);
	}

	gtk_main_quit ();
}

gint
main (gint argc, gchar **argv)
{
	gchar *file = NULL, *oper = NULL;
	gint i;
	GError *error = NULL;

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_thread_init (NULL);

	gtk_init_with_args (
		&argc, &argv, NULL, options, (gchar *) GETTEXT_PACKAGE, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		exit (1);
	}

	if (opt_remaining) {
		for (i = 0; i < g_strv_length (opt_remaining); i++) {
			if (backup_op) {
				oper = _("Backing up to the folder %s");
				d(g_message ("Backing up to the folder %s", (gchar *) opt_remaining[i]));
				bk_file = g_strdup ((gchar *) opt_remaining[i]);
				file = bk_file;
			} else if (restore_op) {
				oper = _("Restoring from the folder %s");
				d(g_message ("Restoring from the folder %s", (gchar *) opt_remaining[i]));
				res_file = g_strdup ((gchar *) opt_remaining[i]);
				file = res_file;
			} else if (check_op) {
				d(g_message ("Checking %s", (gchar *) opt_remaining[i]));
				chk_file = g_strdup ((gchar *) opt_remaining[i]);
			}
		}
	}

	if (gui_arg && !check_op) {
		GtkWidget *widget, *container;
		const gchar *txt, *txt2;
		gchar *str = NULL;
		gchar *markup;

		gtk_window_set_default_icon_name ("evolution");

		/* Backup / Restore only can have GUI. We should restrict the rest */
		progress_dialog = gtk_dialog_new_with_buttons (backup_op ? _("Evolution Backup"): _("Evolution Restore"),
							  NULL,
							  GTK_DIALOG_MODAL,
							  GTK_STOCK_CANCEL,
							  GTK_RESPONSE_REJECT,
							  NULL);

		gtk_dialog_set_has_separator (GTK_DIALOG (progress_dialog), FALSE);
		gtk_container_set_border_width (GTK_CONTAINER (progress_dialog), 12);

		/* Override GtkDialog defaults */
		widget = GTK_DIALOG (progress_dialog)->vbox;
		gtk_box_set_spacing (GTK_BOX (widget), 12);
		gtk_container_set_border_width (GTK_CONTAINER (widget), 0);
		widget = GTK_DIALOG (progress_dialog)->action_area;
		gtk_box_set_spacing (GTK_BOX (widget), 12);
		gtk_container_set_border_width (GTK_CONTAINER (widget), 0);

		if (oper && file)
			str = g_strdup_printf(oper, file);

		container = gtk_table_new (2, 3, FALSE);
		gtk_table_set_col_spacings (GTK_TABLE (container), 12);
		gtk_table_set_row_spacings (GTK_TABLE (container), 12);
		gtk_widget_show (container);

		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (progress_dialog)->vbox), container, FALSE, TRUE, 0);

		widget = gtk_image_new_from_stock (GTK_STOCK_COPY, GTK_ICON_SIZE_DIALOG);
		gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
		gtk_widget_show (widget);

		gtk_table_attach (GTK_TABLE (container), widget, 0, 1, 0, 3, GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

		if (backup_op) {
			txt = _("Backing up Evolution Data");
			txt2 = _("Please wait while Evolution is backing up your data.");
		} else if (restore_op) {
			txt = _("Restoring Evolution Data");
			txt2 = _("Please wait while Evolution is restoring your data.");
		} else {
			/* do not translate these two, it's just a fallback when something goes wrong,
			   we should never get here anyway. */
			txt = "Oops, doing nothing...";
			txt2 = "Should not be here now, really...";
		}

		markup = g_strconcat ("<b><big>", txt, "</big></b>", NULL);
		widget = gtk_label_new (NULL);
		gtk_label_set_line_wrap (GTK_LABEL (widget), FALSE);
		gtk_label_set_markup (GTK_LABEL (widget), markup);
		gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
		gtk_widget_show (widget);
		g_free (markup);

		gtk_table_attach (GTK_TABLE (container), widget, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

		markup = g_strconcat (txt2, " ", _("This may take a while depending on the amount of data in your account."), NULL);
		widget = gtk_label_new (NULL);
		gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
		gtk_label_set_markup (GTK_LABEL (widget), markup);
		gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
		gtk_widget_show (widget);
		g_free (markup);

		gtk_table_attach (GTK_TABLE (container), widget, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

		pbar = gtk_progress_bar_new ();

		if (str) {
			markup = g_strconcat ("<i>", str, "</i>", NULL);
			widget = gtk_label_new (NULL);
			gtk_label_set_markup (GTK_LABEL (widget), markup);
			gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
			g_free (markup);
			g_free (str);
			gtk_table_attach (GTK_TABLE (container), widget, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
			gtk_table_set_row_spacing (GTK_TABLE (container), 2, 6);

			gtk_table_attach (GTK_TABLE (container), pbar, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
		} else
			gtk_table_attach (GTK_TABLE (container), pbar, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

		gtk_window_set_default_size ((GtkWindow *) progress_dialog, 450, 120);
		g_signal_connect (progress_dialog, "response", G_CALLBACK(dlg_response), NULL);
		gtk_widget_show_all (progress_dialog);
	} else if (check_op) {
		/* For sanity we don't need gui */
		check (chk_file);
		exit (result == 0 ? 0 : 1);
	}

	g_idle_add (idle_cb, NULL);
	gtk_main ();

	return result;
}
