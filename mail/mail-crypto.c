/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * mail-crypto.c: OpenPGP en/decryption & signature code
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

#include <config.h>

#ifdef PGP_PROGRAM
#include <stdlib.h>
#include <string.h>

#include "mail-crypto.h"
#include "mail-session.h"

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

#include <camel/camel-mime-filter-from.h>

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
crypto_exec_with_passwd (const char *path, char *argv[], const char *input, int inlen,
			 int passwd_fds[], const char *passphrase,
			 char **output, int *outlen, char **diagnostics)
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
		cleanup_before_exec (passwd_fds[0]);
		
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
	passwd_remaining = passphrase ? strlen (passphrase) : 0;
	passwd_incr = fpathconf (passwd_fds[1], _PC_PIPE_BUF);
	/* Use a reasonable default value on error. */
	if (passwd_incr <= 0)
		passwd_incr = 1024;
	passwd_eof_seen = FALSE;
	
	input_next = input;
	input_remaining = inlen;
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
	if (outlen)
		*outlen = size;
	*diagnostics = diag_buf;
	
	return cleanup_child (child);
}

/*----------------------------------------------------------------------*
 *                     Public crypto functions
 *----------------------------------------------------------------------*/

/**
 * mail_crypto_openpgp_decrypt: pgp decrypt ciphertext
 * @ciphertext: ciphertext to decrypt
 * @outlen: output length of the decrypted data (to be set by #mail_crypto_openpgp_decrypt)
 * @ex: a CamelException
 *
 * Returns an allocated buffer containing the decrypted ciphertext. If
 * the cleartext is plain text then you may treat it like a normal
 * string as it will be NUL terminated, however #outlen is also set in
 * the case that the cleartext is a binary stream.
 **/
char *
mail_crypto_openpgp_decrypt (const char *ciphertext, int cipherlen,
			     int *outlen, CamelException *ex)
{
	int retval, i;
	char *path, *argv[12];
	char *passphrase, *plaintext = NULL, *diagnostics = NULL;
	int passwd_fds[2];
	char passwd_fd[32];

	passphrase = mail_session_request_dialog (
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
	
	/* initialize outlen */
	*outlen = 0;
	
	retval = crypto_exec_with_passwd (path, argv,
					  ciphertext, cipherlen,
					  passwd_fds, passphrase,
					  &plaintext, outlen,
					  &diagnostics);
	
	if (retval != 0 || *outlen == 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "%s", diagnostics);
		g_free (plaintext);
		g_free (diagnostics);
		return NULL;
	}

	g_free (diagnostics);
	
	return plaintext;
}

/**
 * mail_crypto_openpgp_encrypt: pgp encrypt data
 * @in: data to encrypt
 * @inlen: input length of the input data (which may be a binary stream)
 * @recipients: an array of recipients to encrypt to (preferably each
 *              element should be a pgp keyring ID however sometimes email
 *              addresses will work assuming that your pgp keyring has an
 *              entry for that address)
 * @sign: TRUE if you wish to sign the encrypted text as well, FALSE otherwise
 * @userid: userid to sign with assuming #sign is TRUE
 * @ex: a CamelException
 *
 * Encrypts the plaintext to the list of recipients and optionally signs
 **/
char *
mail_crypto_openpgp_encrypt (const char *in, int inlen,
			     const GPtrArray *recipients,
			     gboolean sign, const char *userid,
			     CamelException *ex)
{
	GPtrArray *recipient_list = NULL;
	GPtrArray *argv;
	int retval, r;
	char *path;
	char *passphrase = NULL, *ciphertext = NULL, *diagnostics = NULL;
	int passwd_fds[2];
	char passwd_fd[32];
	
	if (sign) {
		/* we only need the passphrase if we plan to sign */
		passphrase = mail_session_request_dialog (
			_("Please enter your PGP/GPG passphrase."),
			TRUE, "pgp", FALSE);
		if (!passphrase) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
					     _("No password provided."));
			return NULL;
		}
	}
	
	if (pipe (passwd_fds) < 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Couldn't create pipe to GPG/PGP: %s"),
				      g_strerror (errno));
		return NULL;
	}
	
	argv = g_ptr_array_new ();
