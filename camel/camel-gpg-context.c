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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include "gal/util/e-iconv.h"

#include "camel-gpg-context.h"
#include "camel-mime-filter-charset.h"
#include "camel-stream-filter.h"
#include "camel-stream-mem.h"
#include "camel-stream-fs.h"
#include "camel-operation.h"
#include "camel-mime-part.h"
#include "camel-mime-filter-canon.h"

#include "camel-multipart-signed.h"
#include "camel-multipart-encrypted.h"

#define d(x) 

static CamelCipherContextClass *parent_class = NULL;

/**
 * camel_gpg_context_new:
 * @session: session
 *
 * Creates a new gpg cipher context object.
 *
 * Returns a new gpg cipher context object.
 **/
CamelCipherContext *
camel_gpg_context_new (CamelSession *session)
{
	CamelCipherContext *cipher;
	CamelGpgContext *ctx;
	
	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);
	
	ctx = (CamelGpgContext *) camel_object_new (camel_gpg_context_get_type ());
	
	cipher = (CamelCipherContext *) ctx;
	cipher->session = session;
	camel_object_ref (session);
	
	return cipher;
}


/**
 * camel_gpg_context_set_always_trust:
 * @ctx: gpg context
 * @always_trust always truct flag
 *
 * Sets the @always_trust flag on the gpg context which is used for
 * encryption.
 **/
void
camel_gpg_context_set_always_trust (CamelGpgContext *ctx, gboolean always_trust)
{
	g_return_if_fail (CAMEL_IS_GPG_CONTEXT (ctx));
	
	ctx->always_trust = always_trust;
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
	case CAMEL_CIPHER_HASH_TIGER192:
		return "pgp-tiger192";
	case CAMEL_CIPHER_HASH_HAVAL5160:
		return "pgp-haval-5-160";
	}
	
	return NULL;
}

static CamelCipherHash
gpg_id_to_hash (CamelCipherContext *context, const char *id)
{
	if (id) {
		if (!strcmp (id, "pgp-md2"))
			return CAMEL_CIPHER_HASH_MD2;
		else if (!strcmp (id, "pgp-md5"))
			return CAMEL_CIPHER_HASH_MD5;
		else if (!strcmp (id, "pgp-sha1"))
			return CAMEL_CIPHER_HASH_SHA1;
		else if (!strcmp (id, "pgp-ripemd160"))
			return CAMEL_CIPHER_HASH_RIPEMD160;
		else if (!strcmp (id, "tiger192"))
			return CAMEL_CIPHER_HASH_TIGER192;
		else if (!strcmp (id, "haval-5-160"))
			return CAMEL_CIPHER_HASH_HAVAL5160;
	}
	
	return CAMEL_CIPHER_HASH_DEFAULT;
}


enum _GpgCtxMode {
	GPG_CTX_MODE_SIGN,
	GPG_CTX_MODE_VERIFY,
	GPG_CTX_MODE_ENCRYPT,
	GPG_CTX_MODE_DECRYPT,
	GPG_CTX_MODE_IMPORT,
	GPG_CTX_MODE_EXPORT,
};

enum _GpgTrustMetric {
	GPG_TRUST_NONE,
	GPG_TRUST_NEVER,
	GPG_TRUST_UNDEFINED,
	GPG_TRUST_MARGINAL,
	GPG_TRUST_FULLY,
	GPG_TRUST_ULTIMATE
};

struct _GpgCtx {
	enum _GpgCtxMode mode;
	CamelSession *session;
	GHashTable *userid_hint;
	pid_t pid;
	
	char *userid;
	char *sigfile;
	GPtrArray *recipients;
	CamelCipherHash hash;
	
	int stdin_fd;
	int stdout_fd;
	int stderr_fd;
	int status_fd;
	int passwd_fd;  /* only needed for sign/decrypt */
	
	/* status-fd buffer */
	unsigned char *statusbuf;
	unsigned char *statusptr;
	unsigned int statusleft;
	
	char *need_id;
	char *passwd;
	
	CamelStream *istream;
	CamelStream *ostream;
	
	GByteArray *diagbuf;
	CamelStream *diagnostics;
	
	int exit_status;
	
	unsigned int exited:1;
	unsigned int complete:1;
	unsigned int seen_eof1:1;
	unsigned int seen_eof2:1;
	unsigned int always_trust:1;
	unsigned int armor:1;
	unsigned int need_passwd:1;
	unsigned int send_passwd:1;
	
	unsigned int bad_passwds:2;
	
	unsigned int hadsig:1;
	unsigned int badsig:1;
	unsigned int errsig:1;
	unsigned int goodsig:1;
	unsigned int validsig:1;
	unsigned int nopubkey:1;
	unsigned int nodata:1;
	unsigned int trust:3;
	
	unsigned int diagflushed:1;
	
	unsigned int utf8:1;
	
	unsigned int padding:10;
};

