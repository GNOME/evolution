/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* elm-importer.c
 * 
 * Authors: Iain Holmes  <iain@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcheckbutton.h>

#include <gconf/gconf-client.h>

#include <camel/camel-operation.h>

#include "mail-importer.h"

#include "mail/mail-mt.h"
#include "e-util/e-import.h"
#include "e-util/e-error.h"

#define d(x) x

struct _elm_import_msg {
	struct _mail_msg msg;

	EImport *import;
	EImportTargetHome *target;

	GMutex *status_lock;
	char *status_what;
	int status_pc;
	int status_timeout_id;
	CamelOperation *status;
};

static GHashTable *
parse_elm_rc(const char *elmrc)
{
	char line[4096];
	FILE *handle;
	GHashTable *prefs = g_hash_table_new(g_str_hash, g_str_equal);

	if (!g_file_test(elmrc, G_FILE_TEST_IS_REGULAR))
		return prefs;

	handle = fopen (elmrc, "r");
	if (handle == NULL)
		return prefs;

	while (fgets (line, 4096, handle) != NULL) {
		char *linestart, *end;
		char *key, *value;
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

		g_hash_table_insert(prefs, key, value);
	}

	fclose (handle);

	return prefs;
}

static void
elm_free_rc_item(void *k, void *v, void *d)
{
	g_free(k);
	g_free(v);
}

static void
elm_free_rc(void *prefs)
{
	g_hash_table_foreach(prefs, elm_free_rc_item, NULL);
}

static char *
elm_get_rc(EImport *ei, const char *name)
{
	GHashTable *prefs;
	char *elmrc;

	prefs = g_object_get_data((GObject *)ei, "elm-rc");
	if (prefs == NULL) {
		elmrc = g_build_filename(g_get_home_dir(), ".elm/elmrc", NULL);
		prefs = parse_elm_rc(elmrc);
		g_free(elmrc);
		g_object_set_data_full((GObject *)ei, "elm-rc", prefs, elm_free_rc);
	}

	if (prefs == NULL)
		return NULL;
	else
		return g_hash_table_lookup(prefs, name);
}

static gboolean
elm_supported(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	EImportTargetHome *s;
	const char *maildir;
	char *elmdir;
	gboolean mailexists, exists;

	if (target->type != E_IMPORT_TARGET_HOME)
		return FALSE;

	s = (EImportTargetHome *)target;

	elmdir = g_build_filename(s->homedir, ".elm", NULL);
	exists = g_file_test(elmdir, G_FILE_TEST_IS_DIR);
	g_free(elmdir);
	if (!exists)
		return FALSE;

	maildir = elm_get_rc(ei, "maildir");
	if (maildir == NULL)
		maildir = "Mail";

	if (!g_path_is_absolute(maildir))
		elmdir = g_build_filename(s->homedir, maildir, NULL);
	else
		elmdir = g_strdup (maildir);

	mailexists = g_file_test(elmdir, G_FILE_TEST_IS_DIR);
	g_free (elmdir);

	return mailexists;
}

static char *
elm_import_describe (struct _mail_msg *mm, int complete)
{
	return g_strdup (_("Importing Elm data"));
}

static MailImporterSpecial elm_special_folders[] = {
	{ "received", "Inbox" },
	{ 0 },
};

static void
elm_import_import(struct _mail_msg *mm)
{
	struct _elm_import_msg *m = (struct _elm_import_msg *) mm;
	const char *maildir;
	char *elmdir;

	maildir = elm_get_rc(m->import, "maildir");
	if (maildir == NULL)
		maildir = "Mail";

	if (!g_path_is_absolute(maildir))
		elmdir = g_build_filename(m->target->homedir, maildir, NULL);
	else
		elmdir = g_strdup(maildir);

	mail_importer_import_folders_sync(elmdir, elm_special_folders, 0, m->status);
	g_free(elmdir);
}

static void
elm_import_imported(struct _mail_msg *mm)
{
	struct _elm_import_msg *m = (struct _elm_import_msg *)mm;

	printf("importing complete\n");

	if (!camel_exception_is_set(&mm->ex)) {
		GConfClient *gconf;

		gconf = gconf_client_get_default();
		gconf_client_set_bool(gconf, "/apps/evolution/importer/elm/mail", TRUE, NULL);
		g_object_unref(gconf);
	}

	e_import_complete(m->import, (EImportTarget *)m->target);
}

