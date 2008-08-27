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
#include <sys/wait.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-util.h>

#include <libebook/e-book.h>
#include <libecal/e-cal.h>
#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>
#include "e-util/e-util.h"

#define EVOLUTION "evolution"
#define EVOLUTION_DIR "$HOME/.evolution/"
#define EVOLUTION_DIR_BACKUP "$HOME/.evolution-old/"
#define GCONF_DUMP_FILE "backup-restore-gconf.xml"
#define GCONF_DUMP_PATH EVOLUTION_DIR GCONF_DUMP_FILE
#define GCONF_DIR "/apps/evolution"
#define ARCHIVE_NAME "evolution-backup.tar.gz"

static gboolean backup_op = FALSE;
static char *bk_file = NULL;
static gboolean restore_op = FALSE;
static char *res_file = NULL;
static gboolean check_op = FALSE;
static char *chk_file = NULL;
static gboolean restart_arg = FALSE;
static gboolean gui_arg = FALSE;
static gchar **opt_remaining = NULL;
static int result=0;
static GtkWidget *progress_dialog;
static GtkWidget *pbar;
static char *txt = NULL;
gboolean complete = FALSE;

static const GOptionEntry options[] = {
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

#define d(x) x

#define rc(x) G_STMT_START { g_message (x); system (x); } G_STMT_END

#define CANCEL(x) if (x) return;

static void
s (const char *cmd)
{
	if (!cmd)
		return;

	if (strstr (cmd, "$HOME") != NULL) {
		/* read the doc for g_get_home_dir to know why replacing it here */
		const char *home = g_get_home_dir ();
		const char *p, *next;
		GString *str = g_string_new ("");

		p = cmd;
		while (next = strstr (p, "$HOME"), next) {
			if (p + 1 < next)
				g_string_append_len (str, p, next - p);
			g_string_append (str, home);

			p = next + 5;
		}

		g_string_append (str, p);

		rc (str->str);
		g_string_free (str, TRUE);
	} else
		rc (cmd);
}

static void
backup (const char *filename)
{
	char *command;
	char *quotedfname;

	g_return_if_fail (filename && *filename);
	quotedfname = g_shell_quote(filename);
	
	CANCEL (complete);
	txt = _("Shutting down Evolution");
	/* FIXME Will the versioned setting always work? */
	s (EVOLUTION " --force-shutdown");

	s ("rm $HOME/.evolution/.running");

	CANCEL (complete);
	txt = _("Backing Evolution accounts and settings");
	s ("gconftool-2 --dump " GCONF_DIR " > " GCONF_DUMP_PATH);

	CANCEL (complete);
	txt = _("Backing Evolution data (Mails, Contacts, Calendar, Tasks, Memos)");

	/* FIXME stay on this file system ,other options?" */
	/* FIXME compression type?" */
	/* FIXME date/time stamp?" */
	/* FIXME backup location?" */
	command = g_strdup_printf ("cd $HOME && tar chf - .evolution .camel_certs | gzip > %s", quotedfname);
	s (command);
	g_free (command);
	g_free (quotedfname);

	txt = _("Backup complete");

	if (restart_arg) {

		CANCEL (complete);
		txt = _("Restarting Evolution");
		complete=TRUE;

		s (EVOLUTION);
	}

}

static void
ensure_locals (ESourceList *source_list, const char *base_dir)
{
	GSList *groups, *sources;

	if (!source_list)
		return;

	for (groups = e_source_list_peek_groups (source_list); groups; groups = groups->next) {
		char *base_filename, *base_uri;
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);

		if (!group || !e_source_group_peek_base_uri (group) || !g_str_has_prefix (e_source_group_peek_base_uri (group), "file://"))
			continue;

		base_filename = g_build_filename (base_dir, "local", NULL);
		base_uri = g_filename_to_uri (base_filename, NULL, NULL);

		if (base_uri && !g_str_equal (base_uri, e_source_group_peek_base_uri (group))) {
			/* groups base_uri differs from the new one, maybe users imports
			   to the account with different user name, thus fixing this */
			e_source_group_set_base_uri (group, base_uri);
		}

		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);

			if (!source || !e_source_peek_absolute_uri (source))
				continue;

			if (!g_str_has_prefix (e_source_peek_absolute_uri (source), base_uri)) {
				char *abs_uri = e_source_build_absolute_uri (source);

				e_source_set_absolute_uri (source, abs_uri);
				g_free (abs_uri);
			}
		}

		g_free (base_filename);
		g_free (base_uri);
	}
}

