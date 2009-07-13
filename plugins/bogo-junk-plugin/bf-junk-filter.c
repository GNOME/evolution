/*
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
 * Authors:
 *		Mikhail Zabaluev <mikhail.zabaluev@gmail.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright 2005 Mikhail Zabaluev <mikhail.zabaluev@gmail.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define G_LOG_DOMAIN "bf-junk-filter"

#include <glib.h>

#ifndef G_OS_WIN32
#  include <sys/wait.h>
#else
#  include <windows.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <e-util/e-plugin.h>
#include "mail/em-config.h"
#include <mail/em-junk-hook.h>
#include <camel/camel-data-wrapper.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-mime-parser.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-debug.h>
#include <gconf/gconf-client.h>

#ifndef BOGOFILTER_BINARY
#define BOGOFILTER_BINARY "/usr/bin/bogofilter"
#endif

#define BOGOFILTER_ERROR 3

#define EM_JUNK_BF_GCONF_DIR "/apps/evolution/mail/junk/bogofilter"

#define d(x) (camel_debug("junk")?(x):0)

static gboolean enabled = FALSE;

static gchar em_junk_bf_binary[] = BOGOFILTER_BINARY;

static const gchar em_junk_bf_gconf_dir[] = EM_JUNK_BF_GCONF_DIR;
GtkWidget * org_gnome_bogo_convert_unicode (struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data);

/* plugin fonction prototypes */
gboolean em_junk_bf_check_junk (EPlugin *ep, EMJunkHookTarget *target);
gpointer em_junk_bf_validate_binary (EPlugin *ep, EMJunkHookTarget *target);
void em_junk_bf_report_junk (EPlugin *ep, EMJunkHookTarget *target);
void em_junk_bf_report_non_junk (EPlugin *ep, EMJunkHookTarget *target);
void em_junk_bf_commit_reports (EPlugin *ep, EMJunkHookTarget *target);
static gint pipe_to_bogofilter (CamelMimeMessage *msg, const gchar **argv, GError **error);

/* eplugin stuff */
gint e_plugin_lib_enable (EPluginLib *ep, gint enable);

#define EM_JUNK_BF_GCONF_DIR_LENGTH (G_N_ELEMENTS (em_junk_bf_gconf_dir) - 1)

static gboolean em_junk_bf_unicode = TRUE;

static void
init_db ()
{
	CamelStream *stream = camel_stream_fs_new_with_name (WELCOME_MESSAGE, O_RDONLY, 0);
	CamelMimeParser *parser = camel_mime_parser_new ();
	CamelMimeMessage *msg = camel_mime_message_new ();
	const gchar *argv[] = {
		em_junk_bf_binary,
		"-n",
		NULL,
		NULL
	};

	camel_mime_parser_init_with_stream (parser, stream);
	camel_mime_parser_scan_from (parser, FALSE);
	camel_object_unref (stream);

	camel_mime_part_construct_from_parser ((CamelMimePart *) msg, parser);
	camel_object_unref (parser);

	d(fprintf (stderr, "Initing the bogofilter DB with Welcome message\n"));

	if (em_junk_bf_unicode) {
		argv[2] = "--unicode=yes";
	}

	pipe_to_bogofilter (msg, argv, NULL);
	camel_object_unref (msg);

}

static gint
pipe_to_bogofilter (CamelMimeMessage *msg, const gchar **argv, GError **error)
{
	GPid child_pid;
	gint bf_in;
	CamelStream *stream;
	GError *err = NULL;
	gint status;
	gint waitres;
	gint res;
	static gboolean only_once = FALSE;

retry:
	if (camel_debug_start ("junk")) {
		gint i;

		printf ("pipe_to_bogofilter ");
		for (i = 0; argv[i]; i++)
			printf ("%s ", argv[i]);
		printf ("\n");
		camel_debug_end ();
	}

	if (!g_spawn_async_with_pipes (NULL,
				       (gchar **) argv,
				       NULL,
				       G_SPAWN_DO_NOT_REAP_CHILD |
					   G_SPAWN_STDOUT_TO_DEV_NULL,
				       NULL,
				       NULL,
				       &child_pid,
				       &bf_in,
				       NULL,
				       NULL,
				       &err))
	{
		g_warning ("error occurred while spawning %s: %s",
			   argv[0],
			   err->message);
		/* For Translators: The first %s stands for the executable full path with a file name, the second is the error message itself. */
		g_set_error (error, EM_JUNK_ERROR, err->code, _("Error occurred while spawning %s: %s."), argv[0], err->message);

		return BOGOFILTER_ERROR;
	}

	stream = camel_stream_fs_new_with_fd (bf_in);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (msg), stream);
	camel_stream_flush (stream);
	camel_stream_close (stream);
	camel_object_unref (stream);

#ifndef G_OS_WIN32
	waitres = waitpid (child_pid, &status, 0);
	if (waitres < 0 && errno == EINTR) {
		/* child process is hanging... */
		g_warning ("wait for bogofilter child process interrupted, terminating");
		kill (child_pid, SIGTERM);
		sleep (1);
		waitres = waitpid (child_pid, &status, WNOHANG);
		if (waitres == 0) {
			/* ...still hanging, set phasers to KILL */
			g_warning ("bogofilter child process does not respond, killing");
			kill (child_pid, SIGKILL);
			sleep (1);
			waitres = waitpid (child_pid, &status, WNOHANG);
			g_set_error (error, EM_JUNK_ERROR, -2, _("Bogofilter child process does not respond, killing..."));
		} else
			g_set_error (error, EM_JUNK_ERROR, -3, _("Wait for Bogofilter child process interrupted, terminating..."));
	}

	if (waitres >= 0 && WIFEXITED (status)) {
		res = WEXITSTATUS (status);
	} else {
		res = BOGOFILTER_ERROR;
	}