static struct _GpgCtx *
gpg_ctx_new (CamelSession *session)
{
	struct _GpgCtx *gpg;
	const char *charset;
	CamelStream *stream;
	
	gpg = g_new (struct _GpgCtx, 1);
	gpg->mode = GPG_CTX_MODE_SIGN;
	gpg->session = session;
	camel_object_ref (session);
	gpg->userid_hint = g_hash_table_new (g_str_hash, g_str_equal);
	gpg->complete = FALSE;
	gpg->seen_eof1 = TRUE;
	gpg->seen_eof2 = FALSE;
	gpg->pid = (pid_t) -1;
	gpg->exit_status = 0;
	gpg->exited = FALSE;
	
	gpg->userid = NULL;
	gpg->sigfile = NULL;
	gpg->recipients = NULL;
	gpg->hash = CAMEL_CIPHER_HASH_DEFAULT;
	gpg->always_trust = FALSE;
	gpg->armor = FALSE;
	
	gpg->stdin_fd = -1;
	gpg->stdout_fd = -1;
	gpg->stderr_fd = -1;
	gpg->status_fd = -1;
	gpg->passwd_fd = -1;
	
	gpg->statusbuf = g_malloc (128);
	gpg->statusptr = gpg->statusbuf;
	gpg->statusleft = 128;
	
	gpg->bad_passwds = 0;
	gpg->need_passwd = FALSE;
	gpg->send_passwd = FALSE;
	gpg->need_id = NULL;
	gpg->passwd = NULL;
	
	gpg->nodata = FALSE;
	gpg->hadsig = FALSE;
	gpg->badsig = FALSE;
	gpg->errsig = FALSE;
	gpg->goodsig = FALSE;
	gpg->validsig = FALSE;
	gpg->nopubkey = FALSE;
	gpg->trust = GPG_TRUST_NONE;
	
	gpg->istream = NULL;
	gpg->ostream = NULL;
	
	stream = camel_stream_mem_new ();
	gpg->diagbuf = CAMEL_STREAM_MEM (stream)->buffer;
	gpg->diagflushed = FALSE;
	
	if ((charset = e_iconv_locale_charset ()) && strcasecmp (charset, "UTF-8") != 0) {
		CamelMimeFilterCharset *filter;
		CamelStreamFilter *fstream;
		
		gpg->utf8 = FALSE;
		
		if ((filter = camel_mime_filter_charset_new_convert (charset, "UTF-8"))) {
			fstream = camel_stream_filter_new_with_stream (stream);
			camel_stream_filter_add (fstream, (CamelMimeFilter *) filter);
			camel_object_unref (filter);
			camel_object_unref (stream);
			
			stream = (CamelStream *) fstream;
		}
	} else {
		gpg->utf8 = TRUE;
	}
	
	gpg->diagnostics = stream;
	
	return gpg;
}

static void
gpg_ctx_set_mode (struct _GpgCtx *gpg, enum _GpgCtxMode mode)
{
	gpg->mode = mode;
	gpg->need_passwd = ((gpg->mode == GPG_CTX_MODE_SIGN) || (gpg->mode == GPG_CTX_MODE_DECRYPT));
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
	if (gpg->mode != GPG_CTX_MODE_ENCRYPT && gpg->mode != GPG_CTX_MODE_EXPORT)
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
	camel_object_ref (istream);
	if (gpg->istream)
		camel_object_unref (gpg->istream);
	gpg->istream = istream;
}

static void
gpg_ctx_set_ostream (struct _GpgCtx *gpg, CamelStream *ostream)
{
	camel_object_ref (ostream);
	if (gpg->ostream)
		camel_object_unref (gpg->ostream);
	gpg->ostream = ostream;
	gpg->seen_eof1 = FALSE;
}

static const char *
gpg_ctx_get_diagnostics (struct _GpgCtx *gpg)
{
	if (!gpg->diagflushed) {
		gpg->diagflushed = TRUE;
		camel_stream_flush (gpg->diagnostics);
		if (gpg->diagbuf->len == 0)
			return NULL;
		
		g_byte_array_append (gpg->diagbuf, "", 1);
	}
	
	return gpg->diagbuf->data;
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

	if (gpg == NULL)
		return;

	if (gpg->session)
		camel_object_unref (gpg->session);
	
	g_hash_table_foreach (gpg->userid_hint, userid_hint_free, NULL);
	g_hash_table_destroy (gpg->userid_hint);
	
	g_free (gpg->userid);
	
	g_free (gpg->sigfile);
	
	if (gpg->recipients) {
		for (i = 0; i < gpg->recipients->len; i++)
			g_free (gpg->recipients->pdata[i]);
	
		g_ptr_array_free (gpg->recipients, TRUE);
	}
	
	if (gpg->stdin_fd != -1)
		close (gpg->stdin_fd);
	if (gpg->stdout_fd != -1)
		close (gpg->stdout_fd);
	if (gpg->stderr_fd != -1)
		close (gpg->stderr_fd);
	if (gpg->status_fd != -1)
		close (gpg->status_fd);
	if (gpg->passwd_fd != -1)
		close (gpg->passwd_fd);
	
	g_free (gpg->statusbuf);
	
	g_free (gpg->need_id);
	
	if (gpg->passwd) {
		memset (gpg->passwd, 0, strlen (gpg->passwd));
		g_free (gpg->passwd);
	}
	
	if (gpg->istream)
		camel_object_unref (gpg->istream);
	
	if (gpg->ostream)
		camel_object_unref (gpg->ostream);
	
	camel_object_unref (gpg->diagnostics);
	
	g_free (gpg);
}

