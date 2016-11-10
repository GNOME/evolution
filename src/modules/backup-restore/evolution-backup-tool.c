/*
 * evolution-backup-tool.c
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <libedataserver/libedataserver.h>

#ifdef G_OS_WIN32
#ifdef DATADIR
#undef DATADIR
#endif
#endif

#include "e-util/e-util-private.h"
#include "e-util/e-util.h"

#define EVOUSERDATADIR_MAGIC "#EVO_USERDATADIR#"

#define EVOLUTION "evolution"
#define EVOLUTION_DIR "$DATADIR/"
#define EVOLUTION_DIR_FILE EVOLUTION ".dir"
#define DBUS_SOURCE_REGISTRY_SERVICE_FILE "$DBUSDATADIR/org.gnome.evolution.dataserver.Sources.service"

#define ANCIENT_GCONF_DUMP_FILE "backup-restore-gconf.xml"

#define DCONF_DUMP_FILE_EDS "backup-restore-dconf-eds.ini"
#define DCONF_DUMP_FILE_EVO "backup-restore-dconf-evo.ini"

#define DCONF_PATH_EDS "/org/gnome/evolution-data-server/"
#define DCONF_PATH_EVO "/org/gnome/evolution/"

#define KEY_FILE_GROUP "Evolution Backup"

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

static GOptionEntry options[] = {
	{ "backup", '\0', 0, G_OPTION_ARG_NONE, &backup_op,
	  N_("Back up Evolution directory"), NULL },
	{ "restore", '\0', 0, G_OPTION_ARG_NONE, &restore_op,
	  N_("Restore Evolution directory"), NULL },
	{ "check", '\0', 0, G_OPTION_ARG_NONE, &check_op,
	  N_("Check Evolution Back up"), NULL },
	{ "restart", '\0', 0, G_OPTION_ARG_NONE, &restart_arg,
	  N_("Restart Evolution"), NULL },
	{ "gui", '\0', 0, G_OPTION_ARG_NONE, &gui_arg,
	  N_("With Graphical User Interface"), NULL },
	{ G_OPTION_REMAINING, '\0', 0,
	  G_OPTION_ARG_STRING_ARRAY, &opt_remaining },
	{ NULL }
};

#define d(x)

#define print_and_run(x) \
	G_STMT_START { g_message ("%s", x); if (system (x) == -1) g_warning ("%s: Failed to execute '%s'", G_STRFUNC, (x)); } G_STMT_END

static gboolean check (const gchar *filename, gboolean *is_new_format);

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
replace_variables (const gchar *str,
                   gboolean remove_dir_sep)
{
	GString *res = NULL, *use;
	const gchar *strip_datadir, *strip_configdir;

	g_return_val_if_fail (str != NULL, NULL);

	strip_datadir = strip_home_dir (e_get_user_data_dir ());
	strip_configdir = strip_home_dir (e_get_user_config_dir ());

	#define repl(_find, _replace)							\
		use = e_str_replace_string (res ? res->str : str, _find, _replace);	\
		g_return_val_if_fail (use != NULL, NULL);				\
		if (res)								\
			g_string_free (res, TRUE);					\
		res = use;

	repl ("$HOME", g_get_home_dir ());
	repl ("$TMP", g_get_tmp_dir ());
	repl ("$DATADIR", e_get_user_data_dir ());
	repl ("$CONFIGDIR", e_get_user_config_dir ());
	repl ("$STRIPDATADIR", strip_datadir);
	repl ("$STRIPCONFIGDIR", strip_configdir);
	repl ("$DBUSDATADIR", DBUS_SERVICES_DIR);

	#undef repl

	g_return_val_if_fail (res != NULL, NULL);

	if (remove_dir_sep) {
		/* remove trailing dir separator */
		while (res->len > 0 && res->str[res->len - 1] == G_DIR_SEPARATOR) {
			g_string_truncate (res, res->len - 1);
		}
	}

	return res;
}