#if defined(GPG_PATH)
	path = GPG_PATH;
	
	recipient_list = g_ptr_array_new ();
	for (r = 0; r < recipients->len; r++) {
		char *buf, *recipient;
		
		recipient = recipients->pdata[r];
		buf = g_strdup_printf ("-r %s", recipient);
		g_ptr_array_add (recipient_list, buf);
	}
	
	g_ptr_array_add (argv, "gpg");
	g_ptr_array_add (argv, "--verbose");
	g_ptr_array_add (argv, "--yes");
	g_ptr_array_add (argv, "--batch");
	
	g_ptr_array_add (argv, "--armor");
	
	for (r = 0; r < recipient_list->len; r++)
		g_ptr_array_add (argv, recipient_list->pdata[r]);
	
	g_ptr_array_add (argv, "--output");
	g_ptr_array_add (argv, "-");            /* output to stdout */
	
	g_ptr_array_add (argv, "--encrypt");
	
	if (sign) {
		g_ptr_array_add (argv, "--sign");
		
		g_ptr_array_add (argv, "-u");
		g_ptr_array_add (argv, (gchar *) userid);
		
		g_ptr_array_add (argv, "--passphrase-fd");
		sprintf (passwd_fd, "%d", passwd_fds[0]);
		g_ptr_array_add (argv, passwd_fd);
	}
#elif defined(PGP5_PATH)
	path = PGP5_PATH;
	
	recipient_list = g_ptr_array_new ();
	for (r = 0; r < recipients->len; r++) {
		char *buf, *recipient;
		
		recipient = recipients->pdata[r];
		buf = g_strdup_printf ("-r %s", recipient);
		g_ptr_array_add (recipient_list, buf);
	}
	
	g_ptr_array_add (argv, "pgpe");
	
	for (r = 0; r < recipient_list->len; r++)
		g_ptr_array_add (argv, recipient_list->pdata[r]);
	
	g_ptr_array_add (argv, "-f");
	g_ptr_array_add (argv, "-z");
	g_ptr_array_add (argv, "-a");
	g_ptr_array_add (argv, "-o");
	g_ptr_array_add (argv, "-");        /* output to stdout */
	
	if (sign) {
		g_ptr_array_add (argv, "-s");
		
		g_ptr_array_add (argv, "-u");
		g_ptr_array_add (argv, (gchar *) userid);
		
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
	}
#else
	path = PGP_PATH;
	
	recipient_list = g_ptr_array_new ();
	for (r = 0; r < recipients->len; r++) {
		char *buf, *recipient;
		
		recipient = recipients->pdata[r];
		buf = g_strdup_printf ("-r %s", recipient);
		g_ptr_array_add (recipient_list, buf);
	}
	
	g_ptr_array_add (argv, "pgp");
	g_ptr_array_add (argv, "-f");
	g_ptr_array_add (argv, "-e");
	g_ptr_array_add (argv, "-a");
	g_ptr_array_add (argv, "-o");
	g_ptr_array_add (argv, "-");
	
	for (r = 0; r < recipient_list->len; r++)
		g_ptr_array_add (argv, recipient_list->pdata[r]);
	
	if (sign) {
		g_ptr_array_add (argv, "-s");
		
		g_ptr_array_add (argv, "-u");
		g_ptr_array_add (argv, (gchar *) userid);
		
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
	}
#endif
	g_ptr_array_add (argv, NULL);
	
	retval = crypto_exec_with_passwd (path, (char **) argv->pdata,
					  in, inlen, passwd_fds,
					  passphrase, &ciphertext, NULL,
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
	
	g_ptr_array_free (argv, TRUE);
	
	g_free (diagnostics);
	
	return ciphertext;
}

/**
 * mail_crypto_openpgp_clearsign: pgp clearsign plaintext
 * @plaintext: text to sign
 * @userid: user id to sign with
 * @hash: Preferred hash function (md5 or sha1)
 * @ex: a CamelException
 *
 * Clearsigns the plaintext using the user id
 **/

char *
mail_crypto_openpgp_clearsign (const char *plaintext,
			       const char *userid,
			       PgpHashType hash,
			       CamelException *ex)
{
	int retval;
	char *path, *argv[20];
	int i;
	char *passphrase;
	char *ciphertext = NULL;
	char *diagnostics = NULL;
	int passwd_fds[2];
	char passwd_fd[32];
	char *hash_str = NULL;
	
#ifndef PGP_PROGRAM
	camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
			      _("No GPG/PGP program available."));
	return NULL;