static const char *
gpg_hash_str (CamelCipherHash hash)
{
	switch (hash) {
	case CAMEL_CIPHER_HASH_MD2:
		return "--digest-algo=MD2";
	case CAMEL_CIPHER_HASH_MD5:
		return "--digest-algo=MD5";
	case CAMEL_CIPHER_HASH_SHA1:
		return "--digest-algo=SHA1";
	case CAMEL_CIPHER_HASH_RIPEMD160:
		return "--digest-algo=RIPEMD160";
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
	g_ptr_array_add (argv, "--no-tty");
	if (passwd_fd == -1) {
		/* only use batch mode if we don't intend on using the
                   interactive --command-fd option */
		g_ptr_array_add (argv, "--batch");
		g_ptr_array_add (argv, "--yes");
	}
	
	*sfd = buf = g_strdup_printf ("--status-fd=%d", status_fd);
	g_ptr_array_add (argv, buf);
	
	if (passwd_fd != -1) {
		*pfd = buf = g_strdup_printf ("--command-fd=%d", passwd_fd);
		g_ptr_array_add (argv, buf);
	}
	
	switch (gpg->mode) {
	case GPG_CTX_MODE_SIGN:
		g_ptr_array_add (argv, "--sign");
		g_ptr_array_add (argv, "--detach");
		if (gpg->armor)
			g_ptr_array_add (argv, "--armor");
		hash_str = gpg_hash_str (gpg->hash);
		if (hash_str)
			g_ptr_array_add (argv, (char *) hash_str);
		if (gpg->userid) {
			g_ptr_array_add (argv, "-u");
			g_ptr_array_add (argv, (char *) gpg->userid);
		}
		g_ptr_array_add (argv, "--output");
		g_ptr_array_add (argv, "-");
		break;
	case GPG_CTX_MODE_VERIFY:
		if (!camel_session_is_online (gpg->session)) {
			/* this is a deprecated flag to gpg since 1.0.7 */
			/*g_ptr_array_add (argv, "--no-auto-key-retrieve");*/
			g_ptr_array_add (argv, "--keyserver-options");
			g_ptr_array_add (argv, "no-auto-key-retrieve");
		}
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
	case GPG_CTX_MODE_IMPORT:
		g_ptr_array_add (argv, "--import");
		g_ptr_array_add (argv, "-");
		break;
	case GPG_CTX_MODE_EXPORT:
		if (gpg->armor)
			g_ptr_array_add (argv, "--armor");
		g_ptr_array_add (argv, "--export");
		for (i = 0; i < gpg->recipients->len; i++)
			g_ptr_array_add (argv, gpg->recipients->pdata[i]);
		break;
	}
	
	g_ptr_array_add (argv, NULL);
	
	return argv;
}

static int
gpg_ctx_op_start (struct _GpgCtx *gpg)
{
	char *status_fd = NULL, *passwd_fd = NULL;
	int i, maxfd, errnosave, fds[10];
	GPtrArray *argv;
	int flags;
	
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
		/* Loop over all fds. */
		for (i = 3; i < maxfd; i++) {
			/* don't close the status-fd or passwd-fd */
			if (i != fds[7] && i != fds[8])
				fcntl (i, F_SETFD, FD_CLOEXEC);
		}
		
		/* run gpg */
		execvp ("gpg", (char **) argv->pdata);
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
	gpg->stdin_fd = fds[1];
	gpg->stdout_fd = fds[2];
	close (fds[3]);
	gpg->stderr_fd = fds[4];
	close (fds[5]);
	gpg->status_fd = fds[6];
	close (fds[7]);
	if (gpg->need_passwd) {
		close (fds[8]);
		gpg->passwd_fd = fds[9];
		flags = fcntl (gpg->passwd_fd, F_GETFL);
		fcntl (gpg->passwd_fd, F_SETFL, flags | O_NONBLOCK);
	}
	
	flags = fcntl (gpg->stdin_fd, F_GETFL);
	fcntl (gpg->stdin_fd, F_SETFL, flags | O_NONBLOCK);
	
	flags = fcntl (gpg->stdout_fd, F_GETFL);
	fcntl (gpg->stdout_fd, F_SETFL, flags | O_NONBLOCK);
	
	flags = fcntl (gpg->stderr_fd, F_GETFL);
	fcntl (gpg->stderr_fd, F_SETFL, flags | O_NONBLOCK);
	
	flags = fcntl (gpg->status_fd, F_GETFL);
	fcntl (gpg->status_fd, F_SETFL, flags | O_NONBLOCK);
	
	return 0;
	
 exception:
	
	errnosave = errno;
	
	for (i = 0; i < 10; i++) {
		if (fds[i] != -1)
			close (fds[i]);
	}
	
	errno = errnosave;
	
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
	size_t nread, nwritten;
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
	
	printf ("status: %s\n", status);
	
	if (strncmp (status, "[GNUPG:] ", 9) != 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Unexpected GnuPG status message encountered:\n\n%s"),
				      status);
		return -1;
	}
	
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
		
		if (gpg->utf8 || !(user = g_locale_to_utf8 (status, -1, &nread, &nwritten, NULL)))
			user = g_strdup (status);
		
		g_strstrip (user);
		
		g_hash_table_insert (gpg->userid_hint, hint, user);
	} else if (!strncmp (status, "NEED_PASSPHRASE ", 16)) {
		char *userid;
		
		status += 16;
		
		status = next_token (status, &userid);
		if (!userid) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Failed to parse gpg passphrase request."));
			return -1;
		}
		
		g_free (gpg->need_id);
		gpg->need_id = userid;
	} else if (!strncmp (status, "GET_HIDDEN passphrase.enter", 27)) {
		char *prompt, *passwd;
		const char *name;
		
		name = g_hash_table_lookup (gpg->userid_hint, gpg->need_id);
		if (!name)
			name = gpg->need_id;
		
		prompt = g_strdup_printf (_("You need a passphrase to unlock the key for\n"
					    "user: \"%s\""), name);
		
		if ((passwd = camel_session_get_password (gpg->session, NULL, NULL, prompt,  gpg->need_id, CAMEL_SESSION_PASSWORD_SECRET, ex)) && !gpg->utf8) {
			char *opasswd = passwd;
			
			if ((passwd = g_locale_to_utf8 (passwd, -1, &nread, &nwritten, NULL))) {
				memset (opasswd, 0, strlen (opasswd));
				g_free (opasswd);
			} else {
				passwd = opasswd;
			}
		}
		g_free (prompt);
		
		if (passwd == NULL) {
			if (!camel_exception_is_set (ex))
				camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL, _("Cancelled."));
			return -1;
		}
		
		gpg->passwd = g_strdup_printf ("%s\n", passwd);
		memset (passwd, 0, strlen (passwd));
		g_free (passwd);
		
		gpg->send_passwd = TRUE;
	} else if (!strncmp (status, "GOOD_PASSPHRASE", 15)) {
		gpg->bad_passwds = 0;
	} else if (!strncmp (status, "BAD_PASSPHRASE", 14)) {
		gpg->bad_passwds++;
		
		camel_session_forget_password (gpg->session, NULL, NULL, gpg->need_id, ex);
		
		if (gpg->bad_passwds == 3) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					     _("Failed to unlock secret key: 3 bad passphrases given."));
			return -1;
		}
	} else if (!strncmp (status, "UNEXPECTED ", 11)) {
		/* this is an error */
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Unexpected response from GnuPG: %s"),
				      status + 11);
		return -1;
	} else if (!strncmp (status, "NODATA", 6)) {
		/* this is an error */
		/* But we ignore it anyway, we should get other response codes to say why */
		gpg->nodata = TRUE;
	} else {
		/* check to see if we are complete */
		switch (gpg->mode) {
		case GPG_CTX_MODE_SIGN:
			if (!strncmp (status, "SIG_CREATED ", 12)) {
				/* FIXME: save this state? */
			}
			break;
		case GPG_CTX_MODE_VERIFY:
			if (!strncmp (status, "TRUST_", 6)) {
				status += 6;
				if (!strncmp (status, "NEVER", 5)) {
					gpg->trust = GPG_TRUST_NEVER;
				} else if (!strncmp (status, "MARGINAL", 8)) {
					gpg->trust = GPG_TRUST_MARGINAL;
				} else if (!strncmp (status, "FULLY", 5)) {
					gpg->trust = GPG_TRUST_FULLY;
				} else if (!strncmp (status, "ULTIMATE", 8)) {
					gpg->trust = GPG_TRUST_ULTIMATE;
				} else if (!strncmp (status, "UNDEFINED", 9)) {
					gpg->trust = GPG_TRUST_UNDEFINED;
				}
			} else if (!strncmp (status, "GOODSIG ", 8)) {
				gpg->goodsig = TRUE;
				gpg->hadsig = TRUE;
			} else if (!strncmp (status, "VALIDSIG ", 9)) {
				gpg->validsig = TRUE;
			} else if (!strncmp (status, "BADSIG ", 7)) {
				gpg->badsig = FALSE;
				gpg->hadsig = TRUE;
			} else if (!strncmp (status, "ERRSIG ", 7)) {
				/* Note: NO_PUBKEY often comes after an ERRSIG */
				gpg->errsig = FALSE;
				gpg->hadsig = TRUE;
			} else if (!strncmp (status, "NO_PUBKEY ", 10)) {
				gpg->nopubkey = TRUE;
			}
			break;
		case GPG_CTX_MODE_ENCRYPT:
			if (!strncmp (status, "BEGIN_ENCRYPTION", 16)) {
				/* nothing to do... but we know to expect data on stdout soon */
			} else if (!strncmp (status, "END_ENCRYPTION", 14)) {
				/* nothing to do, but we know the end is near? */
			} else if (!strncmp (status, "NO_RECP", 7)) {
				camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
						     _("Failed to encrypt: No valid recipients specified."));
				return -1;
			}
			break;
		case GPG_CTX_MODE_DECRYPT:
			if (!strncmp (status, "BEGIN_DECRYPTION", 16)) {
				/* nothing to do... but we know to expect data on stdout soon */
			} else if (!strncmp (status, "END_DECRYPTION", 14)) {
				/* nothing to do, but we know the end is near? */
			}
			break;
		case GPG_CTX_MODE_IMPORT:
			/* noop */
			break;
		case GPG_CTX_MODE_EXPORT:
			/* noop */
			break;
		}
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
	const char *diagnostics;
	struct timeval timeout;
	const char *mode;
	int maxfd = 0;
	int ready;
	
 retry:
	FD_ZERO (&rdset);
	
	if (!gpg->seen_eof1) {
		FD_SET (gpg->stdout_fd, &rdset);
		maxfd = MAX (maxfd, gpg->stdout_fd);
	}
	
	if (!gpg->seen_eof2) {
		FD_SET (gpg->stderr_fd, &rdset);
		maxfd = MAX (maxfd, gpg->stderr_fd);
	}
	
	if (!gpg->complete) {
		FD_SET (gpg->status_fd, &rdset);
		maxfd = MAX (maxfd, gpg->status_fd);
	}
	
	if (gpg->stdin_fd != -1 || gpg->passwd_fd != -1) {
		FD_ZERO (&wrset);
		if (gpg->stdin_fd != -1) {
			FD_SET (gpg->stdin_fd, &wrset);
			maxfd = MAX (maxfd, gpg->stdin_fd);
		}
		if (gpg->passwd_fd != -1) {
			FD_SET (gpg->passwd_fd, &wrset);
			maxfd = MAX (maxfd, gpg->passwd_fd);
		}
		
		wrsetp = &wrset;
	}
	
	g_assert (maxfd > 0);
	
	timeout.tv_sec = 10; /* timeout in seconds */
	timeout.tv_usec = 0;
	
	if ((ready = select (maxfd + 1, &rdset, wrsetp, NULL, &timeout)) == 0)
		return 0;
	
	if (ready < 0) {
		if (errno == EINTR)
			goto retry;
		
		d(printf ("select() failed: %s\n", strerror (errno)));
		
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
		
		d(printf ("reading from gpg's status-fd...\n"));
		
		do {
			nread = read (gpg->status_fd, buffer, sizeof (buffer));
		} while (nread == -1 && (errno == EINTR || errno == EAGAIN));
		if (nread == -1)
			goto exception;
		
		if (nread > 0) {
			status_backup (gpg, buffer, nread);
			
			if (gpg_ctx_parse_status (gpg, ex) == -1)
				return -1;
		} else {
			gpg->complete = TRUE;
		}
	}
	
	if (FD_ISSET (gpg->stdout_fd, &rdset) && gpg->ostream) {
		char buffer[4096];
		ssize_t nread;
		
		d(printf ("reading gpg's stdout...\n"));
		
		do {
			nread = read (gpg->stdout_fd, buffer, sizeof (buffer));
		} while (nread == -1 && (errno == EINTR || errno == EAGAIN));
		if (nread == -1)
			goto exception;
		
		if (nread > 0) {
			if (camel_stream_write (gpg->ostream, buffer, (size_t) nread) == -1)
				goto exception;
		} else {
			gpg->seen_eof1 = TRUE;
		}
	}
	
	if (FD_ISSET (gpg->stderr_fd, &rdset)) {
		char buffer[4096];
		ssize_t nread;
		
		d(printf ("reading gpg's stderr...\n"));
		
		do {
			nread = read (gpg->stderr_fd, buffer, sizeof (buffer));
		} while (nread == -1 && (errno == EINTR || errno == EAGAIN));
		if (nread == -1)
			goto exception;
		
		if (nread > 0) {
			camel_stream_write (gpg->diagnostics, buffer, nread);
		} else {
			gpg->seen_eof2 = TRUE;
		}
	}
	
	if (wrsetp && gpg->passwd_fd != -1 && FD_ISSET (gpg->passwd_fd, &wrset) && gpg->need_passwd && gpg->send_passwd) {
		ssize_t w, nwritten = 0;
		size_t n;
		
		d(printf ("sending gpg our passphrase...\n"));
		
		/* send the passphrase to gpg */
		n = strlen (gpg->passwd);
		do {
			do {
				w = write (gpg->passwd_fd, gpg->passwd + nwritten, n - nwritten);
			} while (w == -1 && (errno == EINTR || errno == EAGAIN));
			
			if (w > 0)
				nwritten += w;
		} while (nwritten < n && w != -1);
		
		/* zero and free our passwd buffer */
		memset (gpg->passwd, 0, n);
		g_free (gpg->passwd);
		gpg->passwd = NULL;
		
		if (w == -1)
			goto exception;
		
		gpg->send_passwd = FALSE;
	}
	
	if (gpg->istream && wrsetp && gpg->stdin_fd != -1 && FD_ISSET (gpg->stdin_fd, &wrset)) {
		char buffer[4096];
		ssize_t nread;
		
		d(printf ("writing to gpg's stdin...\n"));
		
		/* write our stream to gpg's stdin */
		nread = camel_stream_read (gpg->istream, buffer, sizeof (buffer));
		if (nread > 0) {
			ssize_t w, nwritten = 0;
			
			do {
				do {
					w = write (gpg->stdin_fd, buffer + nwritten, nread - nwritten);
				} while (w == -1 && (errno == EINTR || errno == EAGAIN));
				
				if (w > 0)
					nwritten += w;
			} while (nwritten < nread && w != -1);
			
			d(printf ("wrote %d (out of %d) bytes to gpg's stdin\n", nwritten, nread));
			
			if (w == -1)
				goto exception;
		}
		
		if (camel_stream_eos (gpg->istream)) {
			d(printf ("closing gpg's stdin\n"));
			close (gpg->stdin_fd);
			gpg->stdin_fd = -1;
		}
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
	case GPG_CTX_MODE_IMPORT:
		mode = "import keys";
		break;
	case GPG_CTX_MODE_EXPORT:
		mode = "export keys";
		break;
	default:
		g_assert_not_reached ();
		mode = NULL;
		break;
	}
	
	diagnostics = gpg_ctx_get_diagnostics (gpg);
	if (diagnostics && *diagnostics) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to GPG %s: %s\n\n%s"),
				      mode, g_strerror (errno),
				      diagnostics);
	} else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to GPG %s: %s\n"),
				      mode, g_strerror (errno));
	}
	
	return -1;
}

