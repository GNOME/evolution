/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include "camel-gpg-context.h"
#include "camel-stream-fs.h"
#include "camel-operation.h"

static void camel_gpg_context_class_init (CamelGpgContextClass *klass);
static void camel_gpg_context_init       (CamelGpgContext *obj);
static void camel_gpg_context_finalise   (CamelObject *obj);

static int                  gpg_sign (CamelCipherContext *ctx, const char *userid, CamelCipherHash hash,
				      CamelStream *istream, CamelStream *ostream, CamelException *ex);
static int                  gpg_clearsign (CamelCipherContext *context, const char *userid,
					   CamelCipherHash hash, CamelStream *istream,
					   CamelStream *ostream, CamelException *ex);
static CamelCipherValidity *gpg_verify (CamelCipherContext *context, CamelCipherHash hash,
					CamelStream *istream, CamelStream *sigstream,
					CamelException *ex);
static int                  gpg_encrypt (CamelCipherContext *context, gboolean sign, const char *userid,
					 GPtrArray *recipients, CamelStream *istream, CamelStream *ostream,
					 CamelException *ex);
static int                  gpg_decrypt (CamelCipherContext *context, CamelStream *istream,
					 CamelStream *ostream, CamelException *ex);

static const char *gpg_hash_to_id (CamelCipherContext *context, CamelCipherHash hash);
static CamelCipherHash gpg_id_to_hash (CamelCipherContext *context, const char *id);


static CamelCipherContextClass *parent_class = NULL;


CamelType
camel_gpg_context_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_cipher_context_get_type (),
					    "CamelGpgContext",
					    sizeof (CamelGpgContext),
					    sizeof (CamelGpgContextClass),
					    (CamelObjectClassInitFunc) camel_gpg_context_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_gpg_context_init,
					    (CamelObjectFinalizeFunc) camel_gpg_context_finalise);
	}
	
	return type;
}

static void
camel_gpg_context_class_init (CamelGpgContextClass *klass)
{
	CamelCipherContextClass *cipher_class = CAMEL_CIPHER_CONTEXT_CLASS (klass);
	
	parent_class = CAMEL_CIPHER_CONTEXT_CLASS (camel_type_get_global_classfuncs (camel_cipher_context_get_type ()));
	
	cipher_class->sign = gpg_sign;
	cipher_class->clearsign = gpg_clearsign;
	cipher_class->verify = gpg_verify;
	cipher_class->encrypt = gpg_encrypt;
	cipher_class->decrypt = gpg_decrypt;
	cipher_class->hash_to_id = gpg_hash_to_id;
	cipher_class->id_to_hash = gpg_id_to_hash;
}

static void
camel_gpg_context_init (CamelGpgContext *context)
{
	CamelCipherContext *cipher = (CamelCipherContext *) context;
	
	context->path = NULL;
	context->always_trust = FALSE;
	
	cipher->sign_protocol = "application/pgp-signature";
	cipher->encrypt_protocol = "application/pgp-encrypted";
}

static void
camel_gpg_context_finalise (CamelObject *object)
{
	CamelGpgContext *ctx = (CamelGpgContext *) object;
	
	g_free (ctx->path);
}


/**
 * camel_gpg_context_new:
 * @session: session
 * @path: path to gpg binary
 *
 * Creates a new gpg cipher context object.
 *
 * Returns a new gpg cipher context object.
 **/
CamelCipherContext *
camel_gpg_context_new (CamelSession *session, const char *path)
{
	CamelCipherContext *cipher;
	CamelGpgContext *ctx;
	
	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);
	g_return_val_if_fail (path != NULL, NULL);
	
	ctx = (CamelGpgContext *) camel_object_new (camel_gpg_context_get_type ());
	ctx->path = g_strdup (path);
	
	cipher = (CamelCipherContext *) ctx;
	cipher->session = session;
	camel_object_ref (session);
	
	return cipher;
}

static const char *
gpg_hash_to_id (CamelCipherContext *context, CamelCipherHash hash)
{
	switch (hash) {
	case CAMEL_CIPHER_HASH_MD2:
		return "pgp-md2";
	case CAMEL_CIPHER_HASH_MD5:
		return "pgp-md5";
	case CAMEL_CIPHER_HASH_SHA1:
	case CAMEL_CIPHER_HASH_DEFAULT:
		return "pgp-sha1";
	case CAMEL_CIPHER_HASH_RIPEMD160:
		return "pgp-ripemd160";
	}
	
	return NULL;
}

