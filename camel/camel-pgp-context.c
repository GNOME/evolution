/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-pgp-context.h"

#include "camel-stream-fs.h"
#include "camel-stream-mem.h"

#include <gtk/gtk.h> /* for _() macro */

#include <gal/widgets/e-unicode.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#ifdef ENABLE_THREADS
#include <pthread.h>
#define PGP_LOCK(ctx)   g_mutex_lock (((CamelPgpContext *) ctx)->priv->lock)
#define PGP_UNLOCK(ctx) g_mutex_unlock (((CamelPgpContext *) ctx)->priv->lock);
#else
#define PGP_LOCK(ctx)
#define PGP_UNLOCK(ctx)
#endif

#define d(x)

struct _CamelPgpContextPrivate {
	CamelSession *session;
	CamelPgpType type;
	char *path;
	
#ifdef ENABLE_THREADS
	GMutex *lock;
#endif
};

static CamelObjectClass *parent_class;

static void
camel_pgp_context_init (CamelPgpContext *context)
{
	context->priv = g_new0 (struct _CamelPgpContextPrivate, 1);
#ifdef ENABLE_THREADS
	context->priv->lock = g_mutex_new ();
#endif
}

static void
camel_pgp_context_finalise (CamelObject *o)
{
	CamelPgpContext *context = (CamelPgpContext *)o;
	
	camel_object_unref (CAMEL_OBJECT (context->priv->session));
	
	g_free (context->priv->path);
	
#ifdef ENABLE_THREADS
	g_mutex_free (context->priv->lock);
#endif
	
	g_free (context->priv);
}

static void
camel_pgp_context_class_init (CamelPgpContextClass *camel_pgp_context_class)
{
	parent_class = camel_type_get_global_classfuncs (camel_object_get_type ());
}

CamelType
camel_pgp_context_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_object_get_type (),
					    "CamelPgpContext",
					    sizeof (CamelPgpContext),
					    sizeof (CamelPgpContextClass),
					    (CamelObjectClassInitFunc) camel_pgp_context_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_pgp_context_init,
					    (CamelObjectFinalizeFunc) camel_pgp_context_finalise);
	}

	return type;
}


/**
 * camel_pgp_context_new:
 * @session: CamelSession
 * @type: One of CAMEL_PGP_TYPE_PGP2, PGP5, GPG, or PGP6
 * @path: path to PGP binary
 *
 * This creates a new CamelPgpContext object which is used to sign,
 * verify, encrypt and decrypt streams.
 *
 * Return value: the new CamelPgpContext
 **/
CamelPgpContext *
camel_pgp_context_new (CamelSession *session, CamelPgpType type, const char *path)
{
	CamelPgpContext *context;
	
	g_return_val_if_fail (session != NULL, NULL);
	g_return_val_if_fail (type != CAMEL_PGP_TYPE_NONE, NULL);
	g_return_val_if_fail (path != NULL, NULL);
	
	context = CAMEL_PGP_CONTEXT (camel_object_new (CAMEL_PGP_CONTEXT_TYPE));
	
	camel_object_ref (CAMEL_OBJECT (session));
	context->priv->session = session;
	
	context->priv->type = type;
	context->priv->path = g_strdup (path);
	
	return context;
}



