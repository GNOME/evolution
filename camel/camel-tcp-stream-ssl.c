/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

/* NOTE: This is the default implementation of CamelTcpStreamSSL,
 * used when the Mozilla NSS libraries are used. If you configured
 * OpenSSL support instead, then this file won't be compiled and
 * the CamelTcpStreamSSL implementation in camel-tcp-stream-openssl.c
 * will be used instead.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_NSS
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <nspr.h>
#include <prio.h>
#include <prerror.h>
#include <prerr.h>
#include <secerr.h>
#include <sslerr.h>
#include "nss.h"    /* Don't use <> here or it will include the system nss.h instead */
#include <ssl.h>
#include <cert.h>
#include <certdb.h>
#include <pk11func.h>

/* this is commented because otherwise we get an error about the
   redefinition of MD5Context...yay */
/*#include <e-util/md5-utils.h>*/

#include "camel-tcp-stream-ssl.h"
#include "camel-stream-fs.h"
#include "camel-session.h"
#include "camel-certdb.h"
#include "camel-operation.h"

/* from md5-utils.h */
void md5_get_digest (const char *buffer, int buffer_size, unsigned char digest[16]);

#define IO_TIMEOUT (PR_TicksPerSecond() * 4 * 60)
#define CONNECT_TIMEOUT (PR_TicksPerSecond () * 4 * 60)

static CamelTcpStreamClass *parent_class = NULL;

/* Returns the class for a CamelTcpStreamSSL */
#define CTSS_CLASS(so) CAMEL_TCP_STREAM_SSL_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static ssize_t stream_read (CamelStream *stream, char *buffer, size_t n);
static ssize_t stream_write (CamelStream *stream, const char *buffer, size_t n);
static int stream_flush  (CamelStream *stream);
static int stream_close  (CamelStream *stream);

static PRFileDesc *enable_ssl (CamelTcpStreamSSL *ssl, PRFileDesc *fd);

static int stream_connect    (CamelTcpStream *stream, struct hostent *host, int port);
static int stream_getsockopt (CamelTcpStream *stream, CamelSockOptData *data);
static int stream_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data);
static CamelTcpAddress *stream_get_local_address (CamelTcpStream *stream);
static CamelTcpAddress *stream_get_remote_address (CamelTcpStream *stream);

struct _CamelTcpStreamSSLPrivate {
	PRFileDesc *sockfd;
	
	struct _CamelSession *session;
	char *expected_host;
	gboolean ssl_mode;
	guint32 flags;
};

static void
camel_tcp_stream_ssl_class_init (CamelTcpStreamSSLClass *camel_tcp_stream_ssl_class)
{
	CamelTcpStreamClass *camel_tcp_stream_class =
		CAMEL_TCP_STREAM_CLASS (camel_tcp_stream_ssl_class);
	CamelStreamClass *camel_stream_class =
		CAMEL_STREAM_CLASS (camel_tcp_stream_ssl_class);
	
	parent_class = CAMEL_TCP_STREAM_CLASS (camel_type_get_global_classfuncs (camel_tcp_stream_get_type ()));
	
	/* virtual method overload */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->close = stream_close;
	
	camel_tcp_stream_class->connect = stream_connect;
	camel_tcp_stream_class->getsockopt = stream_getsockopt;
	camel_tcp_stream_class->setsockopt = stream_setsockopt;
	camel_tcp_stream_class->get_local_address  = stream_get_local_address;
	camel_tcp_stream_class->get_remote_address = stream_get_remote_address;
}

static void
camel_tcp_stream_ssl_init (gpointer object, gpointer klass)
{
	CamelTcpStreamSSL *stream = CAMEL_TCP_STREAM_SSL (object);
	
	stream->priv = g_new0 (struct _CamelTcpStreamSSLPrivate, 1);
}

static void
camel_tcp_stream_ssl_finalize (CamelObject *object)
{
	CamelTcpStreamSSL *stream = CAMEL_TCP_STREAM_SSL (object);
	
	if (stream->priv->sockfd != NULL)
		PR_Close (stream->priv->sockfd);

	if (stream->priv->session)
		camel_object_unref(stream->priv->session);

	g_free (stream->priv->expected_host);
	
	g_free (stream->priv);
}


CamelType
camel_tcp_stream_ssl_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_tcp_stream_get_type (),
					    "CamelTcpStreamSSL",
					    sizeof (CamelTcpStreamSSL),
					    sizeof (CamelTcpStreamSSLClass),
					    (CamelObjectClassInitFunc) camel_tcp_stream_ssl_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_tcp_stream_ssl_init,
					    (CamelObjectFinalizeFunc) camel_tcp_stream_ssl_finalize);
	}
	
	return type;
}


/**
 * camel_tcp_stream_ssl_new:
 * @session: active session
 * @expected_host: host that the stream is expected to connect with.
 * @flags: ENABLE_SSL2, ENABLE_SSL3 and/or ENABLE_TLS
 *
 * Since the SSL certificate authenticator may need to prompt the
 * user, a CamelSession is needed. @expected_host is needed as a
 * protection against an MITM attack.
 *
 * Return value: a ssl stream (in ssl mode)
 **/