static CamelCipherHash
gpg_id_to_hash (CamelCipherContext *context, const char *id)
{
	if (!strcmp (id, "pgp-md2"))
		return CAMEL_CIPHER_HASH_MD2;
	else if (!strcmp (id, "pgp-md5"))
		return CAMEL_CIPHER_HASH_MD5;
	else if (!strcmp (id, "pgp-sha1"))
		return CAMEL_CIPHER_HASH_SHA1;
	else if (!strcmp (id, "pgp-ripemd160"))
		return CAMEL_CIPHER_HASH_RIPEMD160;
	
	return CAMEL_CIPHER_HASH_DEFAULT;
}


enum _GpgCtxMode {
	GPG_CTX_MODE_SIGN,
	GPG_CTX_MODE_VERIFY,
	GPG_CTX_MODE_ENCRYPT,
	GPG_CTX_MODE_DECRYPT,
};

struct _GpgCtx {
	enum _GpgCtxMode mode;
	CamelSession *session;
	GHashTable *userid_hint;
	gboolean complete;
	pid_t pid;
	
	char *path;
	char *userid;
	char *sigfile;
	GPtrArray *recipients;
	CamelCipherHash hash;
	gboolean always_trust;
	gboolean armor;
	
	int stdin;
	int stdout;
	int stderr;
	int status_fd;
	int passwd_fd;  /* only needed for sign/decrypt */
	
	/* status-fd buffer */
	unsigned char *statusbuf;
	unsigned char *statusptr;
	unsigned int statusleft;
	
	gboolean need_passwd;
	char *passwd;
	
	CamelStream *istream;
	CamelStream *ostream;
	
	GByteArray *diagnostics;
};

static struct _GpgCtx *
gpg_ctx_new (CamelSession *session, const char *path)
{
	struct _GpgCtx *gpg;
	
	gpg = g_new (struct _GpgCtx, 1);
	gpg->mode = GPG_CTX_MODE_SIGN;
	gpg->session = session;
	camel_object_ref (CAMEL_OBJECT (session));
	gpg->userid_hint = g_hash_table_new (g_str_hash, g_str_equal);
	gpg->complete = FALSE;
	gpg->pid = (pid_t) -1;
	
	gpg->path = g_strdup (path);
	gpg->userid = NULL;
	gpg->sigfile = NULL;
	gpg->recipients = NULL;
	gpg->hash = CAMEL_CIPHER_HASH_DEFAULT;
	gpg->always_trust = FALSE;
	gpg->armor = FALSE;
	
	gpg->stdin = -1;
	gpg->stdout = -1;
	gpg->stderr = -1;
	gpg->status_fd = -1;
	gpg->passwd_fd = -1;
	
	gpg->statusbuf = g_malloc (128);
	gpg->statusptr = gpg->statusbuf;
	gpg->statusleft = 128;
	
	gpg->need_passwd = FALSE;
	gpg->passwd = NULL;
	
	gpg->istream = NULL;
	gpg->ostream = NULL;
	
	gpg->diagnostics = g_byte_array_new ();
	
	return gpg;
}

static void
gpg_ctx_set_mode (struct _GpgCtx *gpg, enum _GpgCtxMode mode)
{
	gpg->mode = mode;
	gpg->need_passwd = mode == GPG_CTX_MODE_SIGN || mode == GPG_CTX_MODE_DECRYPT;
}

static void
gpg_ctx_set_hash (struct _GpgCtx *gpg, CamelCipherHash hash)
{
	gpg->hash = hash;
}

static void
gpg_ctx_set_always_trust (struct _GpgCtx *gpg, gboolean trust)
{
	gpg->always_trust = trust;
}

static void
gpg_ctx_set_userid (struct _GpgCtx *gpg, const char *userid)
{
	g_free (gpg->userid);
	gpg->userid = g_strdup (userid);
}

static void
gpg_ctx_add_recipient (struct _GpgCtx *gpg, const char *keyid)
{
	if (gpg->mode != GPG_CTX_MODE_ENCRYPT)
		return;
	
	if (!gpg->recipients)
		gpg->recipients = g_ptr_array_new ();
	
	g_ptr_array_add (gpg->recipients, g_strdup (keyid));
}

static void
gpg_ctx_set_sigfile (struct _GpgCtx *gpg, const char *sigfile)
{
	g_free (gpg->sigfile);
	gpg->sigfile = g_strdup (sigfile);
}

static void
gpg_ctx_set_armor (struct _GpgCtx *gpg, gboolean armor)
{
	gpg->armor = armor;
}

static void
gpg_ctx_set_istream (struct _GpgCtx *gpg, CamelStream *istream)
{
	camel_object_ref (CAMEL_OBJECT (istream));
	if (gpg->istream)
		camel_object_unref (CAMEL_OBJECT (gpg->istream));
	gpg->istream = istream;
}

static void
gpg_ctx_set_ostream (struct _GpgCtx *gpg, CamelStream *ostream)
{
	camel_object_ref (CAMEL_OBJECT (ostream));
	if (gpg->ostream)
		camel_object_unref (CAMEL_OBJECT (gpg->ostream));
	gpg->istream = ostream;
}

