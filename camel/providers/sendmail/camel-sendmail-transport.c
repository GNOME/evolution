/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-sendmail-transport.c: Sendmail-based transport class. */

/* 
 *
 * Author : 
 *  Dan Winship <danw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#include "camel-sendmail-transport.h"
#include "camel-mime-message.h"
#include "camel-data-wrapper.h"
#include "camel-stream-fs.h"
#include "camel-exception.h"

static gboolean _can_send (CamelTransport *transport, CamelMedium *message);
static gboolean _send (CamelTransport *transport, CamelMedium *message,
		       CamelException *ex);
static gboolean _send_to (CamelTransport *transport, CamelMedium *message,
			  GList *recipients, CamelException *ex);


static void
camel_sendmail_transport_class_init (CamelSendmailTransportClass *camel_sendmail_transport_class)
{
	CamelTransportClass *camel_transport_class =
		CAMEL_TRANSPORT_CLASS (camel_sendmail_transport_class);

	/* virtual method overload */
	camel_transport_class->can_send = _can_send;
	camel_transport_class->send = _send;
	camel_transport_class->send_to = _send_to;
}

GtkType
camel_sendmail_transport_get_type (void)
{
	static GtkType camel_sendmail_transport_type = 0;
	
	if (!camel_sendmail_transport_type)	{
		GtkTypeInfo camel_sendmail_transport_info =	
		{
			"CamelSendmailTransport",
			sizeof (CamelSendmailTransport),
			sizeof (CamelSendmailTransportClass),
			(GtkClassInitFunc) camel_sendmail_transport_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_sendmail_transport_type = gtk_type_unique (CAMEL_TRANSPORT_TYPE, &camel_sendmail_transport_info);
	}
	
	return camel_sendmail_transport_type;
}


static gboolean
_can_send (CamelTransport *transport, CamelMedium *message)
{
	return CAMEL_IS_MIME_MESSAGE (message);
}


static gboolean
_send_internal (CamelMedium *message, char **argv, CamelException *ex)
{
	int fd[2], nullfd, wstat;
	sigset_t mask, omask;
	CamelStream *out;
	pid_t pid;

	g_assert (CAMEL_IS_MIME_MESSAGE (message));

	if (pipe (fd) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not create pipe to sendmail: "
				      "%s: mail not sent",
				      g_strerror (errno));
		return FALSE;
	}

	/* Block SIGCHLD so the calling application doesn't notice
	 * sendmail exiting before we do.
	 */
	sigemptyset (&mask);
	sigaddset (&mask, SIGCHLD);
	sigprocmask (SIG_BLOCK, &mask, &omask);

	pid = fork ();
	switch (pid) {
	case -1:
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not fork sendmail: "
				      "%s: mail not sent", g_strerror (errno));
		sigprocmask (SIG_SETMASK, &omask, NULL);
		return FALSE;

	case 0:
		/* Child process */
		nullfd = open ("/dev/null", O_RDWR);
		dup2 (fd[0], STDIN_FILENO);
		dup2 (nullfd, STDOUT_FILENO);
		dup2 (nullfd, STDERR_FILENO);
		close (nullfd);
		close (fd[1]);

		execv (SENDMAIL_PATH, argv);
		_exit (255);
	}

	/* Parent process. Write the message out. */
	close (fd[0]);
	out = camel_stream_fs_new_with_fd (fd[1]);
	if (camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), out) == -1
	    || camel_stream_close(out) == -1) {
		gtk_object_unref (GTK_OBJECT (out));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not send message: %s",
				      strerror(errno));
		return FALSE;
	}
	gtk_object_unref (GTK_OBJECT (out));

	/* Wait for sendmail to exit. */
	while (waitpid (pid, &wstat, 0) == -1 && errno == EINTR)
		;
	sigprocmask (SIG_SETMASK, &omask, NULL);

	if (!WIFEXITED (wstat)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "sendmail exited with signal %s: "
				      "mail not sent.",
				      g_strsignal (WTERMSIG (wstat)));
		return FALSE;
	} else if (WEXITSTATUS (wstat) != 0) {
		if (WEXITSTATUS (wstat) == 255) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
					     "Could not execute "
					     SENDMAIL_PATH ": mail not sent.");
		} else {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "sendmail exited with status "
					      "%d: mail not sent.",
					      WEXITSTATUS (wstat));
		}
		return FALSE;
	}

	return TRUE;
}

static gboolean
_send_to (CamelTransport *transport, CamelMedium *message,
	  GList *recipients, CamelException *ex)
{
	GList *r;
	char **argv;
	int i, len;
	gboolean status;

	len = g_list_length (recipients);
	argv = g_malloc ((len + 4) * sizeof (char *));
	argv[0] = "sendmail";
	argv[1] = "-i";
	argv[2] = "--";

	for (i = 1, r = recipients; i <= len; i++, r = r->next)
		argv[i + 2] = r->data;
	argv[i + 2] = NULL;

	status = _send_internal (message, argv, ex);
	g_free (argv);
	return status;
}

static gboolean
_send (CamelTransport *transport, CamelMedium *message,
       CamelException *ex)
{
	char *argv[4] = { "sendmail", "-t", "-i", NULL };

	return _send_internal (message, argv, ex);
}
