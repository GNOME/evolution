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

#include <glib.h>
#include <gnome.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <camel/camel-operation.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-control.h>

#include <importer/evolution-intelligent-importer.h>
#include <importer/evolution-importer-client.h>
#include <importer/GNOME_Evolution_Importer.h>

#include "mail-importer.h"

#include "mail/mail-mt.h"

#define KEY "elm-mail-imported"

/*#define SUPER_IMPORTER_DEBUG*/
#ifdef SUPER_IMPORTER_DEBUG
#define d(x) x
#else
#define d(x)
#endif

typedef struct {
	EvolutionIntelligentImporter *ii;

	GHashTable *prefs;

	GMutex *status_lock;
	char *status_what;
	int status_pc;
	int status_timeout_id;
	CamelOperation *cancel;	/* cancel/status port */

	GtkWidget *mail;
	gboolean do_mail;
	gboolean done_mail;

	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *progressbar;
} ElmImporter;

static GtkWidget *
create_importer_gui (ElmImporter *importer)
{
	GtkWidget *dialog;

	dialog = gnome_message_box_new (_("Evolution is importing your old Elm mail"), GNOME_MESSAGE_BOX_INFO, NULL);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Importing..."));

	importer->label = gtk_label_new (_("Please wait"));
	importer->progressbar = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), importer->label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), importer->progressbar, FALSE, FALSE, 0);

	return dialog;
}

static void
elm_store_settings (ElmImporter *importer)
{
	GConfClient *gconf;

	gconf = gconf_client_get_default ();
	gconf_client_set_bool (gconf, "/apps/evolution/importer/elm/mail", importer->done_mail, NULL);
	g_object_unref(gconf);
}

static void
elm_restore_settings (ElmImporter *importer)
{
	GConfClient *gconf = gconf_client_get_default ();

	importer->done_mail = gconf_client_get_bool (gconf, "/apps/evolution/importer/elm/mail", NULL);
	g_object_unref(gconf);
}

static void
parse_elm_rc(ElmImporter *importer, const char *elmrc)
{
	char line[4096];
	FILE *handle;

	if (importer->prefs)
		return;

	importer->prefs = g_hash_table_new(g_str_hash, g_str_equal);

	if (!g_file_exists(elmrc))
		return;

	handle = fopen (elmrc, "r");
	if (handle == NULL)
		return;

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

		g_hash_table_insert (importer->prefs, key, value);
	}

	fclose (handle);
}

static char *
elm_get_rc_value(ElmImporter *importer, const char *value)
{
	return g_hash_table_lookup(importer->prefs, value);
}

static gboolean
elm_can_import(EvolutionIntelligentImporter *ii, void *closure)
{
	ElmImporter *importer = closure;
	const char *maildir;
	char *elmdir, *elmrc;
	gboolean mailexists, exists;
#if 0
	char *aliasfile;
	gboolean aliasexists;
#endif
	struct stat st;

	elm_restore_settings(importer);

	importer->do_mail = !importer->done_mail;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->mail),  importer->do_mail);
	
	elmdir = g_build_filename(g_get_home_dir(), ".elm", NULL);
	exists = lstat(elmdir, &st) == 0 && S_ISDIR(st.st_mode);
	g_free (elmdir);
	if (!exists)
		return FALSE;

	elmrc = g_build_filename(g_get_home_dir(), ".elm/elmrc", NULL);
	parse_elm_rc (importer, elmrc);
	g_free(elmrc);

	maildir = elm_get_rc_value(importer, "maildir");
	if (maildir == NULL)
		maildir = "Mail";

	if (!g_path_is_absolute (maildir))
		elmdir = g_build_filename(g_get_home_dir(), maildir, NULL);
	else
		elmdir = g_strdup (maildir);

	mailexists = lstat(elmdir, &st) == 0 && S_ISDIR(st.st_mode);
	g_free (elmdir);

#if 0
	aliasfile = gnome_util_prepend_user_home (".elm/aliases");
	aliasexists = lstat(aliasfile, &st) == 0 && S_ISREG(st.st_mode);
	g_free (aliasfile);

	exists = (aliasexists || mailexists);
#endif

	return mailexists;
}

/* Almost all that follows is a direct copy of pine-importer.c with
 * search and replace run on it */
struct _elm_import_msg {
	struct _mail_msg msg;

	ElmImporter *importer;
};

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

	if (m->importer->do_mail) {
		const char *maildir;
		char *elmdir;

		maildir = elm_get_rc_value(m->importer, "maildir");
		if (maildir == NULL)
			maildir = "Mail";
		
		if (!g_path_is_absolute(maildir))
			elmdir = g_build_filename(g_get_home_dir(), maildir, NULL);
		else
			elmdir = g_strdup(maildir);

		mail_importer_import_folders_sync(elmdir, elm_special_folders, 0, m->importer->cancel);
	}
}