static char *
gpg_ctx_get_diagnostics (struct _GpgCtx *gpg)
{
	return g_strndup (gpg->diagnostics->data, gpg->diagnostics->len);
}

static void
userid_hint_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);
}

static void
gpg_ctx_free (struct _GpgCtx *gpg)
{
	int i;
	
	if (gpg->session)
		camel_object_unref (CAMEL_OBJECT (gpg->session));
	
	g_hash_table_foreach (gpg->userid_hint, userid_hint_free, NULL);
	g_hash_table_destroy (gpg->userid_hint);
	
	g_free (gpg->path);
	
	g_free (gpg->userid);
	
	g_free (gpg->sigfile);
	
	if (gpg->recipients) {
		for (i = 0; i < gpg->recipients->len; i++)
			g_free (gpg->recipients->pdata[i]);
	
		g_ptr_array_free (gpg->recipients, TRUE);
	}
	
	if (gpg->stdin != -1)
		close (gpg->stdin);
	if (gpg->stdout != -1)
		close (gpg->stdout);
	if (gpg->stderr != -1)
		close (gpg->stderr);
	if (gpg->status_fd != -1)
		close (gpg->status_fd);
	if (gpg->passwd_fd != -1)
		close (gpg->passwd_fd);
	
	g_free (gpg->statusbuf);
	
	if (gpg->passwd)
		g_free (gpg->passwd);
	
	if (gpg->istream)
		camel_object_unref (CAMEL_OBJECT (gpg->istream));
	
	if (gpg->ostream)
		camel_object_unref (CAMEL_OBJECT (gpg->ostream));
	
	g_byte_array_free (gpg->diagnostics, TRUE);
	
	g_free (gpg);
}

static const char *
gpg_hash_str (CamelCipherHash hash)
{
	switch (hash) {
	case CAMEL_CIPHER_HASH_MD2:
		return "MD2";
	case CAMEL_CIPHER_HASH_MD5:
		return "MD5";
	case CAMEL_CIPHER_HASH_SHA1:
		return "SHA1";
	case CAMEL_CIPHER_HASH_RIPEMD160:
		return "RIPEMD160";
	default:
		return NULL;
	}
}

static GPtrArray *
gpg_ctx_get_argv (struct _GpgCtx *gpg, int status_fd, char **sfd, int passwd_fd, char **pfd)
{
	const char *hash_str;
	GPtrArray *argv;
	char *buf;
	int i;
	
	argv = g_ptr_array_new ();
	g_ptr_array_add (argv, "gpg");
	
	g_ptr_array_add (argv, "--verbose");
	g_ptr_array_add (argv, "--no-secmem-warning");
	g_ptr_array_add (argv, "--no-greeting");
	g_ptr_array_add (argv, "--batch");
	g_ptr_array_add (argv, "--yes");
	
	*sfd = buf = g_strdup_printf ("--status-fd=%d", status_fd);
	g_ptr_array_add (argv, buf);
	
	switch (gpg->mode) {
	case GPG_CTX_MODE_SIGN:
		g_ptr_array_add (argv, "--sign");
		g_ptr_array_add (argv, "-b");
		if (gpg->armor)
			g_ptr_array_add (argv, "--armor");
		hash_str = gpg_hash_str (gpg->hash);
		if (hash_str) {
			g_ptr_array_add (argv, "--digest-algo");
			g_ptr_array_add (argv, (char *) hash_str);
		}
		if (gpg->userid) {
			g_ptr_array_add (argv, "-u");
			g_ptr_array_add (argv, (char *) gpg->userid);
		}
		g_ptr_array_add (argv, "--output");
		g_ptr_array_add (argv, "-");
		break;
	case GPG_CTX_MODE_VERIFY:
		if (!camel_session_is_online (gpg->session))
			g_ptr_array_add (argv, "--no-auto-key-retrieve");
		g_ptr_array_add (argv, "--no-tty");
		g_ptr_array_add (argv, "--verify");
		if (gpg->sigfile)
			g_ptr_array_add (argv, gpg->sigfile);
		g_ptr_array_add (argv, "-");
		break;
	case GPG_CTX_MODE_ENCRYPT:
		g_ptr_array_add (argv,  "--encrypt");
		if (gpg->armor)
			g_ptr_array_add (argv, "--armor");
		if (gpg->always_trust)
			g_ptr_array_add (argv, "--always-trust");
		if (gpg->userid) {
			g_ptr_array_add (argv, "-u");
			g_ptr_array_add (argv, (char *) gpg->userid);
		}
		if (gpg->recipients) {
			for (i = 0; i < gpg->recipients->len; i++) {
				g_ptr_array_add (argv, "-r");
				g_ptr_array_add (argv, gpg->recipients->pdata[i]);
			}
		}
		g_ptr_array_add (argv, "--output");
		g_ptr_array_add (argv, "-");
		break;
	case GPG_CTX_MODE_DECRYPT:
		g_ptr_array_add (argv, "--decrypt");
		g_ptr_array_add (argv, "--output");
		g_ptr_array_add (argv, "-");
		break;
	}
	
	if (gpg->need_passwd && passwd_fd != -1) {
		*pfd = buf = g_strdup_printf ("--passphrase-fd=%d", passwd_fd);
		g_ptr_array_add (argv, buf);
	}
	
	printf ("gpg command-line: ");
	for (i = 0; i < argv->len; i++)
		printf ("%s ", argv->pdata[i]);
	printf ("\n");
	
	g_ptr_array_add (argv, NULL);
	
	return argv;
}

