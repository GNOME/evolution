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

#include "mail-session.h"
#include "em-junk-filter.h"

#define d(x) x

static pthread_mutex_t em_junk_sa_test_lock = PTHREAD_MUTEX_INITIALIZER;
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
static gboolean em_junk_sa_use_spamc = FALSE;
static gboolean em_junk_sa_available = FALSE;
static int em_junk_sa_spamd_port = -1;


static const char *
em_junk_sa_get_name (void)
{
	return _("Spamassassin (built-in)");
}

static int
pipe_to_sa (CamelMimeMessage *msg, const char *in, char **argv)
{
	int result, status, errnosav, fds[2];
	CamelStream *stream;
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
	
	if (pipe (fds) == -1) {
		errnosav = errno;
		d(printf ("failed to create a pipe (for use with spamassassin: %s\n", strerror (errno)));
		errno = errnosav;
		return -1;
	}
	
	if (!(pid = fork ())) {
		/* child process */
		int maxfd, fd, nullfd;
		
		nullfd = open ("/dev/null", O_WRONLY);
		
		if (dup2 (fds[0], STDIN_FILENO) == -1 ||
		    dup2 (nullfd, STDOUT_FILENO) == -1 ||
		    dup2 (nullfd, STDERR_FILENO) == -1)
			_exit (255);
		
		setsid ();
		
		maxfd = sysconf (_SC_OPEN_MAX);
		for (fd = 3; fd < maxfd; fd++)
			fcntl (fd, F_SETFD, FD_CLOEXEC);
		
		execvp (argv[0], argv);
		_exit (255);
	} else if (pid < 0) {
		errnosav = errno;
		close (fds[0]);
		close (fds[1]);
		errno = errnosav;
		return -1;
	}
	
	/* parent process */
	close (fds[0]);
	
	if (msg) {
		stream = camel_stream_fs_new_with_fd (fds[1]);
		
		camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (msg), stream);
		camel_stream_flush (stream);
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
		return -1;
}

static int
em_junk_sa_test_spamd_running (int port)
{
	char port_buf[12], *argv[5];
	int i = 0;
	
	d(fprintf (stderr, "test if spamd is running (port %d)\n", port));
	
	argv[i++] = "spamc";
	argv[i++] = "-x";
	
	if (port > 0) {
		sprintf (port_buf, "%d", port);
		argv[i++] = "-p";
		argv[i++] = port_buf;
	}
	
	argv[i] = NULL;
	
	return pipe_to_sa (NULL, "From test@127.0.0.1", argv) == 0;
}

#define SPAMD_PORT      7830
#define MAX_SPAMD_PORT  SPAMD_PORT

static void
em_junk_sa_test_spamd (void)
{
	int port = SPAMD_PORT;
	char *argv[6] = {
		"spamassassin",
		"--version",
		NULL,
	};
	
	if (pipe_to_sa (NULL, NULL, argv) != 0) {
		em_junk_sa_available = FALSE;
		em_junk_sa_tested = TRUE;
		return;
	}
	
	em_junk_sa_available = TRUE;
	em_junk_sa_use_spamc = FALSE;
	
	if (em_junk_sa_test_spamd_running (-1)) {
		em_junk_sa_use_spamc = TRUE;
		em_junk_sa_spamd_port = -1;
	} else {
		for ( ; port < MAX_SPAMD_PORT; port++) {
			if (em_junk_sa_test_spamd_running (port)) {
				em_junk_sa_use_spamc = TRUE;
				em_junk_sa_spamd_port = port;
				break;
			}
		}
	}
	
	if (!em_junk_sa_use_spamc) {
		char port_buf[12];
		int i = 0;
		
		d(fprintf (stderr, "looks like spamd is not running\n"));
		
		argv[i++] = "spamd";
		argv[i++] = "--port";
		argv[i++] = port_buf;
		
		if (mail_session_get_sa_local_only ())
			argv[i++] = "--local";
		
		argv[i++] = "--daemonize";
		argv[i] = NULL;
		
		for ( ; port < MAX_SPAMD_PORT; port++) {
			d(fprintf (stderr, "trying to run spamd at port %d\n", port));
			
			sprintf (port_buf, "%d", port);
			if (!pipe_to_sa (NULL, NULL, argv)) {
				em_junk_sa_use_spamc = TRUE;
				em_junk_sa_spamd_port = port;
				d(fprintf (stderr, "success at port %d\n", port));
				break;
			}
		}
	}
	
	d(fprintf (stderr, "use spamd %d at port %d\n", em_junk_sa_use_spamc, em_junk_sa_spamd_port));
	
	em_junk_sa_tested = TRUE;
}

static gboolean
em_junk_sa_is_available (void)
{
	pthread_mutex_lock (&em_junk_sa_test_lock);
	if (!em_junk_sa_tested)
		em_junk_sa_test_spamd ();
	pthread_mutex_unlock (&em_junk_sa_test_lock);
	
	return em_junk_sa_available;
}

static gboolean
em_junk_sa_check_junk (CamelMimeMessage *msg)
{
	char *argv[4], buf[12];
	int i = 0;
	
	d(fprintf (stderr, "em_junk_sa_check_junk\n"));
	
	if (!em_junk_sa_is_available ())
		return FALSE;
	
	if (em_junk_sa_use_spamc && mail_session_get_sa_use_daemon ()) {
		argv[i++] = "spamc";
		argv[i++] = "-c";
		if (em_junk_sa_spamd_port != -1) {
			sprintf (buf, "%d", em_junk_sa_spamd_port);
			argv[i++] = "-p";
			argv[i++] = buf;
		}
	} else {
		argv[i++] = "spamassassin";
		argv[i++] = "--exit-code";
		if (mail_session_get_sa_local_only ())
			argv[i++] = "--local";
	}
	
	argv[i] = NULL;
	
	return pipe_to_sa (msg, NULL, argv);
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
		if (mail_session_get_sa_local_only ())
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
		if (mail_session_get_sa_local_only ())
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
		if (mail_session_get_sa_local_only ())
			argv[2] = "--local";
		
		pthread_mutex_lock (&em_junk_sa_report_lock);
		pipe_to_sa (NULL, NULL, argv);
		pthread_mutex_unlock (&em_junk_sa_report_lock);
	}
}

const EMJunkPlugin *
em_junk_filter_get_plugin (void)
{
	return &spam_assassin_plugin;
}
