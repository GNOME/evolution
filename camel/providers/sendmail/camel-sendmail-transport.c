/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-sendmail-transport.c: Sendmail-based transport class. */

/* 
 *
 * Authors: Dan Winship <danw@ximian.com>
 *
 * Copyright 2000 Ximian, Inc. (www.ximian.com)
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

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#include "camel-sendmail-transport.h"
#include "camel-mime-filter-crlf.h"
#include "camel-stream-filter.h"
#include "camel-mime-message.h"
#include "camel-data-wrapper.h"
#include "camel-stream-fs.h"
#include "camel-exception.h"
#include "camel-i18n.h"

static char *get_name (CamelService *service, gboolean brief);

static gboolean sendmail_send_to (CamelTransport *transport,
				  CamelMimeMessage *message,
				  CamelAddress *from, CamelAddress *recipients,
				  CamelException *ex);


static void
camel_sendmail_transport_class_init (CamelSendmailTransportClass *camel_sendmail_transport_class)
{
	CamelTransportClass *camel_transport_class =
		CAMEL_TRANSPORT_CLASS (camel_sendmail_transport_class);
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_sendmail_transport_class);

	/* virtual method overload */
	camel_service_class->get_name = get_name;
	camel_transport_class->send_to = sendmail_send_to;
}

CamelType
camel_sendmail_transport_get_type (void)
{
	static CamelType camel_sendmail_transport_type = CAMEL_INVALID_TYPE;
	
	if (camel_sendmail_transport_type == CAMEL_INVALID_TYPE)	{
		camel_sendmail_transport_type =
			camel_type_register (CAMEL_TRANSPORT_TYPE, "CamelSendmailTransport",
					     sizeof (CamelSendmailTransport),
					     sizeof (CamelSendmailTransportClass),
					     (CamelObjectClassInitFunc) camel_sendmail_transport_class_init,
					     NULL,
					     (CamelObjectInitFunc) NULL,
					     NULL);
	}
	
	return camel_sendmail_transport_type;
}


static gboolean
sendmail_send_to (CamelTransport *transport, CamelMimeMessage *message,
		  CamelAddress *from, CamelAddress *recipients,
		  CamelException *ex)
{
	struct _camel_header_raw *header, *savedbcc, *n, *tail;
	const char *from_addr, *addr, **argv;
	int i, len, fd[2], nullfd, wstat;
	CamelStreamFilter *filter;
	CamelMimeFilter *crlf;
	sigset_t mask, omask;
	CamelStream *out;
	pid_t pid;
	
	if (!camel_internet_address_get (CAMEL_INTERNET_ADDRESS (from), 0, NULL, &from_addr))
		return FALSE;
	
	len = camel_address_length (recipients);
	argv = g_malloc ((len + 6) * sizeof (char *));
	argv[0] = "sendmail";
	argv[1] = "-i";
	argv[2] = "-f";
	argv[3] = from_addr;
	argv[4] = "--";
	
	for (i = 0; i < len; i++) {
		if (!camel_internet_address_get (CAMEL_INTERNET_ADDRESS (recipients), i, NULL, &addr)) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Could not parse recipient list"));
			g_free (argv);
			return FALSE;
		}
		
		argv[i + 5] = addr;
	}
	
	argv[i + 5] = NULL;
	
	/* unlink the bcc headers */
	savedbcc = NULL;
	tail = (struct _camel_header_raw *) &savedbcc;
	
	header = (struct _camel_header_raw *) &CAMEL_MIME_PART (message)->headers;
	n = header->next;
	while (n != NULL) {
		if (!strcasecmp (n->name, "Bcc")) {
			header->next = n->next;
			tail->next = n;
			n->next = NULL;
			tail = n;
		} else {
			header = n;
		}
		
		n = header->next;
	}
	
	if (pipe (fd) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create pipe to sendmail: "
					"%s: mail not sent"),
				      g_strerror (errno));
		
		/* restore the bcc headers */
		header->next = savedbcc;
		
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
				      _("Could not fork sendmail: "
					"%s: mail not sent"),
				      g_strerror (errno));
		sigprocmask (SIG_SETMASK, &omask, NULL);
		g_free (argv);
		
		/* restore the bcc headers */
		header->next = savedbcc;
		
		return FALSE;
	case 0:
		/* Child process */
		nullfd = open ("/dev/null", O_RDWR);
		dup2 (fd[0], STDIN_FILENO);
		/*dup2 (nullfd, STDOUT_FILENO);
		  dup2 (nullfd, STDERR_FILENO);*/
		close (nullfd);
		close (fd[1]);
		
		execv (SENDMAIL_PATH, (char **)argv);
		_exit (255);
	}
	g_free (argv);
	
	/* Parent process. Write the message out. */
	close (fd[0]);
	out = camel_stream_fs_new_with_fd (fd[1]);
	
	/* workaround for lame sendmail implementations that can't handle CRLF eoln sequences */
	filter = camel_stream_filter_new_with_stream (out);
	crlf = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_DECODE, CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	camel_stream_filter_add (filter, crlf);
	camel_object_unref (crlf);
	camel_object_unref (out);
	
	out = (CamelStream *) filter;
	if (camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), out) == -1
	    || camel_stream_close (out) == -1) {
		camel_object_unref (CAMEL_OBJECT (out));
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not send message: %s"),
				      g_strerror (errno));
		
		/* Wait for sendmail to exit. */
		while (waitpid (pid, &wstat, 0) == -1 && errno == EINTR)
			;
		
		sigprocmask (SIG_SETMASK, &omask, NULL);
		
		/* restore the bcc headers */
		header->next = savedbcc;
		
		return FALSE;
	}
	
	camel_object_unref (CAMEL_OBJECT (out));
	
	/* Wait for sendmail to exit. */
	while (waitpid (pid, &wstat, 0) == -1 && errno == EINTR)
		;
	
	sigprocmask (SIG_SETMASK, &omask, NULL);
	
	/* restore the bcc headers */
	header->next = savedbcc;
	
	if (!WIFEXITED (wstat)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("sendmail exited with signal %s: "
					"mail not sent."),
				      g_strsignal (WTERMSIG (wstat)));
		return FALSE;
	} else if (WEXITSTATUS (wstat) != 0) {
		if (WEXITSTATUS (wstat) == 255) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Could not execute %s: "
						"mail not sent."),
					      SENDMAIL_PATH);
		} else {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("sendmail exited with status "
						"%d: mail not sent."),
					      WEXITSTATUS (wstat));
		}
		return FALSE;
	}
	
	return TRUE;
}

static char *
get_name (CamelService *service, gboolean brief)
{
	if (brief)
		return g_strdup (_("sendmail"));
	else
		return g_strdup (_("Mail delivery via the sendmail program"));
}