static int
gpg_ctx_op_start (struct _GpgCtx *gpg)
{
	char *status_fd = NULL, *passwd_fd = NULL;
	int i, maxfd, fds[10];
	GPtrArray *argv;
	
	for (i = 0; i < 10; i++)
		fds[i] = -1;
	
	maxfd = gpg->need_passwd ? 10 : 8;
	for (i = 0; i < maxfd; i += 2) {
		if (pipe (fds + i) == -1)
			goto exception;
	}
	
	argv = gpg_ctx_get_argv (gpg, fds[7], &status_fd, fds[8], &passwd_fd);
	
	if (!(gpg->pid = fork ())) {
		/* child process */
		
		if ((dup2 (fds[0], STDIN_FILENO) < 0 ) ||
		    (dup2 (fds[3], STDOUT_FILENO) < 0 ) ||
		    (dup2 (fds[5], STDERR_FILENO) < 0 )) {
			_exit (255);
		}
		
		/* Dissociate from camel's controlling terminal so
		 * that gpg won't be able to read from it.
		 */
		setsid ();
		
		maxfd = sysconf (_SC_OPEN_MAX);
		if (maxfd > 0) {
			/* Loop over all fds. */
			for (i = 0; i < maxfd; i++) {
				if ((i != STDIN_FILENO) &&
				    (i != STDOUT_FILENO) &&
				    (i != STDERR_FILENO) &&
				    (i != fds[7]) &&  /* status fd */
				    (i != fds[8]))    /* passwd fd */
					close (i);
			}
		}
		
		/* run gpg */
		execvp (gpg->path, (char **) argv->pdata);
		_exit (255);
	} else if (gpg->pid < 0) {
		g_ptr_array_free (argv, TRUE);
		g_free (status_fd);
		g_free (passwd_fd);
		goto exception;
	}
	
	g_ptr_array_free (argv, TRUE);
	g_free (status_fd);
	g_free (passwd_fd);
	
	/* Parent */
	close (fds[0]);
	gpg->stdin = fds[1];
	gpg->stdout = fds[2];
	close (fds[3]);
	gpg->stderr = fds[4];
	close (fds[5]);
	gpg->status_fd = fds[6];
	close (fds[7]);
	if (gpg->need_passwd) {
		close (fds[8]);
		gpg->passwd_fd = fds[9];
		fcntl (gpg->passwd_fd, F_SETFL, O_NONBLOCK);
	}
	
	fcntl (gpg->stdin, F_SETFL, O_NONBLOCK);
	fcntl (gpg->stdout, F_SETFL, O_NONBLOCK);
	fcntl (gpg->stderr, F_SETFL, O_NONBLOCK);
	fcntl (gpg->status_fd, F_SETFL, O_NONBLOCK);
	
	return 0;
	
 exception:
	
	for (i = 0; i < 10; i++) {
		if (fds[i] != -1)
			close (fds[i]);
	}
	
	return -1;
}

static const char *
next_token (const char *in, char **token)
{
	const char *start, *inptr = in;
	
	while (*inptr == ' ')
		inptr++;
	
	if (*inptr == '\0' || *inptr == '\n') {
		if (token)
			*token = NULL;
		return inptr;
	}
	
	start = inptr;
	while (*inptr && *inptr != ' ' && *inptr != '\n')
		inptr++;
	
	if (token)
		*token = g_strndup (start, inptr - start);
	
	return inptr;
}