static void
replace_in_file (const gchar *filename,
                 const gchar *find,
                 const gchar *replace)
{
	gchar *content = NULL;
	GError *error = NULL;
	GString *filenamestr = NULL;

	g_return_if_fail (filename != NULL);
	g_return_if_fail (find != NULL);
	g_return_if_fail (*find);
	g_return_if_fail (replace != NULL);

	if (strstr (filename, "$")) {
		filenamestr = replace_variables (filename, TRUE);

		if (!filenamestr) {
			g_warning (
				"%s: Replace variables in '%s' failed!",
				G_STRFUNC, filename);
			return;
		}

		filename = filenamestr->str;
	}

	if (g_file_get_contents (filename, &content, NULL, &error)) {
		GString *str = e_str_replace_string (content, find, replace);

		if (str) {
			if (!g_file_set_contents (filename, str->str, -1, &error) && error) {
				g_warning (
					"%s: cannot write file content, "
					"error: %s", G_STRFUNC, error->message);
				g_error_free (error);
			}

			g_string_free (str, TRUE);
		} else {
			g_warning (
				"%s: Replace of '%s' to '%s' failed!",
				G_STRFUNC, find, replace);
		}

		g_free (content);

	} else if (error != NULL) {
		g_warning (
			"%s: Cannot read file content, error: %s",
			G_STRFUNC, error->message);
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
		GString *str = replace_variables (cmd, FALSE);

		if (str) {
			print_and_run (str->str);
			g_string_free (str, TRUE);
		}
	} else
		print_and_run (cmd);
}

static void
run_evolution_no_wait (void)
{
	g_spawn_command_line_async (EVOLUTION, NULL);
}

static void
write_dir_file (void)
{
	GString *content, *filename;
	GError *error = NULL;

	filename = replace_variables ("$HOME/" EVOLUTION_DIR_FILE, TRUE);
	g_return_if_fail (filename != NULL);

	content = replace_variables (
		"[" KEY_FILE_GROUP "]\n"
		"Version=" VERSION "\n"
		"UserDataDir=$STRIPDATADIR\n"
		"UserConfigDir=$STRIPCONFIGDIR\n"
		, TRUE);
	g_return_if_fail (content != NULL);

	g_file_set_contents (filename->str, content->str, content->len, &error);

	if (error != NULL) {
		g_warning ("Failed to write file '%s': %s\n", filename->str, error->message);
		g_error_free (error);
	}

	g_string_free (filename, TRUE);
	g_string_free (content, TRUE);
}

static gboolean
get_filename_is_xz (const gchar *filename)
{
	gint len;

	if (!filename)
		return FALSE;

	len = strlen (filename);
	if (len < 3)
		return FALSE;

	return g_ascii_strcasecmp (filename + len - 3, ".xz") == 0;
}

static void
backup (const gchar *filename,
        GCancellable *cancellable)
{
	gchar *command;
	gchar *quotedfname;
	gboolean use_xz;

	g_return_if_fail (filename && *filename);

	if (g_cancellable_is_cancelled (cancellable))
		return;

	txt = _("Shutting down Evolution");
	/* FIXME Will the versioned setting always work? */
	run_cmd (EVOLUTION " --quit");

	run_cmd ("rm $DATADIR/.running");

	if (g_cancellable_is_cancelled (cancellable))
		return;

	txt = _("Backing Evolution accounts and settings");
	run_cmd ("dconf dump " DCONF_PATH_EDS " >" EVOLUTION_DIR DCONF_DUMP_FILE_EDS);
	run_cmd ("dconf dump " DCONF_PATH_EVO " >" EVOLUTION_DIR DCONF_DUMP_FILE_EVO);

	replace_in_file (
		EVOLUTION_DIR DCONF_DUMP_FILE_EDS,
		e_get_user_data_dir (), EVOUSERDATADIR_MAGIC);

	replace_in_file (
		EVOLUTION_DIR DCONF_DUMP_FILE_EVO,
		e_get_user_data_dir (), EVOUSERDATADIR_MAGIC);

	write_dir_file ();

	if (g_cancellable_is_cancelled (cancellable))
		return;

	txt = _("Backing Evolution data (Mails, Contacts, Calendar, Tasks, Memos)");

	quotedfname = g_shell_quote (filename);
	use_xz = get_filename_is_xz (filename);

	command = g_strdup_printf (
		"cd $HOME && tar chf - $STRIPDATADIR "
		"$STRIPCONFIGDIR " EVOLUTION_DIR_FILE " | "
		"%s > %s", use_xz ? "xz -z" : "gzip", quotedfname);
	run_cmd (command);

	g_free (command);
	g_free (quotedfname);

	run_cmd ("rm $HOME/" EVOLUTION_DIR_FILE);

	txt = _("Back up complete");

	if (restart_arg) {

		if (g_cancellable_is_cancelled (cancellable))
			return;

		txt = _("Restarting Evolution");
		run_evolution_no_wait ();
	}

}