#endif

	passphrase = mail_session_request_dialog (_("Please enter your PGP/GPG passphrase."),
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
	
	switch (hash) {
	case PGP_HASH_TYPE_MD5:
		hash_str = "MD5";
		break;
	case PGP_HASH_TYPE_SHA1:
		hash_str = "SHA1";
		break;
	default:
		hash_str = NULL;
	}
	
	i = 0;
#if defined(GPG_PATH)
	path = GPG_PATH;
	
	argv[i++] = "gpg";
	
	argv[i++] = "--clearsign";
	
	if (hash_str) {
		argv[i++] = "--digest-algo";
		argv[i++] = hash_str;
	}
	
	if (userid) {
		argv[i++] = "-u";
		argv[i++] = (char *) userid;
	}
	
	argv[i++] = "--verbose";
	argv[i++] = "--yes";
	argv[i++] = "--batch";
	
	argv[i++] = "--armor";
	
	argv[i++] = "--output";
	argv[i++] = "-";            /* output to stdout */
	
	argv[i++] = "--passphrase-fd";
	sprintf (passwd_fd, "%d", passwd_fds[0]);
	argv[i++] = passwd_fd;
#elif defined(PGP5_PATH)
	/* FIXME: modify to respect hash */
	path = PGP5_PATH;
	
	argv[i++] = "pgps";
	
	if (userid) {
		argv[i++] = "-u";
		argv[i++] = (char *) userid;
	}
	
	argv[i++] = "-f";
	argv[i++] = "-z";
	argv[i++] = "-a";
	argv[i++] = "-o";
	argv[i++] = "-";        /* output to stdout */
	
	argv[i++] = "-s";	
	sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
	putenv (passwd_fd);
#else
	/* FIXME: modify to respect hash */
	path = PGP_PATH;

	argv[i++] = "pgp";
	
	argv[i++] = "-f";
	argv[i++] = "-e";
	argv[i++] = "-a";
	argv[i++] = "-o";
	argv[i++] = "-";
	
	argv[i++] = "-s";	
	sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
	putenv (passwd_fd);
#endif
	argv[i++] = NULL;
	
	retval = crypto_exec_with_passwd (path, argv,
					  plaintext, strlen (plaintext),
					  passwd_fds, passphrase,
					  &ciphertext, NULL,
					  &diagnostics);
	
	if (retval != 0 || !*ciphertext) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "%s", diagnostics);
		g_free (ciphertext);
		ciphertext = NULL;
	}
	
	g_free (diagnostics);
	
	return ciphertext;
}


/**
 * mail_crypto_openpgp_sign:
 * @in: input data to sign
 * @inlen: length of input data
 * @userid: userid to sign with
 * @hash: preferred hash type (md5 or sha1)
 * @ex: exception
 *
 * Returns an allocated string containing the detached signature using
 * the preferred hash.
 **/
char *
mail_crypto_openpgp_sign (const char *in, int inlen, const char *userid,
			  PgpHashType hash, CamelException *ex)
{
	char *argv[20];
	char *cyphertext = NULL;
	char *diagnostics = NULL;
	char *passphrase = NULL;
	char *hash_str = NULL;
	char *path;
	int passwd_fds[2];
	char passwd_fd[32];
	int retval, i;
	
	
#ifndef PGP_PROGRAM
	camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
			      _("No GPG/PGP program available."));
	return NULL;
#endif
	
	passphrase = mail_session_request_dialog (_("Please enter your PGP/GPG passphrase."),
						  TRUE, "pgp", FALSE);
	if (!passphrase) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("No password provided."));
		return NULL;
	}
	
	if (pipe (passwd_fds) < 0) {
		g_free (passphrase);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Couldn't create pipe to GPG/PGP: %s"),
				      g_strerror (errno));
		return NULL;
	}
	
	switch (hash) {
	case PGP_HASH_TYPE_MD5:
		hash_str = "MD5";
		break;
	case PGP_HASH_TYPE_SHA1:
		hash_str = "SHA1";
		break;
	default:
		hash_str = NULL;
	}
	
	i = 0;
#if defined(GPG_PATH)
	path = GPG_PATH;
	argv[i++] = "gpg";
	
	argv[i++] = "--sign";
	argv[i++] = "-b";
	if (hash_str) {
		argv[i++] = "--digest-algo";
		argv[i++] = hash_str;
	}
	
	if (userid) {
		argv[i++] = "-u";
		argv[i++] = (char *) userid;
	}
	
	argv[i++] = "--verbose";
	argv[i++] = "--yes";
	argv[i++] = "--batch";
	
	argv[i++] = "--armor";
	
	argv[i++] = "--output";
	argv[i++] = "-";            /* output to stdout */
	
	argv[i++] = "--passphrase-fd";
	sprintf (passwd_fd, "%d", passwd_fds[0]);
	argv[i++] = passwd_fd;