static gboolean
gpg_ctx_op_complete (struct _GpgCtx *gpg)
{
	return gpg->complete && gpg->seen_eof1 && gpg->seen_eof2;
}

#if 0
static gboolean
gpg_ctx_op_exited (struct _GpgCtx *gpg)
{
	pid_t retval;
	int status;
	
	if (gpg->exited)
		return TRUE;
	
	retval = waitpid (gpg->pid, &status, WNOHANG);
	if (retval == gpg->pid) {
		gpg->exit_status = status;
		gpg->exited = TRUE;
		return TRUE;
	}
	
	return FALSE;
}
#endif

static void
gpg_ctx_op_cancel (struct _GpgCtx *gpg)
{
	pid_t retval;
	int status;
	
	if (gpg->exited)
		return;
	
	kill (gpg->pid, SIGTERM);
	sleep (1);
	retval = waitpid (gpg->pid, &status, WNOHANG);
	if (retval == (pid_t) 0) {
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
	
	if (!gpg->exited) {
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
	} else {
		status = gpg->exit_status;
		retval = gpg->pid;
	}
	
	if (retval != (pid_t) -1 && WIFEXITED (status))
		return WEXITSTATUS (status);
	else
		return -1;
}



static int
gpg_sign (CamelCipherContext *context, const char *userid, CamelCipherHash hash, CamelMimePart *ipart, CamelMimePart *opart, CamelException *ex)
{
	struct _GpgCtx *gpg = NULL;
	CamelStream *ostream = camel_stream_mem_new(), *istream;
	CamelDataWrapper *dw;
	CamelContentType *ct;
	int res = -1;
	CamelMimePart *sigpart;
	CamelMultipartSigned *mps;
	
	/* Note: see rfc2015 or rfc3156, section 5 */
	
	/* FIXME: stream this, we stream output at least */
	istream = camel_stream_mem_new();
	if (camel_cipher_canonical_to_stream(ipart, CAMEL_MIME_FILTER_CANON_STRIP|CAMEL_MIME_FILTER_CANON_CRLF|CAMEL_MIME_FILTER_CANON_FROM,
					     istream) == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not generate signing data: %s"), g_strerror(errno));
		goto fail;
	}

	gpg = gpg_ctx_new (context->session);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_SIGN);
	gpg_ctx_set_hash (gpg, hash);
	gpg_ctx_set_armor (gpg, TRUE);
	gpg_ctx_set_userid (gpg, userid);
	gpg_ctx_set_istream (gpg, istream);
	gpg_ctx_set_ostream (gpg, ostream);
	
	if (gpg_ctx_op_start (gpg) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to execute gpg: %s"), g_strerror (errno));
		goto fail;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (camel_operation_cancel_check (NULL)) {
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Cancelled."));
			gpg_ctx_op_cancel (gpg);
			goto fail;
		}
		
		if (gpg_ctx_op_step (gpg, ex) == -1) {
			gpg_ctx_op_cancel (gpg);
			goto fail;
		}
	}
	
	if (gpg_ctx_op_wait (gpg) != 0) {
		const char *diagnostics;
		
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     diagnostics && *diagnostics ? diagnostics :
				     _("Failed to execute gpg."));
		goto fail;
	}

	res = 0;

	dw = camel_data_wrapper_new();
	camel_stream_reset(ostream);
	camel_data_wrapper_construct_from_stream(dw, ostream);

	sigpart = camel_mime_part_new();
	ct = camel_content_type_new("application", "pgp-signature");
	camel_content_type_set_param(ct, "name", "signature.asc");
	camel_data_wrapper_set_mime_type_field(dw, ct);
	camel_content_type_unref(ct);

	camel_medium_set_content_object((CamelMedium *)sigpart, dw);
	camel_object_unref(dw);

	camel_mime_part_set_description(sigpart, _("This is a digitally signed message part"));

	mps = camel_multipart_signed_new();
	ct = camel_content_type_new("multipart", "signed");
	camel_content_type_set_param(ct, "micalg", camel_cipher_hash_to_id(context, hash));
	camel_content_type_set_param(ct, "protocol", context->sign_protocol);
	camel_data_wrapper_set_mime_type_field((CamelDataWrapper *)mps, ct);
	camel_content_type_unref(ct);
	camel_multipart_set_boundary((CamelMultipart *)mps, NULL);

	mps->signature = sigpart;
	mps->contentraw = istream;
	camel_stream_reset(istream);
	camel_object_ref(istream);

	camel_medium_set_content_object((CamelMedium *)opart, (CamelDataWrapper *)mps);
