/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Author:
 *  Radek Doulik <rodo@ximian.com>
 *
 * Copyright 2003 Ximian, Inc. (www.ximian.com)
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>

#include <camel/camel-file-utils.h>
#include <camel/camel-data-wrapper.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-i18n.h>

#include "mail-session.h"
#include "em-junk-filter.h"

#include <gconf/gconf-client.h>

#define d(x) x

static pthread_mutex_t em_junk_sa_init_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t em_junk_sa_report_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *em_junk_sa_get_name (void);
static gboolean em_junk_sa_check_junk (CamelMimeMessage *msg);
static void em_junk_sa_report_junk (CamelMimeMessage *msg);
static void em_junk_sa_report_notjunk (CamelMimeMessage *msg);
static void em_junk_sa_commit_reports (void);

static EMJunkPlugin spam_assassin_plugin = {
	{
		em_junk_sa_get_name,
		1,
		em_junk_sa_check_junk,
		em_junk_sa_report_junk,
		em_junk_sa_report_notjunk,
		em_junk_sa_commit_reports,
	},
	NULL,
	NULL
};

static gboolean em_junk_sa_tested = FALSE;
static gboolean em_junk_sa_spamd_tested = FALSE;
static gboolean em_junk_sa_use_spamc = FALSE;
static gboolean em_junk_sa_available = FALSE;
static int em_junk_sa_spamd_port = -1;
static char *em_junk_sa_spamc_binary = NULL;
static GConfClient *em_junk_sa_gconf = NULL;

/* volatile so not cached between threads */
static volatile gboolean em_junk_sa_local_only;
static volatile gboolean em_junk_sa_use_daemon;
static volatile int em_junk_sa_daemon_port;

static const char *
em_junk_sa_get_name (void)
{
	return _("Spamassassin (built-in)");
}

static int
pipe_to_sa_with_error (CamelMimeMessage *msg, const char *in, char **argv, int rv_err)
{
	int result, status, errnosav, fds[2];
	CamelStream *stream;
	char *program;
	pid_t pid;
	
#if d(!)0
	{
		int i;
		
		printf ("pipe_to_sa ");
		for (i = 0; argv[i]; i++)
			printf ("%s ", argv[i]);
		printf ("\n");
	}
#endif

	program = g_find_program_in_path (argv [0]);
	if (program == NULL) {
		d(printf ("program not found, returning %d\n", rv_err));
		return rv_err;
	}
	g_free (program);
	
	if (pipe (fds) == -1) {
		errnosav = errno;
		d(printf ("failed to create a pipe (for use with spamassassin: %s\n", strerror (errno)));
		errno = errnosav;
		return rv_err;
	}
	
	if (!(pid = fork ())) {
		/* child process */
		int maxfd, fd, nullfd;
		
		nullfd = open ("/dev/null", O_WRONLY);
		
		if (dup2 (fds[0], STDIN_FILENO) == -1 ||
		    dup2 (nullfd, STDOUT_FILENO) == -1 ||
		    dup2 (nullfd, STDERR_FILENO) == -1)
			_exit (rv_err & 0377);
		
		setsid ();
		
		maxfd = sysconf (_SC_OPEN_MAX);
		for (fd = 3; fd < maxfd; fd++)
			fcntl (fd, F_SETFD, FD_CLOEXEC);
		
		execvp (argv[0], argv);
		_exit (rv_err & 0377);
	} else if (pid < 0) {
		errnosav = errno;
		close (fds[0]);
		close (fds[1]);
		errno = errnosav;
		return rv_err;
	}
	
	/* parent process */
	close (fds[0]);
	
	if (msg) {
		stream = camel_stream_fs_new_with_fd (fds[1]);
		
		camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (msg), stream);
		camel_stream_flush (stream);
		camel_stream_close (stream);
		camel_object_unref (stream);
	} else if (in) {
		camel_write (fds[1], in, strlen (in));
		close (fds[1]);
	}
	
	result = waitpid (pid, &status, 0);
	
	if (result == -1 && errno == EINTR) {
		/* child process is hanging... */
		kill (pid, SIGTERM);
		sleep (1);
		result = waitpid (pid, &status, WNOHANG);
		if (result == 0) {
			/* ...still hanging, set phasers to KILL */
			kill (pid, SIGKILL);
			sleep (1);
			result = waitpid (pid, &status, WNOHANG);
		}
	}
	
	if (result != -1 && WIFEXITED (status))
		return WEXITSTATUS (status);
	else
		return rv_err;
}