static int
gpg_ctx_parse_status (struct _GpgCtx *gpg, CamelException *ex)
{
	register unsigned char *inptr;
	const unsigned char *status;
	int len;
	
 parse:
	
	inptr = gpg->statusbuf;
	while (inptr < gpg->statusptr && *inptr != '\n')
		inptr++;
	
	if (*inptr != '\n') {
		/* we don't have enough data buffered to parse this status line */
		return 0;
	}
	
	*inptr++ = '\0';
	status = gpg->statusbuf;
	
	printf ("status: %s", status);
	fflush (stdout);
	
	if (strncmp (status, "[GNUPG:] ", 9) != 0)
		return -1;
	
	status += 9;
	
	if (!strncmp (status, "USERID_HINT ", 12)) {
		char *hint, *user;
		
		status += 12;
		status = next_token (status, &hint);
		if (!hint) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Failed to parse gpg userid hint."));
			return -1;
		}
		
		if (g_hash_table_lookup (gpg->userid_hint, hint)) {
			/* we already have this userid hint... */
			g_free (hint);
			goto recycle;
		}
		
		user = g_strdup (status);
		g_strstrip (user);
		
		g_hash_table_insert (gpg->userid_hint, hint, user);
	} else if (!strncmp (status, "NEED_PASSPHRASE ", 16)) {
		char *prompt, *userid;
		const char *name;
		
		status += 16;
		
		status = next_token (status, &userid);
		if (!userid) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Failed to parse gpg passphrase request."));
			return -1;
		}
		
		name = g_hash_table_lookup (gpg->userid_hint, userid);
		if (!name)
			name = userid;
		
		prompt = g_strdup_printf (_("You need a passphrase to unlock the key for\n"
					    "user: \"%s\""), name);
		
		gpg->passwd = camel_session_get_password (gpg->session, prompt, TRUE, NULL, userid, ex);
		g_free (prompt);
		g_free (userid);
		
		if (gpg->passwd == NULL) {
			if (!camel_exception_is_set (ex))
				camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL, _("Cancelled."));
			return -1;
		}
	} else if (!strncmp (status, "GOOD_PASSPHRASE ", 16)) {
		g_free (gpg->passwd);
		gpg->passwd = NULL;
		gpg->need_passwd = FALSE;
	} else if (!strncmp (status, "BAD_PASSPHRASE ", 15)) {
		g_free (gpg->passwd);
		gpg->passwd = NULL;
		gpg->need_passwd = TRUE;
	} else if (!strncmp (status, "UNEXPECTED ", 11)) {
		/* this is an error */
		return -1;
	} else {
		/* check to see if we are complete */
		switch (gpg->mode) {
		case GPG_CTX_MODE_SIGN:
			if (!strncmp (status, "SIG_CREATED ", 12))
				gpg->complete = TRUE;
			break;
		case GPG_CTX_MODE_VERIFY:
			/* FIXME: we should save this so we can present it to the user? */
			if (!strncmp (status, "TRUST_", 6) || !strncmp (status, "BADSIG ", 7))
				gpg->complete = TRUE;
			break;
		case GPG_CTX_MODE_ENCRYPT:
			if (!strncmp (status, "END_ENCRYPTION", 14))
				gpg->complete = TRUE;
			break;
		case GPG_CTX_MODE_DECRYPT:
			if (!strncmp (status, "END_DECRYPTION", 14))
				gpg->complete = TRUE;
			break;
		}
		
		printf ("okay, that's all folks...\n");
	}
	
 recycle:
	
	/* recycle our statusbuf by moving inptr to the beginning of statusbuf */
	len = gpg->statusptr - inptr;
	memmove (gpg->statusbuf, inptr, len);
	
	len = inptr - gpg->statusbuf;
	gpg->statusleft += len;
	gpg->statusptr -= len;
	
	/* if we have more data, try parsing the next line? */
	if (gpg->statusptr > gpg->statusbuf)
		goto parse;
	
	return 0;
}

#define status_backup(gpg, start, len) G_STMT_START {                     \
	if (gpg->statusleft <= len) {                                     \
		unsigned int slen, soff;                                  \
		                                                          \
		slen = soff = gpg->statusptr - gpg->statusbuf;            \
		slen = slen ? slen : 1;                                   \
		                                                          \
		while (slen < soff + len)                                 \
			slen <<= 1;                                       \
		                                                          \
		gpg->statusbuf = g_realloc (gpg->statusbuf, slen + 1);    \
		gpg->statusptr = gpg->statusbuf + soff;                   \
		gpg->statusleft = slen - soff;                            \
	}                                                                 \
	                                                                  \
	memcpy (gpg->statusptr, start, len);                              \
	gpg->statusptr += len;                                            \
	gpg->statusleft -= len;                                           \
} G_STMT_END