fail:
	camel_object_unref(ostream);
	
	if (gpg)
		gpg_ctx_free (gpg);
	
	return res;
}


static char *
swrite (CamelMimePart *sigpart)
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

	/* TODO: This should probably just write the decoded message content out, not the part + headers */

	ostream = camel_stream_fs_new_with_fd (fd);
	ret = camel_data_wrapper_write_to_stream((CamelDataWrapper *)sigpart, ostream);
	if (ret != -1) {
		ret = camel_stream_flush (ostream);
		if (ret != -1)
			ret = camel_stream_close (ostream);
	}

	camel_object_unref(ostream);

	if (ret == -1) {
		unlink (template);
		g_free (template);
		return NULL;
	}
	
	return template;
}

static CamelCipherValidity *
gpg_verify (CamelCipherContext *context, CamelMimePart *ipart, CamelException *ex)
{
	CamelCipherValidity *validity;
	const char *diagnostics = NULL, *tmp;
	struct _GpgCtx *gpg = NULL;
	char *sigfile = NULL;
	CamelContentType *ct;
	CamelMimePart *sigpart;
	CamelStream *istream = NULL;
	CamelMultipart *mps;

	mps = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)ipart);
	ct = ((CamelDataWrapper *)mps)->mime_type;
	tmp = camel_content_type_param(ct, "protocol");
	if (!camel_content_type_is(ct, "multipart", "signed")
	    || !CAMEL_IS_MULTIPART_SIGNED(mps)
	    || tmp == NULL
	    || g_ascii_strcasecmp(tmp, context->sign_protocol) != 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot verify message signature: Incorrect message format"));
		return NULL;
	}

	if (!(istream = camel_multipart_signed_get_content_stream ((CamelMultipartSigned *) mps, NULL))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot verify message signature: Incorrect message format"));
		return NULL;
	}
	
	if (!(sigpart = camel_multipart_get_part (mps, CAMEL_MULTIPART_SIGNED_SIGNATURE))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot verify message signature: Incorrect message format"));
		camel_object_unref (istream);
		return NULL;
	}
	
	sigfile = swrite (sigpart);
	if (sigfile == NULL) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot verify message signature: could not create temp file: %s"),
				      g_strerror (errno));
		goto exception;
	}
	
	camel_stream_reset(istream);
	gpg = gpg_ctx_new (context->session);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_VERIFY);
	gpg_ctx_set_hash (gpg, camel_cipher_id_to_hash(context, camel_content_type_param(ct, "micalg")));
	gpg_ctx_set_sigfile (gpg, sigfile);
	gpg_ctx_set_istream (gpg, istream);
	
	if (gpg_ctx_op_start (gpg) == -1) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Failed to execute gpg."));
		goto exception;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (camel_operation_cancel_check (NULL)) {
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Cancelled."));
			gpg_ctx_op_cancel (gpg);
			goto exception;
		}
		
		if (gpg_ctx_op_step (gpg, ex) == -1) {
			gpg_ctx_op_cancel (gpg);
			goto exception;
		}
	}
	
	gpg_ctx_op_wait (gpg);
	validity = camel_cipher_validity_new ();
	diagnostics = gpg_ctx_get_diagnostics (gpg);
	camel_cipher_validity_set_description (validity, diagnostics);
	if (gpg->validsig) {
		if (gpg->trust == GPG_TRUST_UNDEFINED || gpg->trust == GPG_TRUST_NONE)
			validity->sign.status = CAMEL_CIPHER_VALIDITY_SIGN_UNKNOWN;
		else if (gpg->trust != GPG_TRUST_NEVER)
			validity->sign.status = CAMEL_CIPHER_VALIDITY_SIGN_GOOD;
		else
			validity->sign.status = CAMEL_CIPHER_VALIDITY_SIGN_BAD;
	} else {
		validity->sign.status = CAMEL_CIPHER_VALIDITY_SIGN_BAD;
	}
	
	gpg_ctx_free (gpg);
	
	if (sigfile) {
		unlink (sigfile);
		g_free (sigfile);
	}
	
	return validity;
	
 exception:
	
	if (gpg != NULL)
		gpg_ctx_free (gpg);
	
	if (istream)
		camel_object_unref(istream);

	if (sigfile) {
		unlink (sigfile);
		g_free (sigfile);
	}
	
	return NULL;
}