static const gchar *
pgp_get_type_as_string (CamelPgpType type)
{
	switch (type) {
	case CAMEL_PGP_TYPE_PGP2:
		return "PGP2.x";
	case CAMEL_PGP_TYPE_PGP5:
		return "PGP5";
	case CAMEL_PGP_TYPE_PGP6:
		return "PGP6";
	case CAMEL_PGP_TYPE_GPG:
		return "GnuPG";
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

static gchar *
pgp_get_passphrase (CamelSession *session, CamelPgpType pgp_type, char *userid)
{
	gchar *passphrase, *prompt;
	const char *type;
	
	type = pgp_get_type_as_string (pgp_type);
	
	if (userid)
		prompt = g_strdup_printf (_("Please enter your %s passphrase for %s"),
					  type, userid);
	else
		prompt = g_strdup_printf (_("Please enter your %s passphrase"),
					  type);
	
	/* Use the userid as a key if possible, else be generic and use the type */
	passphrase = camel_session_query_authenticator (session, CAMEL_AUTHENTICATOR_ASK,
							prompt, TRUE, NULL, userid ? userid : (char *) type,
							NULL);
	g_free (prompt);
	
	return passphrase;
}

static void
pgp_forget_passphrase (CamelSession *session, CamelPgpType pgp_type, char *userid)
{
	const char *type = NULL;
	
	if (!userid)
		type = pgp_get_type_as_string (pgp_type);
	
	camel_session_query_authenticator (session, CAMEL_AUTHENTICATOR_TELL,
					   NULL, FALSE, NULL, userid ? userid : (char *) type,
					   NULL);
}

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
						"%s", path,
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
 * camel_pgp_sign:
 * @context: PGP Context
 * @userid: private key to use to sign the stream
 * @hash: preferred Message-Integrity-Check hash algorithm
 * @istream: input stream
 * @ostream: output stream
 * @ex: exception
 *
 * PGP signs the input stream and writes the resulting signature to the output stream.
 *
 * Return value: 0 for success or -1 for failure.
 **/
int
camel_pgp_sign (CamelPgpContext *context, const char *userid, CamelPgpHashType hash,
		CamelStream *istream, CamelStream *ostream, CamelException *ex)
{
	GByteArray *plaintext;
	CamelStream *stream;
	char *argv[20];
	char *ciphertext = NULL;
	char *diagnostics = NULL;
	char *passphrase = NULL;
	char *hash_str = NULL;
	int passwd_fds[2];
	char passwd_fd[32];
	int retval, i;
	
	PGP_LOCK(context);
	
	/* get the plaintext in a form we can use */
	plaintext = g_byte_array_new ();
	stream = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), plaintext);
	camel_stream_write_to_stream (istream, stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	if (!plaintext->len) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("No plaintext to sign."));
		goto exception;
	}
	
	passphrase = pgp_get_passphrase (context->priv->session, context->priv->type, (char *) userid);
	if (!passphrase) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("No password provided."));
		goto exception;
	}
	
	if (pipe (passwd_fds) < 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Couldn't create pipe to GPG/PGP: %s"),
				      g_strerror (errno));
		goto exception;
	}
	
	switch (hash) {
	case CAMEL_PGP_HASH_TYPE_DEFAULT:
		hash_str = NULL;
		break;
	case CAMEL_PGP_HASH_TYPE_MD5:
		hash_str = "MD5";
		break;
	case CAMEL_PGP_HASH_TYPE_SHA1:
		hash_str = "SHA1";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	i = 0;
	switch (context->priv->type) {
	case CAMEL_PGP_TYPE_GPG:
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
		break;
	case CAMEL_PGP_TYPE_PGP5:
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
		
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
		break;
	case CAMEL_PGP_TYPE_PGP2:
	case CAMEL_PGP_TYPE_PGP6:
		/* FIXME: respect hash */
		argv[i++] = "pgp";
		
		if (userid) {
			argv[i++] = "-u";
			argv[i++] = (char *) userid;
		}
		
		argv[i++] = "-f";
		argv[i++] = "-a";
		argv[i++] = "-o";
		argv[i++] = "-";
		
		argv[i++] = "-sb"; /* create a detached signature */
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	argv[i++] = NULL;
	
	retval = crypto_exec_with_passwd (context->priv->path, argv,
					  plaintext->data, plaintext->len,
					  passwd_fds, passphrase,
					  &ciphertext, NULL,
					  &diagnostics);
	
	g_byte_array_free (plaintext, TRUE);
	g_free (passphrase);
	
	if (retval != 0 || !*ciphertext) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "%s", diagnostics);
		g_free (diagnostics);
		g_free (ciphertext);
		pgp_forget_passphrase (context->priv->session, context->priv->type, (char *) userid);
		
		PGP_UNLOCK(context);
		
		return -1;
	}
	
	g_free (diagnostics);
	
	camel_stream_write (ostream, ciphertext, strlen (ciphertext));
	g_free (ciphertext);
	
	PGP_UNLOCK(context);
	
	return 0;
	
 exception:
	
	g_byte_array_free (plaintext, TRUE);
	
	if (passphrase) {
		pgp_forget_passphrase (context->priv->session, context->priv->type, (char *) userid);
		g_free (passphrase);
	}
	
	PGP_UNLOCK(context);
	
	return -1;
}


