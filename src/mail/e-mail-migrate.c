/*
 * e-mail-migrate.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-mail-migrate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>
#include <errno.h>
#include <ctype.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <shell/e-shell.h>
#include <shell/e-shell-migrate.h>

#include <libemail-engine/libemail-engine.h>

#include "e-mail-backend.h"
#include "em-utils.h"

#define d(x) x

/* 1.4 upgrade functions */

static GtkProgressBar *progress;

static void
em_migrate_set_progress (double percent)
{
	gchar text[5];

	snprintf (text, sizeof (text), "%d%%", (gint) (percent * 100.0f));

	gtk_progress_bar_set_fraction (progress, percent);
	gtk_progress_bar_set_text (progress, text);

	while (gtk_events_pending ())
		g_main_context_iteration (NULL, TRUE);
}

enum {
	CP_UNIQUE = 0,
	CP_OVERWRITE,
	CP_APPEND
};

static gint open_flags[3] = {
	O_WRONLY | O_CREAT | O_TRUNC,
	O_WRONLY | O_CREAT | O_TRUNC,
	O_WRONLY | O_CREAT | O_APPEND,
};

static gboolean
cp (const gchar *src,
    const gchar *dest,
    gboolean show_progress,
    gint mode)
{
	const gint nreadbuf = 65535;
	guchar *readbuf = NULL;
	gssize nread, nwritten;
	gint errnosav, readfd, writefd;
	gsize total = 0;
	struct stat st;
	struct utimbuf ut;

	/* if the dest file exists and has content, abort - we don't
	 * want to corrupt their existing data */
	if (g_stat (dest, &st) == 0 && st.st_size > 0 && mode == CP_UNIQUE) {
		errno = EEXIST;
		return FALSE;
	}

	if (g_stat (src, &st) == -1
	    || (readfd = g_open (src, O_RDONLY | O_BINARY, 0)) == -1)
		return FALSE;

	if ((writefd = g_open (dest, open_flags[mode] | O_BINARY, 0666)) == -1) {
		errnosav = errno;
		close (readfd);
		errno = errnosav;
		return FALSE;
	}

	readbuf = g_new0 (guchar, nreadbuf);
	do {
		do {
			nread = read (readfd, readbuf, nreadbuf);
		} while (nread == -1 && errno == EINTR);

		if (nread == 0)
			break;
		else if (nread < 0)
			goto exception;

		do {
			nwritten = write (writefd, readbuf, nread);
		} while (nwritten == -1 && errno == EINTR);

		if (nwritten < nread)
			goto exception;

		total += nwritten;
		if (show_progress)
			em_migrate_set_progress (((gdouble) total) / ((gdouble) st.st_size));
	} while (total < st.st_size);

	#ifndef G_OS_WIN32
	if (fsync (writefd) == -1)
		goto exception;
	#endif

	close (readfd);
	if (close (writefd) == -1)
		goto failclose;

	ut.actime = st.st_atime;
	ut.modtime = st.st_mtime;
	utime (dest, &ut);
	if (chmod (dest, st.st_mode) == -1) {
		g_warning ("%s: Failed to chmod '%s': %s", G_STRFUNC, dest, g_strerror (errno));
	}

	g_free (readbuf);

	return TRUE;

 exception:

	errnosav = errno;
	close (readfd);
	close (writefd);
	errno = errnosav;

 failclose:

	errnosav = errno;
	unlink (dest);
	errno = errnosav;

	g_free (readbuf);

	return FALSE;
}

static gboolean
emm_setup_initial (const gchar *data_dir)
{
	GDir *dir;
	const gchar *d;
	gchar *local = NULL, *base;
	const gchar * const *language_names;

	/* special-case - this means brand new install of evolution */
	/* FIXME: create default folders and stuff... */

	d (printf ("Setting up initial mail tree\n"));

	base = g_build_filename (data_dir, "local", NULL);
	if (g_mkdir_with_parents (base, 0700) == -1 && errno != EEXIST) {
		g_free (base);
		return FALSE;
	}

	/* e.g. try en-AU then en, etc */
	language_names = g_get_language_names ();
	while (*language_names != NULL) {
		local = g_build_filename (
			EVOLUTION_PRIVDATADIR, "default",
			*language_names, "mail", "local", NULL);
		if (g_file_test (local, G_FILE_TEST_EXISTS))
			break;
		g_free (local);
		local = NULL;
		language_names++;
	}

	/* Make sure we found one. */
	g_return_val_if_fail (local != NULL, FALSE);

	dir = g_dir_open (local, 0, NULL);
	if (dir) {
		while ((d = g_dir_read_name (dir))) {
			gchar *src, *dest;

			src = g_build_filename (local, d, NULL);
			dest = g_build_filename (base, d, NULL);

			cp (src, dest, FALSE, CP_UNIQUE);
			g_free (dest);
			g_free (src);
		}
		g_dir_close (dir);
	}

	g_free (local);
	g_free (base);

	return TRUE;
}