static void
elm_import_imported(struct _mail_msg *mm)
{
}

static void
elm_import_free(struct _mail_msg *mm)
{
	/*struct _elm_import_msg *m = (struct _elm_import_msg *)mm;*/
}

static struct _mail_msg_op elm_import_op = {
	elm_import_describe,
	elm_import_import,
	elm_import_imported,
	elm_import_free,
};

static int
mail_importer_elm_import(ElmImporter *importer)
{
	struct _elm_import_msg *m;
	int id;

	m = mail_msg_new(&elm_import_op, NULL, sizeof (*m));
	m->importer = importer;

	id = m->msg.seq;
	
	e_thread_put(mail_thread_queued, (EMsg *) m);

	return id;
}

static void
elm_status(CamelOperation *op, const char *what, int pc, void *data)
{
	ElmImporter *importer = data;

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
	ElmImporter *importer = data;
	int pc;
	char *what;

	if (!importer->status_what)
		return TRUE;

	g_mutex_lock(importer->status_lock);
	what = importer->status_what;
	importer->status_what = NULL;
	pc = importer->status_pc;
	g_mutex_unlock(importer->status_lock);

	gtk_progress_bar_set_fraction((GtkProgressBar *)importer->progressbar, (gfloat)(pc/100.0));
	gtk_progress_bar_set_text((GtkProgressBar *)importer->progressbar, what);
	
	return TRUE;
}

static void
elm_create_structure (EvolutionIntelligentImporter *ii,
		      void *closure)
{
	ElmImporter *importer = closure;

	if (importer->do_mail) {
		importer->dialog = create_importer_gui(importer);
		gtk_widget_show_all(importer->dialog);
		importer->status_timeout_id = g_timeout_add(100, elm_status_timeout, importer);
		importer->cancel = camel_operation_new(elm_status, importer);

		mail_msg_wait(mail_importer_elm_import(importer));

		camel_operation_unref(importer->cancel);
		g_source_remove(importer->status_timeout_id);
		importer->status_timeout_id = 0;

		importer->done_mail = TRUE;
	}

	elm_store_settings (importer);

	bonobo_object_unref (BONOBO_OBJECT (ii));
}

static void
free_pref(void *key, void *value, void *data)
{
	g_free(key);
	g_free(value);
}

static void
elm_destroy_cb (ElmImporter *importer, GtkObject *object)
{
	elm_store_settings(importer);

	if (importer->status_timeout_id)
		g_source_remove(importer->status_timeout_id);
	g_free(importer->status_what);
	g_mutex_free(importer->status_lock);

	if (importer->dialog)
		gtk_widget_destroy(importer->dialog);

	if (importer->prefs) {
		g_hash_table_foreach(importer->prefs, free_pref, NULL);
		g_hash_table_destroy(importer->prefs);
	}

	g_free(importer);
}

/* Fun initialisation stuff */
/* Fun control stuff */
static void
checkbox_toggle_cb (GtkToggleButton *tb,
		    gboolean *do_item)
{
	*do_item = gtk_toggle_button_get_active (tb);
}

static BonoboControl *
create_checkboxes_control (ElmImporter *importer)
{
	GtkWidget *hbox;
	BonoboControl *control;

	hbox = gtk_vbox_new (FALSE, 2);

	importer->mail = gtk_check_button_new_with_label (_("Mail"));
	gtk_signal_connect (GTK_OBJECT (importer->mail), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_mail);

	gtk_box_pack_start (GTK_BOX (hbox), importer->mail, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);
	control = bonobo_control_new (hbox);
	return control;
}

BonoboObject *
elm_intelligent_importer_new(void)
{
	EvolutionIntelligentImporter *importer;
	BonoboControl *control;
	ElmImporter *elm;
	char *message = N_("Evolution has found Elm mail files\n"
			   "Would you like to import them into Evolution?");

	elm = g_new0 (ElmImporter, 1);
	elm->status_lock = g_mutex_new();
	elm_restore_settings (elm);
	importer = evolution_intelligent_importer_new (elm_can_import,
						       elm_create_structure,
						       _("Elm"),
						       _(message), elm);
	g_object_weak_ref(G_OBJECT (importer), (GWeakNotify)elm_destroy_cb, elm);
	elm->ii = importer;

	control = create_checkboxes_control(elm);
	bonobo_object_add_interface(BONOBO_OBJECT(importer), BONOBO_OBJECT(control));

	return BONOBO_OBJECT(importer);
}
