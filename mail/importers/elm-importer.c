/*
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
 * Authors:
 *		Iain Holmes  <iain@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "mail-importer.h"

#include "mail/e-mail-backend.h"
#include "shell/e-shell.h"

#define d(x)

struct _elm_import_msg {
	MailMsg base;

	EImport *import;
	EImportTargetHome *target;

	GMutex status_lock;
	gchar *status_what;
	gint status_pc;
	gint status_timeout_id;
	GCancellable *status;
};

static GHashTable *
parse_elm_rc (const gchar *elmrc)
{
	gchar line[4096];
	FILE *handle;
	GHashTable *prefs;

	prefs = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	if (!g_file_test (elmrc, G_FILE_TEST_IS_REGULAR))
		return prefs;

	handle = fopen (elmrc, "r");
	if (handle == NULL)
		return prefs;

	while (fgets (line, 4096, handle) != NULL) {
		gchar *linestart, *end;
		gchar *key, *value;
		if (*line == '#' &&
		    (line[1] != '#' && line[2] != '#')) {
			continue;
		} else if (*line == '\n') {
			continue;
		} else if (*line == '#' && line[1] == '#' && line[2] == '#') {
			linestart = line + 4;
		} else {
			linestart = line;
		}

		end = strstr (linestart, " = ");
		if (end == NULL) {
			g_warning ("Broken line");
			continue;
		}

		*end = 0;
		key = g_strdup (linestart);

		linestart = end + 3;
		end = strchr (linestart, '\n');
		if (end == NULL) {
			g_warning ("Broken line");
			g_free (key);
			continue;
		}

		*end = 0;
		value = g_strdup (linestart);

		g_hash_table_insert (prefs, key, value);
	}

	fclose (handle);

	return prefs;
}

static gchar *
elm_get_rc (EImport *ei,
            const gchar *name)
{
	GHashTable *prefs;
	gchar *elmrc;

	prefs = g_object_get_data ((GObject *) ei, "elm-rc");
	if (prefs == NULL) {
		elmrc = g_build_filename (g_get_home_dir (), ".elm/elmrc", NULL);
		prefs = parse_elm_rc (elmrc);
		g_free (elmrc);
		g_object_set_data ((GObject *) ei, "elm-rc", prefs);
	}

	if (prefs == NULL)
		return NULL;
	else
		return g_hash_table_lookup (prefs, name);
}

static gboolean
elm_supported (EImport *ei,
               EImportTarget *target,
               EImportImporter *im)
{
	const gchar *maildir;
	gchar *elmdir;
	gboolean mailexists, exists;

	if (target->type != E_IMPORT_TARGET_HOME)
		return FALSE;

	elmdir = g_build_filename (g_get_home_dir (), ".elm", NULL);
	exists = g_file_test (elmdir, G_FILE_TEST_IS_DIR);
	g_free (elmdir);
	if (!exists)
		return FALSE;

	maildir = elm_get_rc (ei, "maildir");
	if (maildir == NULL)
		maildir = "Mail";

	if (!g_path_is_absolute (maildir))
		elmdir = g_build_filename (g_get_home_dir (), maildir, NULL);
	else
		elmdir = g_strdup (maildir);

	mailexists = g_file_test (elmdir, G_FILE_TEST_IS_DIR);
	g_free (elmdir);

	return mailexists;
}

static gchar *
elm_import_desc (struct _elm_import_msg *m)
{
	return g_strdup (_("Importing Elm data"));
}

static MailImporterSpecial elm_special_folders[] = {
	{ "received", "Inbox" },
	{ NULL },
};

static void
elm_import_exec (struct _elm_import_msg *m,
                 GCancellable *cancellable,
                 GError **error)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailSession *session;
	const gchar *maildir;
	gchar *elmdir;

	/* XXX Dig up the EMailSession from the default EShell.
	 *     Since the EImport framework doesn't allow for user
	 *     data, I don't see how else to get to it. */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));

	maildir = elm_get_rc (m->import, "maildir");
	if (maildir == NULL)
		maildir = "Mail";

	if (!g_path_is_absolute (maildir))
		elmdir = g_build_filename (g_get_home_dir (), maildir, NULL);
	else
		elmdir = g_strdup (maildir);

	mail_importer_import_folders_sync (
		session, elmdir, elm_special_folders, 0, m->status);
	g_free (elmdir);
}

