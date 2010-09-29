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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <libedataserver/e-data-server-util.h>

#ifdef G_OS_WIN32
#ifdef DATADIR
#undef DATADIR
#endif
#include <windows.h>
#include <conio.h>
#ifndef PROCESS_DEP_ENABLE
#define PROCESS_DEP_ENABLE 0x00000001
#endif
#ifndef PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION
#define PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION 0x00000002
#endif
#endif

#include "e-util/e-util-private.h"
#include "e-util/e-util.h"

#define EVOUSERDATADIR_MAGIC "#EVO_USERDATADIR#"

#define EVOLUTION "evolution"
#define EVOLUTION_DIR "$DATADIR/"
#define EVOLUTION_DIR_FILE EVOLUTION ".dir"
#define GCONF_DUMP_FILE "backup-restore-gconf.xml"
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

static gboolean check (const gchar *filename, gboolean *is_new_format);

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
		if (p < next)
			g_string_append_len (str, p, next - p);

		if (replace && *replace)
			g_string_append (str, replace);

		p = next + find_len;
	}

	g_string_append (str, p);

	return str;
}

static const gchar *
strip_home_dir (const gchar *dir)
{
	const gchar *home_dir, *res;

	g_return_val_if_fail (dir != NULL, NULL);

	home_dir = g_get_home_dir ();
	g_return_val_if_fail (home_dir != NULL, dir);
	g_return_val_if_fail (*home_dir != '\0', dir);

	res = dir;
	if (g_str_has_prefix (res, home_dir))
		res += strlen (home_dir);

	if (*res == G_DIR_SEPARATOR)
		res++;

	return res;
}