static int
gpg_ctx_op_step (struct _GpgCtx *gpg, CamelException *ex)
{
	fd_set rdset, wrset, *wrsetp = NULL;
	struct timeval timeout;
	const char *mode;
	int maxfd = 0;
	int ready;
	
 retry:
	FD_ZERO (&rdset);
	FD_SET (gpg->stdout, &rdset);
	FD_SET (gpg->stderr, &rdset);
	FD_SET (gpg->status_fd, &rdset);
	
	maxfd = MAX (gpg->stdout, gpg->stderr);
	maxfd = MAX (maxfd, gpg->status_fd);
	
	if (gpg->stdin != -1 || gpg->passwd_fd != -1) {
		FD_ZERO (&wrset);
		if (gpg->stdin != -1) {
			FD_SET (gpg->stdin, &wrset);
			maxfd = MAX (maxfd, gpg->stdin);
		}
		if (gpg->passwd_fd != -1) {
			FD_SET (gpg->passwd_fd, &wrset);
			maxfd = MAX (maxfd, gpg->passwd_fd);
		}
		
		wrsetp = &wrset;
	}
	
	timeout.tv_sec = 10; /* timeout in seconds */
	timeout.tv_usec = 0;
	
	if ((ready = select (maxfd + 1, &rdset, wrsetp, NULL, &timeout)) == 0)
		return 0;
	
	if (ready < 0) {
		if (errno == EINTR)
			goto retry;
		
		printf ("select() failed: %s\n", g_strerror (errno));
		
		return -1;
	}
	
	/* Test each and every file descriptor to see if it's 'ready',
	   and if so - do what we can with it and then drop through to
	   the next file descriptor and so on until we've done what we
	   can to all of them. If one fails along the way, return
	   -1. */
	
	if (FD_ISSET (gpg->status_fd, &rdset)) {
		/* read the status message and decide what to do... */
		char buffer[4096];
		ssize_t nread;
		
		printf ("reading from gpg's status-fd...\n");
		
		nread = read (gpg->status_fd, buffer, sizeof (buffer));
		if (nread == -1)
			goto exception;
		
		if (nread > 0) {
			status_backup (gpg, buffer, nread);
			
			if (gpg_ctx_parse_status (gpg, ex) == -1)
				return -1;
		}
	}
	
	if (FD_ISSET (gpg->stdout, &rdset) && gpg->ostream) {
		char buffer[4096];
		ssize_t nread;
		
		printf ("reading gpg's stdout...\n");
		
		nread = read (gpg->stdout, buffer, sizeof (buffer));
		if (nread == -1)
			goto exception;
		
		if (camel_stream_write (gpg->ostream, buffer, (size_t) nread) == -1)
			goto exception;
	}
	
	if (FD_ISSET (gpg->stderr, &rdset)) {
		char buffer[4096];
		ssize_t nread;
		
		printf ("reading gpg's stderr...\n");
		
		nread = read (gpg->stderr, buffer, sizeof (buffer));
		if (nread == -1)
			goto exception;
		
		g_byte_array_append (gpg->diagnostics, buffer, nread);
	}
	
	if (wrsetp && FD_ISSET (gpg->passwd_fd, &wrset) && gpg->passwd_fd != -1 && gpg->need_passwd && gpg->passwd) {
		ssize_t w, nwritten = 0;
		size_t n;
		
		printf ("sending gpg our passphrase...\n");
		
		/* send the passphrase to gpg */
		n = strlen (gpg->passwd);
		do {
			do {
				w = write (gpg->passwd_fd, gpg->passwd + nwritten, n - nwritten);
			} while (w == -1 && (errno == EINTR || errno == EAGAIN));
			
			if (w > 0)
				nwritten += w;
		} while (nwritten < n && w != -1);
		
		if (w == -1)
			goto exception;
	}
	
	if (wrsetp && FD_ISSET (gpg->stdin, &wrset) && gpg->stdin != -1) {
		char buffer[4096];
		ssize_t nread;
		
		printf ("writing to gpg's stdin...");
		fflush (stdout);
		
		/* write our stream to gpg's stdin */
		nread = camel_stream_read (gpg->istream, buffer, sizeof (buffer));
		if (nread > 0) {
			ssize_t w, nwritten = 0;
			
			do {
				do {
					printf (".");
					w = write (gpg->stdin, buffer + nwritten, nread - nwritten);
				} while (w == -1 && (errno == EINTR || errno == EAGAIN));
				
				if (w > 0)
					nwritten += w;
			} while (nwritten < nread && w != -1);
			
			if (w == -1) {
				printf ("write failed: %s\n", g_strerror (errno));
				goto exception;
			}
		}
		
		if (camel_stream_eos (gpg->istream)) {
			close (gpg->stdin);
			gpg->stdin = -1;
		}
		
		printf ("done.\n");
	}
	
	return 0;
	
 exception:
	
	switch (gpg->mode) {
	case GPG_CTX_MODE_SIGN:
		mode = "sign";
		break;
	case GPG_CTX_MODE_VERIFY:
		mode = "verify";
		break;
	case GPG_CTX_MODE_ENCRYPT:
		mode = "encrypt";
		break;
	case GPG_CTX_MODE_DECRYPT:
		mode = "decrypt";
		break;
	default:
		g_assert_not_reached ();
		mode = NULL;
		break;
	}
	
	if (gpg->diagnostics->len) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to GPG %s message: %s\n\n%.*s"),
				      mode, g_strerror (errno),
				      gpg->diagnostics->len,
				      gpg->diagnostics->data);
	} else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to GPG %s message: %s\n"),
				      mode, g_strerror (errno));
	}
	
	return -1;
}