static int
gpg_encrypt (CamelCipherContext *context, const char *userid, GPtrArray *recipients, struct _CamelMimePart *ipart, struct _CamelMimePart *opart, CamelException *ex)
{
	CamelGpgContext *ctx = (CamelGpgContext *) context;
	struct _GpgCtx *gpg;
	int i, res = -1;
	CamelStream *istream, *ostream, *vstream;
	CamelMimePart *encpart, *verpart;
	CamelDataWrapper *dw;
	CamelContentType *ct;
	CamelMultipartEncrypted *mpe;

	ostream = camel_stream_mem_new();
	istream = camel_stream_mem_new();
	if (camel_cipher_canonical_to_stream(ipart, CAMEL_MIME_FILTER_CANON_CRLF, istream) == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not generate encrypting data: %s"), g_strerror(errno));
		goto fail1;
	}
	
	gpg = gpg_ctx_new (context->session);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_ENCRYPT);
	gpg_ctx_set_armor (gpg, TRUE);
	gpg_ctx_set_userid (gpg, userid);
	gpg_ctx_set_istream (gpg, istream);
	gpg_ctx_set_ostream (gpg, ostream);
	gpg_ctx_set_always_trust (gpg, ctx->always_trust);

	for (i = 0; i < recipients->len; i++) {
		gpg_ctx_add_recipient (gpg, recipients->pdata[i]);
	}
	
	if (gpg_ctx_op_start (gpg) == -1) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("Failed to execute gpg."));
		goto fail;
	}

	/* FIXME: move tihs to a common routine */
	while (!gpg_ctx_op_complete(gpg)) {
		if (camel_operation_cancel_check(NULL)) {
			camel_exception_set(ex, CAMEL_EXCEPTION_USER_CANCEL, _("Cancelled."));
			gpg_ctx_op_cancel(gpg);
			goto fail;
		}
		
		if (gpg_ctx_op_step (gpg, ex) == -1) {
			gpg_ctx_op_cancel (gpg);
			goto fail;
		}
	}
	
	if (gpg_ctx_op_wait (gpg) != 0) {
		const char *diagnostics;
		
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     diagnostics && *diagnostics ? diagnostics : _("Failed to execute gpg."));
		goto fail;
	}

	res = 0;

	dw = camel_data_wrapper_new();
	camel_data_wrapper_construct_from_stream(dw, ostream);

	encpart = camel_mime_part_new();
	ct = camel_content_type_new("application", "octet-stream");
	camel_content_type_set_param(ct, "name", "encrypted.asc");
	camel_data_wrapper_set_mime_type_field(dw, ct);
	camel_content_type_unref(ct);

	camel_medium_set_content_object((CamelMedium *)encpart, dw);
	camel_object_unref(dw);

	camel_mime_part_set_description(encpart, _("This is a digitally encrypted message part"));

	vstream = camel_stream_mem_new();
	camel_stream_write(vstream, "Version: 1\n", strlen("Version: 1\n"));
	camel_stream_reset(vstream);

	verpart = camel_mime_part_new();
	dw = camel_data_wrapper_new();
	camel_data_wrapper_set_mime_type(dw, context->encrypt_protocol);
	camel_data_wrapper_construct_from_stream(dw, vstream);
	camel_object_unref(vstream);
	camel_medium_set_content_object((CamelMedium *)verpart, dw);
	camel_object_unref(dw);

	mpe = camel_multipart_encrypted_new();
	ct = camel_content_type_new("multipart", "encrypted");
	camel_content_type_set_param(ct, "protocol", context->encrypt_protocol);
	camel_data_wrapper_set_mime_type_field((CamelDataWrapper *)mpe, ct);
	camel_content_type_unref(ct);
	camel_multipart_set_boundary((CamelMultipart *)mpe, NULL);

	mpe->decrypted = ipart;
	camel_object_ref(ipart);

	camel_multipart_add_part((CamelMultipart *)mpe, verpart);
	camel_object_unref(verpart);
	camel_multipart_add_part((CamelMultipart *)mpe, encpart);
	camel_object_unref(encpart);

	camel_medium_set_content_object((CamelMedium *)opart, (CamelDataWrapper *)mpe);