#elif defined(PGP5_PATH)
	path = PGP5_PATH;
	
	/* FIXME: respect hash */
	argv[i++] = "pgps";
	
	if (userid) {
		argv[i++] = "-u";
		argv[i++] = (char *) userid;
	}
	
	argv[i++] = "-b";
	argv[i++] = "-f";
	argv[i++] = "-z";
	argv[i++] = "-a";
	argv[i++] = "-o";
	argv[i++] = "-";        /* output to stdout */
	
	argv[i++] = "-s";	
	sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
	putenv (passwd_fd);
#else
	path = PGP_PATH;
	
	/* FIXME: needs to respect hash and also return only the detached sig */
	argv[i++] = "pgp";
	
	if (userid) {
		argv[i++] = "-u";
		argv[i++] = (char *) userid;
	}
	
	argv[i++] = "-f";
	argv[i++] = "-a";
	argv[i++] = "-o";
	argv[i++] = "-";
	
	argv[i++] = "-s";	
	sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
	putenv (passwd_fd);
#endif
	
	argv[i++] = NULL;
	
	retval = crypto_exec_with_passwd (path, argv,
					  in, inlen,
					  passwd_fds, passphrase,
					  &cyphertext, NULL,
					  &diagnostics);
	
	g_free (passphrase);
	
	if (retval != 0 || !*cyphertext) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "%s", diagnostics);
		g_free (cyphertext);
		cyphertext = NULL;
	}
	
	g_free (diagnostics);
	
	return cyphertext;
}

gboolean
mail_crypto_openpgp_verify (const char *in, int inlen, const char *sigin, int siglen, CamelException *ex)
{
	char *argv[20];
	char *cleartext = NULL;
	char *diagnostics = NULL;
	char *path;
	int passwd_fds[2];
	char passwd_fd[32];
	char *tmp = "/tmp/mail-crypto-XXXXXX";
	int retval, i, clearlen;
	gboolean valid = TRUE;
	
	
#ifndef PGP_PROGRAM
	camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
			      _("No GPG/PGP program available."));
	return FALSE;
#endif
	
	if (pipe (passwd_fds) < 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Couldn't create pipe to GPG/PGP: %s"),
				      g_strerror (errno));
		return FALSE;
	}
	
	i = 0;
#if defined(GPG_PATH)
	path = GPG_PATH;
	
	argv[i++] = "gpg";
	
	argv[i++] = "--verify";
	
	if (sigin != NULL && siglen) {
		/* We are going to verify a detached signature so save
                   the signature to a temp file and write the data to
                   verify to stdin */
		int fd;
		
		fd = mkstemp (tmp);
		if (fd == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Couldn't create temp file: %s"),
					      g_strerror (errno));
			return FALSE;
		}
		
		write (fd, sigin, siglen);
		close (fd);
		
		argv[i++] = tmp;
		argv[i++] = "-";
	} else {
		/* We are going to verify using stdin */
		argv[i++] = "-";
	}
	
	argv[i++] = "--verbose";
	argv[i++] = "--yes";
	argv[i++] = "--batch";
	
	argv[i++] = "--output";
	argv[i++] = "-";            /* output to stdout */
#elif defined (PGP5_PATH)
	path = PGP5_PATH;
	
	argv[i++] = "pgpv";
#else
	path = PGP_PATH;
	
	argv[i++] = "pgp";
#endif
	
	argv[i++] = NULL;
	
	clearlen = 0;
	retval = crypto_exec_with_passwd (path, argv,
					  in, inlen,
					  passwd_fds, NULL,
					  &cleartext, &clearlen,
					  &diagnostics);
	
	/* FIXME: maybe we should always set an exception? */
	if (retval != 0 || clearlen == 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "%s", diagnostics);
		valid = FALSE;
	}
	
	g_free (diagnostics);
	g_free (cleartext);
	
	return valid;
}

/** rfc2015 stuff *******************************/