static int
pipe_to_sa (CamelMimeMessage *msg, const char *in, char **argv)
{
	return pipe_to_sa_with_error (msg, in, argv, -1);
}

static gboolean
em_junk_sa_test_spamd_running (char *binary, int port)
{
	char port_buf[12], *argv[5];
	int i = 0;
	
	d(fprintf (stderr, "test if spamd is running (port %d) using %s\n", port, binary));
	
	argv[i++] = binary;
	argv[i++] = "-x";
	
	if (port > 0) {
		sprintf (port_buf, "%d", port);
		argv[i++] = "-p";
		argv[i++] = port_buf;
	}
	
	argv[i] = NULL;
	
	return pipe_to_sa (NULL, "From test@127.0.0.1", argv) == 0;
}

static void
em_junk_sa_test_spamassassin (void)
{
	char *argv [3] = {
		"spamassassin",
		"--version",
		NULL,
	};
	
	if (pipe_to_sa (NULL, NULL, argv) != 0)
		em_junk_sa_available = FALSE;
	else
		em_junk_sa_available = TRUE;

	em_junk_sa_tested = TRUE;
}

#define MAX_SPAMD_PORTS 1

static gboolean
em_junk_sa_run_spamd (char *binary, int *port)
{
	char *argv[6];
	char port_buf[12];
	int i, p = em_junk_sa_daemon_port;

	d(fprintf (stderr, "looks like spamd is not running\n"));

	i = 0;
	argv[i++] = binary;
	argv[i++] = "--port";
	argv[i++] = port_buf;
		
	if (em_junk_sa_local_only)
		argv[i++] = "--local";
		
	argv[i++] = "--daemonize";
	argv[i] = NULL;
		
	for (i = 0; i < MAX_SPAMD_PORTS; i++, p++) {
		d(fprintf (stderr, "trying to run %s at port %d\n", binary, p));
			
		snprintf (port_buf, 11, "%d", p);
		if (!pipe_to_sa (NULL, NULL, argv)) {
			d(fprintf (stderr, "success at port %d\n", p));
			*port = p;
			return TRUE;
		}
	}

	return FALSE;
}