fail:
	gpg_ctx_free(gpg);
fail1:
	camel_object_unref(istream);
	camel_object_unref(ostream);
		
	return res;
}

static CamelCipherValidity *
gpg_decrypt(CamelCipherContext *context, CamelMimePart *ipart, CamelMimePart *opart, CamelException *ex)
{
	struct _GpgCtx *gpg;
	CamelCipherValidity *valid = NULL;
	CamelStream *ostream, *istream;

	istream = camel_stream_mem_new();
	camel_data_wrapper_write_to_stream(camel_medium_get_content_object((CamelMedium *)ipart), istream);
	camel_stream_reset(istream);

	/* TODO: de-canonicalise end of lines? */
	/*stream = camel_stream_mem_new ();
	filtered_stream = (CamelStream *) camel_stream_filter_new_with_stream (stream);
	crlf_filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_DECODE,
						  CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
	camel_stream_filter_add (CAMEL_STREAM_FILTER (filtered_stream), crlf_filter);
	camel_object_unref (crlf_filter);*/
	ostream = camel_stream_mem_new();

	gpg = gpg_ctx_new (context->session);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_DECRYPT);
	gpg_ctx_set_istream (gpg, istream);
	gpg_ctx_set_ostream (gpg, ostream);
	
	if (gpg_ctx_op_start (gpg) == -1) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Failed to execute gpg."));
		goto fail;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (camel_operation_cancel_check (NULL)) {
			camel_exception_set (ex, CAMEL_EXCEPTION_USER_CANCEL,
					     _("Cancelled."));
			gpg_ctx_op_cancel (gpg);
			goto fail;
		}
		
		if (gpg_ctx_op_step (gpg, ex) == -1) {
			gpg_ctx_op_cancel (gpg);
			goto fail;
		}
	}
	
	if (gpg_ctx_op_wait (gpg) != 0) {
		const char *diagnostics;
		
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     diagnostics && *diagnostics ? diagnostics :
				     _("Failed to execute gpg."));
		goto fail;
	}

	camel_stream_reset(ostream);
	if (camel_data_wrapper_construct_from_stream((CamelDataWrapper *)opart, ostream) != -1) {
		valid = camel_cipher_validity_new();
		valid->encrypt.description = g_strdup(_("Encrypted content"));
		valid->encrypt.status = CAMEL_CIPHER_VALIDITY_ENCRYPT_ENCRYPTED;
		
		if (gpg->hadsig) {
			if (gpg->validsig) {
				if (gpg->trust == GPG_TRUST_UNDEFINED || gpg->trust == GPG_TRUST_NONE)
					valid->sign.status = CAMEL_CIPHER_VALIDITY_SIGN_UNKNOWN;
				else if (gpg->trust != GPG_TRUST_NEVER)
					valid->sign.status = CAMEL_CIPHER_VALIDITY_SIGN_GOOD;
				else
					valid->sign.status = CAMEL_CIPHER_VALIDITY_SIGN_BAD;
			} else if (gpg->nopubkey) {
				valid->sign.status = CAMEL_CIPHER_VALIDITY_SIGN_UNKNOWN;
			} else {
				valid->sign.status = CAMEL_CIPHER_VALIDITY_SIGN_BAD;
			}
		}
	} else {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Unable to parse message content"));
	}
	
 fail:
	camel_object_unref(ostream);
	camel_object_unref(istream);
	gpg_ctx_free (gpg);

	return valid;
}

