/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * mail-crypto.h: OpenPGP en/decryption & signature code
 *
 * FIXME FIXME FIXME: This should be in its own library or component
 */

/*
 * Authors:
 *  Nathan Thompson-Amato <ndt@jps.net>
 *  Dan Winship <danw@helixcode.com>
 *  Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000, Helix Code, Inc. (http://www.helixcode.com)
 *  Copyright 2000, Nathan Thompson-Amato
 *  Copyright 1999, 2000, Anthony Mulcahy
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#ifdef PGP_PROGRAM
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gnome.h>

#include "mail.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <signal.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

static int
cleanup_child (pid_t child)
{
	int status;
	pid_t wait_result;
	sigset_t mask, omask;

	/* PGP5 closes fds before exiting, meaning this might be called
	 * too early. So wait a bit for the result.
	 */
	sigemptyset (&mask);
	sigaddset (&mask, SIGALRM);
	sigprocmask (SIG_BLOCK, &mask, &omask);
	alarm (1);
	wait_result = waitpid (child, &status, 0);
	alarm (0);
	sigprocmask (SIG_SETMASK, &omask, NULL);

	if (wait_result == -1 && errno == EINTR) {
		/* The child is hanging: send a friendly reminder. */
		kill (child, SIGTERM);
		sleep (1);
		wait_result = waitpid (child, &status, WNOHANG);
		if (wait_result == 0) {
			/* Still hanging; use brute force. */
			kill (child, SIGKILL);
			sleep (1);
			wait_result = waitpid (child, &status, WNOHANG);
		}
	}

	if (wait_result != -1 && WIFEXITED (status))
		return WEXITSTATUS (status);
	else
		return -1;
}

static void
cleanup_before_exec (int fd)
{
	int maxfd, i;

	maxfd = sysconf (_SC_OPEN_MAX);
	if (maxfd < 0)
		return;

	/* Loop over all fds. */
	for (i = 0; i < maxfd; i++) {
		if ((STDIN_FILENO != i) &&
		    (STDOUT_FILENO != i) &&
		    (STDERR_FILENO != i) &&
		    (fd != i))
			close (i);
	}
}