static void
em_junk_sa_test_spamd (void)
{
	char *argv[4];
	int i, b;
	gboolean try_system_spamd = TRUE;
	gboolean new_daemon_started = FALSE;
	char *spamc_binaries [3] = {"spamc", "/usr/sbin/spamc", NULL};
	char *spamd_binaries [3] = {"spamd", "/usr/sbin/spamd", NULL};

	if (em_junk_sa_gconf) {
		char *binary;

		binary = gconf_client_get_string (em_junk_sa_gconf, "/apps/evolution/mail/junk/sa/spamc_binary", NULL);
		if (binary) {
			spamc_binaries [0] = binary;
			spamc_binaries [1] = NULL;
		}
		binary = gconf_client_get_string (em_junk_sa_gconf, "/apps/evolution/mail/junk/sa/spamd_binary", NULL);
		if (binary) {
			spamd_binaries [0] = binary;
			spamd_binaries [1] = NULL;
			try_system_spamd = FALSE;
		}
	}

	em_junk_sa_use_spamc = FALSE;

	if (em_junk_sa_local_only && try_system_spamd) {
		   i = 0;
		   argv [i++] = "/bin/sh";
		   argv [i++] = "-c";
		   argv [i++] = "ps ax|grep -v grep|grep -E 'spamd.*(\\-L|\\-\\-local)'|grep -E -v '\\ \\-p\\ |\\ \\-\\-port\\ '";
		   argv[i] = NULL;

		   if (pipe_to_sa (NULL, NULL, argv) != 0) {
			   try_system_spamd = FALSE;
			   d(fprintf (stderr, "there's no system spamd with -L/--local parameter running\n"));
		   }
	}

	/* try to use sytem spamd first */
	if (try_system_spamd) {
		for (b = 0; spamc_binaries [b]; b ++) {
			em_junk_sa_spamc_binary = spamc_binaries [b];
			if (em_junk_sa_test_spamd_running (em_junk_sa_spamc_binary, -1)) {
				em_junk_sa_use_spamc = TRUE;
				em_junk_sa_spamd_port = -1;
				break;
			}
		}
	}

	/* if there's no system spamd running, try to use user one on evo spamd port */
	if (!em_junk_sa_use_spamc) {
		int port = em_junk_sa_daemon_port;

		for (i = 0; i < MAX_SPAMD_PORTS; i ++, port ++) {
			for (b = 0; spamc_binaries [b]; b ++) {
				em_junk_sa_spamc_binary = spamc_binaries [b];
				if (em_junk_sa_test_spamd_running (em_junk_sa_spamc_binary, port)) {
					em_junk_sa_use_spamc = TRUE;
					em_junk_sa_spamd_port = port;
					break;
				}
			}
		}
	}

	/* unsuccessful? try to run one ourselfs */
	if (!em_junk_sa_use_spamc)
		for (b = 0; spamd_binaries [b]; b ++) {
			em_junk_sa_use_spamc = em_junk_sa_run_spamd (spamd_binaries [b], &em_junk_sa_spamd_port);
			if (em_junk_sa_use_spamc) {
				new_daemon_started = TRUE;
				break;
			}
		}
	
	/* new daemon started => let find spamc binary */
	if (em_junk_sa_use_spamc && new_daemon_started) {
		em_junk_sa_use_spamc = FALSE;
		for (b = 0; spamc_binaries [b]; b ++) {
			em_junk_sa_spamc_binary = spamc_binaries [b];
			if (em_junk_sa_test_spamd_running (em_junk_sa_spamc_binary, em_junk_sa_spamd_port)) {
				em_junk_sa_use_spamc = TRUE;
				break;
			}
		}
	}

	d(fprintf (stderr, "use spamd %d at port %d with %s\n", em_junk_sa_use_spamc, em_junk_sa_spamd_port, em_junk_sa_spamc_binary));
	
	em_junk_sa_spamd_tested = TRUE;
}

static gboolean
em_junk_sa_is_available (void)
{
	pthread_mutex_lock (&em_junk_sa_init_lock);

	if (!em_junk_sa_tested)
		em_junk_sa_test_spamassassin ();

	if (em_junk_sa_available && !em_junk_sa_spamd_tested && em_junk_sa_use_daemon)
		em_junk_sa_test_spamd ();

	pthread_mutex_unlock (&em_junk_sa_init_lock);
	
	return em_junk_sa_available;
}

static gboolean
em_junk_sa_check_junk (CamelMimeMessage *msg)
{
	char *argv[5], buf[12];
	int i = 0;
	
	d(fprintf (stderr, "em_junk_sa_check_junk\n"));
	
	if (!em_junk_sa_is_available ())
		return FALSE;
	
	if (em_junk_sa_use_spamc && em_junk_sa_use_daemon) {
		argv[i++] = em_junk_sa_spamc_binary;
		argv[i++] = "-c";
		if (em_junk_sa_spamd_port != -1) {
			sprintf (buf, "%d", em_junk_sa_spamd_port);
			argv[i++] = "-p";
			argv[i++] = buf;
		}
	} else {
		argv [i++] = "spamassassin";
		argv [i++] = "--exit-code";
		if (em_junk_sa_local_only)
			argv [i++] = "--local";
	}
	
	argv[i] = NULL;

	return pipe_to_sa_with_error (msg, NULL, argv, 0) != 0;
}

