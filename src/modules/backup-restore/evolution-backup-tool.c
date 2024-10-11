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

#define ANCIENT_GCONF_DUMP_FILE "backup-restore-gconf.xml"

#define ANCIENT_DCONF_DUMP_FILE_EDS "backup-restore-dconf-eds.ini"
#define ANCIENT_DCONF_DUMP_FILE_EVO "backup-restore-dconf-evo.ini"

#define ANCIENT_DCONF_PATH_EDS "/org/gnome/evolution-data-server/"
#define ANCIENT_DCONF_PATH_EVO "/org/gnome/evolution/"

#define GSETTINGS_DUMP_FILE "backup-restore-gsettings.ini"

#define KEY_FILE_GROUP "Evolution Backup"

enum {
	RESULT_SUCCESS		= 0,
	RESULT_FAILED		= 1,
	RESULT_TAR_NOT_FOUND	= 2,
	RESULT_GZIP_NOT_FOUND	= 3,
	RESULT_XZ_NOT_FOUND	= 4
};

static gboolean backup_op = FALSE;
static gchar *bk_file = NULL;
static gboolean restore_op = FALSE;
static gchar *res_file = NULL;
static gboolean check_op = FALSE;
static gchar *chk_file = NULL;
static gboolean restart_arg = FALSE;
static gboolean gui_arg = FALSE;
static gchar **opt_remaining = NULL;
static gint result = RESULT_SUCCESS;
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

typedef gboolean (* SettingsFunc) (GSettings *settings,
				   GSettingsSchema *schema,
				   const gchar *group,
				   const gchar *key,
				   GKeyFile *keyfile);

static gboolean
settings_foreach_schema_traverse (GSettings *settings,
				  SettingsFunc func,
				  GKeyFile *keyfile)
{
	GSettingsSchema *schema = NULL;
	const gchar *group;
	gchar *schema_id = NULL, *path = NULL, *group_tmp = NULL;
	gchar **strv;
	gint ii;
	gboolean need_sync = FALSE;

	g_object_get (G_OBJECT (settings),
		"settings-schema", &schema,
		"schema-id", &schema_id,
		"path", &path,
		NULL);

	if (!g_settings_schema_get_path (schema)) {
		group_tmp = g_strconcat (schema_id, ":", path, NULL);
		group = group_tmp;
	} else {
		group = schema_id;
	}

	strv = g_settings_schema_list_keys (schema);

	for (ii = 0; strv && strv[ii]; ii++) {
		need_sync = func (settings, schema, group, strv[ii], keyfile) || need_sync;
	}

	g_strfreev (strv);

	strv = g_settings_schema_list_children (schema);
	for (ii = 0; strv && strv[ii]; ii++) {
		GSettings *child;

		child = g_settings_get_child (settings, strv[ii]);
		if (child) {
			if (settings_foreach_schema_traverse (child, func, keyfile))
				g_settings_sync ();
			g_object_unref (child);
		}
	}

	g_strfreev (strv);

	g_settings_schema_unref (schema);
	g_free (group_tmp);
	g_free (schema_id);
	g_free (path);

	return need_sync;
}

static gboolean
settings_foreach_schema (SettingsFunc func,
			 GKeyFile *keyfile,
			 GError **error)
{
	GSettingsSchemaSource *schema_source;
	gchar **non_relocatable = NULL, **relocatable = NULL;
	gint ii;

	schema_source = g_settings_schema_source_get_default ();
	if (!schema_source) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "No GSettings schema found");
		return FALSE;
	}

	g_settings_schema_source_list_schemas (schema_source, TRUE, &non_relocatable, &relocatable);

	for (ii = 0; non_relocatable && non_relocatable[ii]; ii++) {
		if (g_str_has_prefix (non_relocatable[ii], "org.gnome.evolution")) {
			GSettings *settings;

			settings = g_settings_new (non_relocatable[ii]);
			if (settings_foreach_schema_traverse (settings, func, keyfile))
				g_settings_sync ();
			g_object_unref (settings);
		}
	}

	g_strfreev (non_relocatable);
	g_strfreev (relocatable);

	return TRUE;
}

static gboolean
backup_settings_foreach_cb (GSettings *settings,
			    GSettingsSchema *schema,
			    const gchar *group,
			    const gchar *key,
			    GKeyFile *keyfile)
{
	GVariant *variant;

	variant = g_settings_get_user_value (settings, key);

	if (variant) {
		gchar *tmp;

		tmp = g_variant_print (variant, TRUE);
		g_key_file_set_string (keyfile, group, key, tmp);
		g_free (tmp);

		g_variant_unref (variant);
	}

	return FALSE;
}