CamelStream *
camel_tcp_stream_ssl_new (CamelSession *session, const char *expected_host, guint32 flags)
{
	CamelTcpStreamSSL *stream;

	g_assert(CAMEL_IS_SESSION(session));

	stream = CAMEL_TCP_STREAM_SSL (camel_object_new (camel_tcp_stream_ssl_get_type ()));
	
	stream->priv->session = session;
	camel_object_ref(session);
	stream->priv->expected_host = g_strdup (expected_host);
	stream->priv->ssl_mode = TRUE;
	stream->priv->flags = flags;
	
	return CAMEL_STREAM (stream);
}


/**
 * camel_tcp_stream_ssl_new_raw:
 * @session: active session
 * @expected_host: host that the stream is expected to connect with.
 * @flags: ENABLE_SSL2, ENABLE_SSL3 and/or ENABLE_TLS
 *
 * Since the SSL certificate authenticator may need to prompt the
 * user, a CamelSession is needed. @expected_host is needed as a
 * protection against an MITM attack.
 *
 * Return value: a ssl-capable stream (in non ssl mode)
 **/
CamelStream *
camel_tcp_stream_ssl_new_raw (CamelSession *session, const char *expected_host, guint32 flags)
{
	CamelTcpStreamSSL *stream;

	g_assert(CAMEL_IS_SESSION(session));
	
	stream = CAMEL_TCP_STREAM_SSL (camel_object_new (camel_tcp_stream_ssl_get_type ()));
	
	stream->priv->session = session;
	camel_object_ref(session);
	stream->priv->expected_host = g_strdup (expected_host);
	stream->priv->ssl_mode = FALSE;
	stream->priv->flags = flags;
	
	return CAMEL_STREAM (stream);
}


static void
set_errno (int code)
{
	/* FIXME: this should handle more. */
	switch (code) {
	case PR_INVALID_ARGUMENT_ERROR:
		errno = EINVAL;
		break;
	case PR_PENDING_INTERRUPT_ERROR:
		errno = EINTR;
		break;
	case PR_IO_PENDING_ERROR:
		errno = EAGAIN;
		break;
	case PR_WOULD_BLOCK_ERROR:
		errno = EWOULDBLOCK;
		break;
	case PR_IN_PROGRESS_ERROR:
		errno = EINPROGRESS;
		break;
	case PR_ALREADY_INITIATED_ERROR:
		errno = EALREADY;
		break;
	case PR_NETWORK_UNREACHABLE_ERROR:
		errno = EHOSTUNREACH;
		break;
	case PR_CONNECT_REFUSED_ERROR:
		errno = ECONNREFUSED;
		break;
	case PR_CONNECT_TIMEOUT_ERROR:
	case PR_IO_TIMEOUT_ERROR:
		errno = ETIMEDOUT;
		break;
	case PR_NOT_CONNECTED_ERROR:
		errno = ENOTCONN;
		break;
	case PR_CONNECT_RESET_ERROR:
		errno = ECONNRESET;
		break;
	case PR_IO_ERROR:
	default:
		errno = EIO;
		break;
	}
}


/**
 * camel_tcp_stream_ssl_enable_ssl:
 * @ssl: ssl stream
 *
 * Toggles an ssl-capable stream into ssl mode (if it isn't already).
 *
 * Returns 0 on success or -1 on fail.
 **/
int
camel_tcp_stream_ssl_enable_ssl (CamelTcpStreamSSL *ssl)
{
	PRFileDesc *fd;
	
	g_return_val_if_fail (CAMEL_IS_TCP_STREAM_SSL (ssl), -1);
	
	if (ssl->priv->sockfd && !ssl->priv->ssl_mode) {
		if (!(fd = enable_ssl (ssl, NULL))) {
			set_errno (PR_GetError ());
			return -1;
		}
		
		ssl->priv->sockfd = fd;
		
		if (SSL_ResetHandshake (fd, FALSE) == SECFailure) {
			set_errno (PR_GetError ());
			return -1;
		}
		
		if (SSL_ForceHandshake (fd) == -1) {
			set_errno (PR_GetError ());
			return -1;
		}
	}
	
	ssl->priv->ssl_mode = TRUE;
	
	return 0;
}