static gboolean
gpg_ctx_op_complete (struct _GpgCtx *gpg)
{
	return gpg->complete;
}

static void
gpg_ctx_op_cancel (struct _GpgCtx *gpg)
{
	pid_t retval;
	int status;
	
	kill (gpg->pid, SIGTERM);
	sleep (1);
	retval = waitpid (gpg->pid, &status, WNOHANG);
	if (retval == 0) {
		/* no more mr nice guy... */
		kill (gpg->pid, SIGKILL);
		sleep (1);
		waitpid (gpg->pid, &status, WNOHANG);
	}
}

static int
gpg_ctx_op_wait (struct _GpgCtx *gpg)
{
	sigset_t mask, omask;
	pid_t retval;
	int status;
	
	sigemptyset (&mask);
	sigaddset (&mask, SIGALRM);
	sigprocmask (SIG_BLOCK, &mask, &omask);
	alarm (1);
	retval = waitpid (gpg->pid, &status, 0);
	alarm (0);
	sigprocmask (SIG_SETMASK, &omask, NULL);
	
	if (retval == (pid_t) -1 && errno == EINTR) {
		/* The child is hanging: send a friendly reminder. */
		kill (gpg->pid, SIGTERM);
		sleep (1);
		retval = waitpid (gpg->pid, &status, WNOHANG);
		if (retval == (pid_t) 0) {
			/* Still hanging; use brute force. */
			kill (gpg->pid, SIGKILL);
			sleep (1);
			retval = waitpid (gpg->pid, &status, WNOHANG);
		}
	}
	
	if (retval != (pid_t) -1 && WIFEXITED (status))
		return WEXITSTATUS (status);
	else
		return -1;
}



static int
gpg_sign (CamelCipherContext *context, const char *userid, CamelCipherHash hash,
	  CamelStream *istream, CamelStream *ostream, CamelException *ex)
{
	CamelGpgContext *ctx = (CamelGpgContext *) context;
	struct _GpgCtx *gpg;
	
	gpg = gpg_ctx_new (context->session, ctx->path);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_SIGN);
	gpg_ctx_set_hash (gpg, hash);
	gpg_ctx_set_armor (gpg, TRUE);
	gpg_ctx_set_userid (gpg, userid);
	gpg_ctx_set_istream (gpg, istream);
	gpg_ctx_set_ostream (gpg, ostream);
	
	if (gpg_ctx_op_start (gpg) == -1) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Failed to execute gpg."));
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (camel_operation_cancel_check (NULL)) {
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Cancelled."));
			gpg_ctx_op_cancel (gpg);
			gpg_ctx_free (gpg);
			
			return -1;
		}
		
		if (gpg_ctx_op_step (gpg, ex) == -1) {
			gpg_ctx_op_cancel (gpg);
			gpg_ctx_free (gpg);
			
			return -1;
		}
	}
	
	if (gpg_ctx_op_wait (gpg) != 0) {
		char *diagnostics;
		
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, diagnostics);
		g_free (diagnostics);
	}
	gpg_ctx_free (gpg);
	
	return 0;
}


static int
gpg_clearsign (CamelCipherContext *context, const char *userid,
	       CamelCipherHash hash, CamelStream *istream,
	       CamelStream *ostream, CamelException *ex)
{
	return -1;
}


static char *
swrite (CamelStream *istream)
{
	CamelStream *ostream;
	char *template;
	int fd, ret;
	
	template = g_strdup ("/tmp/evolution-pgp.XXXXXX");
	fd = mkstemp (template);
	if (fd == -1) {
		g_free (template);
		return NULL;
	}
	
	ostream = camel_stream_fs_new_with_fd (fd);
	ret = camel_stream_write_to_stream (istream, ostream);
	if (ret != -1) {
		ret = camel_stream_flush (ostream);
		if (ret != -1)
			ret = camel_stream_close (ostream);
	}
	camel_object_unref (CAMEL_OBJECT (ostream));
	
	if (ret == -1) {
		unlink (template);
		g_free (template);
		return NULL;
	}
	
	return template;
}