static gboolean
backup_settings (const gchar *to_filename,
		 GError **error)
{
	GKeyFile *keyfile;
	GString *filename;
	gboolean success;

	filename = replace_variables (to_filename, TRUE);

	if (!filename) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to construct file name");
		return FALSE;
	}

	keyfile = g_key_file_new ();

	if (!settings_foreach_schema (backup_settings_foreach_cb, keyfile, error)) {
		g_string_free (filename, TRUE);
		g_key_file_free (keyfile);
		return FALSE;
	}

	success = g_key_file_save_to_file (keyfile, filename->str, error);

	g_string_free (filename, TRUE);
	g_key_file_free (keyfile);

	return success;
}

static gboolean
restore_settings_foreach_cb (GSettings *settings,
			     GSettingsSchema *schema,
			     const gchar *group,
			     const gchar *key,
			     GKeyFile *keyfile)
{
	gchar *value;

	if (!g_settings_is_writable (settings, key))
		return FALSE;

	value = g_key_file_get_string (keyfile, group, key, NULL);

	if (value) {
		GVariant *variant;

		variant = g_variant_parse (NULL, value, NULL, NULL, NULL);

		if (variant) {
			GSettingsSchemaKey *schema_key;

			schema_key = g_settings_schema_get_key (schema, key);

			if (g_settings_schema_key_range_check (schema_key, variant))
				g_settings_set_value (settings, key, variant);

			g_settings_schema_key_unref (schema_key);
			g_variant_unref (variant);
		}

		g_free (value);
	} else {
		g_settings_reset (settings, key);
	}

	return TRUE;
}

static gboolean
restore_settings (const gchar *from_filename,
		  GError **error)
{
	GKeyFile *keyfile;
	GString *filename;
	gboolean success;

	filename = replace_variables (from_filename, TRUE);

	if (!filename) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to construct file name");
		return FALSE;
	}

	keyfile = g_key_file_new ();

	if (!g_key_file_load_from_file (keyfile, filename->str, G_KEY_FILE_NONE, error)) {
		g_string_free (filename, TRUE);
		g_key_file_free (keyfile);
		return FALSE;
	}

	success = settings_foreach_schema (restore_settings_foreach_cb, keyfile, error);

	g_string_free (filename, TRUE);
	g_key_file_free (keyfile);

	return success;
}

static gboolean
check_prog_exists (const gchar *prog)
{
	gchar *path;

	path = g_find_program_in_path (prog);

	if (!path) {
		g_warning ("Program '%s' does not exist", prog);

		result = RESULT_FAILED;

		if (g_strcmp0 (prog, "tar") == 0)
			result = RESULT_TAR_NOT_FOUND;
		else if (g_strcmp0 (prog, "gzip") == 0)
			result = RESULT_GZIP_NOT_FOUND;
		else if (g_strcmp0 (prog, "xz") == 0)
			result = RESULT_XZ_NOT_FOUND;
		else
			g_warning ("%s: Called with unhandled program '%s'", G_STRFUNC, prog);

		return FALSE;
	}

	g_free (path);

	return TRUE;
}