static ssize_t
stream_read (CamelStream *stream, char *buffer, size_t n)
{
	CamelTcpStreamSSL *tcp_stream_ssl = CAMEL_TCP_STREAM_SSL (stream);
	PRFileDesc *cancel_fd;
	ssize_t nread;
	
	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		return -1;
	}
	
	cancel_fd = camel_operation_cancel_prfd (NULL);
	if (cancel_fd == NULL) {
		do {
			nread = PR_Read (tcp_stream_ssl->priv->sockfd, buffer, n);
			if (nread == -1)
				set_errno (PR_GetError ());
		} while (nread == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
	} else {
		PRSocketOptionData sockopts;
		PRPollDesc pollfds[2];
		gboolean nonblock;
		int error;
		
		/* get O_NONBLOCK options */
		sockopts.option = PR_SockOpt_Nonblocking;
		PR_GetSocketOption (tcp_stream_ssl->priv->sockfd, &sockopts);
		sockopts.option = PR_SockOpt_Nonblocking;
		nonblock = sockopts.value.non_blocking;
		sockopts.value.non_blocking = TRUE;
		PR_SetSocketOption (tcp_stream_ssl->priv->sockfd, &sockopts);

		pollfds[0].fd = tcp_stream_ssl->priv->sockfd;
		pollfds[0].in_flags = PR_POLL_READ;
		pollfds[1].fd = cancel_fd;
		pollfds[1].in_flags = PR_POLL_READ;
		
		do {
			PRInt32 res;

			pollfds[0].out_flags = 0;
			pollfds[1].out_flags = 0;
			nread = -1;

			res = PR_Poll(pollfds, 2, IO_TIMEOUT);
			if (res == -1)
				set_errno(PR_GetError());
			else if (res == 0)
				errno = ETIMEDOUT;
			else if (pollfds[1].out_flags == PR_POLL_READ) {
				errno = EINTR;
				goto failed;
			} else {
				do {
					nread = PR_Read (tcp_stream_ssl->priv->sockfd, buffer, n);
					if (nread == -1)
						set_errno (PR_GetError ());
				} while (nread == -1 && errno == EINTR);
			}
		} while (nread == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
		
		/* restore O_NONBLOCK options */
	failed:
		error = errno;
		sockopts.option = PR_SockOpt_Nonblocking;
		sockopts.value.non_blocking = nonblock;
		PR_SetSocketOption (tcp_stream_ssl->priv->sockfd, &sockopts);
		errno = error;
	}
	
	return nread;
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	CamelTcpStreamSSL *tcp_stream_ssl = CAMEL_TCP_STREAM_SSL (stream);
	ssize_t w, written = 0;
	PRFileDesc *cancel_fd;
	
	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		return -1;
	}
	
	cancel_fd = camel_operation_cancel_prfd (NULL);
	if (cancel_fd == NULL) {
		do {
			do {
				w = PR_Write (tcp_stream_ssl->priv->sockfd, buffer + written, n - written);
				if (w == -1)
					set_errno (PR_GetError ());
			} while (w == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
			
			if (w > 0)
				written += w;
		} while (w != -1 && written < n);
	} else {
		PRSocketOptionData sockopts;
		PRPollDesc pollfds[2];
		gboolean nonblock;
		int error;
		
		/* get O_NONBLOCK options */
		sockopts.option = PR_SockOpt_Nonblocking;
		PR_GetSocketOption (tcp_stream_ssl->priv->sockfd, &sockopts);
		sockopts.option = PR_SockOpt_Nonblocking;
		nonblock = sockopts.value.non_blocking;
		sockopts.value.non_blocking = TRUE;
		PR_SetSocketOption (tcp_stream_ssl->priv->sockfd, &sockopts);
		
		pollfds[0].fd = tcp_stream_ssl->priv->sockfd;
		pollfds[0].in_flags = PR_POLL_WRITE;
		pollfds[1].fd = cancel_fd;
		pollfds[1].in_flags = PR_POLL_READ;
		
		do {
			PRInt32 res;

			pollfds[0].out_flags = 0;
			pollfds[1].out_flags = 0;
			w = -1;

			res = PR_Poll (pollfds, 2, IO_TIMEOUT);
			if (res == -1) {
				set_errno(PR_GetError());
				if (errno == EINTR)
					w = 0;
			} else if (res == 0)
				errno = ETIMEDOUT;
			else if (pollfds[1].out_flags == PR_POLL_READ) {
				errno = EINTR;
			} else {
				do {
					w = PR_Write (tcp_stream_ssl->priv->sockfd, buffer + written, n - written);
					if (w == -1)
						set_errno (PR_GetError ());
				} while (w == -1 && errno == EINTR);
				
				if (w == -1) {
					if (errno == EAGAIN || errno == EWOULDBLOCK)
						w = 0;
				} else
					written += w;
			}
		} while (w != -1 && written < n);
		
		/* restore O_NONBLOCK options */
		error = errno;
		sockopts.option = PR_SockOpt_Nonblocking;
		sockopts.value.non_blocking = nonblock;
		PR_SetSocketOption (tcp_stream_ssl->priv->sockfd, &sockopts);
		errno = error;
	}
	
	if (w == -1)
		return -1;
	
	return written;
}

static int
stream_flush (CamelStream *stream)
{
	/*return PR_Sync (((CamelTcpStreamSSL *)stream)->priv->sockfd);*/
	return 0;
}

static int
stream_close (CamelStream *stream)
{
	if (PR_Close (((CamelTcpStreamSSL *)stream)->priv->sockfd) == PR_FAILURE)
		return -1;
	
	((CamelTcpStreamSSL *)stream)->priv->sockfd = NULL;
	
	return 0;
}

#if 0
/* Since this is default implementation, let NSS handle it. */
static SECStatus
ssl_get_client_auth (void *data, PRFileDesc *sockfd,
		     struct CERTDistNamesStr *caNames,
		     struct CERTCertificateStr **pRetCert,
		     struct SECKEYPrivateKeyStr **pRetKey) 
{
	SECStatus status = SECFailure;
	SECKEYPrivateKey *privkey;
	CERTCertificate *cert;
	void *proto_win;
	
	proto_win = SSL_RevealPinArg (sockfd);
	
	if ((char *) data) {
		cert = PK11_FindCertFromNickname ((char *) data, proto_win);
		if (cert) {
			privKey = PK11_FindKeyByAnyCert (cert, proto_win);
			if (privkey) {
				status = SECSuccess;
			} else {
				CERT_DestroyCertificate (cert);
			}
		}
	} else {
		/* no nickname given, automatically find the right cert */
		CERTCertNicknames *names;
		int i;
		
		names = CERT_GetCertNicknames (CERT_GetDefaultCertDB (), 
					       SEC_CERT_NICKNAMES_USER,
					       proto_win);
		
		if (names != NULL) {
			for (i = 0; i < names->numnicknames; i++) {
				cert = PK11_FindCertFromNickname (names->nicknames[i], 
								  proto_win);
				if (!cert)
					continue;
				
				/* Only check unexpired certs */
				if (CERT_CheckCertValidTimes (cert, PR_Now (), PR_FALSE) != secCertTimeValid) {
					CERT_DestroyCertificate (cert);
					continue;
				}
				
				status = NSS_CmpCertChainWCANames (cert, caNames);
				if (status == SECSuccess) {
					privkey = PK11_FindKeyByAnyCert (cert, proto_win);
					if (privkey)
						break;
					
					status = SECFailure;
					break;
				}
				
				CERT_FreeNicknames (names);
			}
		}
	}
	
	if (status == SECSuccess) {
		*pRetCert = cert;
		*pRetKey  = privkey;
	}
	
	return status;
}
#endif

#if 0
/* Since this is the default NSS implementation, no need for us to use this. */
static SECStatus
ssl_auth_cert (void *data, PRFileDesc *sockfd, PRBool checksig, PRBool is_server)
{
	CERTCertificate *cert;
	SECStatus status;
	void *pinarg;
	char *host;
	
	cert = SSL_PeerCertificate (sockfd);
	pinarg = SSL_RevealPinArg (sockfd);
	status = CERT_VerifyCertNow ((CERTCertDBHandle *)data, cert,
				     checksig, certUsageSSLClient, pinarg);
	
	if (status != SECSuccess)
		return SECFailure;
	
	/* Certificate is OK.  Since this is the client side of an SSL
	 * connection, we need to verify that the name field in the cert
	 * matches the desired hostname.  This is our defense against
	 * man-in-the-middle attacks.
	 */
	
	/* SSL_RevealURL returns a hostname, not a URL. */
	host = SSL_RevealURL (sockfd);
	
	if (host && *host) {
		status = CERT_VerifyCertName (cert, host);
	} else {
		PR_SetError (SSL_ERROR_BAD_CERT_DOMAIN, 0);
		status = SECFailure;
	}
	
	if (host)
		PR_Free (host);
	
	return secStatus;
}
#endif

CamelCert *camel_certdb_nss_cert_get(CamelCertDB *certdb, CERTCertificate *cert);
CamelCert *camel_certdb_nss_cert_add(CamelCertDB *certdb, CERTCertificate *cert);
void camel_certdb_nss_cert_set(CamelCertDB *certdb, CamelCert *ccert, CERTCertificate *cert);

static char *
cert_fingerprint(CERTCertificate *cert)
{
	unsigned char md5sum[16], fingerprint[50], *f;
	int i;
	const char tohex[16] = "0123456789abcdef";

	md5_get_digest (cert->derCert.data, cert->derCert.len, md5sum);
	for (i=0,f = fingerprint; i<16; i++) {
		unsigned int c = md5sum[i];

		*f++ = tohex[(c >> 4) & 0xf];
		*f++ = tohex[c & 0xf];
		*f++ = ':';
	}

	fingerprint[47] = 0;

	return g_strdup(fingerprint);
}

/* lookup a cert uses fingerprint to index an on-disk file */
CamelCert *
camel_certdb_nss_cert_get(CamelCertDB *certdb, CERTCertificate *cert)
{
	char *fingerprint, *path;
	CamelCert *ccert;
	struct stat st;
	size_t nread;
	ssize_t n;
	int fd;
	
	fingerprint = cert_fingerprint (cert);
	ccert = camel_certdb_get_cert (certdb, fingerprint);
	if (ccert == NULL) {
		g_free (fingerprint);
		return ccert;
	}
	
	if (ccert->rawcert == NULL) {
		path = g_strdup_printf ("%s/.camel_certs/%s", getenv ("HOME"), fingerprint);
		if (stat (path, &st) == -1
		    || (fd = open (path, O_RDONLY)) == -1) {
			g_warning ("could not load cert %s: %s", path, strerror (errno));
			g_free (fingerprint);
			g_free (path);
			camel_cert_set_trust (certdb, ccert, CAMEL_CERT_TRUST_UNKNOWN);
			camel_certdb_touch (certdb);
			
			return ccert;
		}
		g_free(path);
		
		ccert->rawcert = g_byte_array_new ();
		g_byte_array_set_size (ccert->rawcert, st.st_size);
		
		nread = 0;
		do {
			do {
				n = read (fd, ccert->rawcert->data + nread, st.st_size - nread);
			} while (n == -1 && errno == EINTR);
			
			if (n > 0)
				nread += n;
		} while (nread < st.st_size && n != -1);
		
		close (fd);
		
		if (nread != st.st_size) {
			g_warning ("cert size read truncated %s: %d != %ld", path, nread, st.st_size);
			g_byte_array_free(ccert->rawcert, TRUE);
			ccert->rawcert = NULL;
			g_free(fingerprint);
			camel_cert_set_trust(certdb, ccert, CAMEL_CERT_TRUST_UNKNOWN);
			camel_certdb_touch(certdb);

			return ccert;
		}
	}

	g_free(fingerprint);
	if (ccert->rawcert->len != cert->derCert.len
	    || memcmp(ccert->rawcert->data, cert->derCert.data, cert->derCert.len) != 0) {
		g_warning("rawcert != derCer");
		camel_cert_set_trust(certdb, ccert, CAMEL_CERT_TRUST_UNKNOWN);
		camel_certdb_touch(certdb);
	}

	return ccert;
}

/* add a cert to the certdb */
CamelCert *
camel_certdb_nss_cert_add(CamelCertDB *certdb, CERTCertificate *cert)
{
	CamelCert *ccert;
	char *fingerprint;

	fingerprint = cert_fingerprint(cert);

	ccert = camel_certdb_cert_new(certdb);
	camel_cert_set_issuer(certdb, ccert, CERT_NameToAscii(&cert->issuer));
	camel_cert_set_subject(certdb, ccert, CERT_NameToAscii(&cert->subject));
	/* hostname is set in caller */
	/*camel_cert_set_hostname(certdb, ccert, ssl->priv->expected_host);*/
	camel_cert_set_fingerprint(certdb, ccert, fingerprint);
	camel_cert_set_trust(certdb, ccert, CAMEL_CERT_TRUST_UNKNOWN);
	g_free(fingerprint);

	camel_certdb_nss_cert_set(certdb, ccert, cert);

	camel_certdb_add(certdb, ccert);

	return ccert;
}

/* set the 'raw' cert (& save it) */
void
camel_certdb_nss_cert_set(CamelCertDB *certdb, CamelCert *ccert, CERTCertificate *cert)
{
	char *dir, *path, *fingerprint;
	CamelStream *stream;
	struct stat st;
	
	fingerprint = ccert->fingerprint;
	
	if (ccert->rawcert == NULL)
		ccert->rawcert = g_byte_array_new ();
	
	g_byte_array_set_size (ccert->rawcert, cert->derCert.len);
	memcpy (ccert->rawcert->data, cert->derCert.data, cert->derCert.len);
	
	dir = g_strdup_printf ("%s/.camel_certs", getenv ("HOME"));
	if (stat (dir, &st) == -1 && mkdir (dir, 0700) == -1) {
		g_warning ("Could not create cert directory '%s': %s", dir, strerror (errno));
		g_free (dir);
		return;
	}
	
	path = g_strdup_printf ("%s/%s", dir, fingerprint);
	g_free (dir);
	
	stream = camel_stream_fs_new_with_name (path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (stream != NULL) {
		if (camel_stream_write (stream, ccert->rawcert->data, ccert->rawcert->len) == -1) {
			g_warning ("Could not save cert: %s: %s", path, strerror (errno));
			unlink (path);
		}
		camel_stream_close (stream);
		camel_object_unref (stream);
	} else {
		g_warning ("Could not save cert: %s: %s", path, strerror (errno));
	}
	
	g_free (path);
}


#if 0
/* used by the mozilla-like code below */
static char *
get_nickname(CERTCertificate *cert)
{
	char *server, *nick = NULL;
	int i;
	PRBool status = PR_TRUE;

	server = CERT_GetCommonName(&cert->subject);
	if (server == NULL)
		return NULL;

	for (i=1;status == PR_TRUE;i++) {
		if (nick) {
			g_free(nick);
			nick = g_strdup_printf("%s #%d", server, i);
		} else {
			nick = g_strdup(server);
		}
		status = SEC_CertNicknameConflict(server, &cert->derSubject, cert->dbhandle);
	}

	return nick;
}
#endif

static SECStatus
ssl_bad_cert (void *data, PRFileDesc *sockfd)
{
	gboolean accept;
	CamelCertDB *certdb = NULL;
	CamelCert *ccert = NULL;
	char *prompt, *cert_str, *fingerprint;
	CamelTcpStreamSSL *ssl;
	CERTCertificate *cert;
	SECStatus status = SECFailure;

	g_return_val_if_fail (data != NULL, SECFailure);
	g_return_val_if_fail (CAMEL_IS_TCP_STREAM_SSL (data), SECFailure);

	ssl = data;
	
	cert = SSL_PeerCertificate (sockfd);
	if (cert == NULL)
		return SECFailure;

	certdb = camel_certdb_get_default();
	ccert = camel_certdb_nss_cert_get(certdb, cert);
	if (ccert == NULL) {
		ccert = camel_certdb_nss_cert_add(certdb, cert);
		camel_cert_set_hostname(certdb, ccert, ssl->priv->expected_host);
	}

	if (ccert->trust == CAMEL_CERT_TRUST_UNKNOWN) {
		status = CERT_VerifyCertNow(cert->dbhandle, cert, TRUE, certUsageSSLClient, NULL);
		fingerprint = cert_fingerprint(cert);
		cert_str = g_strdup_printf (_("Issuer:            %s\n"
					      "Subject:           %s\n"
					      "Fingerprint:       %s\n"
					      "Signature:         %s"),
					    CERT_NameToAscii (&cert->issuer),
					    CERT_NameToAscii (&cert->subject),
					    fingerprint, status == SECSuccess?_("GOOD"):_("BAD"));
		g_free(fingerprint);

		/* construct our user prompt */
		prompt = g_strdup_printf (_("SSL Certificate check for %s:\n\n%s\n\nDo you wish to accept?"),
					  ssl->priv->expected_host, cert_str);
		g_free (cert_str);
	
		/* query the user to find out if we want to accept this certificate */
		accept = camel_session_alert_user (ssl->priv->session, CAMEL_SESSION_ALERT_WARNING, prompt, TRUE);
		g_free(prompt);
		if (accept) {
			camel_certdb_nss_cert_set(certdb, ccert, cert);
			camel_cert_set_trust(certdb, ccert, CAMEL_CERT_TRUST_FULLY);
			camel_certdb_touch(certdb);
		}
	} else {
		accept = ccert->trust != CAMEL_CERT_TRUST_NEVER;
	}

	camel_certdb_cert_unref(certdb, ccert);
	camel_object_unref(certdb);

	return accept ? SECSuccess : SECFailure;

#if 0
	int i, error;
	CERTCertTrust trust;
	SECItem *certs[1];
	int go = 1;
	char *host, *nick;

	error = PR_GetError();

	/* This code is basically what mozilla does - however it doesn't seem to work here
	   very reliably :-/ */
	while (go && status != SECSuccess) {
		char *prompt = NULL;

		printf("looping, error '%d'\n", error);

		switch(error) {
		case SEC_ERROR_UNKNOWN_ISSUER:
		case SEC_ERROR_CA_CERT_INVALID:
		case SEC_ERROR_UNTRUSTED_ISSUER:
		case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
			/* add certificate */
			printf("unknown issuer, adding ... \n");
			prompt = g_strdup_printf(_("Certificate problem: %s\nIssuer: %s"), cert->subjectName, cert->issuerName);

			if (camel_session_alert_user(ssl->priv->session, CAMEL_SESSION_ALERT_WARNING, prompt, TRUE)) {

				nick = get_nickname(cert);
				if (NULL == nick) {
					g_free(prompt);
					status = SECFailure;
					break;
				}

				printf("adding cert '%s'\n", nick);

				if (!cert->trust) {
					cert->trust = (CERTCertTrust*)PORT_ArenaZAlloc(cert->arena, sizeof(CERTCertTrust));
					CERT_DecodeTrustString(cert->trust, "P");
				}
		
				certs[0] = &cert->derCert;
				/*CERT_ImportCerts (cert->dbhandle, certUsageSSLServer, 1, certs, NULL, TRUE, FALSE, nick);*/
				CERT_ImportCerts(cert->dbhandle, certUsageUserCertImport, 1, certs, NULL, TRUE, FALSE, nick);
				g_free(nick);

				printf(" cert type %08x\n", cert->nsCertType);

				memset((void*)&trust, 0, sizeof(trust));
				if (CERT_GetCertTrust(cert, &trust) != SECSuccess) {
					CERT_DecodeTrustString(&trust, "P");
				}
				trust.sslFlags |= CERTDB_VALID_PEER | CERTDB_TRUSTED;
				if (CERT_ChangeCertTrust(cert->dbhandle, cert, &trust) != SECSuccess) {
					printf("couldn't change cert trust?\n");
				}

				/*status = SECSuccess;*/
#if 1
				/* re-verify? */
				status = CERT_VerifyCertNow(cert->dbhandle, cert, TRUE, certUsageSSLServer, NULL);
				error = PR_GetError();
				printf("re-verify status %d, error %d\n", status, error);
#endif

				printf(" cert type %08x\n", cert->nsCertType);
			} else {
				printf("failed/cancelled\n");
				go = 0;
			}

			break;
		case SSL_ERROR_BAD_CERT_DOMAIN:
			printf("bad domain\n");

			prompt = g_strdup_printf(_("Bad certificate domain: %s\nIssuer: %s"), cert->subjectName, cert->issuerName);

			if (camel_session_alert_user (ssl->priv->session, CAMEL_SESSION_ALERT_WARNING, prompt, TRUE)) {
				host = SSL_RevealURL(sockfd);
				status = CERT_AddOKDomainName(cert, host);
				printf("add ok domain name : %s\n", status == SECFailure?"fail":"ok");
				error = PR_GetError();
				if (status == SECFailure)
					go = 0;
			} else {
				go = 0;
			}

			break;
			
		case SEC_ERROR_EXPIRED_CERTIFICATE:
			printf("expired\n");

			prompt = g_strdup_printf(_("Certificate expired: %s\nIssuer: %s"), cert->subjectName, cert->issuerName);

			if (camel_session_alert_user(ssl->priv->session, CAMEL_SESSION_ALERT_WARNING, prompt, TRUE)) {
				cert->timeOK = PR_TRUE;
				status = CERT_VerifyCertNow(cert->dbhandle, cert, TRUE, certUsageSSLClient, NULL);
				error = PR_GetError();
				if (status == SECFailure)
					go = 0;
			} else {
				go = 0;
			}

			break;

		case SEC_ERROR_CRL_EXPIRED:
			printf("crl expired\n");

			prompt = g_strdup_printf(_("Certificate revocation list expired: %s\nIssuer: %s"), cert->subjectName, cert->issuerName);

			if (camel_session_alert_user(ssl->priv->session, CAMEL_SESSION_ALERT_WARNING, prompt, TRUE)) {
				host = SSL_RevealURL(sockfd);
				status = CERT_AddOKDomainName(cert, host);
			}

			go = 0;
			break;

		default:
			printf("generic error\n");
			go = 0;
			break;
		}

		g_free(prompt);
	}

	CERT_DestroyCertificate(cert);

	return status;
#endif
}

static PRFileDesc *
enable_ssl (CamelTcpStreamSSL *ssl, PRFileDesc *fd)
{
	PRFileDesc *ssl_fd;
	
	ssl_fd = SSL_ImportFD (NULL, fd ? fd : ssl->priv->sockfd);
	if (!ssl_fd)
		return NULL;
	
	SSL_OptionSet (ssl_fd, SSL_SECURITY, PR_TRUE);
	if (ssl->priv->flags & CAMEL_TCP_STREAM_SSL_ENABLE_SSL2)
		SSL_OptionSet (ssl_fd, SSL_ENABLE_SSL2, PR_TRUE);
	else
		SSL_OptionSet (ssl_fd, SSL_ENABLE_SSL2, PR_FALSE);
	if (ssl->priv->flags & CAMEL_TCP_STREAM_SSL_ENABLE_SSL3)
		SSL_OptionSet (ssl_fd, SSL_ENABLE_SSL3, PR_TRUE);
	else
		SSL_OptionSet (ssl_fd, SSL_ENABLE_SSL3, PR_FALSE);
	if (ssl->priv->flags & CAMEL_TCP_STREAM_SSL_ENABLE_TLS)
		SSL_OptionSet (ssl_fd, SSL_ENABLE_TLS, PR_TRUE);
	else
		SSL_OptionSet (ssl_fd, SSL_ENABLE_TLS, PR_FALSE);
	
	SSL_SetURL (ssl_fd, ssl->priv->expected_host);
	
	/*SSL_GetClientAuthDataHook (sslSocket, ssl_get_client_auth, (void *) certNickname);*/
	/*SSL_AuthCertificateHook (ssl_fd, ssl_auth_cert, (void *) CERT_GetDefaultCertDB ());*/
	SSL_BadCertHook (ssl_fd, ssl_bad_cert, ssl);
	
	ssl->priv->ssl_mode = TRUE;
	
	return ssl_fd;
}

static int
stream_connect (CamelTcpStream *stream, struct hostent *host, int port)
{
	CamelTcpStreamSSL *ssl = CAMEL_TCP_STREAM_SSL (stream);
	PRNetAddr netaddr;
	PRFileDesc *fd;
	
	g_return_val_if_fail (host != NULL, -1);
	
	memset ((void *) &netaddr, 0, sizeof (PRNetAddr));
#ifdef ENABLE_IPv6
	if (host->h_addrtype == AF_INET6)
		memcpy (&netaddr.ipv6.ip, host->h_addr, sizeof (netaddr.ipv6.ip));
	else
		memcpy (&netaddr.inet.ip, host->h_addr, sizeof (netaddr.inet.ip));
#else
	memcpy (&netaddr.inet.ip, host->h_addr, sizeof (netaddr.inet.ip));
#endif
	
	if (PR_InitializeNetAddr (PR_IpAddrNull, port, &netaddr) == PR_FAILURE) {
		set_errno (PR_GetError ());
		return -1;
	}
	
	fd = PR_OpenTCPSocket (host->h_addrtype);
	if (fd == NULL) {
		set_errno (PR_GetError ());
		return -1;
	}
	
	if (ssl->priv->ssl_mode) {
		PRFileDesc *ssl_fd;
		
		ssl_fd = enable_ssl (ssl, fd);
		if (ssl_fd == NULL) {
			int errnosave;
			
			set_errno (PR_GetError ());
			errnosave = errno;
			PR_Close (fd);
			errno = errnosave;
			
			return -1;
		}
		
		fd = ssl_fd;
	}
	
	if (PR_Connect (fd, &netaddr, CONNECT_TIMEOUT) == PR_FAILURE) {
		int errnosave;
		
		set_errno (PR_GetError ());
		if (errno == EINPROGRESS) {
			gboolean connected = FALSE;
			PRPollDesc poll;
			
			do {
				poll.fd = fd;
				poll.in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
				poll.out_flags = 0;
				
				if (PR_Poll (&poll, 1, CONNECT_TIMEOUT) == PR_FAILURE) {
					set_errno (PR_GetError ());
					goto exception;
				}
				
				if (PR_GetConnectStatus (&poll) == PR_FAILURE) {
					set_errno (PR_GetError ());
					if (errno != EINPROGRESS)
						goto exception;
				} else {
					connected = TRUE;
				}
			} while (!connected);
		} else {
		exception:
			errnosave = errno;
			PR_Close (fd);
			ssl->priv->sockfd = NULL;
			errno = errnosave;
			
			return -1;
		}
		
		errno = 0;
	}
	
	ssl->priv->sockfd = fd;

	return 0;
}


static int
stream_getsockopt (CamelTcpStream *stream, CamelSockOptData *data)
{
	PRSocketOptionData sodata;
	
	memset ((void *) &sodata, 0, sizeof (sodata));
	memcpy ((void *) &sodata, (void *) data, sizeof (CamelSockOptData));
	
	if (PR_GetSocketOption (((CamelTcpStreamSSL *)stream)->priv->sockfd, &sodata) == PR_FAILURE)
		return -1;
	
	memcpy ((void *) data, (void *) &sodata, sizeof (CamelSockOptData));
	
	return 0;
}

static int
stream_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data)
{
	PRSocketOptionData sodata;
	
	memset ((void *) &sodata, 0, sizeof (sodata));
	memcpy ((void *) &sodata, (void *) data, sizeof (CamelSockOptData));
	
	if (PR_SetSocketOption (((CamelTcpStreamSSL *)stream)->priv->sockfd, &sodata) == PR_FAILURE)
		return -1;
	
	return 0;
}

static CamelTcpAddress *
stream_get_local_address (CamelTcpStream *stream)
{
	PRFileDesc *sockfd = CAMEL_TCP_STREAM_SSL (stream)->priv->sockfd;
	int family, length;
	gpointer address;
	PRNetAddr addr;
	
	PR_GetSockName (sockfd, &addr);
	
	if (addr.inet.family == PR_AF_INET) {
		family = CAMEL_TCP_ADDRESS_IPv4;
		address = &addr.inet.ip;
		length = 4;
#ifdef ENABLE_IPv6
	} else if (addr.inet.family == PR_AF_INET6) {
		family = CAMEL_TCP_ADDRESS_IPv6;
		address = &addr.ipv6.ip;
		length = 16;
#endif
	} else
		return NULL;
	
	return camel_tcp_address_new (family, addr.inet.port, length, address);
}

static CamelTcpAddress *
stream_get_remote_address (CamelTcpStream *stream)
{
	PRFileDesc *sockfd = CAMEL_TCP_STREAM_SSL (stream)->priv->sockfd;
	int family, length;
	gpointer address;
	PRNetAddr addr;
	
	PR_GetPeerName (sockfd, &addr);
	
	if (addr.inet.family == PR_AF_INET) {
		family = CAMEL_TCP_ADDRESS_IPv4;
		address = &addr.inet.ip;
		length = sizeof (addr.inet.ip);
#ifdef ENABLE_IPv6
	} else if (addr.inet.family == PR_AF_INET6) {
		family = CAMEL_TCP_ADDRESS_IPv6;
		address = &addr.ipv6.ip;
		length = sizeof (addr.ipv6.ip);
#endif
	} else
		return NULL;
	
	return camel_tcp_address_new (family, addr.inet.port, length, address);
}

#endif /* HAVE_NSS */