/**
 * camel_pgp_clearsign:
 * @context: PGP Context
 * @userid: key id or email address of the private key to sign with
 * @hash: preferred Message-Integrity-Check hash algorithm
 * @istream: input stream
 * @ostream: output stream
 * @ex: exception
 *
 * PGP clearsigns the input stream and writes the resulting clearsign to the output stream.
 *
 * Return value: 0 for success or -1 for failure.
 **/
int
camel_pgp_clearsign (CamelPgpContext *context, const char *userid, CamelPgpHashType hash,
		     CamelStream *istream, CamelStream *ostream, CamelException *ex)
{
	GByteArray *plaintext;
	CamelStream *stream;
	char *argv[15];
	char *ciphertext = NULL;
	char *diagnostics = NULL;
	char *passphrase = NULL;
	char *hash_str = NULL;
	int passwd_fds[2];
	char passwd_fd[32];
	int retval, i;
	
	PGP_LOCK(context);
	
	/* get the plaintext in a form we can use */
	plaintext = g_byte_array_new ();
	stream = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), plaintext);
	camel_stream_write_to_stream (istream, stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	if (!plaintext->len) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("No plaintext to clearsign."));
		goto exception;
	}
	
	passphrase = pgp_get_passphrase (context->priv->session, context->priv->type, (char *) userid);
	if (!passphrase) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("No password provided."));
		goto exception;
	}
	
	if (pipe (passwd_fds) < 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Couldn't create pipe to GPG/PGP: %s"),
				      g_strerror (errno));
		goto exception;
	}
	
	switch (hash) {
	case CAMEL_PGP_HASH_TYPE_DEFAULT:
		hash_str = NULL;
		break;
	case CAMEL_PGP_HASH_TYPE_MD5:
		hash_str = "MD5";
		break;
	case CAMEL_PGP_HASH_TYPE_SHA1:
		hash_str = "SHA1";
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	i = 0;
	switch (context->priv->type) {
	case CAMEL_PGP_TYPE_GPG:
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
		break;
	case CAMEL_PGP_TYPE_PGP5:
		/* FIXME: modify to respect hash */
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
		
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
		break;
	case CAMEL_PGP_TYPE_PGP2:
	case CAMEL_PGP_TYPE_PGP6:
		/* FIXME: modify to respect hash */
		argv[i++] = "pgp";
		
		if (userid) {
			argv[i++] = "-u";
			argv[i++] = (char *) userid;
		}
		
		argv[i++] = "-f";
		argv[i++] = "-a";
		argv[i++] = "-o";
		argv[i++] = "-";
		
		argv[i++] = "-st";
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	argv[i++] = NULL;
	
	retval = crypto_exec_with_passwd (context->priv->path, argv,
					  plaintext->data, plaintext->len,
					  passwd_fds, passphrase,
					  &ciphertext, NULL,
					  &diagnostics);
	
	g_byte_array_free (plaintext, TRUE);
	g_free (passphrase);
	
	if (retval != 0 || !*ciphertext) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "%s", diagnostics);
		g_free (diagnostics);
		g_free (ciphertext);
		pgp_forget_passphrase (context->priv->session, context->priv->type, (char *) userid);
	}
	
	g_free (diagnostics);
	
	camel_stream_write (ostream, ciphertext, strlen (ciphertext));
	g_free (ciphertext);
	
	PGP_UNLOCK(context);
	
	return 0;
	
 exception:
	
	g_byte_array_free (plaintext, TRUE);
	
	if (passphrase) {
		pgp_forget_passphrase (context->priv->session, context->priv->type, (char *) userid);
		g_free (passphrase);
	}
	
	PGP_UNLOCK(context);
	
	return -1;
}


static char *
swrite (CamelStream *istream)
{
	CamelStream *ostream;
	char *template;
	int fd;
	
	template = g_strdup ("/tmp/evolution-pgp.XXXXXX");
	fd = mkstemp (template);
	if (fd == -1) {
		g_free (template);
		return NULL;
	}
	
	ostream = camel_stream_fs_new_with_fd (fd);
	camel_stream_write_to_stream (istream, ostream);
	camel_object_unref (CAMEL_OBJECT (ostream));
	
	return template;
}