static int
gpg_import_keys (CamelCipherContext *context, CamelStream *istream, CamelException *ex)
{
	struct _GpgCtx *gpg;
	
	gpg = gpg_ctx_new (context->session);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_IMPORT);
	gpg_ctx_set_istream (gpg, istream);
	
	if (gpg_ctx_op_start (gpg) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to execute gpg: %s"),
				      errno ? g_strerror (errno) : _("Unknown"));
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (gpg_ctx_op_step (gpg, ex) == -1) {
			gpg_ctx_op_cancel (gpg);
			gpg_ctx_free (gpg);
			
			return -1;
		}
	}
	
	if (gpg_ctx_op_wait (gpg) != 0) {
		const char *diagnostics;
		
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     diagnostics && *diagnostics ? diagnostics :
				     _("Failed to execute gpg."));
		
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	gpg_ctx_free (gpg);
	
	return 0;
}

static int
gpg_export_keys (CamelCipherContext *context, GPtrArray *keys, CamelStream *ostream, CamelException *ex)
{
	struct _GpgCtx *gpg;
	int i;
	
	gpg = gpg_ctx_new (context->session);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_EXPORT);
	gpg_ctx_set_armor (gpg, TRUE);
	gpg_ctx_set_ostream (gpg, ostream);
	
	for (i = 0; i < keys->len; i++) {
		gpg_ctx_add_recipient (gpg, keys->pdata[i]);
	}
	
	if (gpg_ctx_op_start (gpg) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to execute gpg: %s"),
				      errno ? g_strerror (errno) : _("Unknown"));
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (gpg_ctx_op_step (gpg, ex) == -1) {
			gpg_ctx_op_cancel (gpg);
			gpg_ctx_free (gpg);
			
			return -1;
		}
	}
	
	if (gpg_ctx_op_wait (gpg) != 0) {
		const char *diagnostics;
		
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     diagnostics && *diagnostics ? diagnostics :
				     _("Failed to execute gpg."));
		
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	gpg_ctx_free (gpg);
	
	return 0;
}

/* ********************************************************************** */

static void
camel_gpg_context_class_init (CamelGpgContextClass *klass)
{
	CamelCipherContextClass *cipher_class = CAMEL_CIPHER_CONTEXT_CLASS (klass);
	
	parent_class = CAMEL_CIPHER_CONTEXT_CLASS (camel_type_get_global_classfuncs (camel_cipher_context_get_type ()));
	
	cipher_class->hash_to_id = gpg_hash_to_id;
	cipher_class->id_to_hash = gpg_id_to_hash;
	cipher_class->sign = gpg_sign;
	cipher_class->verify = gpg_verify;
	cipher_class->encrypt = gpg_encrypt;
	cipher_class->decrypt = gpg_decrypt;
	cipher_class->import_keys = gpg_import_keys;
	cipher_class->export_keys = gpg_export_keys;
}

static void
camel_gpg_context_init (CamelGpgContext *context)
{
	CamelCipherContext *cipher = (CamelCipherContext *) context;
	
	context->always_trust = FALSE;
	
	cipher->sign_protocol = "application/pgp-signature";
	cipher->encrypt_protocol = "application/pgp-encrypted";
	cipher->key_protocol = "application/pgp-keys";
}

static void
camel_gpg_context_finalise (CamelObject *object)
{
	;
}

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