static void
extract_backup_data (const gchar *filename,
                     gchar **restored_version,
                     gchar **data_dir,
                     gchar **config_dir)
{
	GKeyFile *key_file;
	GError *error = NULL;

	g_return_if_fail (filename != NULL);
	g_return_if_fail (data_dir != NULL);
	g_return_if_fail (config_dir != NULL);

	key_file = g_key_file_new ();
	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, &error);

	if (error != NULL) {
		g_warning ("Failed to read '%s': %s", filename, error->message);
		g_error_free (error);

	/* This is the current format as of Evolution 3.6. */
	} else if (g_key_file_has_group (key_file, KEY_FILE_GROUP)) {
		gchar *tmp;

		tmp = g_key_file_get_value (
			key_file, KEY_FILE_GROUP, "Version", NULL);
		if (tmp != NULL)
			*restored_version = g_strstrip (g_strdup (tmp));
		g_free (tmp);

		tmp = g_key_file_get_value (
			key_file, KEY_FILE_GROUP, "UserDataDir", NULL);
		if (tmp != NULL)
			*data_dir = g_shell_quote (tmp);
		g_free (tmp);

		tmp = g_key_file_get_value (
			key_file, KEY_FILE_GROUP, "UserConfigDir", NULL);
		if (tmp != NULL)
			*config_dir = g_shell_quote (tmp);
		g_free (tmp);

	/* This is the legacy format with no version information. */
	} else if (g_key_file_has_group (key_file, "dirs")) {
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

static gchar *
get_source_manager_reload_command (void)
{
	GString *tmp;
	gchar *command;

	tmp = replace_variables (DBUS_SOURCE_REGISTRY_SERVICE_FILE, TRUE);
	if (tmp) {
		GKeyFile *key_file;
		gchar *str = NULL;

		key_file = g_key_file_new ();
		if (g_key_file_load_from_file (key_file, tmp->str, G_KEY_FILE_NONE, NULL)) {
			str = g_key_file_get_string (key_file, "D-BUS Service", "Name", NULL);
		}
		g_key_file_free (key_file);

		if (str && *str) {
			g_string_assign (tmp, str);
		} else {
			g_string_free (tmp, TRUE);
			tmp = NULL;
		}

		g_free (str);
	}

	if (!tmp)
		tmp = g_string_new ("org.gnome.evolution.dataserver.Sources0");

	command = g_strdup_printf ("gdbus call --session --dest %s "
		"--object-path /org/gnome/evolution/dataserver/SourceManager "
		"--method org.gnome.evolution.dataserver.SourceManager.Reload",
		tmp->str);

	g_string_free (tmp, TRUE);

	return command;
}

static void
unset_eds_migrated_flag (void)
{
	GSettings *settings;

	settings = g_settings_new ("org.gnome.evolution-data-server");
	g_settings_set_boolean (settings, "migrated", FALSE);
	g_object_unref (settings);
}

static void
restore (const gchar *filename,
         GCancellable *cancellable)
{
	gchar *command;
	gchar *quotedfname;
	gboolean is_new_format = FALSE;

	g_return_if_fail (filename && *filename);

	if (!check (filename, &is_new_format)) {
		g_message ("Cannot restore from an incorrect archive '%s'.", filename);
		goto end;
	}

	quotedfname = g_shell_quote (filename);

	if (g_cancellable_is_cancelled (cancellable))
		return;

	/* FIXME Will the versioned setting always work? */
	txt = _("Shutting down Evolution");
	run_cmd (EVOLUTION " --quit");

	if (g_cancellable_is_cancelled (cancellable))
		return;

	txt = _("Back up current Evolution data");
	run_cmd ("mv $DATADIR $DATADIR_old");
	run_cmd ("mv $CONFIGDIR $CONFIGDIR_old");

	if (g_cancellable_is_cancelled (cancellable))
		return;

	txt = _("Extracting files from back up");

	if (is_new_format) {
		GString *dir_fn;
		gchar *data_dir = NULL;
		gchar *config_dir = NULL;
		gchar *restored_version = NULL;
		const gchar *tar_opts;

		if (get_filename_is_xz (filename))
			tar_opts = "-xJf";
		else
			tar_opts = "-xzf";

		command = g_strdup_printf (
			"cd $TMP && tar %s %s " EVOLUTION_DIR_FILE,
			tar_opts, quotedfname);
		run_cmd (command);
		g_free (command);

		dir_fn = replace_variables ("$TMP" G_DIR_SEPARATOR_S EVOLUTION_DIR_FILE, TRUE);
		if (!dir_fn) {
			g_warning ("Failed to create evolution's dir filename");
			goto end;
		}

		/* data_dir and config_dir are quoted inside extract_backup_data */
		extract_backup_data (
			dir_fn->str,
			&restored_version,
			&data_dir,
			&config_dir);

		g_unlink (dir_fn->str);
		g_string_free (dir_fn, TRUE);

		if (!data_dir || !config_dir) {
			g_warning (
				"Failed to get old data_dir (%p)/"
				"config_dir (%p)", data_dir, config_dir);
			g_free (data_dir);
			g_free (config_dir);
			goto end;
		}

		g_mkdir_with_parents (e_get_user_data_dir (), 0700);
		g_mkdir_with_parents (e_get_user_config_dir (), 0700);

		command = g_strdup_printf (
			"cd $DATADIR && tar --strip-components %d %s %s %s",
			 get_dir_level (data_dir), tar_opts, quotedfname, data_dir);
		run_cmd (command);
		g_free (command);

		command = g_strdup_printf (
			"cd $CONFIGDIR && tar --strip-components %d %s %s %s",
			get_dir_level (config_dir), tar_opts, quotedfname, config_dir);
		run_cmd (command);
		g_free (command);

		/* If the back file had version information, set the last
		 * used version in GSettings before restarting Evolution. */
		if (restored_version != NULL && *restored_version != '\0') {
			GSettings *settings;

			settings = e_util_ref_settings ("org.gnome.evolution");
			g_settings_set_string (
				settings, "version", restored_version);
			g_object_unref (settings);
		}

		g_free (data_dir);
		g_free (config_dir);
		g_free (restored_version);
	} else {
		const gchar *decr_opts;

		if (get_filename_is_xz (filename))
			decr_opts = "xz -cd";
		else
			decr_opts = "gzip -cd";

		run_cmd ("mv $HOME/.evolution $HOME/.evolution_old");

		command = g_strdup_printf (
			"cd $HOME && %s %s | tar xf -", decr_opts, quotedfname);
		run_cmd (command);
		g_free (command);
	}

	g_free (quotedfname);

	if (g_cancellable_is_cancelled (cancellable))
		return;

	txt = _("Loading Evolution settings");

	if (is_new_format) {
		/* new format has it in DATADIR... */
		GString *file = replace_variables (EVOLUTION_DIR ANCIENT_GCONF_DUMP_FILE, TRUE);
		if (file && g_file_test (file->str, G_FILE_TEST_EXISTS)) {
			unset_eds_migrated_flag ();

			/* ancient backup */
			replace_in_file (
				EVOLUTION_DIR ANCIENT_GCONF_DUMP_FILE,
				EVOUSERDATADIR_MAGIC, e_get_user_data_dir ());
			run_cmd ("gconftool-2 --load " EVOLUTION_DIR ANCIENT_GCONF_DUMP_FILE);

			/* give a chance to GConf to save what was loaded into a disk */
			g_usleep (G_USEC_PER_SEC * 5);

			/* do not forget to convert GConf keys into GSettings */
			run_cmd ("gsettings-data-convert");
			run_cmd ("rm " EVOLUTION_DIR ANCIENT_GCONF_DUMP_FILE);
		} else {
			replace_in_file (
				EVOLUTION_DIR DCONF_DUMP_FILE_EDS,
				EVOUSERDATADIR_MAGIC, e_get_user_data_dir ());
			run_cmd ("cat " EVOLUTION_DIR DCONF_DUMP_FILE_EDS " | dconf load " DCONF_PATH_EDS);
			run_cmd ("rm " EVOLUTION_DIR DCONF_DUMP_FILE_EDS);

			replace_in_file (
				EVOLUTION_DIR DCONF_DUMP_FILE_EVO,
				EVOUSERDATADIR_MAGIC, e_get_user_data_dir ());
			run_cmd ("cat " EVOLUTION_DIR DCONF_DUMP_FILE_EVO " | dconf load " DCONF_PATH_EVO);
			run_cmd ("rm " EVOLUTION_DIR DCONF_DUMP_FILE_EVO);
		}

		g_string_free (file, TRUE);
	} else {
		gchar *gconf_dump_file;

		unset_eds_migrated_flag ();

		/* ... old format in ~/.evolution */
		gconf_dump_file = g_build_filename (
			"$HOME", ".evolution", ANCIENT_GCONF_DUMP_FILE, NULL);

		replace_in_file (
			gconf_dump_file,
			EVOUSERDATADIR_MAGIC,
			e_get_user_data_dir ());

		command = g_strconcat (
			"gconftool-2 --load ", gconf_dump_file, NULL);
		run_cmd (command);
		g_free (command);

		/* give a chance to GConf to save what was loaded into a disk */
		g_usleep (G_USEC_PER_SEC * 5);

		/* do not forget to convert GConf keys into GSettings */
		run_cmd ("gsettings-data-convert");

		command = g_strconcat ("rm ", gconf_dump_file, NULL);
		run_cmd (command);
		g_free (command);

		g_free (gconf_dump_file);
	}

	if (g_cancellable_is_cancelled (cancellable))
		return;

	txt = _("Removing temporary back up files");
	run_cmd ("rm -rf $DATADIR_old");
	run_cmd ("rm -rf $CONFIGDIR_old");
	run_cmd ("rm $DATADIR/.running");

	if (!is_new_format)
		run_cmd ("rm -rf $HOME/.evolution_old");

	if (g_cancellable_is_cancelled (cancellable))
		return;

	/* Make full-restart background processes after restore */
	run_cmd (EVOLUTION " --force-shutdown");

	txt = _("Reloading registry service");

	/* wait few seconds, till changes settle */
	g_usleep (G_USEC_PER_SEC * 5);

	command = get_source_manager_reload_command ();
	/* This runs migration routines on the newly-restored data. */
	run_cmd (command);
	g_free (command);

end:
	if (restart_arg) {
		if (g_cancellable_is_cancelled (cancellable))
			return;

		txt = _("Restarting Evolution");

		/* wait 5 seconds before restarting evolution, thus any
		 * changes being done are updated in source registry too */
		g_usleep (G_USEC_PER_SEC * 5);

		run_evolution_no_wait ();
	}
}

static gboolean
check (const gchar *filename,
       gboolean *is_new_format)
{
	gchar *command;
	gchar *quotedfname;
	const gchar *tar_opts;
	gboolean is_new = TRUE;

	g_return_val_if_fail (filename && *filename, FALSE);

	if (get_filename_is_xz (filename))
		tar_opts = "-tJf";
	else
		tar_opts = "-tzf";

	quotedfname = g_shell_quote (filename);

	if (is_new_format)
		*is_new_format = FALSE;

	command = g_strdup_printf ("tar %s %s 1>/dev/null", tar_opts, quotedfname);
	result = system (command);
	g_free (command);

	g_message ("First result %d", result);
	if (result) {
		g_free (quotedfname);
		return FALSE;
	}

	command = g_strdup_printf (
		"tar %s %s | grep -e \"%s$\"",
		tar_opts, quotedfname, EVOLUTION_DIR_FILE);
	result = system (command);
	g_free (command);

	if (result) {
		command = g_strdup_printf (
			"tar %s %s | grep -e \"^\\.evolution/$\"",
			tar_opts, quotedfname);
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

	command = g_strdup_printf (
		"tar %s %s | grep -e \"^\\.evolution/%s$\"",
		tar_opts, quotedfname, ANCIENT_GCONF_DUMP_FILE);
	result = system (command);
	g_free (command);

	if (result != 0) {
		/* maybe it's an ancient backup */
		command = g_strdup_printf (
			"tar %s %s | grep -e \"^\\.evolution/%s$\"",
			tar_opts, quotedfname, ANCIENT_GCONF_DUMP_FILE);
		result = system (command);
		g_free (command);
	}

	g_free (quotedfname);

	g_message ("Third result %d", result);

	return result == 0;
}

static gboolean
pbar_update (gpointer user_data)
{
	GCancellable *cancellable = G_CANCELLABLE (user_data);

	gtk_progress_bar_pulse ((GtkProgressBar *) pbar);
	gtk_progress_bar_set_text ((GtkProgressBar *) pbar, txt);

	/* Return TRUE to reschedule the timeout. */
	return !g_cancellable_is_cancelled (cancellable);
}

static gboolean
finish_job (gpointer user_data)
{
	gtk_main_quit ();

	return FALSE;
}

static void
start_job (GTask *task,
	   gpointer source_object,
	   gpointer task_data,
	   GCancellable *cancellable)
{
	if (backup_op)
		backup (bk_file, cancellable);
	else if (restore_op)
		restore (res_file, cancellable);
	else if (check_op)
		check (chk_file, NULL);  /* not cancellable */

	g_main_context_invoke (NULL, finish_job, NULL);
}

static void
dlg_response (GtkWidget *dlg,
              gint response,
              GCancellable *cancellable)
{
	/* We will cancel only backup/restore
	 * operations and not the check operation. */
	g_cancellable_cancel (cancellable);

	/* If the response is not of delete_event then destroy the event. */
	if (response != GTK_RESPONSE_NONE)
		gtk_widget_destroy (dlg);

	/* We will kill just the tar operation. Rest of
	 * them will be just a second of microseconds.*/
	run_cmd ("pkill tar");

	if (bk_file && backup_op && response == GTK_RESPONSE_REJECT) {
		/* Backup was cancelled, delete the
		 * backup file as it is not needed now. */
		gchar *cmd, *filename;

		g_message ("Back up cancelled, removing partial back up file.");

		filename = g_shell_quote (bk_file);
		cmd = g_strconcat ("rm ", filename, NULL);

		run_cmd (cmd);

		g_free (cmd);
		g_free (filename);
	}

	gtk_main_quit ();
}

gint
main (gint argc,
      gchar **argv)
{
	GTask *task;
	GCancellable *cancellable;
	gchar *file = NULL, *oper = NULL;
	const gchar *title = NULL;
	gint ii;
	GError *error = NULL;

#ifdef G_OS_WIN32
	e_util_win32_initialize ();
#endif

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init_with_args (
		&argc, &argv, NULL, options, GETTEXT_PACKAGE, &error);

	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}

	if (opt_remaining != NULL) {
		for (ii = 0; ii < g_strv_length (opt_remaining); ii++) {
			if (backup_op) {
				title = _("Evolution Back Up");
				oper = _("Backing up to the folder %s");
				bk_file = g_strdup ((gchar *) opt_remaining[ii]);
				file = bk_file;
			} else if (restore_op) {
				title = _("Evolution Restore");
				oper = _("Restoring from the folder %s");
				res_file = g_strdup ((gchar *) opt_remaining[ii]);
				file = res_file;
			} else if (check_op) {
				d (g_message ("Checking %s", (gchar *) opt_remaining[ii]));
				chk_file = g_strdup ((gchar *) opt_remaining[ii]);
			}
		}
	}

	cancellable = g_cancellable_new ();

	if (gui_arg && !check_op) {
		GtkWidget *widget, *container;
		GtkWidget *action_area;
		GtkWidget *content_area;
		const gchar *txt, *txt2;
		gchar *str = NULL;
		gchar *markup;

		gtk_window_set_default_icon_name ("evolution");

		/* Backup / Restore only can have GUI.
		 * We should restrict the rest. */
		progress_dialog = gtk_dialog_new_with_buttons (
			title, NULL,
			GTK_DIALOG_MODAL,
			_("_Cancel"), GTK_RESPONSE_REJECT,
			NULL);

		gtk_container_set_border_width (
			GTK_CONTAINER (progress_dialog), 12);

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
			str = g_strdup_printf (oper, file);

		container = gtk_grid_new ();
		gtk_grid_set_column_spacing (GTK_GRID (container), 6);
		gtk_grid_set_row_spacing (GTK_GRID (container), 0);
		gtk_widget_show (container);

		gtk_box_pack_start (
			GTK_BOX (content_area), container, FALSE, TRUE, 0);

		widget = gtk_image_new_from_icon_name (
			"edit-copy", GTK_ICON_SIZE_DIALOG);
		gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
		gtk_widget_show (widget);

		gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 3);
		g_object_set (
			G_OBJECT (widget),
			"halign", GTK_ALIGN_FILL,
			"valign", GTK_ALIGN_FILL,
			"vexpand", TRUE,
			NULL);

		if (backup_op) {
			txt = _("Backing up Evolution Data");
			txt2 = _("Please wait while Evolution is backing up your data.");
		} else if (restore_op) {
			txt = _("Restoring Evolution Data");
			txt2 = _("Please wait while Evolution is restoring your data.");
		} else {
			g_return_val_if_reached (EXIT_FAILURE);
		}

		markup = g_markup_printf_escaped ("<b><big>%s</big></b>", txt);
		widget = gtk_label_new (markup);
		gtk_label_set_line_wrap (GTK_LABEL (widget), FALSE);
		gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
		gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
		gtk_widget_show (widget);
		g_free (markup);

		gtk_grid_attach (GTK_GRID (container), widget, 1, 0, 1, 1);
		g_object_set (
			G_OBJECT (widget),
			"halign", GTK_ALIGN_FILL,
			"hexpand", TRUE,
			"valign", GTK_ALIGN_FILL,
			NULL);

		markup = g_strconcat (
			txt2, " ", _("This may take a while depending "
			"on the amount of data in your account."), NULL);
		widget = gtk_label_new (markup);
		gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
		gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
		gtk_widget_show (widget);
		g_free (markup);

		gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);
		g_object_set (
			G_OBJECT (widget),
			"halign", GTK_ALIGN_FILL,
			"hexpand", TRUE,
			"valign", GTK_ALIGN_FILL,
			NULL);

		pbar = gtk_progress_bar_new ();

		if (str != NULL) {
			markup = g_markup_printf_escaped ("<i>%s</i>", str);
			widget = gtk_label_new (markup);
			gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
			gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
			g_free (markup);
			g_free (str);

			gtk_grid_attach (GTK_GRID (container), widget, 1, 2, 1, 1);
			g_object_set (
				G_OBJECT (widget),
				"halign", GTK_ALIGN_FILL,
				"hexpand", TRUE,
				"valign", GTK_ALIGN_FILL,
				NULL);

			gtk_grid_attach (GTK_GRID (container), pbar, 1, 3, 1, 1);
		} else
			gtk_grid_attach (GTK_GRID (container), pbar, 1, 2, 1, 1);

		g_object_set (
			G_OBJECT (pbar),
			"halign", GTK_ALIGN_FILL,
			"hexpand", TRUE,
			"valign", GTK_ALIGN_FILL,
			NULL);

		g_signal_connect (
			progress_dialog, "response",
			G_CALLBACK (dlg_response), cancellable);
		gtk_widget_show_all (progress_dialog);

	} else if (check_op) {
		/* For sanity we don't need gui */
		check (chk_file, NULL);
		exit (result == 0 ? 0 : 1);
	}

	if (gui_arg) {
		e_named_timeout_add_full (
			G_PRIORITY_DEFAULT,
			50, pbar_update,
			g_object_ref (cancellable),
			(GDestroyNotify) g_object_unref);
	}

	task = g_task_new (cancellable, cancellable, NULL, NULL);
	g_task_run_in_thread (task, start_job);
	g_object_unref (task);

	gtk_main ();

	g_object_unref (cancellable);
	e_util_cleanup_settings ();

	return result;
}