/**
 * camel_pgp_verify:
 * @context: PGP Context
 * @istream: input stream
 * @sigstream: optional detached-signature stream
 * @ex: exception
 *
 * Verifies the PGP signature. If @istream is a clearsigned stream,
 * you should pass %NULL as the sigstream parameter. Otherwise
 * @sigstream is assumed to be the signature stream and is used to
 * verify the integirity of the @istream.
 *
 * Return value: a CamelPgpValidity structure containing information
 * about the integrity of the input stream or %NULL if PGP failed to
 * execute at all.
 **/
CamelPgpValidity *
camel_pgp_verify (CamelPgpContext *context, CamelStream *istream,
		  CamelStream *sigstream, CamelException *ex)
{
	CamelPgpValidity *valid = NULL;
	GByteArray *plaintext;
	CamelStream *stream;
	char *argv[20];
	char *cleartext = NULL;
	char *diagnostics = NULL;
	int passwd_fds[2];
	char *sigfile = NULL;
	int retval, i, clearlen;
	
	PGP_LOCK(context);
	
	/* get the plaintext in a form we can use */
	plaintext = g_byte_array_new ();
	stream = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), plaintext);
	camel_stream_write_to_stream (istream, stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	if (!plaintext->len) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("No plaintext to verify."));
		goto exception;
	}
	
	if (pipe (passwd_fds) < 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Couldn't create pipe to GPG/PGP: %s"),
				      g_strerror (errno));
		goto exception;
	}
	
	if (sigstream != NULL) {
		/* We are going to verify a detached signature so save
		   the signature to a temp file. */
		sigfile = swrite (sigstream);
		if (!sigfile) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Couldn't create temp file: %s"),
					      g_strerror (errno));
			goto exception;
		}
	}
	
	i = 0;
	switch (context->priv->type) {
	case CAMEL_PGP_TYPE_GPG:
		argv[i++] = "gpg";
		
		argv[i++] = "--verify";
		
		argv[i++] = "--no-tty";
		/*argv[i++] = "--verbose";*/
		/*argv[i++] = "--yes";*/
		/*argv[i++] = "--batch";*/
		
		if (sigstream != NULL)
			argv[i++] = sigfile;
		
		argv[i++] = "-";
		break;
	case CAMEL_PGP_TYPE_PGP5:
		argv[i++] = "pgpv";
		
		argv[i++] = "-z";
		
		if (sigstream != NULL)
			argv[i++] = sigfile;
		
		argv[i++] = "-f";
		
		break;
	case CAMEL_PGP_TYPE_PGP2:
	case CAMEL_PGP_TYPE_PGP6:
		argv[i++] = "pgp";
		
		if (sigstream != NULL)
			argv[i++] = sigfile;
		
		argv[i++] = "-f";
		
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	argv[i++] = NULL;
	
	clearlen = 0;
	retval = crypto_exec_with_passwd (context->priv->path, argv,
					  plaintext->data, plaintext->len,
					  passwd_fds, NULL,
					  &cleartext, &clearlen,
					  &diagnostics);
	
	g_byte_array_free (plaintext, TRUE);
	
	/* cleanup */
	if (sigfile) {
		unlink (sigfile);
		g_free (sigfile);
	}
	
	valid = camel_pgp_validity_new ();
	
	if (retval != 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "%s", diagnostics);
		
		camel_pgp_validity_set_valid (valid, FALSE);
	} else {
		camel_pgp_validity_set_valid (valid, TRUE);
	}
	
	if (diagnostics) {
		char *desc;
		
		desc = e_utf8_from_locale_string (diagnostics);
		camel_pgp_validity_set_description (valid, desc);
		g_free (desc);
	}
	
	g_free (diagnostics);
	g_free (cleartext);
	
	PGP_UNLOCK(context);
	
	return valid;
	
 exception:
	
	g_byte_array_free (plaintext, TRUE);
	
	PGP_UNLOCK(context);
	
	return NULL;
}


/**
 * camel_pgp_encrypt:
 * @context: PGP Context
 * @sign: sign as well as encrypt
 * @userid: key id (or email address) to use when signing (assuming @sign is %TRUE)
 * @recipients: an array of recipient key ids and/or email addresses
 * @istream: cleartext input stream
 * @ostream: ciphertext output stream
 * @ex: exception
 *
 * PGP encrypts (and optionally signs) the cleartext input stream and
 * writes the resulting ciphertext to the output stream.
 *
 * Return value: 0 for success or -1 for failure.
 **/