gboolean
is_rfc2015_signed (CamelMimePart *mime_part)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *mp;
	CamelMimePart *part;
	CamelContentType *type;
	const gchar *param;
	int nparts;
	
	/* check that we have a multipart/signed */
	type = camel_mime_part_get_content_type (mime_part);
	if (!header_content_type_is (type, "multipart", "signed"))
		return FALSE;
	
	/* check that we have a protocol param with the value: "application/pgp-signed" */
	param = header_content_type_param (type, "protocol");
	if (!param || g_strcasecmp (param, "\"application/pgp-signed\""))
		return FALSE;
	
	/* check that we have exactly 2 subparts */
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	mp = CAMEL_MULTIPART (wrapper);
	nparts = camel_multipart_get_number (mp);
	if (nparts != 2)
		return FALSE;
	
	/* The first part may be of any type except for 
	 * application/pgp-signature - check it. */
	part = camel_multipart_get_part (mp, 0);
	type = camel_mime_part_get_content_type (part);
	if (header_content_type_is (type, "application", "pgp-signature"))
		return FALSE;
	
	/* The second part should be application/pgp-signature. */
	part = camel_multipart_get_part (mp, 1);
	type = camel_mime_part_get_content_type (part);
	if (!header_content_type_is (type, "application", "pgp-siganture"))
		return FALSE;
	
	/* FIXME: Implement multisig stuff */	
	
	return TRUE;
}

gboolean
is_rfc2015_encrypted (CamelMimePart *mime_part)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *mp;
	CamelMimePart *part;
	CamelContentType *type;
	const gchar *param;
	int nparts;
	
	/* check that we have a multipart/encrypted */
	type = camel_mime_part_get_content_type (mime_part);
	if (!header_content_type_is (type, "multipart", "encrypted"))
		return FALSE;
	
	/* check that we have a protocol param with the value: "application/pgp-encrypted" */
	param = header_content_type_param (type, "protocol");
	if (!param || g_strcasecmp (param, "\"application/pgp-encrypted\""))
		return FALSE;
	
	/* check that we have at least 2 subparts */
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	mp = CAMEL_MULTIPART (wrapper);
	nparts = camel_multipart_get_number (mp);
	if (nparts < 2)
		return FALSE;
	
	/* The first part should be application/pgp-encrypted */
	part = camel_multipart_get_part (mp, 0);
	type = camel_mime_part_get_content_type (part);
	if (!header_content_type_is (type, "application","pgp-encrypted"))
		return FALSE;
	
	/* The second part should be application/octet-stream - this
           is the one we care most about */
	part = camel_multipart_get_part (mp, 1);
	type = camel_mime_part_get_content_type (part);
	if (!header_content_type_is (type, "application","octet-stream"))
		return FALSE;
	
	return TRUE;
}

/**
 * pgp_mime_part_sign:
 * @mime_part: a MIME part that will be replaced by a pgp signed part
 * @userid: userid to sign with
 * @hash: one of PGP_HASH_TYPE_MD5 or PGP_HASH_TYPE_SHA1
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces #part with the generated multipart/signed. On failure,
 * #ex will be set and #part will remain untouched.
 **/