static void
em_rename_view_in_folder (gpointer data,
                          gpointer user_data)
{
	const gchar *filename = data;
	const gchar *views_dir = user_data;
	gchar *folderpos, *dotpos;

	g_return_if_fail (filename != NULL);
	g_return_if_fail (views_dir != NULL);

	folderpos = strstr (filename, "-folder:__");
	if (!folderpos)
		folderpos = strstr (filename, "-folder___");
	if (!folderpos)
		return;

	/* points on 'f' from the "folder" word */
	folderpos++;
	dotpos = strrchr (filename, '.');
	if (folderpos < dotpos && g_str_equal (dotpos, ".xml")) {
		GChecksum *checksum;
		gchar *oldname, *newname, *newfile;
		const gchar *md5_string;

		*dotpos = 0;

		/* use MD5 checksum of the folder URI, to not depend on its length */
		checksum = g_checksum_new (G_CHECKSUM_MD5);
		g_checksum_update (checksum, (const guchar *) folderpos, -1);

		*folderpos = 0;
		md5_string = g_checksum_get_string (checksum);
		newfile = g_strconcat (filename, md5_string, ".xml", NULL);
		*folderpos = 'f';
		*dotpos = '.';

		oldname = g_build_filename (views_dir, filename, NULL);
		newname = g_build_filename (views_dir, newfile, NULL);

		if (g_rename (oldname, newname) == -1) {
			g_warning (
				"%s: Failed to rename '%s' to '%s': %s", G_STRFUNC,
				oldname, newname, g_strerror (errno));
		}

		g_checksum_free (checksum);
		g_free (oldname);
		g_free (newname);
		g_free (newfile);
	}
}

static void
em_rename_folder_views (EShellBackend *shell_backend)
{
	const gchar *config_dir;
	gchar *views_dir;
	GDir *dir;

	g_return_if_fail (shell_backend != NULL);

	config_dir = e_shell_backend_get_config_dir (shell_backend);
	views_dir = g_build_filename (config_dir, "views", NULL);

	dir = g_dir_open (views_dir, 0, NULL);
	if (dir) {
		GSList *to_rename = NULL;
		const gchar *filename;

		while (filename = g_dir_read_name (dir), filename) {
			if (strstr (filename, "-folder:__") ||
			    strstr (filename, "-folder___"))
				to_rename = g_slist_prepend (to_rename, g_strdup (filename));
		}

		g_dir_close (dir);

		g_slist_foreach (to_rename, em_rename_view_in_folder, views_dir);
		g_slist_free_full (to_rename, g_free);
	}

	g_free (views_dir);
}

static gboolean
em_maybe_update_filter_rule_part (xmlNodePtr part)
{
	xmlNodePtr values;
	xmlChar *name, *value;

	name = xmlGetProp (part, (xmlChar *) "name");
	if (name) {
		if (g_strcmp0 ((const gchar *) name, "completed-on") != 0) {
			xmlFree (name);
			return FALSE;
		}

		xmlFree (name);
	} else {
		return FALSE;
	}

	xmlSetProp (part, (xmlChar *) "name", (xmlChar *) "follow-up");

	values = part->children;
	while (values) {
		if (g_strcmp0 ((const gchar *) values->name, "value") == 0) {
			name = xmlGetProp (values, (xmlChar *) "name");
			if (name) {
				if (g_strcmp0 ((const gchar *) name, "date-spec-type") == 0) {
					xmlSetProp (values, (xmlChar *) "name", (xmlChar *) "match-type");

					value = xmlGetProp (values, (xmlChar *) "value");
					if (value) {
						if (g_strcmp0 ((const gchar *) value, "is set") == 0)
							xmlSetProp (values, (xmlChar *) "value", (xmlChar *) "is completed");
						else if (g_strcmp0 ((const gchar *) value, "is not set") == 0)
							xmlSetProp (values, (xmlChar *) "value", (xmlChar *) "is not completed");

						xmlFree (value);
					}
				}

				xmlFree (name);
			}
		}

		values = values->next;
	}

	return TRUE;
}