int
camel_pgp_encrypt (CamelPgpContext *context, gboolean sign, const char *userid, GPtrArray *recipients,
		   CamelStream *istream, CamelStream *ostream, CamelException *ex)
{
	GPtrArray *recipient_list = NULL;
	GByteArray *plaintext;
	CamelStream *stream;
	GPtrArray *argv;
	int retval, r;
	char *ciphertext = NULL;
	char *diagnostics = NULL;
	int passwd_fds[2];
	char passwd_fd[32];
	char *passphrase = NULL;
	
	PGP_LOCK(context);
	
	/* get the plaintext in a form we can use */
	plaintext = g_byte_array_new ();
	stream = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), plaintext);
	camel_stream_write_to_stream (istream, stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	if (!plaintext->len) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("No plaintext to encrypt."));
		goto exception;
	}
	
	if (sign) {
		/* we only need a passphrase if we intend on signing */
		passphrase = pgp_get_passphrase (context->priv->session, context->priv->type,
						 (char *) userid);
		if (!passphrase) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
					     _("No password provided."));
			goto exception;
		}
	}
	
	if (pipe (passwd_fds) < 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Couldn't create pipe to GPG/PGP: %s"),
				      g_strerror (errno));
		
		goto exception;
	}
	
	/* check to make sure we have recipients */
	if (recipients->len == 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("No recipients specified"));
		
		goto exception;
	}
	
	argv = g_ptr_array_new ();
	recipient_list = g_ptr_array_new ();
	
	switch (context->priv->type) {
	case CAMEL_PGP_TYPE_GPG:
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
			g_ptr_array_add (argv, (char *) userid);
			
			g_ptr_array_add (argv, "--passphrase-fd");
			sprintf (passwd_fd, "%d", passwd_fds[0]);
			g_ptr_array_add (argv, passwd_fd);
		}
		break;
	case CAMEL_PGP_TYPE_PGP5:
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
		break;
	case CAMEL_PGP_TYPE_PGP2:
	case CAMEL_PGP_TYPE_PGP6:
		for (r = 0; r < recipients->len; r++) {
			char *buf, *recipient;
			
			recipient = recipients->pdata[r];
			buf = g_strdup (recipient);
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
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	g_ptr_array_add (argv, NULL);
	
	retval = crypto_exec_with_passwd (context->priv->path,
					  (char **) argv->pdata,
					  plaintext->data, plaintext->len,
					  passwd_fds, passphrase,
					  &ciphertext, NULL,
					  &diagnostics);
	
	g_byte_array_free (plaintext, TRUE);
	
	/* free the temp recipient list */
	if (recipient_list) {
		for (r = 0; r < recipient_list->len; r++)
			g_free (recipient_list->pdata[r]);
		g_ptr_array_free (recipient_list, TRUE);
	}
	
	g_free (passphrase);
	g_ptr_array_free (argv, TRUE);
	
	if (retval != 0 || !*ciphertext) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "%s", diagnostics);
		g_free (diagnostics);
		g_free (ciphertext);
		if (sign)
			pgp_forget_passphrase (context->priv->session, context->priv->type,
					       (char *) userid);
		
		PGP_UNLOCK(context);
		
		return -1;
	}
	
	g_free (diagnostics);
	
	camel_stream_write (ostream, ciphertext, strlen (ciphertext));
	g_free (ciphertext);
	
	PGP_UNLOCK(context);
	
	return 0;
	
 exception:
	
	g_byte_array_free (plaintext, TRUE);
	
	if (sign) {
		g_free (passphrase);
		pgp_forget_passphrase (context->priv->session, context->priv->type, (char *) userid);
	}
	
	PGP_UNLOCK(context);
	
	return -1;
}


/**
 * camel_pgp_encrypt:
 * @context: PGP Context
 * @ciphertext: ciphertext stream (ie input stream)
 * @cleartext: cleartext stream (ie output stream)
 * @ex: exception
 *
 * PGP decrypts the ciphertext input stream and writes the resulting
 * cleartext to the output stream.
 *
 * Return value: 0 for success or -1 for failure.
 **/