void
pgp_mime_part_sign (CamelMimePart **mime_part, const gchar *userid, PgpHashType hash, CamelException *ex)
{
	CamelMimePart *part, *signed_part;
	CamelMultipart *multipart;
	CamelMimePartEncodingType encoding;
	CamelContentType *mime_type;
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *crlf_filter, *from_filter;
	CamelStream *stream;
	GByteArray *array;
	gchar *cleartext, *signature;
	gchar *hash_type;
	gint clearlen;
	
	g_return_if_fail (*mime_part != NULL);
	g_return_if_fail (userid != NULL);
	g_return_if_fail (hash != PGP_HASH_TYPE_NONE);
	
	part = *mime_part;
	encoding = camel_mime_part_get_encoding (part);
	
	/* the encoding should really be QP or Base64 */
	if (encoding != CAMEL_MIME_PART_ENCODING_BASE64)
		camel_mime_part_set_encoding (part, CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE);
	
	/* get the cleartext */
	array = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (array);
	crlf_filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_ENCODE,
						  CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	from_filter = CAMEL_MIME_FILTER (camel_mime_filter_from_new ());
	filtered_stream = camel_stream_filter_new_with_stream (stream);
	camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (crlf_filter));
	camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (from_filter));
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (part), CAMEL_STREAM (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (stream));
	cleartext = array->data;
	clearlen = array->len;
	
	/* get the signature */
	signature = mail_crypto_openpgp_sign (cleartext, clearlen, userid, hash, ex);
	g_byte_array_free (array, TRUE);
	if (camel_exception_is_set (ex)) {
		/* restore the original encoding */
		camel_mime_part_set_encoding (part, encoding);
		return;
	}
	
	/* construct the pgp-signature mime part */
	fprintf (stderr, "signature:\n%s\n", signature);
	signed_part = camel_mime_part_new ();
	camel_mime_part_set_content (signed_part, signature, strlen (signature),
				     "application/pgp-signature");
	g_free (signature);
	camel_mime_part_set_encoding (signed_part, CAMEL_MIME_PART_ENCODING_DEFAULT);
	
	/* construct the container multipart/signed */
	multipart = camel_multipart_new ();
	camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (multipart),
					  "multipart/signed");
	switch (hash) {
	case PGP_HASH_TYPE_MD5:
		hash_type = "pgp-md5";
		break;
	case PGP_HASH_TYPE_SHA1:
		hash_type = "pgp-sha1";
		break;
	default:
		hash_type = NULL;
	}
	mime_type = camel_data_wrapper_get_mime_type_field (CAMEL_DATA_WRAPPER (multipart));
	header_content_type_set_param (mime_type, "micalg", hash_type);
	header_content_type_set_param (mime_type, "protocol", "application/pgp-signature");
	camel_multipart_set_boundary (multipart, NULL);
	
	/* add the parts to the multipart */
	camel_multipart_add_part (multipart, part);
	camel_object_unref (CAMEL_OBJECT (part));
	camel_multipart_add_part (multipart, signed_part);
	camel_object_unref (CAMEL_OBJECT (signed_part));
	
	/* replace the input part with the output part */
	*mime_part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (*mime_part),
					 CAMEL_DATA_WRAPPER (multipart));
	camel_object_unref (CAMEL_OBJECT (multipart));
}


/**
 * pgp_mime_part_verify:
 * @mime_part: a multipart/signed MIME Part
 * @ex: exception
 *
 * Returns TRUE if the signature is valid otherwise returns
 * FALSE. Note: you may want to check the exception if it fails as
 * there may be useful information to give to the user; example:
 * verification may have failed merely because the user doesn't have
 * the sender's key on her system.
 **/
gboolean
pgp_mime_part_verify (CamelMimePart *mime_part, CamelException *ex)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *multipart;
	CamelMimePart *part, *sigpart;
	GByteArray *content, *sig;
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *crlf_filter;
	CamelStream *stream;
	gboolean valid = FALSE;
	
	g_return_val_if_fail (mime_part != NULL, FALSE);
	
	if (!is_rfc2015_signed (mime_part))
		return FALSE;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	multipart = CAMEL_MULTIPART (wrapper);
	
	/* get the plain part */
	part = camel_multipart_get_part (multipart, 0);
	content = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (content);
	crlf_filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_ENCODE,
						  CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	filtered_stream = camel_stream_filter_new_with_stream (stream);
	camel_stream_filter_add (filtered_stream, CAMEL_MIME_FILTER (crlf_filter));
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (part), CAMEL_STREAM (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (filtered_stream));
	camel_object_unref (CAMEL_OBJECT (stream));
	
	/* get the signed part */
	sigpart = camel_multipart_get_part (multipart, 1);
	sig = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (sig);
	camel_data_wrapper_write_to_stream (camel_medium_get_content_object (CAMEL_MEDIUM (sigpart)), stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	/* verify */
	valid = mail_crypto_openpgp_verify (content->data, content->len,
					    sig->data, sig->len, ex);
	
	g_byte_array_free (content, TRUE);
	g_byte_array_free (sig, TRUE);
	
	return valid;
}


/**
 * pgp_mime_part_encrypt:
 * @mime_part: a MIME part that will be replaced by a pgp encrypted part
 * @recipients: list of recipient PGP Key IDs
 * @ex: exception which will be set if there are any errors.
 *
 * Constructs a PGP/MIME multipart in compliance with rfc2015 and
 * replaces #mime_part with the generated multipart/signed. On failure,
 * #ex will be set and #part will remain untouched.
 **/