static void
em_update_filter_rules_file (const gchar *filename)
{
	xmlNodePtr set, rule, root;
	xmlDocPtr doc;
	gboolean changed = FALSE;

	if (!filename || !*filename || !g_file_test (filename, G_FILE_TEST_IS_REGULAR))
		return;

	doc = e_xml_parse_file (filename);
	if (!doc)
		return;

	root = xmlDocGetRootElement (doc);
	set = root && g_strcmp0 ((const gchar *) root->name, "filteroptions") == 0 ? root->children : NULL;
	while (set) {
		if (g_strcmp0 ((const gchar *) set->name, "ruleset") == 0) {
			rule = set->children;
			while (rule) {
				if (g_strcmp0 ((const gchar *) rule->name, "rule") == 0) {
					xmlNodePtr partset;

					partset = rule->children;
					while (partset) {
						if (g_strcmp0 ((const gchar *) partset->name, "partset") == 0) {
							xmlNodePtr part;

							part = partset->children;
							while (part) {
								if (g_strcmp0 ((const gchar *) part->name, "part") == 0) {
									changed = em_maybe_update_filter_rule_part (part) || changed;
								}

								part = part->next;
							}
						}

						partset = partset->next;
					}
				}

				rule = rule->next;
			}
		}

		set = set->next;
	}

	if (changed)
		e_xml_save_file (filename, doc);

	xmlFreeDoc (doc);
}

static void
em_update_filter_rules (EShellBackend *shell_backend)
{
	const gchar *config_dir;
	gchar *filename;

	g_return_if_fail (shell_backend != NULL);

	config_dir = e_shell_backend_get_config_dir (shell_backend);

	filename = g_build_filename (config_dir, "filters.xml", NULL);
	em_update_filter_rules_file (filename);
	g_free (filename);

	filename = g_build_filename (config_dir, "searches.xml", NULL);
	em_update_filter_rules_file (filename);
	g_free (filename);

	filename = g_build_filename (config_dir, "vfolders.xml", NULL);
	em_update_filter_rules_file (filename);
	g_free (filename);
}

static void
unset_initial_setup_write_finished_cb (GObject *source_object,
				       GAsyncResult *result,
				       gpointer user_data)
{
	ESource *source;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_SOURCE (source_object));
	g_return_if_fail (result != NULL);

	source = E_SOURCE (source_object);

	if (!e_source_write_finish (source, result, &local_error)) {
		g_warning ("%s: Failed to save source '%s' (%s): %s", G_STRFUNC, e_source_get_uid (source),
			e_source_get_display_name (source), local_error ? local_error->message : "Unknown error");
	}

	g_clear_error (&local_error);
}

static void
em_unset_initial_setup_for_accounts (EShellBackend *shell_backend)
{
	ESourceRegistry *registry;
	GList *sources, *link;

	g_return_if_fail (E_IS_SHELL_BACKEND (shell_backend));

	registry = e_shell_get_registry (e_shell_backend_get_shell (shell_backend));
	sources = e_source_registry_list_sources (registry, E_SOURCE_EXTENSION_MAIL_ACCOUNT);

	for (link = sources; link; link = g_list_next (link)) {
		ESource *source = link->data;
		ESourceMailAccount *mail_account;

		mail_account = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT);
		if (e_source_mail_account_get_needs_initial_setup (mail_account)) {
			e_source_mail_account_set_needs_initial_setup (mail_account, FALSE);

			e_source_write (source, NULL, unset_initial_setup_write_finished_cb, NULL);
		}
	}

	g_list_free_full (sources, g_object_unref);
}

/* The default value for this key changed from 'false' to 'true' in 3.27.90,
   but existing users can be affected by the change when they never changed
   the option, thus make sure their value will remain 'false' here. */
static void
em_ensure_global_view_setting_key (EShellBackend *shell_backend)
{
	GSettings *settings;
	GVariant *value;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	value = g_settings_get_user_value (settings, "global-view-setting");
	if (value)
		g_variant_unref (value);
	else
		g_settings_set_boolean (settings, "global-view-setting", FALSE);

	g_clear_object (&settings);
}

gboolean
e_mail_migrate (EShellBackend *shell_backend,
                gint major,
                gint minor,
                gint micro,
                GError **error)
{
	const gchar *data_dir;

	data_dir = e_shell_backend_get_data_dir (shell_backend);

	if (major == 0)
		return emm_setup_initial (data_dir);

	if (major <= 2 || (major == 3 && minor < 4))
		em_rename_folder_views (shell_backend);

	if (major <= 2 || (major == 3 && minor < 17))
		em_update_filter_rules (shell_backend);

	if (major <= 2 || (major == 3 && minor < 19) || (major == 3 && minor == 19 && micro < 90))
		em_unset_initial_setup_for_accounts (shell_backend);

	if (major <= 2 || (major == 3 && minor < 27) || (major == 3 && minor == 27 && micro < 90))
		em_ensure_global_view_setting_key (shell_backend);

	return TRUE;
}