#else
	WaitForSingleObject (child_pid, INFINITE);
	GetExitCodeProcess (child_pid, &res);
#endif

	g_spawn_close_pid (child_pid);

	if (res < 0 || res > 2) {
		if (!only_once) {
			/* Create wordlist.db */
			only_once = TRUE;
			init_db();

			goto retry;
		}
		g_set_error (error, EM_JUNK_ERROR, res, _("Pipe to Bogofilter failed, error code: %d."), res);

	}

	return res;
}

static void
em_junk_bf_setting_notify (GConfClient *gconf,
                           guint        cnxn_id,
                           GConfEntry  *entry,
                           void        *data)
{
	const gchar *key;
	GConfValue *value;

	value = gconf_entry_get_value (entry);
	if (value == NULL) {
		return;
	}

	key = gconf_entry_get_key (entry);
	g_return_if_fail (key != NULL);

	g_return_if_fail (!strncmp (key, em_junk_bf_gconf_dir, EM_JUNK_BF_GCONF_DIR_LENGTH));
	key += EM_JUNK_BF_GCONF_DIR_LENGTH;

	g_return_if_fail (*key == '/');
	++key;

	if (strcmp (key, "unicode") == 0) {
		em_junk_bf_unicode = gconf_value_get_bool (value);
	}
}

gboolean
em_junk_bf_check_junk (EPlugin *ep, EMJunkHookTarget *target)
{
	CamelMimeMessage *msg = target->m;
	gint rv;

	const gchar *argv[] = {
		em_junk_bf_binary,
		NULL,
		NULL
	};

	d(fprintf (stderr, "em_junk_bf_check_junk\n"));

	if (em_junk_bf_unicode) {
		argv[1] = "--unicode=yes";
	}

	rv = pipe_to_bogofilter (msg, argv, &target->error);

	d(fprintf (stderr, "em_junk_bf_check_junk rv = %d\n", rv));

	return (rv == 0);
}

void
em_junk_bf_report_junk (EPlugin *ep, EMJunkHookTarget *target)
{
	CamelMimeMessage *msg = target->m;

	const gchar *argv[] = {
		em_junk_bf_binary,
		"-s",
		NULL,
		NULL
	};

	d(fprintf (stderr, "em_junk_bf_report_junk\n"));

	if (em_junk_bf_unicode) {
		argv[2] = "--unicode=yes";
	}

	pipe_to_bogofilter (msg, argv, &target->error);
}

void
em_junk_bf_report_non_junk (EPlugin *ep, EMJunkHookTarget *target)
{
	CamelMimeMessage *msg = target->m;

	const gchar *argv[] = {
		em_junk_bf_binary,
		"-n",
		NULL,
		NULL
	};

	d(fprintf (stderr, "em_junk_bf_report_non_junk\n"));

	if (em_junk_bf_unicode) {
		argv[2] = "--unicode=yes";
	}

	pipe_to_bogofilter (msg, argv, &target->error);
}

void
em_junk_bf_commit_reports (EPlugin *ep, EMJunkHookTarget *target)
{
}

gpointer
em_junk_bf_validate_binary (EPlugin *ep, EMJunkHookTarget *target)
{
	return g_file_test (em_junk_bf_binary, G_FILE_TEST_EXISTS) ? (gpointer) "1" : NULL;
}

gint
e_plugin_lib_enable (EPluginLib *ep, gint enable)
{
	GConfClient *gconf;

	if (enable != 1 || enabled == TRUE) {
		return 0;
	}
	enabled = TRUE;
	gconf = gconf_client_get_default();

	gconf_client_add_dir (gconf,
			      em_junk_bf_gconf_dir,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);

	gconf_client_notify_add (gconf,
				 em_junk_bf_gconf_dir,
				 em_junk_bf_setting_notify,
				 NULL, NULL, NULL);

	em_junk_bf_unicode = gconf_client_get_bool (gconf,
		EM_JUNK_BF_GCONF_DIR "/unicode", NULL);

	g_object_unref (gconf);

	return 0;
}

static void
convert_unicode_cb (GtkWidget *widget, gpointer data)
{

	gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	GConfClient *gconf = gconf_client_get_default();

	gconf_client_set_bool (gconf, data, active, NULL);

	g_object_unref (gconf);
}

GtkWidget *
org_gnome_bogo_convert_unicode (struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data)
{
	GtkWidget *check;
	guint i = ((GtkTable *)data->parent)->nrows;

	if (data->old)
                return data->old;

	check = gtk_check_button_new_with_mnemonic (_("Convert message text to _Unicode"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), em_junk_bf_unicode);
	g_signal_connect (GTK_TOGGLE_BUTTON (check), "toggled", G_CALLBACK (convert_unicode_cb), (gpointer) "/apps/evolution/mail/junk/bogofilter/unicode");
	gtk_table_attach((GtkTable *)data->parent, check, 0, 1, i, i+1, 0, 0, 0, 0);
	gtk_widget_show (check);
	return (GtkWidget *)check;
}