static void
em_junk_sa_report_junk (CamelMimeMessage *msg)
{
	char *argv[6] = {
		"sa-learn",
		"--no-rebuild",
		"--spam",
		"--single",
		NULL,
		NULL
	};
	
	d(fprintf (stderr, "em_junk_sa_report_junk\n"));
	
	if (em_junk_sa_is_available ()) {
		if (em_junk_sa_local_only)
			argv[4] = "--local";
		
		pthread_mutex_lock (&em_junk_sa_report_lock);
		pipe_to_sa (msg, NULL, argv);
		pthread_mutex_unlock (&em_junk_sa_report_lock);
	}
}

static void
em_junk_sa_report_notjunk (CamelMimeMessage *msg)
{
	char *argv[6] = {
		"sa-learn",
		"--no-rebuild",
		"--ham",
		"--single",
		NULL,
		NULL
	};
	
	d(fprintf (stderr, "em_junk_sa_report_notjunk\n"));
	
	if (em_junk_sa_is_available ()) {
		if (em_junk_sa_local_only)
			argv[4] = "--local";
		
		pthread_mutex_lock (&em_junk_sa_report_lock);
		pipe_to_sa (msg, NULL, argv);
		pthread_mutex_unlock (&em_junk_sa_report_lock);
	}
}

static void
em_junk_sa_commit_reports (void)
{
	char *argv[4] = {
		"sa-learn",
		"--rebuild",
		NULL,
		NULL
	};
	
	d(fprintf (stderr, "em_junk_sa_commit_reports\n"));
	
	if (em_junk_sa_is_available ()) {
		if (em_junk_sa_local_only)
			argv[2] = "--local";
		
		pthread_mutex_lock (&em_junk_sa_report_lock);
		pipe_to_sa (NULL, NULL, argv);
		pthread_mutex_unlock (&em_junk_sa_report_lock);
	}
}

static void
em_junk_sa_setting_notify(GConfClient *gconf, guint cnxn_id, GConfEntry *entry, void *data)
{
	GConfValue *value;
	char *tkey;

	g_return_if_fail (gconf_entry_get_key (entry) != NULL);
	
	if (!(value = gconf_entry_get_value (entry)))
		return;
	
	tkey = strrchr(entry->key, '/');
	g_return_if_fail (tkey != NULL);

	if (!strcmp(tkey, "local_only"))
		em_junk_sa_local_only = gconf_value_get_bool(value);
	else if (!strcmp(tkey, "use_daemon"))
		em_junk_sa_use_daemon = gconf_value_get_bool(value);
	else if (!strcmp(tkey, "daemon_port"))
		em_junk_sa_daemon_port = gconf_value_get_int(value);
}

const EMJunkPlugin *
em_junk_filter_get_plugin (void)
{
	if (!em_junk_sa_gconf) {
		em_junk_sa_gconf = gconf_client_get_default();
		gconf_client_add_dir (em_junk_sa_gconf, "/apps/evolution/mail/junk/sa", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

		em_junk_sa_local_only = gconf_client_get_bool (em_junk_sa_gconf, "/apps/evolution/mail/junk/sa/local_only", NULL);
		em_junk_sa_use_daemon = gconf_client_get_bool (em_junk_sa_gconf, "/apps/evolution/mail/junk/sa/use_daemon", NULL);
		em_junk_sa_daemon_port = gconf_client_get_int (em_junk_sa_gconf, "/apps/evolution/mail/junk/sa/daemon_port", NULL);

		gconf_client_notify_add(em_junk_sa_gconf, "/apps/evolution/mail/junk/sa",
					(GConfClientNotifyFunc)em_junk_sa_setting_notify,
					NULL, NULL, NULL);
	}

	return &spam_assassin_plugin;
}