int
camel_pgp_decrypt (CamelPgpContext *context, CamelStream *istream,
		   CamelStream *ostream, CamelException *ex)
{
	GByteArray *ciphertext;
	CamelStream *stream;
	char *argv[15];
	char *plaintext = NULL;
	int plainlen;
	char *diagnostics = NULL;
	char *passphrase = NULL;
	int passwd_fds[2];
	char passwd_fd[32];
	int retval, i;
	
	PGP_LOCK(context);
	
	/* get the ciphertext in a form we can use */
	ciphertext = g_byte_array_new ();
	stream = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), ciphertext);
	camel_stream_write_to_stream (istream, stream);
	camel_object_unref (CAMEL_OBJECT (stream));
	
	if (!ciphertext->len) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("No ciphertext to decrypt."));
		
		goto exception;
	}
	
	passphrase = pgp_get_passphrase (context->priv->session, context->priv->type, NULL);
	if (!passphrase) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("No password provided."));
		
		goto exception;
	}
	
	if (pipe (passwd_fds) < 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Couldn't create pipe to GPG/PGP: %s"),
				      g_strerror (errno));
		
		goto exception;
	}
	
	i = 0;
	switch (context->priv->type) {
	case CAMEL_PGP_TYPE_GPG:
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
		break;
	case CAMEL_PGP_TYPE_PGP5:
		argv[i++] = "pgpv";
		argv[i++] = "-f";
		argv[i++] = "+batchmode=1";
		
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
		break;
	case CAMEL_PGP_TYPE_PGP2:
	case CAMEL_PGP_TYPE_PGP6:
		argv[i++] = "pgp";
		argv[i++] = "-f";
		
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	argv[i++] = NULL;
	
	retval = crypto_exec_with_passwd (context->priv->path, argv,
					  ciphertext->data, ciphertext->len,
					  passwd_fds, passphrase,
					  &plaintext, &plainlen,
					  &diagnostics);
	
	g_byte_array_free (ciphertext, TRUE);
	g_free (passphrase);
	
	if (retval != 0 || !*plaintext) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "%s", diagnostics);
		g_free (plaintext);
		g_free (diagnostics);
		
		pgp_forget_passphrase (context->priv->session, context->priv->type, NULL);
		
		PGP_UNLOCK(context);
		
		return -1;
	}
	
	g_free (diagnostics);
	
	camel_stream_write (ostream, plaintext, plainlen);
	g_free (plaintext);
	
	PGP_UNLOCK(context);
	
	return 0;
	
 exception:
	
	g_byte_array_free (ciphertext, TRUE);
	
	if (passphrase) {
		pgp_forget_passphrase (context->priv->session, context->priv->type, NULL);
		g_free (passphrase);
	}
	
	PGP_UNLOCK(context);
	
	return -1;
}


/* PGP vailidity stuff */
struct _CamelPgpValidity {
	gboolean valid;
	gchar *description;
};

CamelPgpValidity *
camel_pgp_validity_new (void)
{
	CamelPgpValidity *validity;
	
	validity = g_new (CamelPgpValidity, 1);
	validity->valid = FALSE;
	validity->description = NULL;
	
	return validity;
}

void
camel_pgp_validity_init (CamelPgpValidity *validity)
{
	g_assert (validity != NULL);
	
	validity->valid = FALSE;
	validity->description = NULL;
}

gboolean
camel_pgp_validity_get_valid (CamelPgpValidity *validity)
{
	if (validity == NULL)
		return FALSE;
	
	return validity->valid;
}

void
camel_pgp_validity_set_valid (CamelPgpValidity *validity, gboolean valid)
{
	g_assert (validity != NULL);
	
	validity->valid = valid;
}

gchar *
camel_pgp_validity_get_description (CamelPgpValidity *validity)
{
	if (validity == NULL)
		return NULL;
	
	return validity->description;
}

void
camel_pgp_validity_set_description (CamelPgpValidity *validity, const gchar *description)
{
	g_assert (validity != NULL);
	
	g_free (validity->description);
	validity->description = g_strdup (description);
}

void
camel_pgp_validity_clear (CamelPgpValidity *validity)
{
	g_assert (validity != NULL);
	
	validity->valid = FALSE;
	g_free (validity->description);
	validity->description = NULL;
}

void
camel_pgp_validity_free (CamelPgpValidity *validity)
{
	if (validity == NULL)
		return;
	
	g_free (validity->description);
	g_free (validity);
}