static void
elm_import_done (struct _elm_import_msg *m)
{
	e_import_complete (m->import, (EImportTarget *) m->target, m->base.error);
}

static void
elm_import_free (struct _elm_import_msg *m)
{
	g_object_unref (m->status);

	g_free (m->status_what);
	g_mutex_clear (&m->status_lock);

	g_source_remove (m->status_timeout_id);
	m->status_timeout_id = 0;

	g_object_unref (m->import);
}

static void
elm_status (CamelOperation *op,
            const gchar *what,
            gint pc,
            gpointer data)
{
	struct _elm_import_msg *importer = data;

	g_mutex_lock (&importer->status_lock);
	g_free (importer->status_what);
	importer->status_what = g_strdup (what);
	importer->status_pc = pc;
	g_mutex_unlock (&importer->status_lock);
}

static gboolean
elm_status_timeout (gpointer data)
{
	struct _elm_import_msg *importer = data;
	gint pc;
	gchar *what;

	if (importer->status_what) {
		g_mutex_lock (&importer->status_lock);
		what = importer->status_what;
		importer->status_what = NULL;
		pc = importer->status_pc;
		g_mutex_unlock (&importer->status_lock);

		e_import_status (
			importer->import, (EImportTarget *)
			importer->target, what, pc);
	}

	return TRUE;
}

static MailMsgInfo elm_import_info = {
	sizeof (struct _elm_import_msg),
	(MailMsgDescFunc) elm_import_desc,
	(MailMsgExecFunc) elm_import_exec,
	(MailMsgDoneFunc) elm_import_done,
	(MailMsgFreeFunc) elm_import_free
};

static gint
mail_importer_elm_import (EImport *ei,
                          EImportTarget *target)
{
	GCancellable *cancellable;
	struct _elm_import_msg *m;
	gint id;

	cancellable = camel_operation_new ();
	m = mail_msg_new_with_cancellable (&elm_import_info, cancellable);
	g_datalist_set_data (&target->data, "elm-msg", m);
	m->import = ei;
	g_object_ref (m->import);
	m->target = (EImportTargetHome *) target;
	m->status_timeout_id =
		e_named_timeout_add (100, elm_status_timeout, m);
	g_mutex_init (&m->status_lock);
	m->status = cancellable;

	g_signal_connect (
		m->status, "status",
		G_CALLBACK (elm_status), m);

	id = m->base.seq;

	mail_msg_fast_ordered_push (m);

	return id;
}

static void
checkbox_toggle_cb (GtkToggleButton *tb,
                    EImportTarget *target)
{
	g_datalist_set_data (
		&target->data, "elm-do-mail",
		GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

static GtkWidget *
elm_getwidget (EImport *ei,
               EImportTarget *target,
               EImportImporter *im)
{
	GtkWidget *box, *w;

	g_datalist_set_data (
		&target->data, "elm-do-mail", GINT_TO_POINTER (TRUE));

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

	w = gtk_check_button_new_with_label (_("Mail"));
	gtk_toggle_button_set_active ((GtkToggleButton *) w, TRUE);
	g_signal_connect (
		w, "toggled",
		G_CALLBACK (checkbox_toggle_cb), target);

	gtk_box_pack_start ((GtkBox *) box, w, FALSE, FALSE, 0);
	gtk_widget_show_all (box);

	return box;
}

static void
elm_import (EImport *ei,
            EImportTarget *target,
            EImportImporter *im)
{
	if (GPOINTER_TO_INT (g_datalist_get_data (&target->data, "elm-do-mail")))
		mail_importer_elm_import (ei, target);
	else
		e_import_complete (ei, target, NULL);
}

static void
elm_cancel (EImport *ei,
            EImportTarget *target,
            EImportImporter *im)
{
	struct _elm_import_msg *m = g_datalist_get_data (&target->data, "elm-msg");

	if (m)
		g_cancellable_cancel (m->status);
}

static EImportImporter elm_importer = {
	E_IMPORT_TARGET_HOME,
	0,
	elm_supported,
	elm_getwidget,
	elm_import,
	elm_cancel,
	NULL, /* get_preview */
};

EImportImporter *
elm_importer_peek (void)
{
	elm_importer.name = _("Evolution Elm importer");
	elm_importer.description = _("Import mail from Elm.");

	return &elm_importer;
}