static int
crypto_exec_with_passwd (char *path, char *argv[], const char *input,
			 int passwd_fds[], const char *passphrase,
			 char **output, char **diagnostics)
{
	fd_set fdset, write_fdset;
	int ip_fds[2], op_fds[2], diag_fds[2];
	int select_result, read_len, write_len;
        size_t tmp_len;
	pid_t child;
	char *buf, *diag_buf;
	const char *passwd_next, *input_next;
	size_t size, alloc_size, diag_size, diag_alloc_size;
        gboolean eof_seen, diag_eof_seen, passwd_eof_seen, input_eof_seen;
        size_t passwd_remaining, passwd_incr, input_remaining, input_incr;
	struct timeval timeout;

	if ((pipe (ip_fds) < 0 ) ||
	    (pipe (op_fds) < 0 ) ||
	    (pipe (diag_fds) < 0 )) {
		*diagnostics = g_strdup_printf ("Couldn't create pipe to %s: "
						"%s", PGP_PROGRAM,
						g_strerror (errno));
		return 0;
	}

	if (!(child = fork ())) {
		/* In child */

                if ((dup2 (ip_fds[0], STDIN_FILENO) < 0 ) ||
		    (dup2 (op_fds[1], STDOUT_FILENO) < 0 ) ||
		    (dup2 (diag_fds[1], STDERR_FILENO) < 0 )) {
			_exit (255);
		}

		/* Dissociate from evolution-mail's controlling
		 * terminal so that pgp/gpg won't be able to read from
		 * it: PGP 2 will fall back to asking for the password
		 * on /dev/tty if the passed-in password is incorrect.
		 * This will make that fail rather than hanging.
		 */
		setsid ();

		/* Close excess fds */
                cleanup_before_exec(passwd_fds[0]);

		execvp (path, argv);
		fprintf (stderr, "Could not execute %s: %s\n", argv[0],
			 g_strerror (errno));
		_exit (255);
	} else if (child < 0) {
		*diagnostics = g_strdup_printf ("Cannot fork %s: %s",
						argv[0], g_strerror (errno));
		return 0;
	}

	/* Parent */
	close (ip_fds[0]);
	close (op_fds[1]);
	close (diag_fds[1]);
	close (passwd_fds[0]);

	timeout.tv_sec = 10; /* timeout in seconds */
	timeout.tv_usec = 0;

	size = diag_size = 0;
	alloc_size = 4096;
	diag_alloc_size = 1024;
        eof_seen = diag_eof_seen = FALSE;

	buf = g_malloc (alloc_size);
	diag_buf = g_malloc (diag_alloc_size);

        passwd_next = passphrase;
        passwd_remaining = strlen (passphrase);
        passwd_incr = fpathconf (passwd_fds[1], _PC_PIPE_BUF);
	/* Use a reasonable default value on error. */
        if (passwd_incr <= 0)
		passwd_incr = 1024;
        passwd_eof_seen = FALSE;

	input_next = input;
	input_remaining = strlen (input);
	input_incr = fpathconf (ip_fds[1], _PC_PIPE_BUF);
	if (input_incr <= 0)
                input_incr = 1024;
	input_eof_seen = FALSE;

	while (!(eof_seen && diag_eof_seen)) {
		FD_ZERO (&fdset);
                if (!eof_seen)
			FD_SET (op_fds[0], &fdset);
                if (!diag_eof_seen)
			FD_SET (diag_fds[0], &fdset);

		FD_ZERO (&write_fdset);
                if (!passwd_eof_seen)
			FD_SET (passwd_fds[1], &write_fdset);
                if (!input_eof_seen)
			FD_SET (ip_fds[1], &write_fdset);

		select_result = select (FD_SETSIZE, &fdset, &write_fdset,
					NULL, &timeout);
		if (select_result < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (select_result == 0) {
			/* timeout */
			break;
		}

                if (FD_ISSET (op_fds[0], &fdset)) {
			/* More output is available. */

			if (size + 4096 > alloc_size) {
				alloc_size += 4096;
				buf = g_realloc (buf , alloc_size);
			}
			read_len = read (op_fds[0], &buf[size],
					 alloc_size - size - 1);
			if (read_len < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			if (read_len == 0)
				eof_seen = TRUE;
			size += read_len;
                }

                if (FD_ISSET(diag_fds[0], &fdset) ) {
			/* More stderr is available. */

			if (diag_size + 1024 > diag_alloc_size) {
				diag_alloc_size += 1024;
				diag_buf = g_realloc (diag_buf,
						      diag_alloc_size);
			}

			read_len = read (diag_fds[0], &diag_buf[diag_size],
					 diag_alloc_size - diag_size - 1);
			if (read_len < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			if (read_len == 0)
				diag_eof_seen = TRUE;
			diag_size += read_len;
                }

                if (FD_ISSET(passwd_fds[1], &write_fdset)) {
			/* Ready for more password input. */

			tmp_len = passwd_incr;
			if (tmp_len > passwd_remaining)
				tmp_len = passwd_remaining;
			write_len = write (passwd_fds[1], passwd_next,
					   tmp_len);
			if (write_len < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			passwd_next += write_len;
			passwd_remaining -= write_len;
			if (passwd_remaining == 0) {
				close (passwd_fds[1]);
				passwd_eof_seen = TRUE;
			}
                }

                if (FD_ISSET(ip_fds[1], &write_fdset)) {
			/* Ready for more ciphertext input. */

			tmp_len = input_incr;
			if (tmp_len > input_remaining)
				tmp_len = input_remaining;
			write_len = write (ip_fds[1], input_next, tmp_len);
			if (write_len < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			input_next += write_len;
			input_remaining -= write_len;
			if (input_remaining == 0 ) {
				close (ip_fds[1]);
				input_eof_seen = TRUE;
			}
                }
	}

	buf[size] = 0;
	diag_buf[diag_size] = 0;
	close (op_fds[0]);
	close (diag_fds[0]);

	*output = buf;
	*diagnostics = diag_buf;

        return cleanup_child (child);
}

/*----------------------------------------------------------------------*
 *                     Public crypto functions
 *----------------------------------------------------------------------*/


char *
mail_crypto_openpgp_decrypt (const char *ciphertext, CamelException *ex)
{
	int retval, i;
	char *path, *argv[12];
	char *passphrase, *plaintext = NULL, *diagnostics = NULL;
	int passwd_fds[2];
	char passwd_fd[32];

	passphrase = mail_request_dialog (
		_("Please enter your PGP/GPG passphrase."),
		TRUE, "pgp", FALSE);
	if (!passphrase) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("No password provided."));
		return NULL;
	}

	if (pipe (passwd_fds) < 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Couldn't create pipe to GPG/PGP: %s"),
				      g_strerror (errno));
		return NULL;
	}

	i = 0;
#if defined(GPG_PATH)
	path = GPG_PATH;

	argv[i++] = "gpg";
	argv[i++] = "--verbose";
	argv[i++] = "--yes";
	argv[i++] = "--batch";

	argv[i++] = "--output";
	argv[i++] = "-";            /* output to stdout */

	argv[i++] = "--decrypt";

	argv[i++] = "--passphrase-fd";
	sprintf (passwd_fd, "%d", passwd_fds[0]);
	argv[i++] = passwd_fd;
#elif defined(PGP5_PATH)
	path = PGP5_PATH;

	argv[i++] = "pgpv";
	argv[i++] = "-f";
	argv[i++] = "+batchmode=1";

	sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
	putenv (passwd_fd);
#else
	path = PGP_PATH;

	argv[i++] = "pgp";
	argv[i++] = "-f";

	sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
	putenv (passwd_fd);
#endif
	argv[i++] = NULL;

	retval = crypto_exec_with_passwd (path, argv, ciphertext, passwd_fds,
					  passphrase, &plaintext,
					  &diagnostics);
	if (retval != 0 || !*plaintext) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "%s", diagnostics);
		g_free (plaintext);
		g_free (diagnostics);
		return NULL;
	}

	g_free (diagnostics);
	return plaintext;
}

