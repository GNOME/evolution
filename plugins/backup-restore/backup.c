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

/* #define s(x) system (x) */
#define s(x) G_STMT_START { g_message (x); system (x); } G_STMT_END

#define CANCEL(x) if (x) return;

static void
backup (const char *filename)
{
	char *command;

	g_return_if_fail (filename && *filename);

	CANCEL (complete);
	txt = _("Shutting down Evolution");
	/* FIXME Will the versioned setting always work? */
	s (EVOLUTION " --force-shutdown");

	CANCEL (complete);
	txt = _("Backing Evolution accounts and settings");
	s ("gconftool-2 --dump " GCONF_DIR " > " GCONF_DUMP_PATH);

	CANCEL (complete);
	txt = _("Backing Evolution data (Mails, Contacts, Calendar, Tasks, Memos)");

	/* FIXME stay on this file system ,other options?" */
	/* FIXME compression type?" */
	/* FIXME date/time stamp?" */
	/* FIXME backup location?" */
	command = g_strdup_printf ("cd $HOME && tar cf - .evolution .camel_certs | gzip > %s", filename);
	s (command);
	g_free (command);

	txt = _("Backup complete");

	if (restart_arg) {

		CANCEL (complete);
		txt = _("Restarting Evolution");
		complete=TRUE;

		s (EVOLUTION);
	}

}

static void
restore (const char *filename)
{
	char *command;

	g_return_if_fail (filename && *filename);

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
	command = g_strdup_printf ("cd $HOME && gzip -cd %s| tar xf -", filename);
	s (command);
	g_free (command);

	CANCEL (complete);
	txt = _("Loading Evolution settings");
	s ("gconftool-2 --load " GCONF_DUMP_PATH);

	CANCEL (complete);
	txt = _("Removing temporary backup files");
	s ("rm -rf " GCONF_DUMP_PATH);
	s ("rm -rf " EVOLUTION_DIR_BACKUP);
	s ("rm -rf $HOME/.camel_certs_old");

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

	g_return_if_fail (filename && *filename);

	command = g_strdup_printf ("tar ztf %s | grep -e \"^\\.evolution/$\"", filename);
	result = system (command);
	g_free (command);

	g_message ("First result %d", result);
	if (result)
		exit (result);

	command = g_strdup_printf ("tar ztf %s | grep -e \"^\\.evolution/%s$\"", filename, GCONF_DUMP_FILE);
	result = system (command);
	g_free (command);

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