/* returns whether changed the item */
static gboolean
fix_account_folder_uri (EAccount *account, e_account_item_t item, const char *base_dir)
{
	gboolean changed = FALSE;
	const char *uri;

	/* the base_dir should always contain that part, so just a sanity check */
	if (!account || !base_dir || strstr (base_dir, "/.evolution/mail/local") == NULL)
		return FALSE;

	uri = e_account_get_string (account, item);
	if (uri && strstr (uri, "/.evolution/mail/local") != NULL) {
		const char *path = strchr (uri, ':') + 1;

		if (!g_str_has_prefix (path, base_dir)) {
			GString *new_uri = g_string_new ("");

			/* prefix, like "mbox:" */
			g_string_append_len (new_uri, uri, path - uri);
			/* middle, changing old patch with new path  */
			g_string_append_len (new_uri, base_dir, strstr (base_dir, "/.evolution/mail/local") - base_dir);
			/* sufix, the rest beginning with "/.evolution/..." */
			g_string_append (new_uri, strstr (uri, "/.evolution/mail/local"));

			changed = TRUE;
			e_account_set_string (account, item, new_uri->str);
			g_string_free (new_uri, TRUE);
		}
	}

	return changed;
}

static void
restore (const char *filename)
{
	struct _calendars { ECalSourceType type; const char *dir; } 
		calendars[] = {
			{ E_CAL_SOURCE_TYPE_EVENT,   "calendar"},
			{ E_CAL_SOURCE_TYPE_TODO,    "tasks"},
			{ E_CAL_SOURCE_TYPE_JOURNAL, "memos"},
			{ E_CAL_SOURCE_TYPE_LAST,    NULL} };
	int i;
	char *command;
	char *quotedfname;
	ESourceList *sources = NULL;
	EAccountList *accounts;
	GConfClient *gconf;
	
	g_return_if_fail (filename && *filename);
	quotedfname = g_shell_quote(filename);
	
	/* FIXME Will the versioned setting always work? */
	CANCEL (complete);
	txt = _("Shutting down Evolution");
	s (EVOLUTION " --force-shutdown");

	CANCEL (complete);
	txt = _("Backup current Evolution data");
	s ("mv " EVOLUTION_DIR " " EVOLUTION_DIR_BACKUP);
	s ("mv $HOME/.camel_certs ~/.camel_certs_old");

	CANCEL (complete);
	txt = _("Extracting files from backup");
	command = g_strdup_printf ("cd $HOME && gzip -cd %s| tar xf -", quotedfname);
	s (command);
	g_free (command);
	g_free (quotedfname);

	CANCEL (complete);
	txt = _("Loading Evolution settings");
	s ("gconftool-2 --load " GCONF_DUMP_PATH);

	CANCEL (complete);
	txt = _("Removing temporary backup files");
	s ("rm -rf " GCONF_DUMP_PATH);
	s ("rm -rf " EVOLUTION_DIR_BACKUP);
	s ("rm -rf $HOME/.camel_certs_old");
	s ("rm $HOME/.evolution/.running");

	CANCEL (complete);
	txt = _("Ensuring local sources");

	if (e_book_get_addressbooks (&sources, NULL)) {
		char *base_dir = g_build_filename (e_get_user_data_dir (), "addressbook", NULL);

		ensure_locals (sources, base_dir);
		g_object_unref (sources);
		sources = NULL;

		g_free (base_dir);
	}

	for (i = 0; calendars[i].dir; i++) {
		if (e_cal_get_sources (&sources, calendars [i].type, NULL)) {
			char *base_dir = g_build_filename (e_get_user_data_dir (), calendars [i].dir, NULL);

			ensure_locals (sources, base_dir);
			g_object_unref (sources);
			sources = NULL;

			g_free (base_dir);
		}
	}

	gconf = gconf_client_get_default ();
	accounts = e_account_list_new (gconf);

	if (accounts) {
		gboolean changed = FALSE;
		char *base_dir = g_build_filename (e_get_user_data_dir (), "mail", "local", NULL);
		EIterator *it;

		for (it = e_list_get_iterator (E_LIST (accounts)); e_iterator_is_valid (it); e_iterator_next (it)) {
			/* why does this return a 'const' object?!? */
			EAccount *account = (EAccount *) e_iterator_get (it);

			if (account) {
				changed = fix_account_folder_uri (account, E_ACCOUNT_DRAFTS_FOLDER_URI, base_dir) || changed;
				changed = fix_account_folder_uri (account, E_ACCOUNT_SENT_FOLDER_URI, base_dir) || changed;
			}
		}

		if (changed)
			e_account_list_save (accounts);

		g_object_unref (it);
		g_object_unref (accounts);
		g_free (base_dir);
	}

	g_object_unref (gconf);

	if (restart_arg) {
		CANCEL (complete);
		txt = _("Restarting Evolution");
		complete=TRUE;
		s (EVOLUTION);
	}

}