static GString *
replace_variables (const gchar *str)
{
	GString *res = NULL, *use;
	const gchar *strip_datadir, *strip_configdir;

	g_return_val_if_fail (str != NULL, NULL);

	strip_datadir = strip_home_dir (e_get_user_data_dir ());
	strip_configdir = strip_home_dir (e_get_user_config_dir ());

	#define repl(_find, _replace)						\
		use = replace_string (res ? res->str : str, _find, _replace);	\
		g_return_val_if_fail (use != NULL, NULL);			\
		if (res)							\
			g_string_free (res, TRUE);				\
		res = use;

	repl ("$HOME", g_get_home_dir ());
	repl ("$TMP", g_get_tmp_dir ());
	repl ("$DATADIR", e_get_user_data_dir ());
	repl ("$CONFIGDIR", e_get_user_config_dir ());
	repl ("$STRIPDATADIR", strip_datadir);
	repl ("$STRIPCONFIGDIR", strip_configdir);

	#undef repl

	g_return_val_if_fail (res != NULL, NULL);

	/* remove trailing dir separator */
	while (res->len > 0 && res->str[res->len - 1] == G_DIR_SEPARATOR) {
		g_string_truncate (res, res->len - 1);
	}

	return res;
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

	if (strstr (filename, "$")) {
		filenamestr = replace_variables (filename);

		if (!filenamestr) {
			g_warning ("%s: Replace variables in '%s' failed!", G_STRFUNC, filename);
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

	if (strstr (cmd, "$") != NULL) {
		/* read the doc for g_get_home_dir to know why replacing it here */
		GString *str = replace_variables (cmd);

		if (str) {
			print_and_run (str->str);
			g_string_free (str, TRUE);
		}
	} else
		print_and_run (cmd);
}

static void
write_dir_file (void)
{
	GString *content, *filename;
	GError *error = NULL;

	filename = replace_variables ("$HOME/" EVOLUTION_DIR_FILE);
	g_return_if_fail (filename != NULL);

	content = replace_variables (
		"[dirs]\n"
		"data=$STRIPDATADIR\n"
		"config=$STRIPCONFIGDIR\n");
	g_return_if_fail (content != NULL);

	g_file_set_contents (filename->str, content->str, content->len, &error);

	if (error) {
		g_warning ("Failed to write file '%s': %s\n", filename->str, error->message);
		g_error_free (error);
	}

	g_string_free (filename, TRUE);
	g_string_free (content, TRUE);
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
	run_cmd (EVOLUTION " --quit");

	run_cmd ("rm $DATADIR/.running");

	CANCEL (complete);
	txt = _("Backing Evolution accounts and settings");
	run_cmd ("gconftool-2 --dump " GCONF_DIR " > " EVOLUTION_DIR GCONF_DUMP_FILE);

	replace_in_file (EVOLUTION_DIR GCONF_DUMP_FILE, e_get_user_data_dir (), EVOUSERDATADIR_MAGIC);

	write_dir_file ();

	CANCEL (complete);
	txt = _("Backing Evolution data (Mails, Contacts, Calendar, Tasks, Memos)");

	/* FIXME stay on this file system ,other options?" */
	/* FIXME compression type?" */
	/* FIXME date/time stamp?" */
	/* FIXME backup location?" */
	command = g_strdup_printf ("cd $HOME && tar chf - $STRIPDATADIR $STRIPCONFIGDIR .camel_certs " EVOLUTION_DIR_FILE " | gzip > %s", quotedfname);
	run_cmd (command);
	g_free (command);
	g_free (quotedfname);

	run_cmd ("rm $HOME/" EVOLUTION_DIR_FILE);

	txt = _("Backup complete");

	if (restart_arg) {

		CANCEL (complete);
		txt = _("Restarting Evolution");
		complete=TRUE;

		run_cmd (EVOLUTION);
	}

}

static void
extract_backup_dirs (const gchar *filename, gchar **data_dir, gchar **config_dir)
{
	GKeyFile *key_file;
	GError *error = NULL;

	g_return_if_fail (filename != NULL);
	g_return_if_fail (data_dir != NULL);
	g_return_if_fail (config_dir != NULL);

	key_file = g_key_file_new ();
	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, &error);

	if (error) {
		g_warning ("Failed to read '%s': %s", filename, error->message);
		g_error_free (error);
	} else {
		gchar *tmp;

		tmp = g_key_file_get_value (key_file, "dirs", "data", NULL);
		if (tmp)
			*data_dir = g_shell_quote (tmp);
		g_free (tmp);

		tmp = g_key_file_get_value (key_file, "dirs", "config", NULL);
		if (tmp)
			*config_dir = g_shell_quote (tmp);
		g_free (tmp);
	}

	g_key_file_free (key_file);
}

static gint
get_dir_level (const gchar *dir)
{
	gint res = 0, i;

	g_return_val_if_fail (dir != NULL, -1);

	for (i = 0; dir[i]; i++) {
		if (dir[i] == '/' || dir[i] == '\\')
			res++;
	}

	if (i > 0)
		res++;

	return res;
}

static void
restore (const gchar *filename)
{
	gchar *command;
	gchar *quotedfname;
	gboolean is_new_format = FALSE;

	g_return_if_fail (filename && *filename);

	if (!check (filename, &is_new_format)) {
		g_message ("Cannot restore from an incorrect archive '%s'.", filename);
		goto end;
	}

	quotedfname = g_shell_quote(filename);

	/* FIXME Will the versioned setting always work? */
	CANCEL (complete);
	txt = _("Shutting down Evolution");
	run_cmd (EVOLUTION " --quit");

	CANCEL (complete);
	txt = _("Back up current Evolution data");
	run_cmd ("mv $DATADIR $DATADIR_old");
	run_cmd ("mv $CONFIGDIR $CONFIGDIR_old");
	run_cmd ("mv $HOME/.camel_certs $HOME/.camel_certs_old");

	CANCEL (complete);
	txt = _("Extracting files from backup");

	if (is_new_format) {
		GString *dir_fn;
		gchar *data_dir = NULL, *config_dir = NULL;

		command = g_strdup_printf ("cd $TMP && tar xzf %s " EVOLUTION_DIR_FILE, quotedfname);
		run_cmd (command);
		g_free (command);

		dir_fn = replace_variables ("$TMP" G_DIR_SEPARATOR_S EVOLUTION_DIR_FILE);
		if (!dir_fn) {
			g_warning ("Failed to create evolution's dir filename");
			goto end;
		}

		/* data_dir and config_dir are quoted inside extract_backup_dirs */
		extract_backup_dirs (dir_fn->str, &data_dir, &config_dir);

		g_unlink (dir_fn->str);
		g_string_free (dir_fn, TRUE);

		if (!data_dir || !config_dir) {
			g_warning ("Failed to get old data_dir (%p)/config_dir (%p)", data_dir, config_dir);
			g_free (data_dir);
			g_free (config_dir);
			goto end;
		}

		g_mkdir_with_parents (e_get_user_data_dir (), 0700);
		g_mkdir_with_parents (e_get_user_config_dir (), 0700);

		command = g_strdup_printf ("cd $DATADIR && tar xzf %s %s --strip-components=%d", quotedfname, data_dir, get_dir_level (data_dir));
		run_cmd (command);
		g_free (command);

		command = g_strdup_printf ("cd $CONFIGDIR && tar xzf %s %s --strip-components=%d", quotedfname, config_dir, get_dir_level (config_dir));
		run_cmd (command);
		g_free (command);

		command = g_strdup_printf ("cd $HOME && tar xzf %s .camel_certs", quotedfname);
		run_cmd (command);
		g_free (command);

		g_free (data_dir);
		g_free (config_dir);
	} else {
		run_cmd ("mv $HOME/.evolution $HOME/.evolution_old");

		command = g_strdup_printf ("cd $HOME && gzip -cd %s | tar xf -", quotedfname);
		run_cmd (command);
		g_free (command);
	}

	g_free (quotedfname);

	CANCEL (complete);
	txt = _("Loading Evolution settings");

	if (is_new_format) {
		/* new format has it in DATADIR... */
		replace_in_file (EVOLUTION_DIR GCONF_DUMP_FILE, EVOUSERDATADIR_MAGIC, e_get_user_data_dir ());
		run_cmd ("gconftool-2 --load " EVOLUTION_DIR GCONF_DUMP_FILE);
		run_cmd ("rm " EVOLUTION_DIR GCONF_DUMP_FILE);
	} else {
		/* ... old format in ~/.evolution */
		replace_in_file ("$HOME/.evolution/" GCONF_DUMP_FILE, EVOUSERDATADIR_MAGIC, e_get_user_data_dir ());
		run_cmd ("gconftool-2 --load " "$HOME/.evolution/" GCONF_DUMP_FILE);
		run_cmd ("rm " "$HOME/.evolution/" GCONF_DUMP_FILE);
	}

	CANCEL (complete);
	txt = _("Removing temporary backup files");
	run_cmd ("rm -rf $DATADIR_old");
	run_cmd ("rm -rf $CONFIGDIR_old");
	run_cmd ("rm -rf $HOME/.camel_certs_old");
	run_cmd ("rm $DATADIR/.running");

	if (!is_new_format)
		run_cmd ("rm -rf $HOME/.evolution_old");

	CANCEL (complete);
	txt = _("Ensuring local sources");

 end:
	if (restart_arg) {
		CANCEL (complete);
		txt = _("Restarting Evolution");
		complete=TRUE;
		run_cmd (EVOLUTION);
	}
}

static gboolean
check (const gchar *filename, gboolean *is_new_format)
{
	gchar *command;
	gchar *quotedfname;
	gboolean is_new = TRUE;

	g_return_val_if_fail (filename && *filename, FALSE);
	quotedfname = g_shell_quote(filename);

	if (is_new_format)
		*is_new_format = FALSE;

	command = g_strdup_printf ("tar ztf %s 1>/dev/null", quotedfname);
	result = system (command);
	g_free (command);

	g_message ("First result %d", result);
	if (result) {
		g_free (quotedfname);
		return FALSE;
	}

	command = g_strdup_printf ("tar ztf %s | grep -e \"%s$\"", quotedfname, EVOLUTION_DIR_FILE);
	result = system (command);
	g_free (command);

	if (result) {
		command = g_strdup_printf ("tar ztf %s | grep -e \"^\\.evolution/$\"", quotedfname);
		result = system (command);
		g_free (command);
		is_new = FALSE;
	}

	g_message ("Second result %d", result);
	if (result) {
		g_free (quotedfname);
		return FALSE;
	}

	if (is_new) {
		if (is_new_format)
			*is_new_format = TRUE;
		g_free (quotedfname);
		return TRUE;
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
		check (chk_file, NULL);

	complete = TRUE;

	return GINT_TO_POINTER(result);
}

static gboolean
idle_cb(gpointer data)
{
	if (gui_arg) {
		/* Show progress dialog */
		gtk_progress_bar_pulse ((GtkProgressBar *)pbar);
		g_timeout_add (50, pbar_update, NULL);
	}

	g_thread_create (thread_start, NULL, FALSE, NULL);

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

#ifdef G_OS_WIN32
	/* Reduce risks */
	{
		typedef BOOL (WINAPI *t_SetDllDirectoryA) (LPCSTR lpPathName);
		t_SetDllDirectoryA p_SetDllDirectoryA;

		p_SetDllDirectoryA = GetProcAddress (GetModuleHandle ("kernel32.dll"), "SetDllDirectoryA");
		if (p_SetDllDirectoryA)
			(*p_SetDllDirectoryA) ("");
	}
#ifndef _WIN64
	{
		typedef BOOL (WINAPI *t_SetProcessDEPPolicy) (DWORD dwFlags);
		t_SetProcessDEPPolicy p_SetProcessDEPPolicy;

		p_SetProcessDEPPolicy = GetProcAddress (GetModuleHandle ("kernel32.dll"), "SetProcessDEPPolicy");
		if (p_SetProcessDEPPolicy)
			(*p_SetProcessDEPPolicy) (PROCESS_DEP_ENABLE|PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION);
	}
#endif
#endif

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
		GtkWidget *action_area;
		GtkWidget *content_area;
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

#if !GTK_CHECK_VERSION(2,90,7)
		g_object_set (progress_dialog, "has-separator", FALSE, NULL);
#endif
		gtk_container_set_border_width (GTK_CONTAINER (progress_dialog), 12);

		action_area = gtk_dialog_get_action_area (
			GTK_DIALOG (progress_dialog));
		content_area = gtk_dialog_get_content_area (
			GTK_DIALOG (progress_dialog));

		/* Override GtkDialog defaults */
		gtk_box_set_spacing (GTK_BOX (content_area), 12);
		gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);
		gtk_box_set_spacing (GTK_BOX (action_area), 12);
		gtk_container_set_border_width (GTK_CONTAINER (action_area), 0);

		if (oper && file)
			str = g_strdup_printf(oper, file);

		container = gtk_table_new (2, 3, FALSE);
		gtk_table_set_col_spacings (GTK_TABLE (container), 12);
		gtk_table_set_row_spacings (GTK_TABLE (container), 12);
		gtk_widget_show (container);

		gtk_box_pack_start (
			GTK_BOX (content_area), container, FALSE, TRUE, 0);

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
		check (chk_file, NULL);
		exit (result == 0 ? 0 : 1);
	}

	g_idle_add (idle_cb, NULL);
	gtk_main ();

	return result;
}