static void
elm_import_free(struct _mail_msg *mm)
{
	struct _elm_import_msg *m = (struct _elm_import_msg *)mm;

	camel_operation_unref(m->status);

	g_free(m->status_what);
	g_mutex_free(m->status_lock);

	g_source_remove(m->status_timeout_id);
	m->status_timeout_id = 0;

	g_object_unref(m->import);
}

static void
elm_status(CamelOperation *op, const char *what, int pc, void *data)
{
	struct _elm_import_msg *importer = data;

	if (pc == CAMEL_OPERATION_START)
		pc = 0;
	else if (pc == CAMEL_OPERATION_END)
		pc = 100;

	g_mutex_lock(importer->status_lock);
	g_free(importer->status_what);
	importer->status_what = g_strdup(what);
	importer->status_pc = pc;
	g_mutex_unlock(importer->status_lock);
}

static gboolean
elm_status_timeout(void *data)
{
	struct _elm_import_msg *importer = data;
	int pc;
	char *what;

	if (importer->status_what) {
		g_mutex_lock(importer->status_lock);
		what = importer->status_what;
		importer->status_what = NULL;
		pc = importer->status_pc;
		g_mutex_unlock(importer->status_lock);

		e_import_status(importer->import, (EImportTarget *)importer->target, what, pc);
	}

	return TRUE;
}

static struct _mail_msg_op elm_import_op = {
	elm_import_describe,
	elm_import_import,
	elm_import_imported,
	elm_import_free,
};

static int
mail_importer_elm_import(EImport *ei, EImportTarget *target)
{
	struct _elm_import_msg *m;
	int id;

	m = mail_msg_new(&elm_import_op, NULL, sizeof (*m));
	g_datalist_set_data(&target->data, "elm-msg", m);
	m->import = ei;
	g_object_ref(m->import);
	m->target = (EImportTargetHome *)target;
	m->status_timeout_id = g_timeout_add(100, elm_status_timeout, m);
	m->status_lock = g_mutex_new();
	m->status = camel_operation_new(elm_status, m);

	id = m->msg.seq;
	
	e_thread_put(mail_thread_queued, (EMsg *)m);

	return id;
}

static void
checkbox_toggle_cb (GtkToggleButton *tb, EImportTarget *target)
{
	g_datalist_set_data(&target->data, "elm-do-mail", GINT_TO_POINTER(gtk_toggle_button_get_active(tb)));
}

static GtkWidget *
elm_getwidget(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWidget *box, *w;
	GConfClient *gconf;
	gboolean done_mail;

	gconf = gconf_client_get_default ();
	done_mail = gconf_client_get_bool (gconf, "/apps/evolution/importer/elm/mail", NULL);
	g_object_unref(gconf);

	g_datalist_set_data(&target->data, "elm-do-mail", GINT_TO_POINTER(!done_mail));

	box = gtk_vbox_new(FALSE, 2);

	w = gtk_check_button_new_with_label(_("Mail"));
	gtk_toggle_button_set_active((GtkToggleButton *)w, !done_mail);
	g_signal_connect(w, "toggled", G_CALLBACK(checkbox_toggle_cb), target);

	gtk_box_pack_start((GtkBox *)box, w, FALSE, FALSE, 0);
	gtk_widget_show_all(box);

	return box;
}

static void
elm_import(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	if (GPOINTER_TO_INT(g_datalist_get_data(&target->data, "elm-do-mail")))
		mail_importer_elm_import(ei, target);
	else
		e_import_complete(ei, target);
}

static void
elm_cancel(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	struct _elm_import_msg *m = g_datalist_get_data(&target->data, "elm-msg");

	if (m)
		camel_operation_cancel(m->status);
}

static EImportImporter elm_importer = {
	E_IMPORT_TARGET_HOME,
	0,
	elm_supported,
	elm_getwidget,
	elm_import,
	elm_cancel,
};

EImportImporter *
elm_importer_peek(void)
{
	elm_importer.name = _("Evolution Elm importer");
	elm_importer.description = _("Import mail from Elm.");

	return &elm_importer;
}