void
pgp_mime_part_encrypt (CamelMimePart **mime_part, const GPtrArray *recipients, CamelException *ex)
{
	CamelMultipart *multipart;
	CamelMimePart *part, *version_part, *encrypted_part;
	CamelContentType *mime_type;
	CamelStream *stream;
	GByteArray *contents;
	gchar *ciphertext;
	
	g_return_if_fail (*mime_part != NULL);
	g_return_if_fail (recipients != NULL);
	
	part = *mime_part;
	
	/* get the contents */
	contents = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (contents);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (part), stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	/* pgp encrypt */
	ciphertext = mail_crypto_openpgp_encrypt (contents->data,
						  contents->len,
						  recipients, FALSE, NULL, ex);
	if (camel_exception_is_set (ex))
		return;
	
	/* construct the version part */
	version_part = camel_mime_part_new ();
	camel_mime_part_set_encoding (version_part, CAMEL_MIME_PART_ENCODING_7BIT);
	camel_mime_part_set_content (version_part, "Version: 1", strlen ("Version: 1"),
				     "application/pgp-encrypted");
	
	/* construct the pgp-encrypted mime part */
	encrypted_part = camel_mime_part_new ();
	camel_mime_part_set_encoding (encrypted_part, CAMEL_MIME_PART_ENCODING_7BIT);
	camel_mime_part_set_content (encrypted_part, ciphertext, strlen (ciphertext),
				     "application/octet-stream");
	g_free (ciphertext);
	
	/* construct the container multipart/signed */
	multipart = camel_multipart_new ();
	camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (multipart),
					  "multipart/encrypted");
	camel_multipart_set_boundary (multipart, NULL);
	mime_type = camel_data_wrapper_get_mime_type_field (CAMEL_DATA_WRAPPER (multipart));
	header_content_type_param (mime_type, "protocol", "application/pgp-encrypted");
	
	/* add the parts to the multipart */
	camel_multipart_add_part (multipart, version_part);
	camel_object_unref (CAMEL_OBJECT (version_part));
	camel_multipart_add_part (multipart, encrypted_part);
	camel_object_unref (CAMEL_OBJECT (encrypted_part));
	
	/* replace the input part with the output part */
	*mime_part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (*mime_part),
					 CAMEL_DATA_WRAPPER (multipart));
	camel_object_unref (CAMEL_OBJECT (multipart));
	
	/* destroy the original part */
	camel_object_unref (CAMEL_OBJECT (part));
}


/**
 * pgp_mime_part_decrypt:
 * @mime_part: a multipart/encrypted MIME Part
 * @ex: exception
 *
 * Returns the decrypted MIME Part on success or NULL on fail.
 **/
CamelMimePart *
pgp_mime_part_decrypt (CamelMimePart *mime_part, CamelException *ex)
{
	CamelDataWrapper *wrapper;
	CamelMultipart *multipart;
	CamelMimeParser *parser;
	CamelMimePart *encrypted_part, *part;
	CamelContentType *mime_type;
	CamelStream *stream;
	GByteArray *content;
	gchar *cleartext, *ciphertext = NULL;
	int cipherlen, clearlen;
	
	g_return_val_if_fail (mime_part != NULL, NULL);
	
	/* make sure the mime part is a multipart/encrypted */
	if (!is_rfc2015_encrypted (mime_part))
		return NULL;
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	multipart = CAMEL_MULTIPART (wrapper);
	
	/* get the encrypted part (second part) */
	encrypted_part = camel_multipart_get_part (multipart, 1 /* second part starting at 0 */);
	mime_type = camel_mime_part_get_content_type (encrypted_part);
	if (!header_content_type_is (mime_type, "application", "octet-stream"))
		return NULL;
	
	/* get the ciphertext */
	content = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (content);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (encrypted_part), stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	ciphertext = content->data;
	cipherlen = content->len;
	
	/* get the cleartext */
	cleartext = mail_crypto_openpgp_decrypt (ciphertext, cipherlen, &clearlen, ex);
	g_byte_array_free (content, TRUE);
	if (camel_exception_is_set (ex))
		return NULL;
	
	/* create a stream based on the returned cleartext */
	stream = camel_stream_mem_new ();
	camel_stream_write (stream, cleartext, clearlen);
	camel_stream_reset (stream);
	g_free (cleartext);
	
	/* construct the new decrypted mime part from the stream */
	part = camel_mime_part_new ();
	parser = camel_mime_parser_new ();
	camel_mime_parser_init_with_stream (parser, stream);
	camel_mime_part_construct_from_parser (part, parser);
	camel_object_unref (CAMEL_OBJECT (parser));
	camel_object_unref (CAMEL_OBJECT (stream));
	
	return part;
}

#endif /* PGP_PROGRAM */