static void
backup (const gchar *filename,
        GCancellable *cancellable)
{
	gchar *command;
	gchar *quotedfname;
	gboolean use_xz;
	GError *error = NULL;

	g_return_if_fail (filename && *filename);

	if (g_cancellable_is_cancelled (cancellable))
		return;

	if (!check_prog_exists ("tar"))
		return;

	use_xz = get_filename_is_xz (filename);
	if (use_xz) {
		if (!check_prog_exists ("xz"))
			return;
	} else {
		if (!check_prog_exists ("gzip"))
			return;
	}

	txt = _("Shutting down Evolution");
	/* FIXME Will the versioned setting always work? */
	run_cmd (EVOLUTION " --quit");

	run_cmd ("rm $CONFIGDIR/.running");

	if (g_cancellable_is_cancelled (cancellable))
		return;

	txt = _("Backing Evolution accounts and settings");
	if (!backup_settings (EVOLUTION_DIR GSETTINGS_DUMP_FILE, &error)) {
		g_warning ("Failed to backup settings: %s", error ? error->message : "Unknown error");
		g_clear_error (&error);
	}

	replace_in_file (
		EVOLUTION_DIR GSETTINGS_DUMP_FILE,
		e_get_user_data_dir (), EVOUSERDATADIR_MAGIC);

	write_dir_file ();

	if (g_cancellable_is_cancelled (cancellable))
		return;

	txt = _("Backing Evolution data (Mails, Contacts, Calendar, Tasks, Memos)");

	quotedfname = g_shell_quote (filename);

	command = g_strdup_printf (
		"cd $HOME && tar chf - $STRIPDATADIR "
		"$STRIPCONFIGDIR " EVOLUTION_DIR_FILE " | "
		"%s > %s", use_xz ? "xz -z" : "gzip", quotedfname);
	run_cmd (command);

	g_free (command);
	g_free (quotedfname);

	run_cmd ("rm $HOME/" EVOLUTION_DIR_FILE);

	txt = _("Checking validity of the archive");

	if (!check (filename, NULL)) {
		g_message ("Failed to validate content of '%s', the archive is broken", filename);
		return;
	}

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

#define DEFAULT_SOURCES_DBUS_NAME "org.gnome.evolution.dataserver.Sources0"

static gchar *
get_source_manager_reload_command (void)
{
	GString *tmp;
	gchar *command;

	tmp = replace_variables ("$DBUSDATADIR", TRUE);

	if (tmp) {
		/* The service file is named based on the service name, thus search for it in the D-Bus directory. */
		GDir *dir;

		dir = g_dir_open (tmp->str, 0, NULL);

		if (dir) {
			gchar *base_filename;
			gint base_filename_len;

			g_string_free (tmp, TRUE);
			tmp = NULL;

			base_filename = g_strdup (EDS_SOURCES_DBUS_SERVICE_NAME);

			if (!base_filename || !*base_filename) {
				g_free (base_filename);
				base_filename = g_strdup (DEFAULT_SOURCES_DBUS_NAME);
			}

			base_filename_len = strlen (base_filename);

			while (base_filename_len > 0 && base_filename[base_filename_len - 1] >= '0' && base_filename[base_filename_len - 1] <= '9') {
				base_filename_len--;
				base_filename[base_filename_len] = '\0';
			}

			while (!tmp) {
				const gchar *name;

				name = g_dir_read_name (dir);

				if (!name)
					break;

				if (g_ascii_strncasecmp (name, base_filename, base_filename_len) == 0 &&
				    g_ascii_strncasecmp (name + strlen (name) - 8, ".service", 8) == 0) {
					gchar *filename;

					filename = g_strconcat ("$DBUSDATADIR", G_DIR_SEPARATOR_S, name, NULL);
					tmp = replace_variables (filename, TRUE);
					g_free (filename);

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
				}
			}

			g_free (base_filename);
			g_dir_close (dir);
		} else {
			g_string_free (tmp, TRUE);
			tmp = NULL;
		}
	}

	command = g_strdup_printf ("gdbus call --session --dest %s "
		"--object-path /org/gnome/evolution/dataserver/SourceManager "
		"--method org.gnome.evolution.dataserver.SourceManager.Reload",
		tmp ? tmp->str : DEFAULT_SOURCES_DBUS_NAME);

	if (tmp)
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
			if (file)
				g_string_free (file, TRUE);

			file = replace_variables (EVOLUTION_DIR ANCIENT_DCONF_DUMP_FILE_EDS, TRUE);

			if (file && g_file_test (file->str, G_FILE_TEST_EXISTS)) {
				replace_in_file (
					EVOLUTION_DIR ANCIENT_DCONF_DUMP_FILE_EDS,
					EVOUSERDATADIR_MAGIC, e_get_user_data_dir ());
				run_cmd ("cat " EVOLUTION_DIR ANCIENT_DCONF_DUMP_FILE_EDS " | dconf load " ANCIENT_DCONF_PATH_EDS);
				run_cmd ("rm " EVOLUTION_DIR ANCIENT_DCONF_DUMP_FILE_EDS);

				replace_in_file (
					EVOLUTION_DIR ANCIENT_DCONF_DUMP_FILE_EVO,
					EVOUSERDATADIR_MAGIC, e_get_user_data_dir ());
				run_cmd ("cat " EVOLUTION_DIR ANCIENT_DCONF_DUMP_FILE_EVO " | dconf load " ANCIENT_DCONF_PATH_EVO);
				run_cmd ("rm " EVOLUTION_DIR ANCIENT_DCONF_DUMP_FILE_EVO);
			} else {
				GError *error = NULL;

				if (!restore_settings (EVOLUTION_DIR GSETTINGS_DUMP_FILE, &error)) {
					g_warning ("Failed to restore settings: %s", error ? error->message : "Unknown error");
					g_clear_error (&error);
				}
				run_cmd ("rm " EVOLUTION_DIR GSETTINGS_DUMP_FILE);
			}
		}

		if (file)
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
	run_cmd ("rm $CONFIGDIR/.running");

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

	if (!check_prog_exists ("tar"))
		return FALSE;

	if (get_filename_is_xz (filename)) {
		if (!check_prog_exists ("xz"))
			return FALSE;
		tar_opts = "-tJf";
	} else {
		if (!check_prog_exists ("gzip"))
			return FALSE;
		tar_opts = "-tzf";
	}

	quotedfname = g_shell_quote (filename);

	if (is_new_format)
		*is_new_format = FALSE;

	command = g_strdup_printf ("tar %s %s 1>/dev/null", tar_opts, quotedfname);
	result = system (command);
	g_free (command);

	g_message ("First result %d", result);
	if (result) {
		result = RESULT_FAILED;
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
		result = RESULT_FAILED;
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

	if (result != RESULT_SUCCESS)
		result = RESULT_FAILED;

	return result == RESULT_SUCCESS;
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
	if (backup_op && result != 0) {
		GtkWidget *widget;

		widget = gtk_message_dialog_new (progress_dialog ? GTK_WINDOW (progress_dialog) : NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			_("Failed to verify the backup file “%s”, the archive is broken."),
			bk_file);
		gtk_dialog_run (GTK_DIALOG (widget));
		gtk_widget_destroy (widget);
	}

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

	/* to pair the tool with the main app in the desktop environment (like in the GNOME shell) */
	g_set_prgname ("org.gnome.Evolution");

	gtk_init_with_args (
		&argc, &argv, NULL, options, GETTEXT_PACKAGE, &error);

	if (error != NULL) {
		if (gui_arg)
			g_printerr ("Failed to initialize gtk+. Do not use --gui, to run without it. Reported error: %s\n", error->message);

		g_clear_error (&error);

		if (gui_arg)
			exit (EXIT_FAILURE);
	}

	if (opt_remaining != NULL) {
		for (ii = 0; ii < g_strv_length (opt_remaining); ii++) {
			if (backup_op) {
				title = _("Evolution Back Up");
				oper = _("Backing up to the file %s");
				bk_file = g_strdup ((gchar *) opt_remaining[ii]);
				file = bk_file;
			} else if (restore_op) {
				title = _("Evolution Restore");
				oper = _("Restoring from the file %s");
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
		const gchar *txt1, *txt2;
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
		gtk_widget_show (widget);

		gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 3);
		g_object_set (
			G_OBJECT (widget),
			"halign", GTK_ALIGN_START,
			"valign", GTK_ALIGN_START,
			"vexpand", TRUE,
			NULL);

		if (backup_op) {
			txt1 = _("Backing up Evolution Data");
			txt2 = _("Please wait while Evolution is backing up your data.");
		} else if (restore_op) {
			txt1 = _("Restoring Evolution Data");
			txt2 = _("Please wait while Evolution is restoring your data.");
		} else {
			g_return_val_if_reached (EXIT_FAILURE);
		}

		markup = g_markup_printf_escaped ("<b><big>%s</big></b>", txt1);
		widget = gtk_label_new (markup);
		gtk_label_set_line_wrap (GTK_LABEL (widget), FALSE);
		gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
		gtk_label_set_xalign (GTK_LABEL (widget), 0);
		gtk_label_set_yalign (GTK_LABEL (widget), 0);
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
		gtk_label_set_width_chars (GTK_LABEL (widget), 20);
		gtk_label_set_xalign (GTK_LABEL (widget), 0);
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
			gtk_label_set_xalign (GTK_LABEL (widget), 0);
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
			"show-text", TRUE,
			NULL);

		g_signal_connect (
			progress_dialog, "response",
			G_CALLBACK (dlg_response), cancellable);
		gtk_widget_show_all (progress_dialog);

	} else if (check_op) {
		/* For sanity we don't need gui */
		check (chk_file, NULL);
		exit (result);
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
	e_misc_util_free_global_memory ();

	return result;
}