static void
check (const char *filename)
{
	char *command;
	char *quotedfname;

	g_return_if_fail (filename && *filename);
	quotedfname = g_shell_quote(filename);
	
	command = g_strdup_printf ("tar ztf %s | grep -e \"^\\.evolution/$\"", quotedfname);
	result = system (command);
	g_free (command);

	g_message ("First result %d", result);
	if (result)
		exit (result);

	command = g_strdup_printf ("tar ztf %s | grep -e \"^\\.evolution/%s$\"", quotedfname, GCONF_DUMP_FILE);
	result = system (command);
	g_free (command);
	g_free (quotedfname);
	
	g_message ("Second result %d", result);

}

static gboolean
pbar_update()
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
	s ("pkill tar");

	gtk_main_quit ();
}

int
main (int argc, char **argv)
{
	GnomeProgram *program;
	GOptionContext *context;
	char *file = NULL, *oper = NULL;
	gint i;

	gtk_init (&argc, &argv);
	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	program = gnome_program_init (PACKAGE, VERSION, LIBGNOME_MODULE, argc, argv,
				      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      GNOME_PARAM_GOPTION_CONTEXT, context,
				      GNOME_PARAM_NONE);

	if (opt_remaining) {
		for (i = 0; i < g_strv_length (opt_remaining); i++) {
			if (backup_op) {
				oper = _("Backing up to the folder %s");
				d(g_message ("Backing up to the folder %s", (char *) opt_remaining[i]));
				bk_file = g_strdup ((char *) opt_remaining[i]);
				file = bk_file;
			} else if (restore_op) {
				oper = _("Restoring from the folder %s");
				d(g_message ("Restoring from the folder %s", (char *) opt_remaining[i]));
				res_file = g_strdup ((char *) opt_remaining[i]);
				file = res_file;
			} else if (check_op) {
				d(g_message ("Checking %s", (char *) opt_remaining[i]));
				chk_file = g_strdup ((char *) opt_remaining[i]);
			}
		}
	}

	if (gui_arg && !check_op) {
		GtkWidget *widget, *container;
		char *str = NULL, *txt;
		const char *txt2;

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

		txt = g_strconcat ("<b><big>", txt, "</big></b>", NULL);
		widget = gtk_label_new (NULL);
		gtk_label_set_line_wrap (GTK_LABEL (widget), FALSE);
		gtk_label_set_markup (GTK_LABEL (widget), txt);
		gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
		gtk_widget_show (widget);
		g_free (txt);

		gtk_table_attach (GTK_TABLE (container), widget, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

		txt = g_strconcat (txt2, " ", _("This may take a while depending on the amount of data in your account."), NULL);
		widget = gtk_label_new (NULL);
		gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
		gtk_label_set_markup (GTK_LABEL (widget), txt);
		gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
		gtk_widget_show (widget);
		g_free (txt);

		gtk_table_attach (GTK_TABLE (container), widget, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

		pbar = gtk_progress_bar_new ();

		if (str) {
			txt = g_strconcat ("<i>", str, "</i>", NULL);
			widget = gtk_label_new (NULL);
			gtk_label_set_markup (GTK_LABEL (widget), txt);
			gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
			g_free (txt);
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
		 exit (result);
	}

	g_idle_add (idle_cb, NULL);
	gtk_main ();

	return result;
}