char *
mail_crypto_openpgp_encrypt (const char *plaintext,
			     const GPtrArray *recipients,
			     gboolean sign, CamelException *ex)
{
	GPtrArray *recipient_list = NULL;
	int retval, i, r;
	char *path, *argv[12];
	char *passphrase, *ciphertext = NULL, *diagnostics = NULL;
	int passwd_fds[2];
	char passwd_fd[32];
	
	passphrase = mail_request_dialog (
		_("Please enter your PGP/GPG passphrase."),
		TRUE, "pgp", FALSE);
	if (!passphrase) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("No password provided."));
		return NULL;
	}

	if (pipe (passwd_fds) < 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Couldn't create pipe to GPG/PGP: %s"),
				      g_strerror (errno));
		return NULL;
	}
	
	i = 0;
#if defined(GPG_PATH)
	path = GPG_PATH;
	
	recipient_list = g_ptr_array_new ();
	for (r = 0; r < recipients->len; r++) {
		char *buf, *recipient;
		
		recipient = recipients->pdata[i];
		buf = g_strdup_printf ("-r %s", recipient);
		g_ptr_array_add (recipient_list, buf);
	}
	
	argv[i++] = "gpg";
	argv[i++] = "--verbose";
	argv[i++] = "--yes";
	argv[i++] = "--batch";
	
	argv[i++] = "--armor";
	
	for (r = 0; r < recipient_list->len; r++)
		argv[i++] = recipient_list->pdata[r];
	
	argv[i++] = "--output";
	argv[i++] = "-";            /* output to stdout */
	
	argv[i++] = "--encrypt";
	
	if (sign) {
		argv[i++] = "--sign";
		
		argv[i++] = "--passphrase-fd";
		sprintf (passwd_fd, "%d", passwd_fds[0]);
		argv[i++] = passwd_fd;
	}
#elif defined(PGP5_PATH) /* FIXME: from here down needs to be modified to work correctly */
	path = PGP5_PATH;
	
	argv[i++] = "pgpe";
	argv[i++] = "-f";
	argv[i++] = "-z";
	argv[i++] = "-a";
	
	if (sign)
		argv[i++] = "-s";
	
	sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
	putenv (passwd_fd);
#else
	path = PGP_PATH;
	
	argv[i++] = "pgp";
	argv[i++] = "-f";
	
	sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
	putenv (passwd_fd);
#endif
	argv[i++] = NULL;

	retval = crypto_exec_with_passwd (path, argv, plaintext, passwd_fds,
					  passphrase, &ciphertext,
					  &diagnostics);
	
	if (retval != 0 || !*ciphertext) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "%s", diagnostics);
		g_free (ciphertext);
		ciphertext = NULL;
	}
	
	if (recipient_list) {
		for (r = 0; r < recipient_list->len; r++)
			g_free (recipient_list->pdata[r]);
		g_ptr_array_free (recipient_list, TRUE);
	}
	
	g_free (diagnostics);
	return ciphertext;
}

#endif /* PGP_PROGRAM */