static CamelCipherValidity *
gpg_verify (CamelCipherContext *context, CamelCipherHash hash,
	    CamelStream *istream, CamelStream *sigstream,
	    CamelException *ex)
{
	CamelGpgContext *ctx = (CamelGpgContext *) context;
	CamelCipherValidity *validity;
	char *diagnostics = NULL;
	struct _GpgCtx *gpg;
	char *sigfile = NULL;
	gboolean valid;
	
	if (sigstream != NULL) {
		/* We are going to verify a detached signature so save
		   the signature to a temp file. */
		sigfile = swrite (sigstream);
		if (sigfile == NULL) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Cannot verify this message: couldn't create temp file: %s"),
					      g_strerror (errno));
			return NULL;
		}
	}
	
	gpg = gpg_ctx_new (context->session, ctx->path);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_VERIFY);
	gpg_ctx_set_hash (gpg, hash);
	gpg_ctx_set_sigfile (gpg, sigfile);
	gpg_ctx_set_istream (gpg, istream);
	
	if (gpg_ctx_op_start (gpg) == -1) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Failed to execute gpg."));
		printf ("gpg_ctx_op_start() failed\n");
		gpg_ctx_free (gpg);
		goto exception;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (camel_operation_cancel_check (NULL)) {
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Cancelled."));
			printf ("user cancelled gpg operation\n");
			gpg_ctx_op_cancel (gpg);
			goto exception;
		}
		
		if (gpg_ctx_op_step (gpg, ex) == -1) {
			diagnostics = gpg_ctx_get_diagnostics (gpg);
			printf ("gpg_ctx_op_step() failed: \n%s\n\n", diagnostics);
			gpg_ctx_op_cancel (gpg);
			g_free (diagnostics);
			goto exception;
		}
	}
	
	diagnostics = gpg_ctx_get_diagnostics (gpg);
	
	valid = gpg_ctx_op_wait (gpg) == 0;
	gpg_ctx_free (gpg);
	
	validity = camel_cipher_validity_new ();
	camel_cipher_validity_set_valid (validity, valid);
	camel_cipher_validity_set_description (validity, diagnostics);
	g_free (diagnostics);
	
	if (sigfile) {
		unlink (sigfile);
		g_free (sigfile);
	}
	
	return validity;
	
 exception:
	
	diagnostics = gpg_ctx_get_diagnostics (gpg);
	
	
	gpg_ctx_free (gpg);
		
	if (sigfile) {
		unlink (sigfile);
		g_free (sigfile);
	}
	
	return NULL;
}


static int
gpg_encrypt (CamelCipherContext *context, gboolean sign, const char *userid,
	     GPtrArray *recipients, CamelStream *istream, CamelStream *ostream,
	     CamelException *ex)
{
	CamelGpgContext *ctx = (CamelGpgContext *) context;
	struct _GpgCtx *gpg;
	int i;
	
	gpg = gpg_ctx_new (context->session, ctx->path);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_ENCRYPT);
	gpg_ctx_set_armor (gpg, TRUE);
	gpg_ctx_set_userid (gpg, userid);
	gpg_ctx_set_istream (gpg, istream);
	gpg_ctx_set_ostream (gpg, ostream);
	gpg_ctx_set_always_trust (gpg, TRUE);
	
	for (i = 0; i < recipients->len; i++) {
		gpg_ctx_add_recipient (gpg, recipients->pdata[i]);
	}
	
	if (gpg_ctx_op_start (gpg) == -1) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Failed to execute gpg."));
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (camel_operation_cancel_check (NULL)) {
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Cancelled."));
			gpg_ctx_op_cancel (gpg);
			gpg_ctx_free (gpg);
			
			return -1;
		}
		
		if (gpg_ctx_op_step (gpg, ex) == -1) {
			gpg_ctx_op_cancel (gpg);
			gpg_ctx_free (gpg);
			
			return -1;
		}
	}
	
	if (gpg_ctx_op_wait (gpg) != 0) {
		char *diagnostics;
		
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, diagnostics);
		g_free (diagnostics);
	}
	
	gpg_ctx_free (gpg);
	
	return 0;
}


static int
gpg_decrypt (CamelCipherContext *context, CamelStream *istream,
	     CamelStream *ostream, CamelException *ex)
{
	CamelGpgContext *ctx = (CamelGpgContext *) context;
	struct _GpgCtx *gpg;
	
	gpg = gpg_ctx_new (context->session, ctx->path);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_DECRYPT);
	gpg_ctx_set_istream (gpg, istream);
	gpg_ctx_set_ostream (gpg, ostream);
	
	if (gpg_ctx_op_start (gpg) == -1) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Failed to execute gpg."));
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (camel_operation_cancel_check (NULL)) {
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Cancelled."));
			gpg_ctx_op_cancel (gpg);
			gpg_ctx_free (gpg);
			
			return -1;
		}
		
		if (gpg_ctx_op_step (gpg, ex) == -1) {
			gpg_ctx_op_cancel (gpg);
			gpg_ctx_free (gpg);
			
			return -1;
		}
	}
	
	if (gpg_ctx_op_wait (gpg) != 0) {
		char *diagnostics;
		
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, diagnostics);
		g_free (diagnostics);
	}
	
	gpg_ctx_free (gpg);
	
	return 0;
}
